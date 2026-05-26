#include "pebs.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "core-util.h"
#include "hemem-shared.h"
#include "mm-async.h"
#include "proc-mgr.h"
#include "shared.h"
#include "stats/pebs-stats.h"
#include "type/page.h"
#include "type/process.h"
#include "ucm-config.h"
#include "util/compiler.h"
#include "util/log.h"

static inline size_t get_basepage_index_in_hugepage(uint64_t addr) {
#define PAGE_4KB_MASK 0x1FF000  // 0001 1111 1111 0000 0000 0000

    size_t base_page_num = (addr & PAGE_4KB_MASK) >> 12;

    assert(base_page_num < N_BASEPAGES_IN_HUGEPAGE);

    return base_page_num;
}

static inline bool safe_add_uint32(uint32_t a, uint32_t b, uint32_t *result) {
    if (a > UINT32_MAX - b) {
        return false;
    }
    *result = a + b;
    return true;
}

#ifdef CONFIG_PEBS_SKEWNESS
static void halve_skewness_counters(struct hemem_page *page) {
    for (size_t i = 0; i < N_BASEPAGES_IN_HUGEPAGE; i++) {
        page->subpage_skewness[i] >>= 1;
    }
}

static void update_skewness_counter(struct hemem_page *page,
                                    uint64_t pebs_sample_va) {
    size_t basepage = get_basepage_index_in_hugepage(pebs_sample_va);

    if (unlikely(!safe_add_uint32(page->subpage_skewness[basepage], 1,
                                  &page->subpage_skewness[basepage]))) {
        halve_skewness_counters(page);
        // just halved; no need to do overflow checking
        page->subpage_skewness[basepage]++;
    }
}
#endif

static void process_valid_pebs_sample(struct hemem_process *process,
                                      struct hemem_page *page,
                                      __maybe_unused uint64_t pebs_sample_va) {
    uint64_t new_hotness =
        access_to_index(page->accesses[FASTMEM], page->accesses[SLOWMEM]);
    // check for hotness change and add to ring
    if (new_hotness > page->hot) {
        mm_mark_page_hot_async(process, page);
    } else if (new_hotness < page->hot) {
        mm_mark_page_cold_async(process, page);
    }

    page->accesses[FASTMEM] >>=
        (process->process_clock[FASTMEM] - page->local_clock[FASTMEM]);
    page->accesses[SLOWMEM] >>=
        (process->process_clock[SLOWMEM] - page->local_clock[SLOWMEM]);
    page->local_clock[FASTMEM] = process->process_clock[FASTMEM];
    page->local_clock[SLOWMEM] = process->process_clock[SLOWMEM];

#ifdef CONFIG_PEBS_SKEWNESS
    update_skewness_counter(page, pebs_sample_va);
#endif

    if (page->accesses[FASTMEM] > PEBS_COOLING_THRESHOLD) {
        process->mm.epoch_did_cool = true;
        mm_request_cool_fastmem(process);
        mm_request_cool_slowmem(process);
    }
}

static void process_perf_sample(struct perf_sample *ps, size_t core_idx) {
    pebs_stats_t *pebs_stats = pebs_stats_get();

    pebs_stats->total_samples_cnt++;

    if (ps->addr == 0) {
        pebs_stats->zero_pages_cnt++;
        return;
    }

    struct hemem_process *process = find_process(ps->pid);
    if (process == NULL) {
        pebs_stats->non_hemem_proc_samples_cnt++;
        return;
    }

    __u64 pfn = ps->addr & HUGE_PFN_MASK;
    struct hemem_page *page = mm_find_page(&process->mm, pfn);

    // Page access stats
    if (page == NULL) {
        pebs_stats->non_hemem_page_samples_cnt++;
        return;
    } else {
        pebs_stats->valid_samples_cnt++;
        if (!page->in_dram) {
            pebs_stats->cxl_samples_cnt++;
        }
    }

    if (page->va == 0) {
        assert(0);
    }

    process->policy.sample_count[core_idx]++;

    size_t tier = (page->in_dram) ? FASTMEM : SLOWMEM;

    // if we have CXL counters enabled, we ignore PEBS samples for CXL.
#ifdef CONFIG_CXL_TELEM
    bool count_cxl_sample = false;
#else
    bool count_cxl_sample = true;
#endif

    bool do_update = tier == FASTMEM || (tier == SLOWMEM && count_cxl_sample);
    if (do_update) {
#if defined(CONFIG_POLICY_QOS) || defined(CONFIG_POLICY_FAIR_SHARE)
        process->policy.access_count[tier]++;
#endif
        page->accesses[tier]++;
        page->tot_accesses[tier]++;

        process_valid_pebs_sample(process, page, ps->addr);
    }
}

void on_pebs_sample_received(struct perf_event_header *ph, size_t core_idx) {
    pebs_stats_t *pebs_stats = pebs_stats_get();
    struct perf_sample *ps;

    assert(ph != NULL);

    switch (ph->type) {
        case PERF_RECORD_SAMPLE:
            ps = (struct perf_sample *)ph;
            process_perf_sample(ps, core_idx);
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

void pebs_handler_init() { pebs_stats_init(); }
