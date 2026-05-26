#ifndef HEMEM_MEMOPS_H
#define HEMEM_MEMOPS_H

#include <stddef.h>

void memops_init();
void memops_shutdown();
void pprint_memops_opts();

void hemem_memcpy(void* dst, const void* src, size_t len);
void hemem_memset(void* dst, int value, size_t len);

#endif /* End of HEMEM_MEMOPS_H */
