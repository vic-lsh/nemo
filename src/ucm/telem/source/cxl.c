#include "cxl.h"

// #define CXL_COUNTER_LOGGING

#include <errno.h>
#include <inttypes.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>

#include "core-util.h"
#include "cxl-dev/cxl-dev.h"
#include "mm-async.h"
#include "mm.h"
#include "physmem/physmem.h"
#include "ucm.h"
#include "util/log.h"

size_t counter_start;
size_t counter_end;
access_counter_t page_access_cnts[N_COUNTERS];

#ifdef CXL_COUNTER_LOGGING
static FILE *cxl_counter_log_fp;
static const char *cxl_counter_log_path = "cxl_counter_diffs.csv";
#endif

access_counter_t cxl_count_get(size_t counter_idx) {
    return page_access_cnts[counter_idx];
}

access_counter_t cxl_count_refresh(size_t counter_idx) {
    uint64_t diff = cxl_dev_read_counter_diff(counter_idx);
    page_access_cnts[counter_idx] += diff;
    return page_access_cnts[counter_idx];
}

// determine the cxl counter range that we need to scan for
static void compute_cxl_counter_range(size_t *p_start, size_t *p_end) {
    // TODO: add back logic to ignore certain counters at the beginning and end
    // of the address range, because DAX reduces the address space somewhat.
    *p_start = 0;
    *p_end = N_COUNTERS;
}

#ifdef CXL_COUNTER_LOGGING
static void log_counter_diffs(const access_counter_t *diffs, size_t start,
                              size_t end) {
    if (cxl_counter_log_fp == NULL) {
        return;
    }

    for (size_t i = start; i < end; i++) {
        if (i > start) {
            fputc(',', cxl_counter_log_fp);
        }
        fprintf(cxl_counter_log_fp, "%" PRIu64, (uint64_t)diffs[i]);
    }
    fputc('\n', cxl_counter_log_fp);
    fflush(cxl_counter_log_fp);
}
#else
static void log_counter_diffs(const access_counter_t *diffs, size_t start,
                              size_t end) {
    (void)diffs;
    (void)start;
    (void)end;
}
#endif

int cxl_init() {
    int ret;
    ret = cxl_dev_init(get_slowmem_mmap_addr());
    if (ret != 0) {
        LOG_ERR("cxl_dev init failed\n");
        return ret;
    }
    compute_cxl_counter_range(&counter_start, &counter_end);
    LOG("cxl scan counters from %lu to %lu\n", counter_start, counter_end);

#ifdef CXL_COUNTER_LOGGING
    cxl_counter_log_fp = fopen(cxl_counter_log_path, "w");
    if (cxl_counter_log_fp == NULL) {
        LOG_ERR("failed to open CXL counter log %s: %s\n", cxl_counter_log_path,
                strerror(errno));
        return -errno;
    }
#endif

    return ret;
}

void cxl_shutdown() {
#ifdef CXL_COUNTER_LOGGING
    if (cxl_counter_log_fp != NULL) {
        fclose(cxl_counter_log_fp);
        cxl_counter_log_fp = NULL;
    }
#endif
}

static void scan_counters(size_t start, size_t end,
                          cxl_counter_update_handler_t handler) {
    access_counter_t tmp_cnts[N_COUNTERS];
    for (size_t i = start; i < end; i++) {
        tmp_cnts[i] = 0;
    }
    cxl_dev_update_all_counters(tmp_cnts);

    log_counter_diffs(tmp_cnts, start, end);

    for (size_t i = start; i < end; i++) {
        unsigned int prev_cnt = cxl_count_get(i);
        // unsigned int curr_cnt = cxl_count_refresh(i);
        //  do the rest of cxl_count_refresh
        uint64_t diff = tmp_cnts[i];
        page_access_cnts[i] += diff;
        uint64_t curr_cnt = page_access_cnts[i];

        size_t cxl_devdax_offset = (i - start) * HUGEPAGE_SIZE;

        handler(cxl_devdax_offset, prev_cnt, curr_cnt);
    }
}

void cxl_epoch_scan(cxl_counter_update_handler_t handler) {
    scan_counters(counter_start, counter_end, handler);
}
