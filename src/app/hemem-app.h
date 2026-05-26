#ifndef HEMEM_APP_H

#define HEMEM_APP_H

#include <assert.h>
#include <inttypes.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "hemem-shared.h"
#include "interpose.h"
#include "physmem.h"
#include "remap.h"
#include "util/log.h"

#define MAX_UFFD_MSGS (1)
#define MAX_MEM_LEN_PER_REQ (512 * HUGEPAGE_SIZE)
// #define ONE_MEM_REQUEST

extern __thread bool internal_call;
extern double target_miss_ratio;

typedef struct {
    pthread_mutex_t init_mu;
    _Atomic bool is_init;
    /**
     * File descriptor to do IPC request/response with the UCM.
     */
    int ipc_fd;
    /**
     * File descriptor to handle userfault. Opened on the app side,
     * read by the UCM.
     */
    long uffd;
    /**
     * Cached PID returned by getpid().
     */
    pid_t self_pid;
    /**
     * PID of this process in the host PID namespace.
     * This can be different from the value received from getpid() for processes
     * in a different PID namespace (e.g., containers).
     *
     * Given by the UCM during initial registration.
     */
    pid_t host_pid;

    remap_ctx_t remap_ctx;
    physmem_ctx_t mem_ctx;
} app_ctx_t;

app_ctx_t* get_app_ctx();

/**
 * Re-initialize the app's connection with ucm.
 * This is typically done after the app has forked.
 */
int app_ctx_reinit(app_ctx_t* ctx);

void hemem_app_init();
void hemem_app_stop();
void* hemem_mmap(void* addr, size_t length, int prot, int flags, int fd,
                 off_t offset);
int hemem_munmap(void* addr, size_t length);

int record_remap_channel();

#endif /* HEMEM_APP_H */
