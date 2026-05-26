#include "ipc.h"

#include <fcntl.h>
#include <pthread.h>
#include <sys/epoll.h>
#include <sys/mman.h>

#include "core.h"
#include "epoll-util.h"
#include "hemem-shared.h"
#include "ipc-chan/server.h"
#include "ipc-shared.h"
#include "mm.h"
#include "physmem/config.h"
#include "physmem/physmem.h"
#include "policy/policy.h"
#include "proc-mgr.h"
#include "support/container.h"
#include "type/process.h"
#include "ucm-config.h"
#include "ucm.h"
#include "uffd.h"
#include "util/log.h"
#include "util/thread.h"

#ifdef QEMU
#include "ipc/qemu.h"
#include "telem/source/qemu-pml.h"
#endif

int listen_fd = -1;
int request_epoll_fd = -1;

pthread_t request_thread;
pthread_t listen_thread;

static enum ucm_ipc_result do_app_request(int fd, void *request,
                                          void *response) {
    ssize_t len;
    int ret;
    size_t request_size = ((struct msg_header *)request)->msg_size;

    ret = write_msg(fd, request, request_size);
    if (ret != 0) {
        perror("request send fails");
        return IPC_ERRED;
    }

    len = read(fd, response, MAX_SIZE);
    if (len <= 0) {
        if (len == 0 || errno == ECONNRESET) {
            LOG_WARN("application exited\n");
            return IPC_APP_EXITED;
        }

        perror("error reading header");
        return IPC_ERRED;
    }

    size_t expected = sizeof(struct msg_header);
    if ((size_t)len < expected) {
        LOG_ERR("invalid header received; expected %lu bytes, got %lu\n",
                expected, len);
        return IPC_INVALID_MSG;
    }

    return IPC_SUCCESS;
}

static struct hemem_process *ipc_handle_add_process(
    struct add_process_request *request,
    struct add_process_response *response) {
    pid_t pid = map_nspid_to_pid(request->pid_namespace, request->pid);
    if (pid < 0) {
        LOG_ERR(
            "could not find application-provided pid %d under namespace %ld\n",
            request->pid, request->pid_namespace);
        assert(0);
    }

    if (pid != request->pid) {
        LOG("new app container proc id %d host pid %d\n", request->pid, pid);
    }

    bool found = false;
    struct hemem_process *process = find_process(pid);
    if (unlikely(process)) {
        found = true;
        LOG_WARN("attempted to add process %d but it already exists\n", pid);
    } else {
        process = hemem_process_init(pid);
        assert(process);
    }

    // TODO: in the case where the process is found, do we need to lock before
    // updating these fields?
    // set request-specific process fields
#if defined(CONFIG_POLICY_QOS) || defined(CONFIG_POLICY_FAIR_SHARE)
    process_policy_set_miss_ratio(&process->policy, request->target_miss_ratio);
#endif
    process->zero = request->zero;
    // TODO: revisit how to set fastmem limit (or whether we should set this at
    // proc creation at all). allow all processes to use up fastmem for now.
    process->mm.max_fastmem = get_fastmem_size();

    if (!found) {
        add_process(process);
        LOG("ipc added process %d\n", pid);
    }

    response->header.status = SUCCESS;
    response->header.pid = pid;
    response->header.operation = ADD_PROCESS;
    response->header.msg_size = sizeof(struct add_process_response);
    response->host_pid = pid;

    snprintf(response->fastmem_path, MAX_MEM_PATH_LEN, "%s",
             get_fastmem_file_path());
    snprintf(response->slowmem_path, MAX_MEM_PATH_LEN, "%s",
             get_slowmem_file_path());

    return process;
}

static int ipc_handle_record_uffd(int fd, struct hemem_process *process,
                                  struct record_uffd_response *response) {
    process->uffd = recv_fd(fd);
    process->valid_uffd = true;

    add_process_uffd(process, process->uffd);

    response->header.status = SUCCESS;
    response->header.pid = process->pid;
    response->header.operation = RECORD_UFFD;
    response->header.msg_size = sizeof(struct record_uffd_response);
    return 0;
}

static int ipc_add_process_handshake(int fd, struct add_process_request *req,
                                     char *resp_buf) {
    int ret;

    struct add_process_response *resp = (struct add_process_response *)resp_buf;

    struct hemem_process *process = ipc_handle_add_process(req, resp);

    ret = write_msg(fd, (char *)resp, resp->header.msg_size);
    if (ret != 0) {
        LOG_ERR("failed to respond to add process request\n");
        assert(0);
    }

    memset(resp_buf, 0, MAX_SIZE);
    ret = ipc_handle_record_uffd(fd, process,
                                 (struct record_uffd_response *)resp_buf);

    if (ret != 0) {
        LOG_ERR("failed to receive UFFD from process %d\n", process->pid);
        assert(0);
    }

    return 0;
}

static int ipc_handle_remove_process(struct remove_process_request *request,
                                     struct remove_process_response *response) {
    struct hemem_process *process;
    pid_t pid = request->header.pid;

    process = find_process(request->header.pid);
    if (process != NULL) {
        process->exited = true;
        LOG("ipc removed process %d\n", pid);
        ucm_remove_process(process);
    }

    response->header.status = SUCCESS;
    response->header.pid = pid;
    response->header.operation = REMOVE_PROCESS;
    response->header.msg_size = sizeof(struct remove_process_response);

    return 0;
}

static int ipc_handle_alloc_space(struct alloc_request *request,
                                  struct alloc_response *response) {
    pid_t pid = request->header.pid;

    response->header.pid = pid;
    response->header.operation = ALLOC_SPACE;

    struct hemem_process *process = find_process(pid);
    if (process == NULL) {
        LOG_ERR("alloc space req failed: couldn't find process %d\n", pid);
        response->header.status = FAILED;
        response->num_pages = 0;
        response->header.msg_size = sizeof(struct alloc_response);
        // response->pages = NULL;
        return -1;
    }

    ssize_t num_pages = 0;
    if (request->map_fixed) {
        num_pages = ucm_map_fixed(process, (uint64_t)request->addr,
                                  request->length, response->pages);
    } else {
        num_pages = ucm_allocate_memory(process, (uint64_t)request->addr,
                                        request->length, response->pages);
    }

    bool failed = num_pages < 0;

    response->num_pages = failed ? 0 : num_pages;
    response->header.msg_size =
        sizeof(struct alloc_response) +
        response->num_pages * sizeof(struct hemem_page_app);
    response->header.status = failed ? FAILED : SUCCESS;

    return 0;
}

static int ipc_handle_free_space(struct free_request *request,
                                 struct free_response *response) {
    pid_t pid = request->header.pid;
    struct hemem_process *process;

    response->header.pid = pid;
    response->header.operation = FREE_SPACE;
    response->header.msg_size = sizeof(struct free_response);

    process = find_process(request->header.pid);
    if (process == NULL) {
        LOG_ERR("free space req failed: couldn't find process %d\n", pid);
        response->header.status = FAILED;
        return -1;
    }

    ucm_deallocate_memory(process, (uint64_t)request->addr, request->length);

    response->header.status = SUCCESS;
    return 0;
}

static int ipc_handle_get_process_remap_fd(
    int fd, struct record_remap_fd_request *request,
    struct record_remap_fd_response *response) {
    pid_t pid = request->header.pid;
    struct hemem_process *process;

    response->header.pid = pid;
    response->header.operation = RECORD_REMAP_FD;
    response->header.msg_size = sizeof(struct record_remap_fd_response);

    process = find_process(request->header.pid);
    if (process == NULL) {
        LOG_ERR("get_process_remap_fd failed: couldn't find process %d\n", pid);
        response->header.status = FAILED;
        return -1;
    }

    process->remap_fd = fd;
    response->header.status = SUCCESS;
    return 0;
}

enum ucm_ipc_result ipc_remap_pages(struct hemem_process *process,
                                    struct hemem_page_app *fault_pages,
                                    int num_fault_pages) {
    enum ucm_ipc_result ret;

    struct remap_request *request;
    struct remap_response *response;
    size_t msg_size = sizeof(struct remap_request) +
                      sizeof(struct hemem_page_app) * num_fault_pages;

    request = (struct remap_request *)malloc(msg_size);
    if (request == NULL) {
        LOG_ERR("failed to allocate for remap request\n");
        ret = IPC_ERRED;
        goto bad_req_alloc;
    }
    memset(request, 0, msg_size);

    response = (void *)calloc(1, MAX_SIZE);
    if (response == NULL) {
        LOG_ERR("failed to allocate for remap response\n");
        ret = IPC_ERRED;
        goto bad_resp_alloc;
    }

    request->header.pid = process->pid;
    request->header.operation = REMAP_PAGES;
    request->header.msg_size = msg_size;

    request->num_pages = num_fault_pages;
    memcpy(request->pages, fault_pages,
           sizeof(struct hemem_page_app) * num_fault_pages);

    pthread_mutex_lock(&process->remap_fd_lock);
    enum ucm_ipc_result rc =
        do_app_request(process->remap_fd, request, response);
    pthread_mutex_unlock(&process->remap_fd_lock);

    if (rc != IPC_SUCCESS) {
        ret = rc;
        goto erred;
    }

    if (response->va != fault_pages->va) {
        LOG_ERR("BUG: bad remap response: expect va %p got va %p\n",
                (void *)fault_pages->va, (void *)response->va);
        assert(0);
    }

    ret = (response->header.status == SUCCESS) ? IPC_SUCCESS : IPC_ERRED;

erred:
    free(response);
bad_resp_alloc:
    free(request);
bad_req_alloc:
    return ret;
}

static void ipc_handle_invalid_request() {
    fprintf(stderr, "invalid request\n");
    assert(0);
    // response_header->operation = request_header->operation;
    // response_header->pid = request_header->pid;
    // response_header->status = INVALID_REQUEST;
    // response_header->msg_size = sizeof(struct msg_header);
    // break;
}

int process_msg(int fd) {
    int ret;
    ssize_t len;
    char recv_buf[MAX_SIZE];
    char *send_buf;
    struct msg_header *request_header;
    struct msg_header *response_header;
    char *new_send_buf;
    struct alloc_request *alloc_req;

    len = read(fd, recv_buf, MAX_SIZE);
    if (len < 0) {
        perror("error reading response");
        assert(0);
    }
    if ((size_t)len < sizeof(struct msg_header)) {
        LOG_ERR("invalid request: recvd len %lu expected %lu\n", len,
                sizeof(struct msg_header));
        assert(0);
    }

    send_buf = malloc(MAX_SIZE);
    memset(send_buf, 0, MAX_SIZE);
    request_header = (struct msg_header *)recv_buf;

    LOG_DEBUG("processing request %d\n", request_header->operation);
    switch (request_header->operation) {
        case ALLOC_SPACE:
            alloc_req = (struct alloc_request *)recv_buf;
            len = sizeof(struct alloc_response) +
                  sizeof(struct hemem_page_app) *
                      (alloc_req->length / HUGEPAGE_SIZE +
                       (alloc_req->length % HUGEPAGE_SIZE != 0));
            if (len > MAX_SIZE) {
                new_send_buf = realloc(send_buf, len);
                send_buf = new_send_buf;
            }
            ret = ipc_handle_alloc_space((struct alloc_request *)recv_buf,
                                         (struct alloc_response *)send_buf);
            break;
        case FREE_SPACE:
            ret = ipc_handle_free_space((struct free_request *)recv_buf,
                                        (struct free_response *)send_buf);
            break;
        case ADD_PROCESS:
            ret = ipc_add_process_handshake(
                fd, (struct add_process_request *)recv_buf, send_buf);
            break;
        case REMOVE_PROCESS:
            ret = ipc_handle_remove_process(
                (struct remove_process_request *)recv_buf,
                (struct remove_process_response *)send_buf);
            break;
        case RECORD_REMAP_FD:
            ret = ipc_handle_get_process_remap_fd(
                fd, (struct record_remap_fd_request *)recv_buf,
                (struct record_remap_fd_response *)send_buf);
            delete_epoll_ctl(request_epoll_fd, fd);
            break;
        default:
            ipc_handle_invalid_request();
            return -1;
    }

    response_header = (struct msg_header *)send_buf;
    ret = write_msg(fd, send_buf, response_header->msg_size);
    LOG_DEBUG("responded to request %d\n", request_header->operation);

    free(send_buf);
    if (ret != 0) {
        return -1;
    }

    return 0;
}

void *handle_request_thread() {
    int num_ready_fds;
    struct epoll_event epoll_events[MAX_EPOLL_EVENTS];
    int ready_fd;
    int ret;

    thread_pin_self(REQUEST_THREAD_CPU);
    pthread_setname_np(pthread_self(), "ipc-server-thr");

    while (1) {
        num_ready_fds =
            epoll_wait(request_epoll_fd, epoll_events, MAX_EPOLL_EVENTS, -1);
        for (int i = 0; i < num_ready_fds; i++) {
            ready_fd = epoll_events[i].data.fd;

            if ((epoll_events[i].events & EPOLLERR /* fd erred */) ||
                (epoll_events[i].events & EPOLLHUP /* fd hung up */) ||
                (!(epoll_events[i].events & EPOLLIN /* avail for read */))) {
                goto del_ready_fd;
            }

            ret = process_msg(ready_fd);
            if (ret != 0) {
                goto del_ready_fd;
            }
            continue;

        del_ready_fd:
            ret = delete_epoll_ctl(request_epoll_fd, ready_fd);
            if (ret != 0) {
                perror("delete_epoll_ctl");
            }
            LOG("deleted epoll fd %d\n", ready_fd);
            close(ready_fd);
        }
    }
}

void *accept_new_app_thread() {
    int ret;
    int cli_fd;
    socklen_t cli_addr_len;
    struct sockaddr_un cli_addr;

    thread_pin_self(LISTEN_THREAD_CPU);
    pthread_setname_np(pthread_self(), "new-app-thr");

    while (1) {
        cli_addr_len = sizeof(struct sockaddr_un);
        cli_fd = accept(listen_fd, (struct sockaddr *)&cli_addr, &cli_addr_len);
#ifdef HEMEM_DEBUG
        printf("accept, cli_fd=%d\n", cli_fd);
#endif

        if (cli_fd == -1) {
            perror("accept");
            assert(0);
        }

        ret = add_epoll_ctl(request_epoll_fd, cli_fd);
        if (ret != 0) {
            perror("add_epoll_ctl");
            assert(0);
        }
    }

    return NULL;
}

int ipc_init_ucm() {
    int ret;

    signal(SIGPIPE, SIG_IGN);

    listen_fd = channel_server_init();
    if (listen_fd < 0) {
        perror("channel_server_init");
        assert(0);
    }

    request_epoll_fd = epoll_create1(0);
    if (request_epoll_fd == -1) {
        perror("epoll_create1");
        assert(0);
    }

    ret = pthread_create(&listen_thread, NULL, accept_new_app_thread, 0);
    if (ret != 0) {
        perror("pthread_create accept_new_app_thread");
        assert(0);
    }

    ret = pthread_create(&request_thread, NULL, handle_request_thread, 0);
    if (ret != 0) {
        perror("pthread_create handle_request_thread");
        return -1;
    }

#ifdef QEMU
    ret = ipc_qemu_init();
    if (ret != 0) {
        LOG_ERR("failed to initialize ipc server for qemu\n");
        assert(0);
    }
#endif

    return 0;
}
