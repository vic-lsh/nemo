#define _GNU_SOURCE

#include "stats/printer.h"

#include <pthread.h>

#include "mm-async.h"
#include "mm.h"
#include "opts/opts.h"
#include "physmem/config.h"
#include "proc-mgr.h"
#include "stats/cxl-stats.h"
#include "stats/pebs-stats.h"
#include "stats/stats.h"
#include "telem/handler/pebs.h"
#include "type/process.h"
#include "ucm-config.h"
#include "util/log.h"
#include "util/shared.h"
#include "util/thread.h"

pthread_t stats_thread;

// Remove all content on stdout and move the print cursor to screen top-left.
static void clear_stdout() {
    // Clear the screen
    printf("\033[H\033[J");
    // Flush stdout to ensure the screen is cleared before printing
    fflush(stdout);
}

static void process_clear_stats(struct hemem_process *process) {
    process->stats.cxl.unknown_page_count = 0;
    process_policy_epoch_clear(&process->policy);
}

static void pprint_process_hotness_bins(struct hemem_process *process) {
    LOG_STATS("\t%-27s", "Bin upper bound");
    for (int i = 0; i < NUM_HOTNESS_LEVELS - 1; i++) {
        LOG_STATS("%4lu,", (1UL << i) - 1);
    }
    LOG_STATS("%4s\n", "INF");

    size_t fast_sum = 0;
    size_t fastmem_n_coldest = process->mm.fastmem_lists[COLD].numentries;
    LOG_STATS("\tFast tier bins (# pages): [%4lu", fastmem_n_coldest);
    fast_sum += fastmem_n_coldest;
    for (int i = 1; i < NUM_HOTNESS_LEVELS; i++) {
        size_t num_entries = process->mm.fastmem_lists[i].numentries;
        LOG_STATS(",%4lu", num_entries);
        fast_sum += num_entries;
    }
    LOG_STATS("]\n");
    UNUSED(fast_sum);

    size_t slow_sum = 0;
    size_t slowmem_n_coldest = process->mm.slowmem_lists[COLD].numentries;
    LOG_STATS("\tSlow tier bins (# pages): [%4lu", slowmem_n_coldest);
    slow_sum += slowmem_n_coldest;
    for (int i = 1; i < NUM_HOTNESS_LEVELS; i++) {
        size_t num_entries = process->mm.slowmem_lists[i].numentries;
        LOG_STATS(",%4lu", num_entries);
        slow_sum += num_entries;
    }
    LOG_STATS("]\n\n");
    UNUSED(slow_sum);
}

static void print_pebs_sample_count(struct hemem_process *process) {
#ifdef PER_CORE_PEBS_COUNTS
    LOG_STATS("\tPer-core PEBS sample counts:\n");
    LOG_STATS("\t[");
    int c = 0;
    for (int i = LAST_HEMEM_THREAD + 1; i < HEMEM_NCORES; i++, c++) {
        if (c != 0 && (c % 8) == 0) {
            LOG_STATS("\n\t ");
        }
        LOG_STATS("%d: %5" PRIu64, i, process->samples[i]);
        if ((i + 1) != HEMEM_NCORES) {
            LOG_STATS(", ");
        }
    }
    LOG_STATS("]\n");
#else
    uint64_t pebs_samples_sum = 0;
    for (int i = LAST_HEMEM_THREAD + 1; i < HEMEM_NCORES; i++) {
        pebs_samples_sum += process->policy.sample_count[i];
    }
    LOG_STATS("\tPEBS samples: %lu ", pebs_samples_sum);
#endif
}

static void pprint_process(struct hemem_process *process) {
    LOG_STATS("Process %d stats:\n", process->pid);

    pprint_process_hotness_bins(process);

    LOG_STATS(
        "\tmiss ratios: curr: %.3f%%, target: %.3f%%\t"
        "allocations: fast: %ld, slow: %ld\n",
        process->policy.current_miss_ratio * 100.0,
        process->policy.target_miss_ratio * 100.0, process->mm.current_fastmem,
        process->mm.current_slowmem);

    print_pebs_sample_count(process);

    LOG_STATS("\tpf misses: %lu miss but allocated %lu\n",
              process->stats.pf.miss_faults,
              process->stats.pf.miss_but_allocated_faults);

    cool_stats_t *cool_stats = mm_get_cool_stats();
    LOG_STATS(
        "\tmigration_up: [%lu] migrations_down: [%lu] cools: [%lu] dram "
        "cools [%lu] cxl cools [%lu]\n",
        process->stats.migration.up_count, process->stats.migration.down_count,
        process->mm.cools, cool_stats->cools_started[FASTMEM],
        cool_stats->cools_started[SLOWMEM]);

    // LOG_STATS("\tunknown cxl page counts %lu\n",
    //           process->stats.cxl.unknown_page_count);

    LOG_STATS("\n");
}

static void print_process_stats() {
#define BYTE_TO_GB(bytes) ((double)(bytes) / (1024.0 * 1024.0 * 1024.0))

    double fastmem_usage_gb = 0, slowmem_usage_gb = 0;

    PROCESS_FOR_EACH(process) {
        fastmem_usage_gb += BYTE_TO_GB(process->mm.current_fastmem);
        slowmem_usage_gb += BYTE_TO_GB(process->mm.current_slowmem);
        pprint_process(process);
        process_clear_stats(process);
    }

    double fastmem_sz_gb = (double)get_fastmem_size() / GB(1);
    double slowmem_sz_gb = (double)get_slowmem_size() / GB(1);
    LOG_STATS("Total allocation: %.2f / %.2f GB DRAM, %.2f GB / %.2f CXL\n",
              fastmem_usage_gb, fastmem_sz_gb, slowmem_usage_gb, slowmem_sz_gb);
    fflush(stdout);
    LOG_STATS("\n");
}

static void hemem_print_stats() {
    ucm_stats_t *stats = ucm_stats_get();

    LOG_STATS(
        "mem_allocated: [%lu]\tpages_allocated: [%lu]\tmissing_faults_handled: "
        "[%lu]\nbytes_migrated: [%lu]\tmigrations_up: "
        "[%lu]\tmigrations_down: "
        "[%lu]\tmigration_waits: [%lu]\n",
        stats->mem.mem_allocated, stats->mem.pages_allocated,
        stats->pagefaults.missing_faults_handled,
        stats->migration.bytes_migrated, stats->migration.migrations_up,
        stats->migration.migrations_down, stats->migration.migration_waits);

    LOG_STATS("num_processes: %lu\t", processes_list.numentries);
    LOG_STATS("fastmem_free_pages: %lu\t", get_fastmem_free_page_cnt());
    LOG_STATS("slowmem_free_pages: %lu\t", get_slowmem_free_page_cnt());
    // LOG_STATS("\n# pending hot reqs: %lu\t",
    //           hot_ring_requests - hot_ring_requests_handled);
    // LOG_STATS("# pending cold reqs: %lu\t",
    //           cold_ring_requests - cold_ring_requests_handled);
    // LOG_STATS("# pending free reqs: %lu\t",
    //           free_ring_requests - free_ring_requests_handled);
    LOG_STATS("\n");

#ifdef CONFIG_PEBS
    pebs_stats_print();
#endif

    print_process_stats();

#ifdef CONFIG_CXL_TELEM
    cxl_stats_print();
#endif
}

__attribute__((unused)) static void *hemem_stats_thread(void *opaque) {
    struct ucm_opts *opts = (struct ucm_opts *)opaque;

    thread_pin_self(STATS_THREAD_CPU);
    pthread_setname_np(pthread_self(), "stats-thr");

    uint secs = 0;

#ifdef CONFIG_CXL_TELEM
    char *policy = "Local(PEBS) + Remote(CXL-Counters)";
#else
    char *policy = "Local(PEBS) + Remote(PEBS)";
#endif

    for (;;) {
        sleep(1);

        clear_stdout();

        printf("Runtime: %us\tPolicy: %s\tMigrationEnabled: %s\n", ++secs,
               policy, opts->policy.enable_migration ? "true" : "false");
        hemem_print_stats();
        ucm_stats_epoch_clear();

        // pebs_print_hottest_dram_pages();
    }
    return NULL;
}

void ucm_stats_printer_init(struct ucm_opts *opts) {
#ifdef CONFIG_STATS_DASHBOARD
    int ret = pthread_create(&stats_thread, NULL, hemem_stats_thread, opts);
    assert(ret == 0);
#endif
}
