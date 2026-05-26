#include "core-util.h"

#include "telem/source/pebs.h"
#include "util/log.h"

static inline uint64_t _access_to_index(uint64_t num) {
    if (num <= 0) {
        return 0;
    }
    uint64_t ret = 64 - __builtin_clzll(num);
    if (ret > NUM_HOTNESS_LEVELS - 1) {
        return NUM_HOTNESS_LEVELS - 1;
    }
    return ret;
}

inline uint64_t access_to_index(uint64_t dram_access, uint64_t cxl_access) {
#ifdef CONFIG_CXL_TELEM
    // This assumes the CXL counters don't do any sampling.
    // Here we lower the CXL counter value to use the same fidelity as PEBS.
    cxl_access /= PEBS_SAMPLE_PERIOD;
#endif
    return _access_to_index(dram_access + cxl_access);
}
