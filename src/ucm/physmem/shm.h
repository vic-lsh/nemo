#ifndef UCM_PHYSMEM_SHM_H
#define UCM_PHYSMEM_SHM_H

#include <stddef.h>

/**
 * Create shared memory region under the provided path, maps it, and returns
 * the mapped location.
 */
void* shm_create_mmap(char* shm_path, size_t shm_size);

/**
 * Close the mmaped region opened by `shm_create_mmap`.
 */
void shm_close_mmap(void* shm_region, size_t shm_size);

#endif /* UCM_PHYSMEM_SHM_H */
