#ifndef UCM_MM_CONFIG_H
#define UCM_MM_CONFIG_H

// Defines the usable space in the CXL slow tier.
#define CXL_USABLE_SIZE (1550 * (1024L * 1024L * 1024L) / 100)
// Defines the total capacity in the CXL slow tier.
// moved to libs/cxl-dev
#include "cxl-dev/cxl-dev.h"
//#define CXL_CAPACITY (16 * (1024L * 1024L * 1024L))

_Static_assert(CXL_USABLE_SIZE <= CXL_CAPACITY,
               "Slow tier usable size must be <= its capacity");

#define FASTMEM_DEVDAX_PATH "/dev/dax0.0"
#define SLOWMEM_DEVDAX_PATH "/dev/dax1.0"

#define FASTMEM_SHM_PATH "/mnt/nemo_shm/fastmem"
#define SLOWMEM_SHM_PATH "/mnt/nemo_shm/slowmem"

#endif /* UCM_MM_CONFIG_H */
