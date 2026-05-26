#include <stdatomic.h>

#include "physmem.h"
#define _GNU_SOURCE

#include <linux/userfaultfd.h>
#include <pthread.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/syscall.h>

#include "hemem-app.h"
#include "hemem-shared.h"
#include "interpose.h"
#include "ipc-chan/client.h"
#include "ipc-shared.h"
#include "ipc.h"
#include "remap.h"
#include "uffd.h"
#include "util/log.h"
#include "util/proc.h"

size_t remap_count = 0;

typedef struct {
    int remap_fd;
    long uffd;
    int host_pid;
    physmem_ctx_t* mem_ctx;
} remap_thread_ctx_t;

static void remap_page(struct hemem_page_app* page, long uffd,
                       physmem_ctx_t* mem_ctx) {
    void* newptr;
    uint64_t pagesize;
    int fd;
    bool in_dram = page->in_dram;

    pagesize = pt_to_pagesize(page->pt);

    if (in_dram) {
        fd = mem_ctx->fastmem_fd;
    } else {
        fd = mem_ctx->slowmem_fd;
    }

    LOG_DEBUG("remap mmapping va %p to %lu remap count %lu\n", (void*)page->va,
              page->devdax_offset / MB(2), remap_count++);

    newptr = libc_mmap((void*)page->va, pagesize, PROT_READ | PROT_WRITE,
                       MAP_SHARED | MAP_POPULATE | MAP_FIXED, fd,
                       page->devdax_offset);
    if (newptr == MAP_FAILED) {
        perror("newptr mmap");
        assert(0);
    }

    // re-register new mmap region with userfaultfd
    if (uffd_register_page(uffd, page->va, pagesize) != 0) {
        assert(0);
    }
    assert((uint64_t)newptr != 0);
    assert((uint64_t)newptr % pagesize == 0);
}

void handle_remap_request(remap_thread_ctx_t* thr_ctx,
                          struct remap_request* request) {
    int num_pages = request->num_pages;

    for (int i = 0; i < num_pages; i++) {
        remap_page(&(request->pages[i]), thr_ctx->uffd, thr_ctx->mem_ctx);
    }

    struct remap_response response;

    response.header.status = SUCCESS;
    response.header.pid = thr_ctx->host_pid;
    response.header.msg_size = sizeof(response);
    response.header.operation = REMAP_PAGES;
    response.va = request->pages[0].va;

    int len = write(thr_ctx->remap_fd, &response, sizeof(response));
    if (len < 0) {
        perror("remap reply");
        assert(0);
    }
}

void handle_remap_bufread(remap_thread_ctx_t* thr_ctx, char* remap_buf,
                          size_t buflen) {
    struct msg_header* header;
    size_t offset = 0;

    // Handle 1 or more remap requests.
    while (offset < buflen) {
        size_t sz_remaining = buflen - offset;

        if (sz_remaining < sizeof(*header)) {
            LOG_ERR("remaining sz %lu expected %lu buf total %lu\n",
                    sz_remaining, sizeof(*header), buflen);
            assert(0);
        }

        header = (struct msg_header*)(&remap_buf[offset]);
        if (header->operation != REMAP_PAGES) {
            LOG_ERR("Invaid remap request op %d\n", header->operation);
            assert(0);
        }

        struct remap_request* request = (struct remap_request*)remap_buf;

        handle_remap_request(thr_ctx, request);

        offset += header->msg_size;
    }
}

void remap_thread_init() {
    pthread_t thread_id;
    struct sched_param param;
    int policy;

    thread_id = pthread_self();
    pthread_getschedparam(thread_id, &policy, &param);
    param.sched_priority = sched_get_priority_max(policy);
    if (pthread_setschedparam(thread_id, policy, &param) != 0) {
        perror("Failed to set thread priority to max");
        assert(0);
    }
    // LOG("remap thread priority set to maximum: %d\n", param.sched_priority);

    pthread_setname_np(pthread_self(), "remap-thr");
}

void* remap_thread_func(void* opaque) {
    remap_thread_ctx_t* thr_ctx = (remap_thread_ctx_t*)opaque;

    char remap_buf[MAX_SIZE];

    remap_thread_init();

    internal_call = true;

    while (1) {
        LOG_DEBUG("remap reading from file %d\n", thr_ctx->remap_fd);
        int len = read(thr_ctx->remap_fd, remap_buf, MAX_SIZE);
        if (len < 0) {
            LOG_ERR("remap_fd read failed\n");
            perror("remap_fd read erred");
            break;
        }
        if (len == 0) {
            LOG_WARN("remap_fd closed\n");
            perror("remap_fd closed before application exit");
            break;
        }
        handle_remap_bufread(thr_ctx, remap_buf, (size_t)len);
    }

    free(thr_ctx);

    LOG("remap thread done\n");

    internal_call = false;
    return NULL;
}

int remap_app_init(remap_ctx_t* ctx, int host_pid, long uffd,
                   physmem_ctx_t* mem_ctx) {
    remap_init_state_t expected_state = NOT_INIT;
    if (!atomic_compare_exchange_strong(&ctx->init_state, &expected_state,
                                        INIT_IN_PROGRESS)) {
        LOG_WARN("remap_app init already in progress\n");
        return 0;
    }
    LOG("init in progress\n");

    remap_thread_ctx_t* args = calloc(1, sizeof(remap_thread_ctx_t));
    args->remap_fd = ctx->remap_fd;
    args->host_pid = host_pid;
    args->uffd = uffd;
    args->mem_ctx = mem_ctx;

    if (pthread_create(&ctx->remap_thread, NULL, remap_thread_func, args)) {
        perror("remap pthread create");
        assert(0);
    }

    assert(atomic_exchange(&ctx->init_state, DID_INIT) == INIT_IN_PROGRESS);
    LOG("init done\n");

    return 0;
}

int remap_app_destroy(remap_ctx_t* ctx) {
    LOG("running remap_app_destroy\n");
    remap_init_state_t curr_state = ctx->init_state;
    if (curr_state == NOT_INIT) {
        LOG_WARN("remap_app_destroy invoked while remap wasn't init\n");
        return 0;
    }

    LOG_WARN("curr state %d\n", curr_state);
    while (ctx->init_state != DID_INIT) {
    }
    // NOTE: no need to transition to a DEINIT_IN_PROGRESS state, as we only
    // expect one call to `remap_app_destroy`.

    LOG("closing file %d\n", ctx->remap_fd);
    close(ctx->remap_fd);
    LOG("joining remap thread\n");
    // TODO: figure out a better way to join the remap thread.
    // Closing the file descriptor doesn't cause it to unblock from read().
    // pthread_join(ctx->remap_thread, NULL);
    pthread_cancel(ctx->remap_thread);
    LOG("joined remap thread\n");

    ctx->init_state = NOT_INIT;

    return 0;
}
