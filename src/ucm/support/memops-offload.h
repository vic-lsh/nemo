#ifndef HEMEM_MEMOPS_OFFLOAD_H
#define HEMEM_MEMOPS_OFFLOAD_H

#include <stddef.h>

void memops_offload_init();
void memops_offload_shutdown();
void memops_offload_memcpy(void* dst, const void* src, size_t len);
void memops_offload_memset(void* dst, int value, size_t len);

#endif /* end of HEMEM_MEMOPS_OFFLOAD_H */
