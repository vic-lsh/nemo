#ifndef HEMEM_UCM_POLICY_STATE_H
#define HEMEM_UCM_POLICY_STATE_H

#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>

#include "ucm-config.h"

// This should be instantiated for QOS and FAIR_SHARE.
struct process_policy {
    // Track number of accesses for each memory type.
    _Atomic uint64_t access_count[NPBUFTYPES];
    _Atomic uint64_t sample_count[HEMEM_NCORES];
    double target_miss_ratio;
    double volatile current_miss_ratio;
    int64_t fastmem_delta;
    double deviation_ratio;

    // Number of bytes to migrate up in an epoch.
    uint64_t migration_up_bytes;
    // Number of bytes to migrate down in an epoch.
    uint64_t migration_down_bytes;
};

void process_policy_init(struct process_policy* policy);

void process_policy_destroy(struct process_policy* policy);

void process_policy_epoch_clear(struct process_policy* policy);

void process_policy_on_new_mem_access_sample(struct process_policy* policy);

#if defined(CONFIG_POLICY_QOS) || defined(CONFIG_POLICY_FAIR_SHARE)
void process_policy_set_miss_ratio(struct process_policy* policy,
                                   double miss_ratio);
double process_policy_calc_miss_ratio(struct process_policy* policy);
#endif

#endif /* HEMEM_UCM_POLICY_STATE_H */
