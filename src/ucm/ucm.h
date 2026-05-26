#ifndef HEMEM_UCM_H
#define HEMEM_UCM_H

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <sys/time.h>

#include "ds/fifo.h"
#include "ds/spsc-ring.h"
#include "ds/uthash.h"
#include "hemem-shared.h"
#include "ipc-shared.h"
#include "support/memops.h"
#include "type/page.h"
#include "type/process.h"
#include "ucm-config.h"

/**
 * Top-level function that does memory allocation to a process.
 *
 * @return
 * The function returns number of pages allocated. Information about each page
 * allocated is written to the out pointer `pages`, where `pages` is an array
 * allocated page entries.
 */
ssize_t ucm_allocate_memory(struct hemem_process *process, uint64_t start_addr,
                            size_t length, struct hemem_page_app *pages);
/**
 * Top-level function that handles application MAP_FIXED mmaps. This function
 * finds the pages that should be already mapped to the application, based on
 * the address given.
 *
 * @return
 * The function returns number of pages allocated. Information about each page
 * allocated is written to the out pointer `pages`, where `pages` is an array
 * allocated page entries.
 */
ssize_t ucm_map_fixed(struct hemem_process *process, uint64_t start_addr,
                      size_t length, struct hemem_page_app *pages);

/**
 * Top-level function that does memory deallocation request from a process.
 *
 * @return
 * Number of pages freed.
 */
size_t ucm_deallocate_memory(struct hemem_process *process, uint64_t start_addr,
                             size_t length);

/**
 * Top-level function that handles deleting a process.
 *
 * Once a process pointer has been passed into this function, it can no longer
 * be dereferenced.
 */
void ucm_remove_process(struct hemem_process *process);

#endif /* HEMEM_UCM_H */
