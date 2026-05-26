#define _GNU_SOURCE

#include "core.h"

#include <asm/unistd.h>
#include <assert.h>
#include <inttypes.h>
#include <linux/hw_breakpoint.h>
#include <linux/perf_event.h>
#include <pthread.h>
#include <sched.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <unistd.h>

#include "policy/policy.h"
#include "proc-mgr.h"
#include "telem/engine.h"
#include "telem/handler/cxl.h"
#include "util/compiler.h"
#ifdef CONFIG_PEBS_LOG
#include "telem/handler/pebs_printer.h"
#endif
#include "opts/opts.h"
#include "telem/handler/pebs.h"
#include "telem/source/cxl.h"
#include "telem/source/pebs.h"
#include "ucm-config.h"
#include "util/log.h"
#include "util/thread.h"
#include "util/timer.h"

uint64_t scan_epoch = 0;

size_t ucm_get_curr_epoch() { return scan_epoch; }

typedef struct {
    wall_and_cpu_time_t pebs;
    wall_and_cpu_time_t cxl;
    wall_and_cpu_time_t policy;
} thread_time_t;

static double epoch_elapsed_time_us(const telem_poll_ctx_t *telem,
                                    const policy_epoch_ctx_t *policy) {
    double sum = 0;
    sum += telem->pebs.wall_time_us;
    sum += telem->cxl.wall_time_us;
    sum += policy->elapsed.wall_time_us;
    return sum;
}

static void report_epoch_time(const telem_poll_ctx_t *telem_ctx,
                              const policy_epoch_ctx_t *policy_ctx) {
    uint64_t wall_time_total_us = telem_ctx->pebs.wall_time_us +
                                  telem_ctx->cxl.wall_time_us +
                                  policy_ctx->elapsed.wall_time_us;
    uint64_t cpu_time_total_ns = telem_ctx->pebs.cpu_time_ns +
                                 telem_ctx->cxl.cpu_time_ns +
                                 policy_ctx->elapsed.cpu_time_ns;

    LOG("EPOCH %lu: "
        "Total %.1fms WC (%.3fms CPU)"
        "\tPEBS %.1fms WC (%.3fms CPU)"
        "\tCXL counters %.1fms WC (%.3fms CPU)"
        "\tPolicy %.1fms WC (%.3fms CPU)"
        "\n",
        scan_epoch, us_to_ms(wall_time_total_us), ns_to_ms(cpu_time_total_ns),
        us_to_ms(telem_ctx->pebs.wall_time_us),
        ns_to_ms(telem_ctx->pebs.cpu_time_ns),
        us_to_ms(telem_ctx->cxl.wall_time_us),
        ns_to_ms(telem_ctx->cxl.cpu_time_ns),
        us_to_ms(policy_ctx->elapsed.wall_time_us),
        ns_to_ms(policy_ctx->elapsed.cpu_time_ns));
}

static void *core_loop(struct ucm_opts *opts) {
    UNUSED(opts);

    thread_pin_self(CORE_THREAD_CPU);
    pthread_setname_np(pthread_self(), "core-thr");

    telem_poll_ctx_t telem_ctx;
    policy_epoch_ctx_t policy_ctx;
    for (;;) {
        // poll telem
        telem_engine_poll(&telem_ctx);

        // delete dead processes
        // (do so before policy epoch, b/c policy likely scans the proc list)
        size_t nprocs_removed = proc_mgr_prune();
        if (unlikely(nprocs_removed)) {
            LOG("removed %lu dead processes\n", nprocs_removed);
        }

        // do policy
        ucm_policy_epoch(&policy_ctx, scan_epoch);

        report_epoch_time(&telem_ctx, &policy_ctx);

        double epoch_dur_us = epoch_elapsed_time_us(&telem_ctx, &policy_ctx);
        double epoch_dur_ms = us_to_ms(epoch_dur_us);
        if (epoch_dur_ms < EPOCH_INTERVAL_MS) {
            usleep((EPOCH_INTERVAL_MS - epoch_dur_ms) * 1000);
        }

        scan_epoch++;
    }

    return NULL;
}

void ucm_core_shutdown() {
#ifdef CONFIG_PEBS
    pebs_shutdown();
#endif
#ifdef CONFIG_CXL_TELEM
    cxl_shutdown();
#endif
}

int ucm_core_enter(struct ucm_opts *opts) {
    int ret = 0;
    LOG("core_init: started\n");

    ret = telem_engine_init();
    if (ret < 0) {
        return ret;
    }

    ucm_policy_init(&opts->policy);

    LOG("core_init: done\n");
    LOG("ucm ready!\n");

    core_loop(opts);

    ucm_core_shutdown();
    return 0;
}
