#ifndef HEMEM_PAGE_ITERATOR_H
#define HEMEM_PAGE_ITERATOR_H

#include <stdbool.h>

#include "type/page.h"
#include "type/process.h"

/**
 * Iterate a process's pages, starting from the hottest pages.
 */
struct page_rev_it {
    // Internal parameters
    struct hemem_process* process;
    struct hemem_page* next;
    struct page_list* levels;
    int curr_level;
    bool init;
    bool done;

    // Optional user-configurable parameters:
    // set after the struct is initialized

    // The level to start iterating from.
    int max_level;
    // The lowest level the iteration will go (inclusive).
    int min_level;
};

void fastmem_page_rev_it_init(struct page_rev_it* it,
                              struct hemem_process* process);
void slowmem_page_rev_it_init(struct page_rev_it* it,
                              struct hemem_process* process);
struct hemem_page* page_rev_it_next(struct page_rev_it* it);

#endif /* End of HEMEM_PAGE_ITERATOR_H */
