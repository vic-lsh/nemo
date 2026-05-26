#ifndef UCM_PHYSMEM_DAX_H
#define UCM_PHYSMEM_DAX_H

#include <sys/mman.h>

/**
 * Open DAX file as a MMAP region. Region size is specified via `dax_size`.
 *
 * @return
 * The mmaped region if successful.
 * The DAX file descriptor via the outptr `dax_fd`, if provided.
 */
void* devdax_open_mmap(char* dax_path, size_t dax_size, int* dax_fd);

/**
 * Close the mmaped region opened by `devdax_open_mmap`.
 */
void devdax_close_mmap(void* dax_mmap_region, size_t dax_size, int* dax_fd);

#endif /* UCM_PHYSMEM_DAX_H */
