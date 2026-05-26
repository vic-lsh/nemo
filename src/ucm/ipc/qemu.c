#include "qemu.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdint.h>
#include <sys/epoll.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#include "ds/uthash.h"
#include "epoll-util.h"
#include "telem/source/qemu-pml.h"
#include "ucm-config.h"
#include "util/log.h"
#include "util/thread.h"

#define SOCKET_PATH "/tmp/nemo_pml"
#define SHM_SIZE (1024 * 1024)  // 1MB

// state associated with each QEMU client fd
struct qemu_client_info {
    // key
    int client_fd;

    // values
    pid_t pid;
    void *shm_region;

    UT_hash_handle hh;
};

struct qemu_ipc_ctx {
    // ipc thread handles
    pthread_t pml_thread;
    pthread_t qemu_new_conn_thread;

    // fd for accepting connections from QEMU.
    int qemu_server_fd;
    // epoll fd for receiving PML updates from various QEMU clients.
    int pml_update_epoll;

    // Hashmap root. Maps QEMU client fds to `qemu_client_info`.
    struct qemu_client_info *client_map;
};

static struct qemu_ipc_ctx _static_ctx;

// payload sent from QEMU. ensure consistency between this and the QEMU-side
// sending logic.
struct qemu_pml_payload {
    uint64_t nbytes;
    uint64_t hva;
};

// payload during setup sent from QEMU.
struct qemu_setup_payload {
    pid_t pid;
};

static int accept_qemu_client(int server_fd, pid_t *client_pid) {
    int client_fd;
    struct sockaddr_un client_addr;
    socklen_t client_addr_len;
    client_addr_len = sizeof(struct sockaddr_un);
    if ((client_fd = accept(server_fd, (struct sockaddr *)&client_addr,
                            &client_addr_len)) == -1) {
        perror("accept error");
        LOG_ERR("error in accepting connection from QEMU\n");
        return -1;
    }

    struct qemu_setup_payload payload;
    int bytes_received = recv(client_fd, &payload, sizeof(payload), 0);
    if (bytes_received <= 0) {
        LOG_ERR("client handshake failed\n");
    }

    assert(client_pid);
    *client_pid = payload.pid;
    LOG("Client from pid %u connected.\n", *client_pid);

    return client_fd;
}

static void *setup_shm_region(int client_fd, pid_t qemu_pid) {
    char shm_name[256];
    int ret =
        snprintf(shm_name, sizeof(shm_name), "/nemo-shm-%d", (int)qemu_pid);
    if (ret < 0) {
        perror("snprintf failed");
        assert(0);
    } else if (ret >= (int)sizeof(shm_name)) {
        LOG_ERR("snprintf truncation detected: buffer size insufficient\n");
        assert(0);
    }

    int shm_fd = shm_open(shm_name, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("shm_open error");
        assert(0);
    }

    if (ftruncate(shm_fd, SHM_SIZE) == -1) {
        perror("ftruncate error");
        assert(0);
    }

    void *shm_ptr =
        mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shm_ptr == MAP_FAILED) {
        perror("mmap error");
        assert(0);
    }
    close(shm_fd);

    LOG("Server created and mapped shared memory region: %s\n", shm_name);

    // tell client about shm location
    if (send(client_fd, shm_name, strlen(shm_name) + 1, 0) == -1) {
        perror("send SHM_NAME error");
        assert(0);
    }
    LOG_DEBUG("Server sent SHM name '%s' to client.\n", shm_name);

    return shm_ptr;
}

static void handle_qemu_client_msg(struct qemu_ipc_ctx *ctx, int client_fd) {
    struct qemu_pml_payload payload;

    struct qemu_client_info *cl = 0;

    HASH_FIND(hh, ctx->client_map, &client_fd, sizeof(client_fd), cl);
    if (!cl) {
        LOG_ERR("client_fd %d not found\n", client_fd);
        assert(cl);
    }

    int bytes_received = recv(client_fd, &payload, sizeof(payload), 0);
    if (bytes_received > 0) {
        LOG("bitmap size: %lu bytes hva %p\n", payload.nbytes,
            (void *)payload.hva);
        process_pml_bitmap(cl->shm_region, payload.nbytes, cl->pid,
                           payload.hva);
    } else if (bytes_received == 0) {
        LOG("Client disconnected (socket).\n");
        return;
    } else {
        perror("Receive failed");
        return;
    }
}

static void handle_dropped_qemu_connection(struct qemu_ipc_ctx *ctx,
                                           int closed_fd) {
    delete_epoll_ctl(ctx->pml_update_epoll, closed_fd);
    struct qemu_client_info *cl = 0;
    HASH_FIND(hh, ctx->client_map, &closed_fd, sizeof(closed_fd), cl);
    assert(cl);
    HASH_DELETE(hh, ctx->client_map, cl);
    LOG_ERR("QEMU client from pid %d error/hangup detected\n", (int)cl->pid);
    free(cl);
}

static void *handle_pml_updates(void *opaque) {
    // TODO: new CPU
    thread_pin_self(LISTEN_THREAD_CPU);
    pthread_setname_np(pthread_self(), "handle-pml-thr");

    struct qemu_ipc_ctx *ctx = (struct qemu_ipc_ctx *)opaque;

#define MAX_EVENTS 64
    struct epoll_event events[MAX_EVENTS];

    while (1) {
        int nfds = epoll_wait(ctx->pml_update_epoll, events, MAX_EVENTS, -1);
        if (nfds == -1) {
            if (errno == EINTR) continue;
            perror("epoll_wait failed");
            break;
        }

        for (int i = 0; i < nfds; i++) {
            int ready_fd = events[i].data.fd;

            if (events[i].events & (EPOLLERR | EPOLLHUP)) {
                handle_dropped_qemu_connection(ctx, ready_fd);
            } else if (events[i].events & EPOLLIN) {
                handle_qemu_client_msg(ctx, ready_fd);
            } else {
                assert(0);
            }
        }
    }

    return 0;
}

/**
 * Handles the setup of one QEMU client.
 *
 * The high-level flow is as follows:
 *
 * - Block until a new connection arrives.
 *
 * - QEMU sends a payload of the format `struct qemu_setup_payload`.
 *     Right now, this only includes the pid of the QEMU process.
 *
 * - We respond to QEMU to open a shared memory region
 *     Right now, the reply contrains the name of the shared memory region.
 *     See `setup_shm_region`.
 *
 * - Setup done.
 *
 * - Periodically, the QEMU client sends PML update messages in the form of
 *   `struct qemu_pml_payload`.
 *   The update payload simply notifies the UCM to check updates. The updates
 *   are written to the shared memory region that's set up for QEMU.
 */
static void accept_and_setup_qemu_client(struct qemu_ipc_ctx *ctx) {
    struct qemu_client_info *cl = malloc(sizeof(struct qemu_client_info));

    cl->client_fd = accept_qemu_client(ctx->qemu_server_fd, &cl->pid);
    cl->shm_region = setup_shm_region(cl->client_fd, cl->pid);

    HASH_ADD(hh, ctx->client_map, client_fd, sizeof(cl->client_fd), cl);
    add_epoll_ctl(ctx->pml_update_epoll, cl->client_fd);
}

static void *handle_new_qemu_connection(void *opaque) {
    // TODO: new CPU
    thread_pin_self(LISTEN_THREAD_CPU);
    pthread_setname_np(pthread_self(), "new-qemu-thr");

    struct qemu_ipc_ctx *ctx = (struct qemu_ipc_ctx *)opaque;

    while (1) {
        accept_and_setup_qemu_client(ctx);
    }

    return 0;
}

static int init_server_for_qemu() {
    int server_fd;
    struct sockaddr_un server_addr;

    if ((server_fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        perror("socket error");
        return -1;
    }

    memset(&server_addr, 0, sizeof(struct sockaddr_un));
    server_addr.sun_family = AF_UNIX;
    strncpy(server_addr.sun_path, SOCKET_PATH,
            sizeof(server_addr.sun_path) - 1);

    unlink(SOCKET_PATH);

    if (bind(server_fd, (struct sockaddr *)&server_addr,
             sizeof(struct sockaddr_un)) == -1) {
        perror("bind error");
        close(server_fd);
        return -1;
    }

    if (listen(server_fd, 5) == -1) {
        perror("listen error");
        close(server_fd);
        return -1;
    }
    LOG("Server listening on %s\n", SOCKET_PATH);
    return server_fd;
}

// epoll used to wait for bitmap updates from multiple QEMUs
static int init_epoll() {
    int pml_update_epoll = epoll_create1(0);
    if (pml_update_epoll == -1) {
        perror("epoll_create1");
        assert(0);
    }
    return pml_update_epoll;
}

int ipc_qemu_init() {
    int ret;
    memset(&_static_ctx, 0, sizeof(_static_ctx));

    int server_fd = init_server_for_qemu();
    if (server_fd < 0) {
        return -1;
    }
    _static_ctx.qemu_server_fd = server_fd;

    _static_ctx.pml_update_epoll = init_epoll();
    assert(_static_ctx.pml_update_epoll);

    ret = pthread_create(&_static_ctx.qemu_new_conn_thread, NULL,
                         handle_new_qemu_connection, &_static_ctx);
    if (ret != 0) {
        perror("pthread_create handle_new_qemu_connection");
        return -1;
    }
    ret = pthread_create(&_static_ctx.pml_thread, NULL, handle_pml_updates,
                         &_static_ctx);
    if (ret != 0) {
        perror("pthread_create handle_pml_thread");
        return -1;
    }

    return 0;
}
