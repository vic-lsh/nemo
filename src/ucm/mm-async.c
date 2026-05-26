#include "mm-async.h"

#include <string.h>
#include <sys/time.h>

#include "core-util.h"
#include "ds/spsc-ring.h"
#include "util/log.h"

#define HOT_RING_REQS_THRESHOLD (1024 * 1024)
#define COLD_RING_REQS_THRESHOLD (1024 * 1024)
#define COOLING_PAGES (16384)

cool_stats_t cool_stats;

_Atomic volatile uint64_t free_ring_requests = 0;
_Atomic volatile uint64_t hot_ring_requests = 0;
_Atomic volatile uint64_t cold_ring_requests = 0;

_Atomic volatile uint64_t free_ring_requests_handled = 0;
_Atomic volatile uint64_t hot_ring_requests_handled = 0;
_Atomic volatile uint64_t cold_ring_requests_handled = 0;

void __proc_cool_slowmem_async(struct hemem_process *proc, bool force) {
    if (force || !proc->mm.need_cool_slowmem) {
        proc->mm.need_cool_slowmem = true;
        proc->process_clock[SLOWMEM]++;
        proc->mm.cools++;
        cool_stats.cools_started[SLOWMEM]++;
    }
}

void mm_request_soft_cool_slowmem(struct hemem_process *proc) {
    __proc_cool_slowmem_async(proc, /* force = */ false);
}

void mm_request_cool_slowmem(struct hemem_process *proc) {
    __proc_cool_slowmem_async(proc, /* force = */ true);
}

void __proc_cool_fastmem_async(struct hemem_process *proc, bool force) {
    if (force || !proc->mm.need_cool_fastmem) {
        proc->mm.need_cool_fastmem = true;
        proc->process_clock[FASTMEM]++;
        proc->mm.cools++;
        cool_stats.cools_started[FASTMEM]++;
    }
}

void mm_request_soft_cool_fastmem(struct hemem_process *proc) {
    __proc_cool_fastmem_async(proc, /* force = */ false);
}

void mm_request_cool_fastmem(struct hemem_process *proc) {
    __proc_cool_fastmem_async(proc, /* force = */ true);
}

bool mm_mark_page_hot_async(struct hemem_process *process,
                            struct hemem_page *page) {
    bool expected = false;
    if (!unlikely(atomic_compare_exchange_strong(&page->ring_present, &expected,
                                                 true))) {
        return false;
    }
    ring_buf_put(process->mm.hot_ring, (uint64_t *)page);
    hot_ring_requests++;
    return true;
}

bool mm_mark_page_cold_async(struct hemem_process *process,
                             struct hemem_page *page) {
    bool expected = false;
    if (!unlikely(atomic_compare_exchange_strong(&page->ring_present, &expected,
                                                 true))) {
        return false;
    }
    ring_buf_put(process->mm.cold_ring, (uint64_t *)page);
    cold_ring_requests++;
    return true;
}

// moves page to hot list -- called by migrate thread
void make_hot(struct hemem_process *process, struct hemem_page *page,
              uint64_t new_hot) {
    assert(page != NULL);
    assert(page->va != 0);

    assert(page->pid == process->pid);

    if (page->hot == new_hot) {
        if (page->in_dram) {
            assert(page->list == &(process->mm.fastmem_lists[new_hot]));
        } else {
            assert(page->list == &(process->mm.slowmem_lists[new_hot]));
        }

        return;
    }

    if (page->in_dram) {
        assert(page->list == &(process->mm.fastmem_lists[page->hot]));
        page_list_remove(&(process->mm.fastmem_lists[page->hot]), page);
        page->hot = new_hot;
        enqueue_page(&(process->mm.fastmem_lists[page->hot]), page);
    } else {
        assert(page->list == &(process->mm.slowmem_lists[page->hot]));
        page_list_remove(&(process->mm.slowmem_lists[page->hot]), page);
        page->hot = new_hot;
        enqueue_page(&(process->mm.slowmem_lists[page->hot]), page);
    }
}

// moves page to cold list -- called by migrate thread
void make_cold(struct hemem_process *process, struct hemem_page *page,
               uint64_t new_hot) {
    assert(page != NULL);
    assert(page->va != 0);

    assert(page->pid == process->pid);

    if (page->hot == new_hot) {
        if (page->in_dram) {
            assert(page->list == &(process->mm.fastmem_lists[new_hot]));
        } else {
            assert(page->list == &(process->mm.slowmem_lists[new_hot]));
        }

        return;
    }

    if (page->in_dram) {
        assert(page->list == &(process->mm.fastmem_lists[page->hot]));
        page_list_remove(&(process->mm.fastmem_lists[page->hot]), page);
        page->hot = new_hot;
        enqueue_page(&(process->mm.fastmem_lists[new_hot]), page);
    } else {
        assert(page->list == &(process->mm.slowmem_lists[page->hot]));
        page_list_remove(&(process->mm.slowmem_lists[page->hot]), page);
        page->hot = new_hot;
        enqueue_page(&(process->mm.slowmem_lists[page->hot]), page);
    }
}

static struct hemem_page *__partial_cool_impl(struct hemem_process *process,
                                              bool fastmem) {
    struct hemem_page *p, *current;
    uint64_t tmp_accesses[NPBUFTYPES];
    int cool_list_index;
    struct page_list *cur_bins;
    int i, j;
    uint64_t new_hotness;

    struct timeval start, end;

    // do we even need to be cooling right now? If not, just return
    // where we left off last time we needed to cool. Next time this function
    // is called when cooling is needed, we pick up from here
    if (fastmem && !(process->mm.need_cool_fastmem)) {
        return process->mm.cur_cool_in_fastmem;
    } else if (!fastmem && !(process->mm.need_cool_slowmem)) {
        return process->mm.cur_cool_in_slowmem;
    }

    // we cool backwards through the page lists, the same order the pages
    // were inserted. The idea is, in this way, we cool the oldest pages first
    if (fastmem && (process->mm.cur_cool_in_fastmem == NULL) &&
        (process->mm.cur_cool_in_fastmem_list == 0)) {
        for (i = NUM_HOTNESS_LEVELS - 1;
             i > 0 && process->mm.cur_cool_in_fastmem == NULL; i--) {
            // find the current oldest hottest page in DRAM
            process->mm.cur_cool_in_fastmem = process->mm.fastmem_lists[i].last;
            process->mm.cur_cool_in_fastmem_list = i;
        }
        // dram hot list might be empty, in which case we have nothing to cool
        if (process->mm.cur_cool_in_fastmem == NULL) {
            process->mm.cur_cool_in_fastmem_list = 0;
            process->mm.need_cool_fastmem = false;
            cool_stats.cools_completed[FASTMEM]++;
            return NULL;
        }
    } else if ((!fastmem) && (process->mm.cur_cool_in_slowmem == NULL) &&
               (process->mm.cur_cool_in_slowmem_list == 0)) {
        for (i = NUM_HOTNESS_LEVELS - 1;
             i > 0 && process->mm.cur_cool_in_slowmem == NULL; i--) {
            // find the current oldest hottest page in NVM
            process->mm.cur_cool_in_slowmem = process->mm.slowmem_lists[i].last;
            process->mm.cur_cool_in_slowmem_list = i;
        }
        // nvm hot list might be empty, in which case we have nothing to cool
        if (process->mm.cur_cool_in_slowmem == NULL) {
            process->mm.cur_cool_in_slowmem_list = 0;
            process->mm.need_cool_slowmem = false;
            cool_stats.cools_completed[SLOWMEM]++;
            return NULL;
        }
    }

    gettimeofday(&start, NULL);

    // set hot and cold list pointers as appropriate for memory type
    // set current to the current cooled page for the memory type here as well
    if (fastmem) {
        current = process->mm.cur_cool_in_fastmem;
        cool_list_index = process->mm.cur_cool_in_fastmem_list;
        cur_bins = process->mm.fastmem_lists;
        if (current) {
            assert(current->pid == process->pid);
            assert(current->list == &(cur_bins[current->hot]));
        }
    } else {
        current = process->mm.cur_cool_in_slowmem;
        cool_list_index = process->mm.cur_cool_in_slowmem_list;
        cur_bins = process->mm.slowmem_lists;
        if (current) {
            assert(current->pid == process->pid);
            assert(current->list == &(cur_bins[current->hot]));
        }
    }

    // start from the current cooled page. This is either where we left off
    // last time or the end of the page list if we've gone throug the whole list
    p = current;
    for (i = 0; i < COOLING_PAGES; i++) {
        if (p == NULL) {
            // we have finished iterating the current list.
            // move to the next non-empty list.
            for (j = cool_list_index - 1; j > 0 && p == NULL; j--) {
                p = cur_bins[j].last;
                cool_list_index = j;
            }
            if (p == NULL) {
                // we've cooled everything thre is to cool
                cool_list_index = 0;
                current = NULL;
                if (fastmem) {
                    process->mm.need_cool_fastmem = false;
                    cool_stats.cools_completed[FASTMEM]++;
                } else {
                    process->mm.need_cool_slowmem = false;
                    cool_stats.cools_completed[SLOWMEM]++;
                }
                break;
            }
        }

        // sanity check we grabbed a page in the appropriate memory type and
        // from the appropriate list
        if (fastmem) {
            assert(p->pid == process->pid);
            assert(p->in_dram);
            assert(p->list == &(process->mm.fastmem_lists[p->hot]));
        } else {
            assert(p->pid == process->pid);
            assert(!p->in_dram);
            assert(p->list == &(process->mm.slowmem_lists[p->hot]));
        }

        // compute the access samples this page would have had if it were up to
        // date with cooling
        for (int j = 0; j < NPBUFTYPES; j++) {
            tmp_accesses[j] = p->accesses[j] >>
                              (process->process_clock[j] - p->local_clock[j]);
        }

        struct hemem_page *curr = p;
        // look up prev-page now before potentially moving the current page
        p = prev_page(&(cur_bins[cool_list_index]), p);

        // is the page still hot if it was up to date with cooling?
        new_hotness =
            access_to_index(tmp_accesses[FASTMEM], tmp_accesses[SLOWMEM]);
        if (new_hotness != curr->hot) {
            curr->hot = new_hotness;
            page_list_remove(curr->list, curr);
            enqueue_page(&(cur_bins[new_hotness]), curr);
        }
    }

    if (fastmem) {
        process->mm.cur_cool_in_fastmem_list = cool_list_index;
    } else {
        process->mm.cur_cool_in_slowmem_list = cool_list_index;
    }

    gettimeofday(&end, NULL);
    LOG_TIME("partial_cool: %f s\n", elapsed_secs(&start, &end));

    return current;
}

struct hemem_page *incremental_fastmem_cooling(struct hemem_process *process) {
    return __partial_cool_impl(process, /* fastmem = */ true);
}

struct hemem_page *incremental_slowmem_cooling(struct hemem_process *process) {
    return __partial_cool_impl(process, /* fastmem = */ false);
}

// convenience function for catching pages where we migrate or otherwise move a
// page from one list to another. If the page in question was our "bookmark"
// pointer for cooling, then we need to update that bookmark. Here, for
// simplicity, we just reset our bookmark to the end of the hot list
void update_current_cool_page(struct hemem_process *process,
                              struct hemem_page *page) {
    if (page == process->mm.cur_cool_in_fastmem) {
        // first a set of sanity checks
        assert(page->pid == process->pid);
        assert(page->in_dram);
        assert(page->list == &(process->mm.fastmem_lists[page->hot]));
        // then just reset the bookmark pointer to the last page in list
        // just restart
        process->mm.cur_cool_in_fastmem = NULL;
        process->mm.cur_cool_in_fastmem_list = 0;
    } else if (page == process->mm.cur_cool_in_slowmem) {
        // first, a bunch of sanity checks
        assert(page->pid == process->pid);
        assert(!(page->in_dram));
        assert(page->list == &(process->mm.slowmem_lists[page->hot]));
        // then just reset the bookmark pointer to the last page in list
        process->mm.cur_cool_in_slowmem = NULL;
        process->mm.cur_cool_in_slowmem_list = 0;
    }
}

static void handle_free_page_ring(struct hemem_process *process) {
    struct hemem_page *page = NULL;

    // free pages using free page ring buffer
    // we take all pages from the free ring rather than until
    // meeting some threshold of requests handled to free up
    // as much space as quick as possible
    while (!ring_buf_empty(process->mm.free_page_ring)) {
        struct page_list *list;
        pthread_mutex_lock(&(process->mm.free_page_ring_lock));
        page = (struct hemem_page *)ring_buf_get(process->mm.free_page_ring);
        pthread_mutex_unlock(&(process->mm.free_page_ring_lock));
        if (page == NULL) {
            // ring buffer was empty
            break;
        }

        list = page->list;
        assert(list != NULL);

        assert(page->pid == process->pid);
        // list sanity checks
        // either in the correct list or in a ring.
        if (page->in_dram) {
            assert(page->list == &(process->mm.fastmem_lists[page->hot]) ||
                   page->ring_present);
        } else {
            assert(page->list == &(process->mm.slowmem_lists[page->hot]) ||
                   page->ring_present);
        }

        // check whether the page being freed is our bookmark cool page
        update_current_cool_page(process, page);

        // remove page from its list and put it into the appropriate free list
        page_list_remove(list, page);

        // reset page stats
        page->present = false;
        page->pid = -1;
        page->hot = COLD;
        for (int i = 0; i < NPBUFTYPES; i++) {
            page->accesses[i] = 0;
            page->tot_accesses[i] = 0;
        }

        if (page->in_dram) {
            return_free_fastmem_page(page);
            process->mm.current_fastmem -= pt_to_pagesize(page->pt);
        } else {
            return_free_slowmem_page(page);
            process->mm.current_slowmem -= pt_to_pagesize(page->pt);
        }
        page->in_free_ring = false;

        free_ring_requests_handled++;
    }
}

// handle hot requests from hot buffer by moving pages to hot list
void handle_hot_requests(struct hemem_process *process) {
    int num_ring_reqs = 0;
    struct hemem_page *page = NULL;
    uint64_t tmp_accesses[NPBUFTYPES];
    uint64_t new_hotness;

    uint64_t prev_hot_requests_handled = hot_ring_requests_handled;

    while (!ring_buf_empty(process->mm.hot_ring) &&
           num_ring_reqs < HOT_RING_REQS_THRESHOLD) {
        page = (struct hemem_page *)ring_buf_get(process->mm.hot_ring);
        if (page == NULL) {
            LOG("hot ring was empty\n");
            // ring buffer was empty
            break;
        }

        if (!page->present) {
            assert_page_freed(page);
            LOG_DEBUG("page was freed\n");
            hot_ring_requests_handled++;
            continue;
        }

        assert(page->pid == process->pid);

        // compute the access samples this page would have had if it were up to
        // date with cooling
        for (int j = 0; j < NPBUFTYPES; j++) {
            tmp_accesses[j] = page->accesses[j] >> (process->process_clock[j] -
                                                    page->local_clock[j]);
        }
        new_hotness =
            access_to_index(tmp_accesses[FASTMEM], tmp_accesses[SLOWMEM]);
        // LOG("Hot request page %p %lu -> %d dram %lu nvm %lu\n",
        //     (void *)page->va, page->hot, new_hotness, tmp_accesses[FASTMEM],
        //     tmp_accesses[SLOWMEM]);

        // is page even still hot?
        if (new_hotness < page->hot) {
            // LOG("Page no longer hot: new hotness %d page->hot %lu\n",
            //     new_hotness, page->hot);
            // page has been cooled and is no longer hot, just move to cold list
            // first, check to see if we need to update our cooling bookmark
            update_current_cool_page(process, page);
            page->ring_present = false;
            num_ring_reqs++;
            make_cold(process, page, new_hotness);
            hot_ring_requests_handled++;
            continue;
        }

        // page is still hot, so we can move it to the hot list
        // do we need to update our cooling bookmark?
        update_current_cool_page(process, page);
        page->ring_present = false;
        num_ring_reqs++;
        make_hot(process, page, new_hotness);
        // printf("hot ring, hot pages:%llu\n", num_ring_reqs);

        hot_ring_requests_handled++;
    }

    __attribute__((unused)) uint64_t handled =
        hot_ring_requests_handled - prev_hot_requests_handled;
    LOG_DEBUG("Handled %lu hot requests\n", handled);
}

// handle cold requests from cold buffer by moving pages to cold list
void handle_cold_requests(struct hemem_process *process) {
    int num_ring_reqs = 0;
    struct hemem_page *page = NULL;
    uint64_t tmp_accesses[NPBUFTYPES];
    uint64_t new_hotness;

    uint64_t prev_cold_requests_handled = cold_ring_requests_handled;

    while (!ring_buf_empty(process->mm.cold_ring) &&
           num_ring_reqs < COLD_RING_REQS_THRESHOLD) {
        page = (struct hemem_page *)ring_buf_get(process->mm.cold_ring);
        if (page == NULL) {
            // ring buffer was empty
            break;
        }

        if (!page->present) {
            assert_page_freed(page);
            cold_ring_requests_handled++;
            continue;
        }

        assert(page->pid == process->pid);

        // compute the access samples this page would have had if it were up to
        // date with cooling
        for (int j = 0; j < NPBUFTYPES; j++) {
            tmp_accesses[j] = page->accesses[j] >> (process->process_clock[j] -
                                                    page->local_clock[j]);
        }
        new_hotness =
            access_to_index(tmp_accesses[FASTMEM], tmp_accesses[SLOWMEM]);
        // LOG("Cold request page %p %lu -> %d\n", (void *)page->va, page->hot,
        //     new_hotness);

        if (new_hotness >= page->hot) {
            // page is now hot and should actually move to the hot list
            // if not already there
            update_current_cool_page(process, page);
            page->ring_present = false;
            num_ring_reqs++;
            make_hot(process, page, new_hotness);
            cold_ring_requests_handled++;
            continue;
        }

        // check if we need to update our cooling bookmark, then move page
        // to the cold list
        update_current_cool_page(process, page);
        page->ring_present = false;
        num_ring_reqs++;
        make_cold(process, page, new_hotness);
        // printf("cold ring, cold pages:%llu\n", num_ring_reqs);
        cold_ring_requests_handled++;
    }

    __attribute__((unused)) uint64_t handled =
        cold_ring_requests_handled - prev_cold_requests_handled;
    LOG_DEBUG("Handled %lu cold requests\n", handled);
}

// The PEBS thread communicates with the policy thread via request rings. The
// only thread allowed to maniuplate the hot and cold lists is the policy thread
// to prevent deadlocks or race conditions, and the ring buffers solve that.
// Here, the policy thread will handle the ring buffer requests by placing
// the pages in the ring buffers into the appropriate lists
void mm_drain_async_requests(struct hemem_process *process) {
    handle_free_page_ring(process);
    handle_hot_requests(process);
    handle_cold_requests(process);
}

void mm_internal_enqueue_page_removal(struct hemem_process *process,
                                      struct hemem_page *page) {
    assert(page != NULL);

    // LOG("pebs: remove page, put this page into free_page_ring: va: 0x%lx\n",
    //     page->va);

    if (page->in_free_ring) {
        // requesting to free a page that is already being freed isn't a
        // necessarily a bug. for example, the app could first request to free
        // this page; while the page is in the free ring, the app exits and
        // the ucm requests frees all the pages again.
        return;
    }

    page->in_free_ring = true;

    while (ring_buf_full(process->mm.free_page_ring))
        ;
    pthread_mutex_lock(&(process->mm.free_page_ring_lock));
    ring_buf_put(process->mm.free_page_ring, (uint64_t *)page);
    free_ring_requests++;
    pthread_mutex_unlock(&(process->mm.free_page_ring_lock));
}

cool_stats_t *mm_get_cool_stats() { return &cool_stats; }

void mm_async_init() { memset(&cool_stats, 0, sizeof(cool_stats_t)); }
