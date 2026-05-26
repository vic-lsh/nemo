#ifndef HEMEM_SUPPORT_DSA_H
#define HEMEM_SUPPORT_DSA_H

#include <linux/idxd.h>
#include <stdbool.h>
#include <stddef.h>

int dsa_init();
void dsa_shutdown();
int dsa_memcpy(void *dst, const void *src, size_t len);
int dsa_memset(void *dst, int value, size_t len);

/**
 * Initiates a memcpy operation on the DSA, but does not wait for completion.
 * Uses the `dsa_completion_record` handle to indicate completion status.
 */
int dsa_memcpy_nb(void *dst, const void *src, size_t len,
                  struct dsa_completion_record *cr);

/**
 * Busy polls for DSA record completion.
 */
void dsa_poll_complete(struct dsa_completion_record *cr);

#endif /* End of HEMEM_SUPPORT_DSA_H */
