#ifndef HEMEM_SHARED_H
#define HEMEM_SHARED_H

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>
#include <unistd.h>

#define BASEPAGE_SIZE (4UL * 1024UL)
#define HUGEPAGE_SIZE (2UL * 1024UL * 1024UL)
#define GIGAPAGE_SIZE (1024UL * 1024UL * 1024UL)
#define PAGE_ROUND_UP(x) (((x) + (HUGEPAGE_SIZE)-1) & (~((HUGEPAGE_SIZE)-1)))
#define PAGE_ROUND_DOWN(x) ((x) & ~(HUGEPAGE_SIZE - 1))
#define N_HUGEPAGES_IN_GB (512)
#define N_BASEPAGES_IN_HUGEPAGE (512)

#define CACHELINE_SIZE (64UL)
#define CACHELINES_IN_HUGEPAGE (HUGEPAGE_SIZE / CACHELINE_SIZE)
#define CACHELINES_IN_BASEPAGE (BASEPAGE_SIZE / CACHELINE_SIZE)

#define BASEPAGE_MASK (BASEPAGE_SIZE - 1)
#define HUGEPAGE_MASK (HUGEPAGE_SIZE - 1)
#define GIGAPAGE_MASK (GIGAPAGE_SIZE - 1)

#define BASE_PFN_MASK (BASEPAGE_MASK ^ UINT64_MAX)
#define HUGE_PFN_MASK (HUGEPAGE_MASK ^ UINT64_MAX)
#define GIGA_PFN_MASK (GIGAPAGE_MASK ^ UINT64_MAX)

#define MAX_FAULT_PAGES 16

enum pagetypes { HUGEP = 0, BASEP = 1, NPAGETYPES };

static inline uint64_t pt_to_pagesize(enum pagetypes pt) {
    switch (pt) {
        case HUGEP:
            return HUGEPAGE_SIZE;
        case BASEP:
            return BASEPAGE_SIZE;
        default:
            assert(!"Unknown page type");
    }
}

static inline enum pagetypes pagesize_to_pt(uint64_t pagesize) {
    switch (pagesize) {
        case BASEPAGE_SIZE:
            return BASEP;
        case HUGEPAGE_SIZE:
            return HUGEP;
        default:
            assert(!"Unknown page ssize");
    }
}

static inline size_t pt_to_cacheline_count(enum pagetypes pt) {
    switch (pt) {
        case HUGEP:
            return CACHELINES_IN_HUGEPAGE;
        case BASEP:
            return CACHELINES_IN_BASEPAGE;
        default:
            assert(!"Unknown page type");
    }
}

#endif /* HEMEM_SHARED_H */
