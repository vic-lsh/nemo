#include "stats/stats.h"

#include <string.h>

#include "stats/printer.h"

ucm_stats_t stats;

void ucm_stats_init(struct ucm_opts* opts) {
    memset(&stats, 0, sizeof(ucm_stats_t));
    ucm_stats_printer_init(opts);
}

ucm_stats_t* ucm_stats_get() { return &stats; }

void ucm_stats_epoch_clear() {
    ucm_stats_t* stats = ucm_stats_get();

    stats->mem.pages_allocated = 0;
    stats->mem.pages_freed = 0;
    stats->pagefaults.missing_faults_handled = 0;
    stats->migration.migrations_up = 0;
    stats->migration.migrations_down = 0;
}

void ucm_stats_record_migration_down(struct hemem_process* process,
                                     struct hemem_page* page, size_t pagesize) {
    process->stats.migration.down_count++;
    page->migrations_down++;
    stats.migration.migrations_down++;
    stats.migration.bytes_migrated += pagesize;
}

void ucm_stats_record_migration_up(struct hemem_process* process,
                                   struct hemem_page* page, size_t pagesize) {
    process->stats.migration.up_count++;
    page->migrations_up++;
    stats.migration.migrations_up++;
    stats.migration.bytes_migrated += pagesize;
}

void ucm_stats_record_page_freed(struct hemem_process* process,
                                 size_t pagesize) {
    stats.mem.pages_freed++;
    stats.mem.mem_allocated -= pagesize;
    process->mm.mem_allocated -= pagesize;
}

void ucm_stats_record_page_allocated(struct hemem_process* process,
                                     size_t pagesize) {
    stats.mem.mem_allocated += pagesize;
    stats.mem.pages_allocated++;
    process->mm.mem_allocated += pagesize;
}

void ucm_stats_record_migration_wait(struct hemem_process* process,
                                     size_t migration_wait_time_us) {
    stats.migration.migration_waits++;
    process->stats.migration.wp_wait_count++;
    process->stats.migration.wp_wait_us += migration_wait_time_us;
}
