#ifndef HEMEM_MEMCPY_PAR_H
#define HEMEM_MEMCPY_PAR_H

#include <stddef.h>

void memcpy_par_init();
void memcpy_par_shutdown();
void memcpy_par(void* dst, const void* src, size_t len);

#endif /* end of HEMEM_MEMCPY_HAR_H */
