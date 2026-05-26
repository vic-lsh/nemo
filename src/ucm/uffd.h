#ifndef HEMEM_UFFD_H
#define HEMEM_UFFD_H
#define _GNU_SOURCE

#include "type/process.h"

/**
 * Initialize logic for handling userfaultfd events asynchronously.
 */
int uffd_init();
void add_process_uffd(struct hemem_process* process, long uffd);

#endif /* HEMEM_UFFD_H */
