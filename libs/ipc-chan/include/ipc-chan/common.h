#pragma once

#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/un.h>
#include <unistd.h>

#include "util/bug.h"
#include "util/compiler.h"
#include "util/log.h"

#define CHAN_SOCKET_PATH "/tmp/nemo/ucm_server.sock"

#define MAX_SIZE 262128

enum channel_type { REQUEST = 0, REMAP = 1 };

struct addr_ps {
    unsigned long addr;
    int page_size;
};

#define errExit(msg)        \
    do {                    \
        perror(msg);        \
        exit(EXIT_FAILURE); \
    } while (0)

/*
 * Because of kernel doing kmalloc for user data passed
 * in SCM messages, and there is kernel's SCM_MAX_FD as a limit
 * for descriptors passed at once we're trying to reduce
 * the pressue on kernel memory manager and use predefined
 * known to work well size of the message buffer.
 */
#define CR_SCM_MSG_SIZE (1024)
#define CR_SCM_MAX_FD (252)

struct scm_fdset {
    struct msghdr hdr;
    struct iovec iov;
    char msg_buf[CR_SCM_MSG_SIZE];
};

#ifndef F_GETOWNER_UIDS
#define F_GETOWNER_UIDS 17
#endif

#define __sys(foo) foo
#define __sys_err(ret) ret

int send_fds(int sock, struct sockaddr_un *saddr, int len, int *fds, int nr_fds,
             void *data, unsigned ch_size);
int __recv_fds(int sock, int *fds, int nr_fds, void *data, unsigned ch_size,
               int flags);

static inline int recv_fds(int sock, int *fds, int nr_fds, void *data,
                           unsigned ch_size) {
    return __recv_fds(sock, fds, nr_fds, data, ch_size, 0);
}

static inline int send_fd(int sock, struct sockaddr_un *saddr, int saddr_len,
                          int fd) {
    return send_fds(sock, saddr, saddr_len, &fd, 1, NULL, 0);
}

static inline int recv_fd(int sock) {
    int fd, ret;

    ret = recv_fds(sock, &fd, 1, NULL, 0);
    if (ret) return -1;

    return fd;
}

static inline int write_msg(int fd, char *msg, size_t msg_size) {
    ssize_t nbytes, remaining;
    char *msg_ptr = msg;

    remaining = msg_size;
    while (remaining) {
        nbytes = send(fd, msg_ptr, remaining, MSG_NOSIGNAL);
        if (nbytes == -1) {
            if (errno == EPIPE) {
                LOG_WARN("write_msg failed b/c the application terminated\n");
                return -1;
            } else {
                perror("write_msg");
                LOG_ERR("write_msg failed\n");
                assert(0);
            }
        }
        remaining -= nbytes;
        msg_ptr += nbytes;
    }

    return 0;
}

static inline int read_msg(int fd, char *msg, size_t msg_size) {
    size_t nbytes, remaining;
    char *msg_ptr = msg;

    remaining = msg_size;
    while ((nbytes = read(fd, msg_ptr, remaining)) < remaining) {
        if (nbytes <= 0) {
            return -1;
        }

        msg_ptr += nbytes;
        remaining -= nbytes;
    }

    return 0;
}
