#include "cxl.h"

#include "core-util.h"
#include "core.h"
#include "mm-async.h"
#include "shared.h"
#include "type/page.h"
#include "type/process.h"
#include "util/log.h"

// Counts number of cxl counters where it can't be attributed to a process that
// we know about.
uint64_t cxl_unknown_proc_sample_cnt = 0;

static bool _detect_hot_cxl_page(uint prev_cnt, uint new_count) {
    size_t hot_thresh = 0;

    bool is_hot = new_count > (prev_cnt + hot_thresh) ||
                  new_count < prev_cnt /* wrapped around */;
    return is_hot;
}

static void cxl_find_proc_page(uint64_t cxl_devdax_offset,
                               struct hemem_process **pproc,
                               struct hemem_page **ppage) {
    struct hemem_process *proc = mm_find_proc_by_cxl_offset(cxl_devdax_offset);

    // TODO: if base page is supported, we may want to support finding the base
    // page as well as the hugepage it used to be a part of
    if (!proc) {
        LOG_ERR_DEBUG("%lu proc not found\n",
                      cxl_devdax_offset / HUGEPAGE_SIZE);
        return;
    }
    *pproc = proc;

    struct hemem_page *page =
        mm_find_page_by_cxl_offset(&proc->mm, cxl_devdax_offset);
    if (page) {
        assert(!page->in_dram);
    }
    *ppage = page;
}

static unsigned long counter_safe_add(unsigned long a, long b) {
    if (b >= 0) {
        // overflow is fine for our counter.
        return a + b;
    } else {
        // b is negative, so we're effectively subtracting its absolute value
        unsigned long abs_b = (unsigned long)(-b);
        if (abs_b > a) {
            // prevent underflow.
            return 0;
        }
        return a - abs_b;
    }
}

static void cxl_page_update_stats(struct hemem_process *proc,
                                  struct hemem_page *page, int64_t delta) {
    page->accesses[SLOWMEM] = counter_safe_add(page->accesses[SLOWMEM], delta);
    page->tot_accesses[SLOWMEM] =
        counter_safe_add(page->tot_accesses[SLOWMEM], delta);

    if (page->accesses[SLOWMEM] > CXL_COOLING_THRESHOLD) {
        mm_request_soft_cool_slowmem(proc);
    }

    page->accesses[FASTMEM] >>=
        (proc->process_clock[FASTMEM] - page->local_clock[FASTMEM]);
    page->accesses[SLOWMEM] >>=
        (proc->process_clock[SLOWMEM] - page->local_clock[SLOWMEM]);
    page->local_clock[FASTMEM] = proc->process_clock[FASTMEM];
    page->local_clock[SLOWMEM] = proc->process_clock[SLOWMEM];

    uint64_t new_hotness =
        access_to_index(page->accesses[FASTMEM], page->accesses[SLOWMEM]);

    // check for hotness change and add to ring
    size_t curr_epoch = ucm_get_curr_epoch();
    if (new_hotness > page->hot) {
        if (mm_mark_page_hot_async(proc, page)) {
            LOG_DEBUG(
                "epoch %lu CXL hot: page (va: %p) from hot %lu to %d, "
                "dram %lu nvm %lu\n",
                curr_epoch, (void *)page->va, page->hot, new_hotness,
                page->accesses[FASTMEM], page->accesses[SLOWMEM]);
        }
    } else if (new_hotness < page->hot) {
        if (mm_mark_page_cold_async(proc, page)) {
            LOG_DEBUG(
                "epoch %lu CXL cold request: page (va: %p) from hot %lu to "
                "%d\n",
                curr_epoch, (void *)page->va, page->hot, new_hotness);
        }
    }
}

// Determines the delta to apply to a cxl access counter.
//
// Note that the delta could be negative -- this is because we may have
// over-counted before due to migration traffic.
static int64_t cxl_calc_access_count_delta(access_counter_t prev_cnt,
                                           access_counter_t curr_cnt,
                                           struct hemem_page *page) {
    int64_t access_cnt_delta =
        (curr_cnt > prev_cnt) ? (curr_cnt - prev_cnt) : curr_cnt;
    if (curr_cnt < prev_cnt) {
        LOG_WARN("OVERFLOW: page prev %lu curr %lu\n", prev_cnt, curr_cnt);
    }

    // Optimistically reset the migration compensation.
    uint64_t migration_comp =
        (int64_t)atomic_exchange(&page->migration_access_comp, 0);
    int64_t delta = access_cnt_delta - migration_comp;

    return delta;
}

// Try to apply an access count delta (could increment or decrement).
//
// Take special care to avoid counter underflow. If about to underflow, deposit
// back some migration traffic deductions to be applied in the next epoch.
void cxl_apply_access_count_delta(int64_t delta, struct hemem_process *proc,
                                  struct hemem_page *page) {
    // prevent underflow
    if (delta > 0) {
        proc->policy.access_count[SLOWMEM] += delta;
    } else {
        access_counter_t delta_abs = -delta;
        uint64_t curr = proc->policy.access_count[SLOWMEM];
        if (delta_abs > curr) {
            proc->policy.access_count[SLOWMEM] -= curr;
            if (ucm_get_curr_epoch() <= page->last_migrated_epoch + 1) {
                // only add back if the migration happened recently
                uint64_t remaining_delta = labs(delta) - curr;
                page->migration_access_comp += remaining_delta;
            }
        } else {
            proc->policy.access_count[SLOWMEM] += delta;
        }
    }
}

static access_counter_t cxl_update_stats(access_counter_t prev_cnt,
                                         access_counter_t curr_cnt,
                                         struct hemem_process *proc,
                                         struct hemem_page *page) {
    int64_t delta = cxl_calc_access_count_delta(prev_cnt, curr_cnt, page);

#if defined(CONFIG_POLICY_QOS) || defined(CONFIG_POLICY_FAIR_SHARE)
    cxl_apply_access_count_delta(delta, proc, page);
#endif

    cxl_page_update_stats(proc, page, delta);

    return delta;
}

void on_cxl_counter_update(size_t cxl_devdax_offset, access_counter_t prev_cnt,
                           access_counter_t curr_cnt) {
    if (curr_cnt == prev_cnt || ucm_get_curr_epoch() == 0) {
        return;
    }
    if (_detect_hot_cxl_page(prev_cnt, curr_cnt)) {
        struct hemem_process *proc = NULL;
        struct hemem_page *page = NULL;
        cxl_find_proc_page(cxl_devdax_offset, &proc, &page);
        if (!proc) {
            // most of this is due to migration traffic
            cxl_unknown_proc_sample_cnt++;
            return;
        }
        if (!page) {
            // most of this is due to migration traffic
            proc->stats.cxl.unknown_page_count++;
            return;
        }

        cxl_update_stats(prev_cnt, curr_cnt, proc, page);
    }
}
