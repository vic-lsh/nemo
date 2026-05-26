#ifndef HEMEM_POLICY_H
#define HEMEM_POLICY_H

/**
 * This file defines entry points for various customizable policies
 * (when to allocate, what to allocate, what to migrate, etc.).
 *
 * Other parts of the UCM invokes into the top-level functions in this file.
 * Policy functions in this file dispatch into the actual policy impls
 * internally, based on build or runtime configurations.
 */

#include <stdbool.h>

#include "policy-state.h"
#include "policy/alloc.h"
#include "type/page.h"
#include "type/process.h"
#include "util/timer.h"

#define EWMA_FRAC (0.5)

#define EPOCH_INTERVAL_MS (1000)

// Migration rate configurations
#define PEBS_MIGRATE_RATE (256UL * 1024UL * 1024UL)
#define FAIR_SHARE_DRAM (1UL * 1024UL * 1024UL * 1024UL)  // 1GB

struct policy_opts {
    bool enable_migration;
    enum ALLOC_MODE alloc_mode;
};

typedef struct {
    wall_and_cpu_time_t elapsed;
} policy_epoch_ctx_t;

void ucm_policy_init(struct policy_opts *opts);

// Invoked per epoch for policy action.
void ucm_policy_epoch(policy_epoch_ctx_t *ctx, size_t epoch);

// Allocate a page to the provided process.
struct hemem_page *ucm_allocate_page(struct hemem_process *process);

struct hemem_page *find_promotion_candidate(struct hemem_process *process);
struct hemem_page *find_demotion_candidate(struct hemem_process *process);

void pprint_policy_opts(const struct policy_opts *opts);

#endif /* HEMEM_POLICY_H */
