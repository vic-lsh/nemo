#ifndef HEMEM_UCM_STATS_H
#define HEMEM_UCM_STATS_H
#include <stdint.h>

#include "opts/opts.h"
#include "type/page.h"
#include "type/process.h"

typedef struct mem_stats {
    uint64_t mem_allocated;
    uint64_t pages_allocated;
    uint64_t pages_freed;
    uint64_t memsets;
} mem_stats_t;

typedef struct pagefault_stats {
    uint64_t wp_faults_handled;
    uint64_t missing_faults_handled;
} pagefault_stats_t;

typedef struct migration_stats {
    uint64_t migration_waits;
    uint64_t migrations_up;
    uint64_t migrations_down;
    uint64_t bytes_migrated;
} migration_stats_t;

typedef struct ucm_stats {
    mem_stats_t mem;
    pagefault_stats_t pagefaults;
    migration_stats_t migration;
} ucm_stats_t;

void ucm_stats_init(struct ucm_opts* opts);
ucm_stats_t* ucm_stats_get();
/**
 * Clear certain stats; should be called per epoch.
 */
void ucm_stats_epoch_clear();

void ucm_stats_record_migration_down(struct hemem_process* process,
                                     struct hemem_page* page, size_t pagesize);
void ucm_stats_record_migration_up(struct hemem_process* process,
                                   struct hemem_page* page, size_t pagesize);
void ucm_stats_record_page_freed(struct hemem_process* process,
                                 size_t pagesize);
void ucm_stats_record_page_allocated(struct hemem_process* process,
                                     size_t pagesize);
void ucm_stats_record_migration_wait(struct hemem_process* process,
                                     size_t migration_wait_time_us);

#endif /* HEMEM_UCM_STATS_H */
