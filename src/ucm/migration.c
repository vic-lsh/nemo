#include "migration.h"

#include <errno.h>
#include <linux/userfaultfd.h>
#include <sys/ioctl.h>

#include "core-util.h"
#include "core.h"
#include "hemem-shared.h"
#include "ipc/ipc.h"
#include "mm.h"
#include "physmem/config.h"
#include "policy/policy.h"
#include "stats/stats.h"
#include "support/memops.h"
#include "type/process.h"
#include "ucm.h"
#include "util/compiler.h"
#include "util/log.h"
#include "util/timer.h"

// Execute a block of code with a likely branch prediction.
// This macro uses an `if` statement that is always true at compile time
// to hint at the likely execution path for branch prediction optimization.
#define likely_block(body)                \
    do {                                  \
        int __likely_case = 1;            \
        if (likely(__likely_case == 1)) { \
            body                          \
        }                                 \
    } while (0)

static void ucm_wp_page(struct hemem_page *page, bool protect) {
    uint64_t addr = page->va;
    struct uffdio_writeprotect wp;
    struct timeval start, end;
    int ret;
    uint64_t pagesize = pt_to_pagesize(page->pt);

    assert(addr != 0);
    assert(addr % HUGEPAGE_SIZE == 0);

    gettimeofday(&start, NULL);
    wp.range.start = addr;
    wp.range.len = pagesize;
    wp.mode = (protect ? UFFDIO_WRITEPROTECT_MODE_WP : 0);
    ret = ioctl(page->uffd, UFFDIO_WRITEPROTECT, &wp);

    if (ret < 0) {
        if (errno == EBADF || errno == ENOENT) {
            // page was freed, probably fine to just ignore
            LOG_WARN("tried to write-protect a freed page\n");
        } else {
            perror("uffdio writeprotect");
            LOG_ERR("failed to write-protect page va %p\n", (void *)page->va);
            assert(0);
        }
    }
    gettimeofday(&end, NULL);

    LOG_TIME("uffdio_writeprotect: %f s\n", elapsed_secs(&start, &end));
}

static void ucm_migrate_one_page(
    struct hemem_process *process, struct hemem_page *page,
    // NOTE: use const bool to hint generating two migration code paths
    const bool dst_is_fastmem, uint64_t dst_offset) {
    void *old_addr;
    void *new_addr;
    struct timeval migrate_start, migrate_end;
    struct timeval remap_start, remap_end;
    struct timeval start, end;
    uint64_t old_addr_offset, new_addr_offset;
    uint64_t pagesize;
    struct hemem_page_app page_app;

    assert(page != NULL);

    if (dst_is_fastmem) {
        assert(!page->in_dram);
    } else {
        assert(page->in_dram);
    }

    gettimeofday(&migrate_start, NULL);

    pagesize = pt_to_pagesize(page->pt);

    old_addr_offset = page->devdax_offset;
    new_addr_offset = dst_offset;

    size_t slowmem_sz = get_slowmem_size();
    size_t fastmem_sz = get_fastmem_size();

    size_t old_tier_sz;
    size_t new_tier_sz;
    if (dst_is_fastmem) {
        new_tier_sz = fastmem_sz;
        old_tier_sz = slowmem_sz;
    } else {
        new_tier_sz = slowmem_sz;
        old_tier_sz = fastmem_sz;
    }

    assert((uint64_t)old_addr_offset < old_tier_sz);
    assert((uint64_t)old_addr_offset + pagesize <= old_tier_sz);
    assert((uint64_t)new_addr_offset < new_tier_sz);
    assert((uint64_t)new_addr_offset + pagesize <= new_tier_sz);

    old_addr =
        devdax_offset_to_addr(old_addr_offset, /*is_fastmem=*/!dst_is_fastmem);
    new_addr =
        devdax_offset_to_addr(new_addr_offset, /*is_fastmem=*/dst_is_fastmem);

    assert((uint64_t)old_addr % pagesize == 0);
    assert((uint64_t)new_addr % pagesize == 0);

    gettimeofday(&start, NULL);
    hemem_memcpy(new_addr, old_addr, pagesize);
    gettimeofday(&end, NULL);

    LOG_TIME("memcpy_to_dram: %f s\n", elapsed_secs(&start, &end));

    if (dst_is_fastmem) {
        ucm_stats_record_migration_up(process, page, pagesize);
    } else {
        ucm_stats_record_migration_down(process, page, pagesize);
    }

    page->devdax_offset = dst_offset;

    page_app.va = page->va;
    page_app.devdax_offset = page->devdax_offset;
    page_app.in_dram = dst_is_fastmem;
    page_app.pt = page->pt;

    gettimeofday(&remap_start, NULL);
    enum ucm_ipc_result rc = ipc_remap_pages(process, &page_app, 1);
    gettimeofday(&remap_end, NULL);

    LOG_TIME("hemem_remap_page_%s: %f s\n", (dst_is_fastmem) ? "up" : "down",
             elapsed_secs(&remap_start, &remap_end));
    gettimeofday(&migrate_end, NULL);
    LOG_TIME("hemem_migrate_%s: %f s\n", (dst_is_fastmem) ? "up" : "down",
             elapsed_secs(&migrate_start, &migrate_end));

    switch (rc) {
        case IPC_SUCCESS:
            likely_block({
                page->in_dram = dst_is_fastmem;
                if (dst_is_fastmem) {
                    mm_remove_page_from_cxl(&process->mm, page);
                } else {
                    mm_add_page_to_cxl(&process->mm, page);
                }
            });
            break;
        case IPC_APP_EXITED:
            ucm_remove_process(process);
            break;
        default:
            LOG_ERR("ipc_remap_pages erred in page promotion %d\n", rc);
            assert(0);
    }
}

static void ucm_migrate_up(struct hemem_process *process,
                           struct hemem_page *page, uint64_t fastmem_offset) {
    ucm_migrate_one_page(process, page, /*dst_is_fastmem=*/true,
                         fastmem_offset);
}

static void ucm_migrate_down(struct hemem_process *process,
                             struct hemem_page *page, uint64_t slowmem_offset) {
    ucm_migrate_one_page(process, page, /*dst_is_fastmem=*/false,
                         slowmem_offset);
}

void demote_one_page(struct hemem_process *process, struct hemem_page *page,
                     uint64_t slowmem_offset) {
    struct timeval start, end;

    gettimeofday(&start, NULL);

    assert(page->pid == process->pid);

    wall_and_cpu_time_t lock_time;
    TIME_OP(lock_time, { pthread_mutex_lock(&page->remap_lock); });
    if (lock_time.wall_time_us > 200) {
        LOG_WARN("demote_one_page lock time %f us page %p\n",
                 lock_time.wall_time_us, (void *)page->va);
    }
    page->migrating = true;
    page->migration_uffd_unmap_pending = true;
    page->last_migrated_epoch = ucm_get_curr_epoch();

    // no need to increment -- if there's any existing compensation value
    // it must be out-of-date since we limit one page's migration frequency.
    size_t migration_access_comp = pt_to_cacheline_count(page->pt);
    uint64_t curr = page->migration_access_comp;
    page->migration_access_comp = max(curr, migration_access_comp);
    ucm_wp_page(page, true);
    ucm_migrate_down(process, page, slowmem_offset);
    process->mm.current_fastmem -= pt_to_pagesize(page->pt);
    process->mm.current_slowmem += pt_to_pagesize(page->pt);

    assert(!page->in_dram);
    page->migrating = false;
    pthread_mutex_unlock(&page->remap_lock);

    gettimeofday(&end, NULL);
    LOG_TIME("migrate_down: %f s\n", elapsed_secs(&start, &end));
}

void promote_one_page(struct hemem_process *process, struct hemem_page *page,
                      uint64_t fastmem_offset) {
    struct timeval start, end;

    gettimeofday(&start, NULL);

    LOG_DEBUG("promoting page %p from offt %lu to %lu\n", (void *)page->va,
              page->devdax_offset, fastmem_offset);

    assert(page->pid == process->pid);

    wall_and_cpu_time_t lock_time;
    TIME_OP(lock_time, { pthread_mutex_lock(&page->remap_lock); });
    if (lock_time.wall_time_us > 200) {
        LOG_WARN("promote_one_page lock time %f us page %p\n",
                 lock_time.wall_time_us, (void *)page->va);
    }
    page->migrating = true;
    page->migration_uffd_unmap_pending = true;
    page->last_migrated_epoch = ucm_get_curr_epoch();

    // no need to increment -- if there's any existing compensation value
    // it must be out-of-date since we limit one page's migration frequency.
    uint64_t migration_access_comp = pt_to_cacheline_count(page->pt);
    uint64_t curr = page->migration_access_comp;
    page->migration_access_comp = max(curr, migration_access_comp);
    ucm_wp_page(page, true);
    ucm_migrate_up(process, page, fastmem_offset);
    process->mm.current_fastmem += pt_to_pagesize(page->pt);
    process->mm.current_slowmem -= pt_to_pagesize(page->pt);

    assert(page->in_dram);
    page->migrating = false;
    // notify that remapping is done
    pthread_mutex_unlock(&page->remap_lock);

    gettimeofday(&end, NULL);
    LOG_TIME("migrate_up: %f s\n", elapsed_secs(&start, &end));
}

uint64_t process_migrate_down_bytes(struct hemem_process *process,
                                    uint64_t migrate_down_bytes) {
    uint64_t migrated_bytes;
    uint64_t old_offset;
    struct hemem_page *cp, *np;
    struct timeval now;

    for (migrated_bytes = 0; migrated_bytes < migrate_down_bytes;) {
        if (migrated_bytes >= PEBS_MIGRATE_RATE) {
            break;
        }

        cp = find_demotion_candidate(process);
        if (cp == NULL) {
            // no dram pages to move down
            break;
        }

        assert(cp->pid == process->pid);
        assert(cp->va);

        if (cp == process->mm.cur_cool_in_fastmem) {
            assert(cp->in_dram);
            // then just reset the bookmark pointer to the last page in list
            // just restart
            process->mm.cur_cool_in_fastmem = NULL;
            process->mm.cur_cool_in_fastmem_list = 0;
        }

        np = get_free_slowmem_page();
        if (np != NULL) {
            assert(!(np->present));
            assert(np->pid == -1);

            old_offset = cp->devdax_offset;
            if (!mm_find_page(&process->mm, cp->va)) {
                LOG_WARN("can't find page for va %p before migrate down\n",
                         (void *)cp->va);
                continue;
            }
            demote_one_page(process, cp, np->devdax_offset);
            np->devdax_offset = old_offset;
            np->in_dram = true;
            np->present = false;
            assert(np->hot == COLD);
            for (int i = 0; i < NPBUFTYPES; i++) {
                assert(np->accesses[i] == 0);
                assert(np->tot_accesses[i] == 0);
            }

            enqueue_page(&(process->mm.slowmem_lists[cp->hot]), cp);

            return_free_fastmem_page(np);
            migrated_bytes += pt_to_pagesize(cp->pt);
        } else {
            // no free NVM pages to move, so put it back into
            // dram cold list and bail out
            gettimeofday(&now, NULL);
            LOG("policy thread found no slowmem free pages\n");
            enqueue_page(&(process->mm.fastmem_lists[cp->hot]), cp);
            break;
        }
        // assert(np != NULL);
    }
    gettimeofday(&now, NULL);
    // LOG("%f\tprocess %d has migrated %ld bytes down\n", elapsed(&startup,
    // &now), process->pid, migrated_bytes);
    return migrated_bytes;
}

uint64_t process_migrate_up_bytes(struct hemem_process *process,
                                  uint64_t migrate_up_bytes) {
    uint64_t migrated_bytes;
    uint64_t old_offset;
    uint64_t tmp_accesses[NPBUFTYPES];
    struct hemem_page *p, *np;
    struct timeval now;
    int new_hotness;

    for (migrated_bytes = 0; migrated_bytes < migrate_up_bytes;) {
        if (migrated_bytes >= PEBS_MIGRATE_RATE) {
            break;
        }
        p = find_promotion_candidate(process);
        if (p == NULL) {
            // truly nothing in any list
            break;
        }

        if (p == process->mm.cur_cool_in_slowmem) {
            assert(!p->in_dram);
            // then just reset the bookmark pointer to the last page in list
            // just restart
            process->mm.cur_cool_in_slowmem = NULL;
            process->mm.cur_cool_in_slowmem_list = 0;
        }

        assert(p->pid == process->pid);
        assert(p->va);

        // compute the access samples this page would have had if it were up to
        // date with cooling
        for (int j = 0; j < NPBUFTYPES; j++) {
            tmp_accesses[j] = p->accesses[j] >>
                              (process->process_clock[j] - p->local_clock[j]);
        }

        new_hotness =
            access_to_index(tmp_accesses[FASTMEM], tmp_accesses[SLOWMEM]);
        /*
        if (new_hotness == COLD) {
          p->hot = new_hotness;
          enqueue_page(&(process->mm.slowmem_lists[p->hot]), p);
          continue;
        }
        */

        np = get_free_fastmem_page();
        if (np == NULL) {
            gettimeofday(&now, NULL);
            LOG("policy thread found no fastmem free pages\n");

            p->hot = new_hotness;
            enqueue_page(&(process->mm.slowmem_lists[p->hot]), p);
            break;
            //}
        }
        assert(!np->present);
        assert(np->pid == -1);

        if (!mm_find_page(&process->mm, p->va)) {
            LOG_ERR("can't find page for va %p before migrate up\n",
                    (void *)p->va);
        }

        old_offset = p->devdax_offset;
        promote_one_page(process, p, np->devdax_offset);
        np->devdax_offset = old_offset;
        np->in_dram = false;
        np->present = false;
        assert(np->hot == COLD);
        for (int i = 0; i < NPBUFTYPES; i++) {
            assert(np->accesses[i] == 0);
            assert(np->tot_accesses[i] == 0);
        }

        p->hot = new_hotness;
#ifdef CONFIG_PEBS
        enqueue_page(&(process->mm.fastmem_lists[p->hot]), p);
#else
        enqueue_page(&(process->mm.fastmem_lists[COLD]), p);
#endif

        return_free_slowmem_page(np);
        migrated_bytes += pt_to_pagesize(p->pt);
    }
    gettimeofday(&now, NULL);
    // LOG("%f\tprocess %d has migrated %ld bytes up\n", elapsed(&startup,
    // &now), process->pid, migrated_bytes);
    return migrated_bytes;
}
