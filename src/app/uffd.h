#ifndef HEMEM_APP_UFFD_H
#define HEMEM_APP_UFFD_H

#include <stddef.h>
#include <stdint.h>

long uffd_open();
int uffd_register_page(long uffd, uint64_t page_boundary, size_t page_size);
int uffd_unregister_page(long uffd, uint64_t page_boundary, size_t page_size);

#endif /* HEMEM_APP_UFFD_H */
