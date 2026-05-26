#include "pebs_printer.h"

#include <stdio.h>
#include <stdlib.h>

#include "stats/pebs-stats.h"
#include "util/compiler.h"
#include "util/log.h"

typedef struct {
    FILE* pebs_f;
} printer_ctx_t;

printer_ctx_t g_ctx;

static void pebs_log_to_file(printer_ctx_t* ctx, struct perf_sample* ps) {
    fprintf(ctx->pebs_f, "%u,%llu,%llu\n", ps->tid, ps->time, ps->addr);
}

static void process_perf_sample(printer_ctx_t* ctx, struct perf_sample* ps,
                                size_t core_idx) {
    UNUSED(core_idx);
    pebs_log_to_file(ctx, ps);
}

static void log_pebs_sample_internal(printer_ctx_t* ctx,
                                     struct perf_event_header* ph,
                                     size_t core_idx) {
    pebs_stats_t* pebs_stats = pebs_stats_get();

    struct perf_sample* ps;

    assert(ph != NULL);

    switch (ph->type) {
        case PERF_RECORD_SAMPLE:
            pebs_stats->total_samples_cnt++;
            ps = (struct perf_sample*)ph;
            process_perf_sample(ctx, ps, core_idx);
            break;
        case PERF_RECORD_THROTTLE:
            pebs_stats->throttle_cnt++;
            break;
        case PERF_RECORD_UNTHROTTLE:
            pebs_stats->unthrottle_cnt++;
            break;
        case PERF_RECORD_LOST_SAMPLES:
        case PERF_RECORD_LOST:
            pebs_stats->lost_samples++;
            break;
        default:
            fprintf(stderr, "Unknown type %u\n", ph->type);
            break;
    }
}

void log_pebs_sample(struct perf_event_header* ph, size_t core_idx) {
    log_pebs_sample_internal(&g_ctx, ph, core_idx);
}

void pebs_printer_handler_init(char* log_filename) {
    g_ctx.pebs_f = fopen(log_filename, "w");
    if (!g_ctx.pebs_f) {
        perror("pebs_log_open");
        LOG_ERR("failed to open pebs log file '%s'\n", log_filename);
        exit(1);
    }
    pebs_stats_init();
}
