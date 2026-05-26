#include "page-iterator.h"

#include <stdbool.h>

#include "util/compiler.h"
#include "util/log.h"

static void page_rev_it_init(struct page_rev_it* it,
                             struct hemem_process* process, bool is_fastmem) {
    assert(process);
    memset(it, 0, sizeof(struct page_rev_it));
    it->process = process;
    it->levels = is_fastmem ? it->process->mm.fastmem_lists
                            : it->process->mm.slowmem_lists;
    it->max_level = NUM_HOTNESS_LEVELS;
    it->min_level = 0;
    it->init = true;
}

void fastmem_page_rev_it_init(struct page_rev_it* it,
                              struct hemem_process* process) {
    page_rev_it_init(it, process, true);
}

void slowmem_page_rev_it_init(struct page_rev_it* it,
                              struct hemem_process* process) {
    page_rev_it_init(it, process, false);
}

static struct hemem_page* find_hottest_page(struct page_rev_it* it,
                                            int* p_level) {
    for (int level = it->max_level; level >= it->min_level; level--) {
        if (it->levels[level].last) {
            *p_level = level;
            return it->levels[level].last;
        }
    }

    return NULL;
}

// must only call when the iterator has been initialized.
struct hemem_page* __advance_page_rev_it(struct page_rev_it* it) {
    // this function updates `it->next`.

    // what was previously next has become `curr`.
    struct hemem_page* curr = it->next;

    while (it->curr_level >= it->min_level) {
        struct hemem_page* prev =
            prev_page(&(it->levels[it->curr_level]), curr);
        if (prev) {
            // there's still page at this level, record current pos and return
            it->next = prev;
            return prev;
        }
        // there's no page at this level, move to the level below
        // reset `curr` so that `prev_page()` next level returns list tail.
        curr = NULL;
        it->curr_level--;
    }

    // if we reached here, then we've searched all the lists and found nothing.
    // the search is completed.
    it->done = true;
    return NULL;
}

struct hemem_page* page_rev_it_next(struct page_rev_it* it) {
    if (it->done) return NULL;

    // fast path -- we already have the next page lined up
    struct hemem_page* curr;
    if (likely(it->next)) {
        curr = it->next;
        // obtain the future next page.
        //
        // getting the `curr->next` is important because `curr` could be removed
        // from its list once it's returned to the caller. at that point, there
        // would be no way to continue the iteration.
        it->next = __advance_page_rev_it(it);
        if (!it->done) {
            assert(it->next != curr);
        }
        return curr;
    }

    // slow path -- first time setting up the iterator
    curr = it->next = find_hottest_page(it, &it->curr_level);
    if (!curr) {
        // there's no page to iterate
        it->done = true;
        return NULL;
    }
    it->next = __advance_page_rev_it(it);
    if (!it->done) {
        assert(it->next != curr);
    }
    return curr;
}
