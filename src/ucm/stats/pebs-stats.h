#ifndef PEBS_STATS_H
#define PEBS_STATS_H

#include <stdint.h>

#include "ucm-config.h"

typedef struct pebs_stats {
    // Sample counter that gets incremented whenever a sample is processed,
    // regardless of its validity.
    uint64_t total_samples_cnt;
    // Samples that cannot be attributed to a hemem process.
    // Must be other processes running on the CPUs we're sampling on.
    uint64_t non_hemem_proc_samples_cnt;
    // Sample attributed to hemem process, but not to a hemem page.
    uint64_t non_hemem_page_samples_cnt;
    // Valid samples -- samples with both hemem process and page attributed.
    uint64_t valid_samples_cnt;
    // Samples that fall under the CXL region of memory.
    uint64_t cxl_samples_cnt;
    uint64_t zero_pages_cnt;
    uint64_t throttle_cnt;
    uint64_t unthrottle_cnt;
    uint64_t lost_samples;
} pebs_stats_t;

pebs_stats_t *pebs_stats_get();

void pebs_stats_init();

void pebs_stats_print();

#endif /* End PEBS_STATS_H */
