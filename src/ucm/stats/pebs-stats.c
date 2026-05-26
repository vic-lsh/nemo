#include "stats/pebs-stats.h"

#include <string.h>

#include "telem/source/pebs.h"
#include "util/log.h"

size_t total_over_time = 0;
size_t valid_over_time = 0;

pebs_stats_t g_pebs_stats;

static void pebs_stats_epoch_reset(pebs_stats_t *stats) {
    stats->total_samples_cnt = 0;
    stats->non_hemem_proc_samples_cnt = 0;
    stats->non_hemem_page_samples_cnt = 0;
    stats->valid_samples_cnt = 0;
    stats->cxl_samples_cnt = 0;
}

static uint64_t pebs_stats_other_samples_cnt(pebs_stats_t *stats) {
    return stats->non_hemem_page_samples_cnt +
           stats->non_hemem_proc_samples_cnt;
}

static void pebs_stats_print_internal(pebs_stats_t *pebs_stats) {
    total_over_time += pebs_stats->total_samples_cnt;
    valid_over_time += pebs_stats->valid_samples_cnt;

    LOG_STATS("\nPEBS (per-epoch): ");
    LOG_STATS("cxl: %lu ", pebs_stats->cxl_samples_cnt);
    LOG_STATS("other: %lu ", pebs_stats_other_samples_cnt(pebs_stats));
    LOG_STATS("(no proc %lu no page %lu) ",
              pebs_stats->non_hemem_proc_samples_cnt,
              pebs_stats->non_hemem_page_samples_cnt);
    LOG_STATS("lost: %lu ", pebs_stats->lost_samples);
    LOG_STATS("thrtl: %lu ", pebs_stats->throttle_cnt);
    LOG_STATS("unthr: %lu ", pebs_stats->unthrottle_cnt);

    LOG_STATS("\nPEBS (all time):  ");
    size_t invalid_pebs_samples = total_over_time - valid_over_time;
    LOG_STATS("invalid: %lu ", invalid_pebs_samples);
    LOG_STATS("total: %lu ", total_over_time);
    LOG_STATS("sample rate: %lu", PEBS_SAMPLE_PERIOD);
    LOG_STATS("\n\n");

    pebs_stats_epoch_reset(pebs_stats);
}

void pebs_stats_print() { pebs_stats_print_internal(&g_pebs_stats); }

pebs_stats_t *pebs_stats_get() { return &g_pebs_stats; }

void pebs_stats_init() { memset(&g_pebs_stats, 0, sizeof(g_pebs_stats)); }
