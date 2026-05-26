#include "policy-state.h"

#include <assert.h>

#include "telem/source/pebs.h"
#include "util/compiler.h"
#include "util/log.h"

void process_policy_init(struct process_policy* policy) {
    policy->current_miss_ratio = -1;
    policy->fastmem_delta = 0;
    policy->migration_up_bytes = 0;
    policy->migration_down_bytes = 0;
}

void process_policy_destroy(struct process_policy* policy) {
    UNUSED(policy);
    // no-op for now
}

void process_policy_epoch_clear(struct process_policy* policy) {
    for (int i = LAST_HEMEM_THREAD + 1; i < HEMEM_NCORES; i++) {
        policy->sample_count[i] = 0;
    }
}

#if defined(CONFIG_POLICY_QOS) || defined(CONFIG_POLICY_FAIR_SHARE)
void process_policy_set_miss_ratio(struct process_policy* policy,
                                   double miss_ratio) {
    policy->target_miss_ratio = miss_ratio;
}

double process_policy_calc_miss_ratio(struct process_policy* policy) {
#ifdef CONFIG_CXL_TELEM
    uint64_t fast_tier_sampling_rate = PEBS_SAMPLE_PERIOD;
#else
    uint64_t fast_tier_sampling_rate = 1;
#endif

    uint64_t slow_tier_access = policy->access_count[SLOWMEM];
    uint64_t fast_tier_access = policy->access_count[FASTMEM];

    double miss_ratio =
        (1.0 * slow_tier_access) /
        (1.0 * (slow_tier_access + fast_tier_access * fast_tier_sampling_rate));

    if ((miss_ratio >= 0 && miss_ratio <= 1)) {
        LOG_DEBUG("miss ratio %.5f slow tier %ld fast tier %lu\n", miss_ratio,
                  slow_tier_access, fast_tier_access);
        return miss_ratio;
    } else {
        LOG_ERR(
            "dram access %lu nvm access %lu dram factor %ld miss ratio %f\n",
            policy->access_count[FASTMEM], policy->access_count[SLOWMEM],
            fast_tier_sampling_rate, miss_ratio);
        assert(0);
    }
}
#endif
