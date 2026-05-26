#include "physmem.h"

#include <stdlib.h>
#include <string.h>

#include "dax.h"
#include "mm.h"
#include "physmem/config.h"
#include "shm.h"
#include "util/log.h"

int fastmem_fd = -1;
void *fastmem_ptr = 0;

int slowmem_fd = -1;
void *slowmem_ptr = 0;

/** Module-local state. */
typedef struct {
    physmem_mode_t mode;
    size_t fastmem_sz;
    size_t slowmem_sz;
} physmem_ctx_t;

physmem_ctx_t g_physmem_ctx;

size_t physmem_get_fastmem_size() { return g_physmem_ctx.fastmem_sz; }

size_t physmem_get_slowmem_size() { return g_physmem_ctx.slowmem_sz; }

static void physmem_devdax_init(physmem_ctx_t *ctx) {
    fastmem_ptr =
        devdax_open_mmap(FASTMEM_DEVDAX_PATH, ctx->fastmem_sz, &fastmem_fd);
    if (!fastmem_ptr) {
        LOG_ERR("fastmem init failed, aborting...\n");
        exit(1);
    }
    LOG("Fast tier memory VA:\t[%p, %p)\n", fastmem_ptr,
        fastmem_ptr + ctx->fastmem_sz);

    slowmem_ptr =
        // devdax_open_mmap(SLOWMEM_DEVDAX_PATH, ctx->slowmem_sz, &slowmem_fd);
        devdax_open_mmap(SLOWMEM_DEVDAX_PATH, CXL_CAPACITY, &slowmem_fd);
    if (!slowmem_ptr) {
        LOG_ERR("slowmem init failed, aborting...\n");
        exit(1);
    }
}

static void physmem_shm_init(physmem_ctx_t *ctx) {
    fastmem_ptr = shm_create_mmap(FASTMEM_SHM_PATH, ctx->fastmem_sz);
    if (!fastmem_ptr) {
        LOG_ERR("fastmem init failed, aborting...\n");
        exit(1);
    }
    LOG("Fast tier memory VA:\t[%p, %p)\n", fastmem_ptr,
        fastmem_ptr + ctx->fastmem_sz);

    slowmem_ptr = shm_create_mmap(SLOWMEM_SHM_PATH, ctx->slowmem_sz);
    if (!slowmem_ptr) {
        LOG_ERR("slowmem init failed, aborting...\n");
        exit(1);
    }
}

char *get_fastmem_file_path() {
    switch (g_physmem_ctx.mode) {
        case USE_DAX:
            return FASTMEM_DEVDAX_PATH;
        case USE_SHM:
            return FASTMEM_SHM_PATH;
        default:
            LOG_ERR("this is a bug; encountered unsupported physmem mode %d\n",
                    g_physmem_ctx.mode);
            exit(1);
    }
}

char *get_slowmem_file_path() {
    switch (g_physmem_ctx.mode) {
        case USE_DAX:
            return SLOWMEM_DEVDAX_PATH;
        case USE_SHM:
            return SLOWMEM_SHM_PATH;
        default:
            LOG_ERR("this is a bug; encountered unsupported physmem mode %d\n",
                    g_physmem_ctx.mode);
            exit(1);
    }
}

physmem_mode_t get_physmem_mode() { return g_physmem_ctx.mode; }

void physmem_init(mm_opts_t *opts) {
    g_physmem_ctx.mode = opts->physmem_mode;
    g_physmem_ctx.fastmem_sz = opts->fastmem_size;
    g_physmem_ctx.slowmem_sz = opts->slowmem_size;
    switch (g_physmem_ctx.mode) {
        case USE_DAX:
            LOG("initializing memory with DAX\n");
            physmem_devdax_init(&g_physmem_ctx);
            break;
        case USE_SHM:
            LOG("initializing memory with SHM\n");
            physmem_shm_init(&g_physmem_ctx);
            break;
        default:
            LOG_ERR("this is a bug; encountered unsupported physmem mode %d\n",
                    g_physmem_ctx.mode);
            exit(1);
    }
}

void *get_slowmem_mmap_addr() { return slowmem_ptr; }

void *devdax_offset_to_addr(uint64_t offset, bool is_fastmem) {
    if (is_fastmem) {
        return (void *)((uint64_t)fastmem_ptr + offset);
    } else {
        return (void *)((uint64_t)slowmem_ptr + offset);
    }
}
