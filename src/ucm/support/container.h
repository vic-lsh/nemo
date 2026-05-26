#ifndef UCM_SUPPORT_CONTAINER_H
#define UCM_SUPPORT_CONTAINER_H

#include <stddef.h>

/**
 * Given a pid in the container, as well as its pid namespace id,
 * return the pid of the same process in the host pid namespace.
 */
int map_nspid_to_pid(long namespace_id, int target_container_pid);

#endif /* UCM_SUPPORT_CONTAINER_H */
