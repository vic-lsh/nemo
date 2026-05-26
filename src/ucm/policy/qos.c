#include "qos.h"

#include <stdint.h>
#include <sys/time.h>

#include "ds/proc-array.h"
#include "migrate.h"
#include "migration.h"
#include "mm-async.h"
#include "mm.h"
#include "policy-state.h"
#include "policy.h"
#include "policy/alloc.h"
#include "proc-mgr.h"
#include "type/page.h"
#include "type/process.h"
#include "ucm-config.h"
#include "util/compiler.h"
#include "util/log.h"

// Constants related to ratio calculation, where ratio is defined as:
//     CurrentMissRatio / TargetMissRatio
//
// Ratios above QOS_NEED_MEMORY_THRESHOLD indicate processes need memory, and
// vice versa. We cap ratios to QOS_DEVIATION_RATIO_MAX.
#define QOS_NEED_MEMORY_THRESHOLD 1.05
#define QOS_TAKE_MEMORY_THRESHOLD 0.95
#define QOS_DEVIATION_RATIO_MAX ((double)100)

typedef struct {
    struct policy_opts *opts;

    /* page allocator */
    page_alloc_fn_t page_alloc;

    uint64_t per_process_migrate_share;
    /** List of processes which need more fastmem. Cleared on each epoch. */
    process_array_t need_fastmem;
    /**
     * List of processes which can live with fewer fastmem.
     * Cleared on each epoch.
     */
    process_array_t take_fastmem;
    /** Approx. measure of how much fastmem demand there is in this epoch. */
    double fastmem_demand_total;
    /** Approx. measure of how much fastmem supply there is in this epoch. */
    double fastmem_supply_total;
} qos_policy_state_t;

static uint64_t calculate_intra_process_migration_bytes(
    struct hemem_process *process) {
    const size_t kMinSlowmemLevel = 3;

    int64_t fastmem_bin_counts[NUM_HOTNESS_LEVELS];
    for (int i = 0; i < NUM_HOTNESS_LEVELS; i++) {
        fastmem_bin_counts[i] = process->mm.fastmem_lists[i].numentries;
    }

    // Swap the hottest slowmem pages with the coldest fastmem pages
    uint64_t migrate_down_bytes = 0;
    for (size_t i = NUM_HOTNESS_LEVELS - 1; i >= kMinSlowmemLevel; i--) {
        int64_t n_slowmem_pages_left = process->mm.slowmem_lists[i].numentries;
        if (n_slowmem_pages_left == 0) continue;

#ifdef CONFIG_CXL_TELEM
        size_t max_fastmem_level = i - 2;
#else
        size_t max_fastmem_level = i;
#endif

        for (size_t j = 0; j < max_fastmem_level; j++) {
            if (n_slowmem_pages_left <= 0) break;
            if (fastmem_bin_counts[j] == 0) continue;

            int64_t pages_to_swap =
                min(fastmem_bin_counts[j], n_slowmem_pages_left);
            fastmem_bin_counts[j] -= pages_to_swap;
            n_slowmem_pages_left -= pages_to_swap;

            // TODO: this will not be correct once page size is non-uniform
            migrate_down_bytes += pages_to_swap * HUGEPAGE_SIZE;
        }
    }

    return migrate_down_bytes;
}

static void set_process_migration_targets(struct hemem_process *process,
                                          qos_policy_state_t *s) {
    struct process_policy *policy = &process->policy;

    if (policy->fastmem_delta > 0) {  // Process gets more DRAM

        policy->migration_up_bytes = policy->fastmem_delta;

        int64_t free_pages_bytes = get_fastmem_free_size();
        // if process needs more fastmem than available, demote some pages
        // TODO: maybe the it's better to demote from other processes?
        int64_t migrate_down = policy->fastmem_delta - free_pages_bytes;
        policy->migration_down_bytes = (migrate_down < 0) ? 0L : migrate_down;

    } else if (policy->fastmem_delta < 0) {  // Process loses DRAM

        policy->migration_up_bytes = 0;
        policy->migration_down_bytes = -1 * policy->fastmem_delta;

    } else {  // Process DRAM allocation is stable

        uint64_t migrate_down_bytes =
            calculate_intra_process_migration_bytes(process);
        uint64_t one_way_migration_share = s->per_process_migrate_share / 2;
        policy->migration_down_bytes =
            min(migrate_down_bytes, one_way_migration_share);
        policy->migration_up_bytes = policy->migration_down_bytes;  // Swap
    }
}

static void do_migration(qos_policy_state_t *s) {
    PROCESS_FOR_EACH(process) {
        pthread_mutex_lock(&(process->process_lock));

        // first, set number of bytes to migrate up/down for each process
        set_process_migration_targets(process, s);

        // do migration down to make room in fastmem
        process_migrate_down_bytes(process,
                                   process->policy.migration_down_bytes);

        pthread_mutex_unlock(&(process->process_lock));
    }

    // do promotion
    PROCESS_FOR_EACH(process) {
        pthread_mutex_lock(&(process->process_lock));
        process_migrate_up_bytes(process, process->policy.migration_up_bytes);
        pthread_mutex_unlock(&(process->process_lock));
    }
}

static double update_process_miss_ratio(struct process_policy *policy) {
    uint64_t curr_epoch_accesses =
        policy->access_count[FASTMEM] + policy->access_count[SLOWMEM];

    if (curr_epoch_accesses == 0) {
        // No access count info in the last epoch
        // TODO(vic): is this the right approach? shouldn't the approach be to
        // age out the existing miss ratio?
        return 0;
    }

    double miss_ratio = process_policy_calc_miss_ratio(policy);
    if (policy->current_miss_ratio == -1) {
        return miss_ratio;
    } else {
        return (EWMA_FRAC * miss_ratio) +
               ((1 - EWMA_FRAC) * policy->current_miss_ratio);
    }
}

static void check_mem_demand_supply(qos_policy_state_t *s) {
    process_array_clear(&s->take_fastmem);
    process_array_clear(&s->need_fastmem);
    s->fastmem_demand_total = 0;
    s->fastmem_supply_total = 0;

    // iterate once to handle ring requests and calculate current miss ratios
    PROCESS_FOR_EACH(process) {
        pthread_mutex_lock(&(process->process_lock));
        struct process_policy *policy = &process->policy;
        mm_drain_async_requests(process);

        policy->fastmem_delta = 0;

        policy->current_miss_ratio = update_process_miss_ratio(policy);
        policy->access_count[FASTMEM] = 0;
        policy->access_count[SLOWMEM] = 0;

        // how are we doing on our miss ratio vs target?
        policy->deviation_ratio =
            policy->current_miss_ratio / policy->target_miss_ratio;

        if (policy->deviation_ratio > QOS_NEED_MEMORY_THRESHOLD) {
            // not meeting target, need more fast mem
            process_array_append(&s->need_fastmem, process);

            policy->deviation_ratio =
                min(QOS_DEVIATION_RATIO_MAX, policy->deviation_ratio);

            s->fastmem_demand_total += policy->deviation_ratio;
        } else if (policy->deviation_ratio < QOS_TAKE_MEMORY_THRESHOLD) {
            // below target, can take fast mem
            process_array_append(&s->take_fastmem, process);

            // invert the deviation ratio
            policy->deviation_ratio =
                ((policy->target_miss_ratio / policy->current_miss_ratio));

            policy->deviation_ratio =
                min(QOS_DEVIATION_RATIO_MAX, policy->deviation_ratio);

            s->fastmem_supply_total += policy->deviation_ratio;
        }

        pthread_mutex_unlock(&(process->process_lock));
    }

    LOG_DEBUG("ratio of memory needed %.4f\tratio of memory taking %.4f\n",
              s->fastmem_demand_total, s->fastmem_supply_total);
}

static uint64_t allocate_demotion_budgets(qos_policy_state_t *s,
                                          uint64_t max_demotion_bytes) {
    uint64_t total_take_bytes = 0;
    process_array_iter_t it;
    process_array_iter_init(&it);

    struct hemem_process *process;
    while ((process = process_array_next(&s->take_fastmem, &it))) {
        struct process_policy *policy = &process->policy;
        if (s->fastmem_supply_total <= 0) {
            policy->fastmem_delta = 0;
            continue;
        }

        double my_portion = policy->deviation_ratio / s->fastmem_supply_total;
        int64_t delta = my_portion * max_demotion_bytes;
        delta -= (delta % HUGEPAGE_SIZE);  // align to page size

        // Don't take more memory than the process currently has in fastmem.
        delta = min(delta, (int64_t)process->mm.current_fastmem);

        policy->fastmem_delta = -delta;  // Negative delta means taking memory.
        total_take_bytes += delta;
    }

    return total_take_bytes;
}

static void allocate_promotion_budgets(qos_policy_state_t *s,
                                       uint64_t max_promotion_bytes) {
    process_array_iter_t it;
    process_array_iter_init(&it);

    struct hemem_process *process;
    while ((process = process_array_next(&s->need_fastmem, &it))) {
        struct process_policy *policy = &process->policy;
        if (s->fastmem_demand_total <= 0) {
            policy->fastmem_delta = 0;
            continue;
        }

        double my_portion = policy->deviation_ratio / s->fastmem_demand_total;
        int64_t delta = my_portion * max_promotion_bytes;
        delta -= (delta % HUGEPAGE_SIZE);  // align to page size

        // Don't give a process more memory than its total allocation.
        if (process->mm.current_fastmem + delta > process->mm.mem_allocated) {
            delta = process->mm.mem_allocated - process->mm.current_fastmem;
            delta = (delta < 0) ? 0 : delta;
        }

        policy->fastmem_delta = delta;
    }
}

static void allocate_migration_budget(qos_policy_state_t *s) {
    uint64_t interprocess_migrate = PEBS_MIGRATE_RATE / 2;
    uint64_t intraprocess_migrate = PEBS_MIGRATE_RATE / 2;

    uint64_t interprocess_migrate_one_way = interprocess_migrate / 2;

    uint64_t total_demotion_bytes =
        allocate_demotion_budgets(s, interprocess_migrate_one_way);

    uint64_t promotion_budget = total_demotion_bytes + get_fastmem_free_size();
    allocate_promotion_budgets(s, promotion_budget);

    size_t num_procs =
        (processes_list.numentries > 0) ? processes_list.numentries : 1;
    s->per_process_migrate_share = intraprocess_migrate / num_procs;
}

static void do_partial_cool() {
    PROCESS_FOR_EACH(process) {
        pthread_mutex_lock(&(process->process_lock));

        process->mm.cur_cool_in_fastmem = incremental_fastmem_cooling(process);
        process->mm.cur_cool_in_slowmem = incremental_slowmem_cooling(process);

        pthread_mutex_unlock(&(process->process_lock));
    }
}

__maybe_unused static void request_cooling() {
    PROCESS_FOR_EACH(proc) {
        mm_request_soft_cool_fastmem(proc);
        mm_request_soft_cool_slowmem(proc);
    }
}

void qos_policy_epoch(void *opaque, size_t epoch_num) {
    UNUSED(epoch_num);
#ifdef false
    if (epoch_num % 40 == 0) {
        request_cooling();
    }
#endif

    qos_policy_state_t *s = (qos_policy_state_t *)opaque;

    // 1. check which processes need fastmem (and which have too much)
    // this compares each process's current miss ratio to their targets.
    check_mem_demand_supply(s);

    if (s->opts->enable_migration) {
        // 2. Allocatge migration budgets based on the extent to which a process
        // needs more fastmem. Processes that don't need as much fastmem can
        // donate fastmem by demotion.
        allocate_migration_budget(s);

        // 3. Actually carry out the migrations based on migration budget.
        do_migration(s);
    }

    // 4. do cooling, if needed
    do_partial_cool();
}

void *qos_policy_state_init(struct policy_opts *opts) {
    qos_policy_state_t *s = calloc(1, sizeof(qos_policy_state_t));
    if (!s) {
        return NULL;
    }

    s->opts = opts;

    s->page_alloc = get_alloc_fn_by_mode(opts->alloc_mode);

    bool ret;
    ret = process_array_init(&s->need_fastmem, MAX_PROCESSES);
    if (!ret) {
        LOG_ERR("failed to init 'need_fastmem' list\n");
        exit(1);
    }
    ret = process_array_init(&s->take_fastmem, MAX_PROCESSES);
    if (!ret) {
        LOG_ERR("failed to init 'take_slowmem' list\n");
        exit(1);
    }

    return s;
}

void qos_policy_state_destroy(void *opaque) {
    qos_policy_state_t *s = (qos_policy_state_t *)opaque;
    free(s);
}

static struct hemem_page *qos_allocate_page(void *opaque,
                                            struct hemem_process *process) {
    qos_policy_state_t *s = (qos_policy_state_t *)opaque;
    return s->page_alloc(process);
}

static struct hemem_page *qos_find_demotion_candidate(
    void *opaque, struct hemem_process *process) {
    UNUSED(opaque);
    return find_coldest_fastmem_page(process);
}

static struct hemem_page *qos_find_promotion_candidate(
    void *opaque, struct hemem_process *process) {
    UNUSED(opaque);
    return find_hottest_slowmem_page(process);
}

int qos_policy_init(policy_t *p, struct policy_opts *opts) {
    p->opaque = qos_policy_state_init(opts);
    if (!p->opaque) {
        LOG_ERR("failed to initialize QoS policy state\n");
        exit(1);
    }

    p->epoch_handler = qos_policy_epoch;
    p->allocate_page = qos_allocate_page;
    p->find_demotion_candidate = qos_find_demotion_candidate;
    p->find_promotion_candidate = qos_find_promotion_candidate;

    return 0;
}
