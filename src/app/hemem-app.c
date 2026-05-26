#define _GNU_SOURCE

#include "hemem-app.h"

#include <assert.h>
#include <fcntl.h>
#include <inttypes.h>
#include <linux/userfaultfd.h>
#include <poll.h>
#include <pthread.h>
#include <pwd.h>
#include <sched.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "hemem-shared.h"
#include "ipc-chan/client.h"
#include "ipc-shared.h"
#include "ipc.h"
#include "mmap.h"
#include "physmem.h"
#include "remap.h"
#include "uffd.h"
#include "util/log.h"
#include "util/proc.h"

#define PAGE_ROUND_UP(x) (((x) + (HUGEPAGE_SIZE)-1) & (~((HUGEPAGE_SIZE)-1)))

app_ctx_t _global_ctx = {
    .init_mu = PTHREAD_MUTEX_INITIALIZER,
    .is_init = false,
};

__thread bool internal_call = false;

double target_miss_ratio = 0.1;

void* hemem_mmap(void* addr, size_t length, int prot, int flags, int fd,
                 off_t offset) {
    return hemem_mmap_impl(&_global_ctx, addr, length, prot, flags, fd, offset);
}

// TODO: apps may maintain multiple MAP_FIXED mappings, so unmapping one
// doesn't mean that we should free it.
// We'd need a way to refcount.
int hemem_munmap(void* addr, size_t length) {
    return hemem_munmap_impl(&_global_ctx, addr, length);
}

void hemem_app_stop() {
    //    ipc_remove_process();
}

static void normal_exit_handler(void) { hemem_app_stop(); }

static void signal_handler(int sig) {
    LOG_ERR("caught signal %d, cleaning up...\n", sig);
    hemem_app_stop();
}

__attribute__((unused)) static void install_exit_handlers() {
    atexit(normal_exit_handler);
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
}

app_ctx_t* get_app_ctx() { return &_global_ctx; }

static void app_ctx_close(app_ctx_t* ctx) {
    LOG("destroying app_ctx\n");
    close(ctx->ipc_fd);
    close(ctx->uffd);
    remap_app_destroy(&ctx->remap_ctx);
    physmem_ctx_destroy(&ctx->mem_ctx);
}

static int app_ctx_init(app_ctx_t* ctx) {
    ctx->self_pid = getpid();

    ctx->ipc_fd = channel_client_init(REQUEST);
    if (ctx->ipc_fd < 0) {
        LOG_ERR("ipc channel init\n");
        goto cleanup_ipc_fd;
    }

    ctx->remap_ctx.remap_fd = channel_client_init(REMAP);
    if (ctx->remap_ctx.remap_fd < 0) {
        LOG_ERR("remap channel init");
        goto cleanup_remap_fd;
    }

    ctx->uffd = uffd_open();
    if (ctx->uffd < 0) {
        LOG_ERR("failed to create uffd\n");
        goto cleanup_uffd;
    }
    LOG("app uffd = %ld\n", ctx->uffd);

    enum status_code rc = ipc_add_process(ctx);

    if (rc != SUCCESS) {
        LOG_ERR("add process %d failed\n", ctx->self_pid);
        goto cleanup_add_proc;
    }

    if (physmem_ctx_init(&ctx->mem_ctx) != 0) {
        LOG_ERR(
            "failed to init physical memory: fastmem path '%s' slowmem path "
            "'%s'\n",
            ctx->mem_ctx.fastmem_path, ctx->mem_ctx.slowmem_path);
        goto cleanup_physmem;
    }

    remap_app_init(&ctx->remap_ctx, ctx->host_pid, ctx->uffd, &ctx->mem_ctx);

    return 0;

cleanup_physmem:
    physmem_ctx_destroy(&ctx->mem_ctx);
cleanup_add_proc:
cleanup_uffd:
    close(ctx->remap_ctx.remap_fd);
cleanup_remap_fd:
    close(ctx->ipc_fd);
cleanup_ipc_fd:
    return -1;
}

int app_ctx_reinit(app_ctx_t* ctx) {
    app_ctx_close(ctx);
    return app_ctx_init(ctx);
}

void hemem_app_init() {
    pthread_mutex_lock(&_global_ctx.init_mu);

    LOG("hemem_app_init\n");
    if (_global_ctx.is_init) {
        LOG_DEBUG("duplicate init call, stopping init.\n");
        goto out;
    } else {
        LOG_DEBUG("not init yet, going to init\n");
    }

    internal_call = true;

    uid_t uid = getuid();
    struct passwd* pw = getpwuid(uid);

    if (pw) {
        LOG("Username: %s, pid %d pid ns %ld\n", pw->pw_name, getpid(),
            get_pid_namespace());
    } else {
        perror("getpwuid");
        exit(1);
    }

    char* target_miss_ratio_str = getenv("MISS_RATIO");
    if (target_miss_ratio_str != NULL) {
        target_miss_ratio = atof(target_miss_ratio_str);
    }

    int rc = app_ctx_init(&_global_ctx);
    if (rc != 0) {
        LOG_ERR("app_ctx_init failed\n");
        exit(1);
    }

    // TODO: this doesn't seem to work well yet; we'll revisit this.
    // install_exit_handlers();

    _global_ctx.is_init = true;

out:
    internal_call = false;
    pthread_mutex_unlock(&_global_ctx.init_mu);
}
