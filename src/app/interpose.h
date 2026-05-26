#include <stdlib.h>
#include <unistd.h>

#include "util/shared.h"

#ifdef QEMU
// with QEMU, intercepting large memory mmap is fine b/c they allocate for VM
// physical memory (which is large).
#define MMAP_INTERPOSE_SZ_THRESH GB(1)
#else
#define MMAP_INTERPOSE_SZ_THRESH KB(4)
#endif

// function pointers to libc functions
extern void* (*libc_mmap)(void* addr, size_t length, int prot, int flags,
                          int fd, off_t offset);
extern int (*libc_munmap)(void* addr, size_t length);
extern void* (*libc_malloc)(size_t size);
extern void (*libc_free)(void* p);
extern void* (*libc_sbrk)(intptr_t increment);
