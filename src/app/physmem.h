#ifndef HEMEM_APP_PHYSMEM_H
#define HEMEM_APP_PHYSMEM_H

#define _GNU_SOURCE

#include <stdbool.h>

#include "ipc-shared.h"

typedef struct {
    char fastmem_path[MAX_MEM_PATH_LEN];
    char slowmem_path[MAX_MEM_PATH_LEN];

    int fastmem_fd;
    int slowmem_fd;
} physmem_ctx_t;

int physmem_ctx_init(physmem_ctx_t* ctx);
int physmem_ctx_destroy(physmem_ctx_t* ctx);
bool is_our_mem_fd(physmem_ctx_t* ctx, int fd);

#endif /* HEMEM_APP_PHYSMEM_H */
