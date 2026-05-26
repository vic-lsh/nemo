#ifndef REMAP_H

#define REMAP_H

#define _GNU_SOURCE

#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>

#include "physmem.h"

typedef enum {
    NOT_INIT = 0,
    INIT_IN_PROGRESS = 1,
    DID_INIT = 2,
} remap_init_state_t;

typedef struct {
    pthread_t remap_thread;
    /**
     * File descriptor to receive remap requests from the UCM.
     */
    int remap_fd;
    _Atomic remap_init_state_t init_state;
} remap_ctx_t;

int remap_app_init(remap_ctx_t* ctx, int host_pid, long uffd,
                   physmem_ctx_t* mem_ctx);
int remap_app_destroy(remap_ctx_t* ctx);

#endif /* REMAP_H */
