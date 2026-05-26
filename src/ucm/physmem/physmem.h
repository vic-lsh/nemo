#ifndef HEMEM_UCM_PHYSMEM_H
#define HEMEM_UCM_PHYSMEM_H

#include "mm.h"

/**
 * Low-level module for reserving physical memory that the UCM manages.
 */

void physmem_init(mm_opts_t* opts);

physmem_mode_t get_physmem_mode();

size_t physmem_get_fastmem_size();

size_t physmem_get_slowmem_size();

void* get_slowmem_mmap_addr();

/**
 * Return the file path where the app-side lib can open the fast memory region.
 */
char* get_fastmem_file_path();

/**
 * Return the file path where the app-side lib can open the fast memory region.
 */
char* get_slowmem_file_path();

#endif /* HEMEM_UCM_PHYSMEM_H */
