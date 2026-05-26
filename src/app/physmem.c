#include "physmem.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

#include "util/log.h"

int physmem_ctx_init(physmem_ctx_t* ctx) {
    ctx->fastmem_fd = open(ctx->fastmem_path, O_RDWR);
    if (ctx->fastmem_fd < 0) {
        perror("fastmem open");
        LOG_ERR("fastmem open failed at path '%s'\n", ctx->fastmem_path);
        return -errno;
    }
    LOG("fastmem opened at path '%s' fd %d\n", ctx->fastmem_path,
        ctx->fastmem_fd);

    ctx->slowmem_fd = open(ctx->slowmem_path, O_RDWR);
    if (ctx->slowmem_fd < 0) {
        perror("slowmem open");
        LOG_ERR("slowmem open failed at path '%s'\n", ctx->slowmem_path);
        return -errno;
    }
    LOG("slowmem open at path '%s' fd %d\n", ctx->slowmem_path,
        ctx->slowmem_fd);

    return 0;
}

int physmem_ctx_destroy(physmem_ctx_t* ctx) {
    close(ctx->fastmem_fd);
    close(ctx->slowmem_fd);
    return 0;
}

bool is_our_mem_fd(physmem_ctx_t* ctx, int fd) {
    return (fd == ctx->fastmem_fd) || (fd == ctx->slowmem_fd);
}
