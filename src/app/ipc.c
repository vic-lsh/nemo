#include "ipc.h"

#include <pthread.h>
#include <stdio.h>
#include <sys/stat.h>

#include "hemem-app.h"
#include "hemem-shared.h"
#include "ipc-chan/client.h"
#include "ipc-shared.h"
#include "util/proc.h"

pthread_mutex_t channel_lock = PTHREAD_MUTEX_INITIALIZER;

static __thread char thr_resp_buf[MAX_SIZE];

static inline int chan_type_to_fd(app_ctx_t* ctx, enum channel_type ch) {
    switch (ch) {
        case REQUEST:
            return ctx->ipc_fd;
        case REMAP:
            return ctx->remap_ctx.remap_fd;
        default:
            LOG_ERR("found unrecognized channel type %d\n", ch);
            assert(0);
    }
}

static enum status_code request_roundtrip(app_ctx_t* ctx, enum channel_type ch,
                                          void* request, void* response) {
    ssize_t len;
    int ret;

    int fd = chan_type_to_fd(ctx, ch);

    struct msg_header* request_header = (struct msg_header*)request;
    size_t request_size = request_header->msg_size;

    if (ch == REQUEST) {
        pthread_mutex_lock(&channel_lock);
    }

    if (request_header->operation == RECORD_UFFD) {
        ret =
            send_fd(fd, NULL, 0, ((struct record_uffd_request*)request)->uffd);
    } else {
        ret = write_msg(fd, request, request_size);
    }
    if (ret != 0) {
        perror("request send failed");
        assert(0);
    }

    len = read(fd, response, MAX_SIZE);
    if (len < 0) {
        perror("header read failed");
        assert(0);
    }
    if ((size_t)len < sizeof(struct msg_header)) {
        assert(!"invalid header");
    }

    struct msg_header* hdr = response;
    if ((size_t)len != hdr->msg_size) {
        LOG_ERR("invalid response: header msg size = %lu, read %lu, \n",
                hdr->msg_size, len);
        assert(0);
    }

    if (ch == REQUEST) {
        pthread_mutex_unlock(&channel_lock);
    }

    struct msg_header* resp = response;
    return resp->status;
}

static bool app_did_fork(app_ctx_t* ctx) { return ctx->self_pid != getpid(); }

static enum channel_type map_op_type_to_chan(enum operation op) {
    switch (op) {
        case RECORD_REMAP_FD:
            return REMAP;
        default:
            return REQUEST;
    }
}

static enum status_code do_request_internal(app_ctx_t* ctx, enum operation op,
                                            void* request, size_t req_len,
                                            void* response, bool check_fork) {
    // We use `check_fork` to avoid recursively checking fork. We turn off fork
    // checking for requests that is done in the context of re-registering the
    // application after fork.
    if (check_fork && app_did_fork(ctx)) {
        LOG_WARN("app did fork!\n");
        int rc = app_ctx_reinit(ctx);
        if (rc != 0) {
            LOG_ERR("app failed to reinit after fork\n");
            exit(1);
        }
        LOG_WARN("app-ctx reinit after fork\n");
    }

    // every request begins with a header
    struct msg_header* hdr = (struct msg_header*)request;

    hdr->operation = op;
    hdr->pid = ctx->host_pid;
    hdr->msg_size = req_len;

    LOG_DEBUG("do_req_internal op type %d msg size %lu\n", hdr->operation,
              hdr->msg_size);

    enum channel_type ch = map_op_type_to_chan(op);

    return request_roundtrip(ctx, ch, request, response);
}

#define do_request(ctx, op, request_ptr, response_ptr)              \
    do_request_internal(ctx, op, request_ptr, sizeof(*request_ptr), \
                        response_ptr, /*check_fork=*/true)

#define do_request_no_fork_check(ctx, op, request_ptr, response_ptr) \
    do_request_internal(ctx, op, request_ptr, sizeof(*request_ptr),  \
                        response_ptr, /*check_fork=*/false)

// ================== Request operation-specific Logic =======================

struct alloc_response* ipc_alloc_space(app_ctx_t* ctx, void* addr,
                                       size_t length, bool map_fixed) {
    struct alloc_request request;

    request.addr = addr;
    request.length = length;
    request.map_fixed = map_fixed;

    do_request(ctx, ALLOC_SPACE, &request, &thr_resp_buf);

    struct alloc_response* response = (void*)&(thr_resp_buf);
    return response;
}

enum status_code ipc_free_space(app_ctx_t* ctx, void* addr, size_t length) {
    struct free_request request;
    request.addr = addr;
    request.length = length;

    return do_request(ctx, FREE_SPACE, &request, &thr_resp_buf);
}

static int ipc_set_process_remap_fd(app_ctx_t* ctx) {
    struct record_remap_fd_request request;

    assert(ctx->host_pid);
    assert(ctx->remap_ctx.remap_fd);

    return do_request_no_fork_check(ctx, RECORD_REMAP_FD, &request,
                                    &thr_resp_buf);
}

enum status_code ipc_add_process(app_ctx_t* ctx) {
    enum status_code status;

    struct add_process_request add_app_req;
    add_app_req.pid_namespace = get_pid_namespace();
    LOG("app pid namespace %ld\n", add_app_req.pid_namespace);
    add_app_req.pid = ctx->self_pid;
    add_app_req.target_miss_ratio = target_miss_ratio;
    // TODO: revisit how this is set
    add_app_req.req_dram = 0;
#ifdef LLAMA
    add_app_req.zero = true;
#else
    add_app_req.zero = false;
#endif

    status =
        do_request_no_fork_check(ctx, ADD_PROCESS, &add_app_req, &thr_resp_buf);
    if (status != SUCCESS) {
        return status;
    }

    struct add_process_response* resp = ((void*)&thr_resp_buf);
    ctx->host_pid = resp->host_pid;

    LOG_DEBUG("ucm assigned fastmem path %s\n", resp->fastmem_path);
    LOG_DEBUG("ucm assigned slowmem path %s\n", resp->slowmem_path);

    strncpy(ctx->mem_ctx.fastmem_path, resp->fastmem_path,
            MAX_MEM_PATH_LEN - 1);
    ctx->mem_ctx.fastmem_path[MAX_MEM_PATH_LEN - 1] = '\0';

    strncpy(ctx->mem_ctx.slowmem_path, resp->slowmem_path,
            MAX_MEM_PATH_LEN - 1);
    ctx->mem_ctx.slowmem_path[MAX_MEM_PATH_LEN - 1] = '\0';

    struct record_uffd_request record_uffd_req;
    record_uffd_req.uffd = ctx->uffd;
    status = do_request_no_fork_check(ctx, RECORD_UFFD, &record_uffd_req,
                                      &thr_resp_buf);
    if (status != SUCCESS) {
        return status;
    }

    status = ipc_set_process_remap_fd(ctx);
    if (status != SUCCESS) {
        LOG_ERR("process registration failed at remap fd\n");
        assert(0);
    }

    return SUCCESS;
}

enum status_code ipc_remove_process(app_ctx_t* ctx) {
    struct remove_process_request request;
    // Nothing to do here; the request header embeds the application PID, which
    // is all that's needed to do request removal.
    return do_request(ctx, REMOVE_PROCESS, &request, &thr_resp_buf);
}
