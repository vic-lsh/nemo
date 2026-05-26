#include "policy.h"

#include "core.h"
#include "policy/alloc.h"
#include "policy/fair-share.h"
#include "policy/qos.h"
#include "policy/random-evict.h"
#include "util/log.h"

static policy_t g_policy;

struct hemem_page *find_promotion_candidate(struct hemem_process *process) {
    return g_policy.find_promotion_candidate(g_policy.opaque, process);
}

struct hemem_page *find_demotion_candidate(struct hemem_process *process) {
    return g_policy.find_demotion_candidate(g_policy.opaque, process);
}

static void epoch_ctx_reset(policy_epoch_ctx_t *ctx) {
    memset(ctx, 0, sizeof(*ctx));
}

void ucm_policy_epoch(policy_epoch_ctx_t *ctx, size_t epoch) {
    epoch_ctx_reset(ctx);
    TIME_OP(ctx->elapsed, { g_policy.epoch_handler(g_policy.opaque, epoch); });
}

struct hemem_page *ucm_allocate_page(struct hemem_process *process) {
    return g_policy.allocate_page(g_policy.opaque, process);
}

void pprint_policy_opts(const struct policy_opts *opts) {
    LOG_NOPATH("enable_migration: %s\n", bool_str(opts->enable_migration));
    LOG_NOPATH("allocation_mode: %s\n", pprint_alloc_mode(opts->alloc_mode));
}

void ucm_policy_init(struct policy_opts *opts) {
    int ret;
#ifdef CONFIG_POLICY_QOS
    ret = qos_policy_init(&g_policy, opts);
#elif defined(CONFIG_POLICY_FAIR_SHARE)
    ret = fair_share_policy_init(&g_policy, opts);
#else
    // unsupported policy
    assert(0);
#endif

    assert(ret == 0);
}
