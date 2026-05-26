#ifndef HEMEM_UCM_MM_H
#define HEMEM_UCM_MM_H

#include <pthread.h>
#include <stdint.h>

#include "ds/fifo.h"
#include "ds/spsc-ring.h"
#include "stdbool.h"
#include "stddef.h"
#include "ucm-config.h"

/**
 * Defines how the physical memory is instantiated.
 */
typedef enum {
    /**
     * Use /dev/dax files. This typically means using real (CXL) device memory.
     */
    USE_DAX,
    /**
     * Use POSIX shared memory files (via shm_open).
     */
    USE_SHM
} physmem_mode_t;

/**
 * Arguments for initializing mm.
 */
typedef struct {
    physmem_mode_t physmem_mode;
    size_t fastmem_size;
    size_t slowmem_size;
} mm_opts_t;

struct hemem_process;
struct hemem_page;

/**
 * Per-process memory management state.
 */
struct process_mem {
    volatile uint64_t mem_allocated;
    volatile uint64_t current_fastmem;
    volatile uint64_t current_slowmem;
    uint64_t max_fastmem;

    struct page_list fastmem_lists[NUM_HOTNESS_LEVELS + 1];
    struct page_list slowmem_lists[NUM_HOTNESS_LEVELS + 1];
    int cur_cool_in_fastmem_list;
    int cur_cool_in_slowmem_list;

    struct hemem_page *cur_cool_in_fastmem;
    struct hemem_page *cur_cool_in_slowmem;
    /* Whether the process was cooled under the current epoch.
     * Reset by the scanning thread. */
    volatile bool epoch_did_cool;
    volatile bool need_cool_fastmem;
    volatile bool need_cool_slowmem;
    uint64_t cools;

    volatile ring_handle_t hot_ring;
    volatile ring_handle_t cold_ring;
    volatile ring_handle_t free_page_ring;
    pthread_mutex_t free_page_ring_lock;

    struct hemem_page *pages;
    struct hemem_page *cxl_offset_to_page;
    pthread_mutex_t pages_lock;
};

void mm_init(mm_opts_t *opts);

size_t get_default_page_size();
size_t get_default_page_type();

// Global memory management APIs.

struct hemem_page *get_free_fastmem_page();
struct hemem_page *get_free_slowmem_page();
void return_free_fastmem_page(struct hemem_page *page);
void return_free_slowmem_page(struct hemem_page *page);

size_t get_fastmem_size();
size_t get_slowmem_size();
size_t get_fastmem_free_size();
size_t get_slowmem_free_size();
size_t get_fastmem_free_page_cnt();
size_t get_slowmem_free_page_cnt();

void *devdax_offset_to_addr(uint64_t offset, bool is_fastmem);

void assert_page_freed(struct hemem_page *page);

// Per-process level APIs.
void mm_add_page(struct process_mem *mm, struct hemem_page *page);
struct hemem_page *mm_find_page(struct process_mem *mm, uint64_t va);
struct hemem_page *mm_find_page_by_cxl_offset(struct process_mem *mm,
                                              uint64_t devdax_offset);
void mm_remove_page(struct process_mem *mm, struct hemem_page *page);

struct hemem_process *mm_find_proc_by_cxl_offset(uint64_t cxl_devdax_offset);

// TODO: the cxl-specific APIs are awkward -- think of better APIs.
void mm_add_page_to_cxl(struct process_mem *mm, struct hemem_page *page);
void mm_remove_page_from_cxl(struct process_mem *mm, struct hemem_page *page);

// Misc APIs
void pprint_mm_opts(const mm_opts_t *opt);

#endif /* HEMEM_UCM_MM_H */
