#ifndef APP_MMAP_H
#define APP_MMAP_H

#include "hemem-app.h"

void* hemem_mmap_impl(app_ctx_t* ctx, void* addr, size_t length, int prot,
                      int flags, int fd, off_t offset);
int hemem_munmap_impl(app_ctx_t* ctx, void* addr, size_t length);
#endif /* APP_MMAP_H */
