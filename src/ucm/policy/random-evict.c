#include "random-evict.h"

#include <stdlib.h>

#include "core.h"
#include "ds/fifo.h"
#include "ds/page-iterator.h"
#include "migration.h"
#include "mm-async.h"
#include "mm.h"
#include "proc-mgr.h"
#include "type/page.h"
#include "type/process.h"
#include "ucm.h"
#include "util/compiler.h"
#include "util/log.h"

#define MAX_MIGRATION_PER_EPOCH 256
#define MIGRATION_EPOCH_GAP 10

// caller must have removed the page to demote from hotness bins.
static bool do_migrate_down(struct hemem_page* curr_p,
                            struct hemem_process* process) {
    struct hemem_page* new_p = get_free_slowmem_page();
    if (unlikely(!new_p)) {
        // no free memory in slowmem -- cannot demote
        enqueue_page(&(process->mm.fastmem_lists[curr_p->hot]), curr_p);
        return false;
    }

    assert(!(new_p->present));
    assert(new_p->pid == -1);

    uint64_t old_offset = curr_p->devdax_offset;
    // if (!find_page(process, curr_p->va)) {
    //     LOG_ERR("can't find page for va %p before migrate down\n",
    //             (void*)curr_p->va);
    //     return false;
    // }

    demote_one_page(process, curr_p, new_p->devdax_offset);

    assert(!curr_p->in_dram);

    enqueue_page(&(process->mm.slowmem_lists[curr_p->hot]), curr_p);

    // the swap is complete -- page previously in slowmem is in fastmem
    new_p->devdax_offset = old_offset;
    new_p->in_dram = true;
    new_p->present = false;
    assert(new_p->hot == COLD);
    for (int i = 0; i < NPBUFTYPES; i++) {
        assert(new_p->accesses[i] == 0);
        assert(new_p->tot_accesses[i] == 0);
    }
    return_free_fastmem_page(new_p);

    return true;
}

static bool do_migrate_up(struct hemem_page* p, struct hemem_process* process) {
    struct hemem_page* np = get_free_fastmem_page();
    if (np == NULL) {
        LOG_ERR("can't migrate up -- no free fastmem");
        return false;
    }
    assert(!np->present);
    assert(np->pid == -1);

    page_list_remove(&(process->mm.slowmem_lists[p->hot]), p);

    // if (!find_page(process, p->va)) {
    //     LOG_ERR("can't find page for va %p before migrate up\n",
    //     (void*)p->va); return false;
    // }

    uint64_t old_offset = p->devdax_offset;
    promote_one_page(process, p, np->devdax_offset);
    np->devdax_offset = old_offset;
    np->in_dram = false;
    np->present = false;
    assert(np->hot == COLD);
    for (int i = 0; i < NPBUFTYPES; i++) {
        assert(np->accesses[i] == 0);
        assert(np->tot_accesses[i] == 0);
    }
    return_free_slowmem_page(np);

    p->hot = COLD;
    enqueue_page(&(process->mm.fastmem_lists[COLD]), p);
    return true;
}

static bool do_migration(struct hemem_page* page_to_promote,
                         struct hemem_page* page_to_demote,
                         struct hemem_process* process) {
    LOG_DEBUG("promoting %p demoting %p\n", page_to_promote, page_to_demote);
    if (!do_migrate_down(page_to_demote, process)) {
        LOG_ERR("failed to demote page %p\n", page_to_demote);
        return false;
    }
    return do_migrate_up(page_to_promote, process);
}

__attribute__((unused)) static struct hemem_page* pick_fastmem_victim_by_fifo(
    struct hemem_process* process) {
    return dequeue_page(&(process->mm.fastmem_lists[COLD]));
}

__attribute__((unused)) static struct hemem_page*
pick_fastmem_victim_by_fifo_hystoresis(struct hemem_process* process) {
    size_t curr_epoch = ucm_get_curr_epoch();

    struct page_list* list = &process->mm.fastmem_lists[COLD];

    struct hemem_page* curr = prev_page(list, NULL);
    while (curr &&
           (curr->last_migrated_epoch + MIGRATION_EPOCH_GAP > curr_epoch)) {
        if (curr->last_migrated_epoch == 0) break;
        curr = prev_page(list, curr);
    }

    if (curr) {
        page_list_remove(list, curr);
        return curr;
    }

    return NULL;
}

static unsigned int random_uint(unsigned int max) {
    return (unsigned int)rand() % (max + 1);
}

__attribute__((unused)) static struct hemem_page* random_pick_fastmem_victim(
    struct hemem_process* process) {
    struct page_list* list = &process->mm.fastmem_lists[COLD];

    size_t repeats = 0;
    while (repeats++ < 3) {
        size_t len = list->numentries;
        if (len == 0) return NULL;

        size_t rand_iter = random_uint(len);
        struct hemem_page* curr = list->last;
        for (size_t i = 0; i < rand_iter && curr; i++) {
            curr = prev_page(list, curr);
        }
        if (!curr) {
            // list length changed, retry
            continue;
        }

        size_t curr_epoch = ucm_get_curr_epoch();
        while (curr &&
               (curr->last_migrated_epoch + MIGRATION_EPOCH_GAP > curr_epoch)) {
            if (curr->last_migrated_epoch == 0) break;
            curr = prev_page(list, curr);
        }

        if (curr) {
            page_list_remove(list, curr);
            return curr;
        }
    }

    return NULL;
}

static struct hemem_page* pick_fastmem_victim(struct hemem_process* process) {
    return random_pick_fastmem_victim(process);
}

__attribute__((unused)) static void migrate_pages(
    struct hemem_process* process) {
    size_t pages_migrated = 0;

    struct page_rev_it it;
    slowmem_page_rev_it_init(&it, process);
    // don't move pages in the bottom levels
    it.min_level = HOT5;

    // add another condition related to miss ratio
    while (pages_migrated < MAX_MIGRATION_PER_EPOCH) {
        struct hemem_page* to_promote = page_rev_it_next(&it);
        if (!to_promote) {
            LOG_WARN("no page to promote after %lu iteration\n",
                     pages_migrated);
            return;
        }
        LOG("promotion candidate %lu level %ld\n", pages_migrated,
            to_promote->hot);
        assert(!to_promote->in_dram);

        struct hemem_page* to_demote = pick_fastmem_victim(process);
        if (!to_demote) {
            LOG_WARN("no page to demote after %lu iteration\n", pages_migrated);
            return;
        }
        assert(to_demote->in_dram);

        if (!do_migration(to_promote, to_demote, process)) {
            break;
        }

        assert(!to_demote->in_dram);
        assert(to_promote->in_dram);

        pages_migrated++;
    }

    LOG_DEBUG("policy done -- migrated %lu pages\n", pages_migrated);
}

static void process_policy_loop(struct hemem_process* process) {
    mm_drain_async_requests(process);
    migrate_pages(process);
}

void hemem_random_evict_policy(struct policy_opts* opts) {
    UNUSED(opts);
    PROCESS_FOR_EACH(process) { process_policy_loop(process); }
}
