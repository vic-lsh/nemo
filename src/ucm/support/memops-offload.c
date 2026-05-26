#include "memops-offload.h"

#include <assert.h>

#include "dsa.h"
#include "util/log.h"

void memops_offload_init() {
    int ret = dsa_init();
    if (ret) {
        LOG_ERR("dsa init error %d\n", ret);
        assert(0);
    }
}

void memops_offload_shutdown() { dsa_shutdown(); }

void memops_offload_memcpy(void* dst, const void* src, size_t len) {
    int rc = dsa_memcpy(dst, src, len);
    assert(!rc);
}

void memops_offload_memset(void* dst, int value, size_t len) {
    int rc = dsa_memset(dst, value, len);
    assert(!rc);
}
