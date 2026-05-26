#include "memops.h"

#include <string.h>

#include "memcpy-par.h"
#include "memops-offload.h"
#include "ucm-config.h"
#include "util/log.h"

void pprint_memops_opts() {
#ifdef CONFIG_DMA
    const char *mode = "DMA";
#elif defined(CONFIG_PAR_MEMCPY)
    const char *mode = "parallel memcpy";
#else
    const char *mode = "memcpy";
#endif

    LOG_NOPATH("memory copy mode: %s\n", mode);
}

void memops_init() {
#ifdef CONFIG_DMA
    memops_offload_init();
#endif
#ifdef CONFIG_PAR_MEMCPY
    memcpy_par_init();
#endif
}

void memops_shutdown() {
#ifdef CONFIG_DMA
    memops_offload_shutdown();
#endif
#ifdef CONFIG_PAR_MEMCPY
    memcpy_par_shutdown();
#endif
}

void hemem_memcpy(void *dst, const void *src, size_t len) {
#ifdef CONFIG_DMA
    memops_offload_memcpy(dst, src, len);
#elif defined(CONFIG_PAR_MEMCPY)
    memcpy_par(dst, src, len);
#else
    memcpy(dst, src, len);
#endif
}

void hemem_memset(void *dst, int value, size_t len) {
#ifdef CONFIG_DMA
    memops_offload_memset(dst, value, len);
#else
    memset(dst, value, len);
#endif
}
