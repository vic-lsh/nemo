#ifndef UTIL_PROC_H
#define UTIL_PROC_H

/**
 * Get the pid namespace that the process belongs to.
 */
long get_pid_namespace();

/**
 * Get the cgroup(s) that the process belongs to.
 */
int get_cgroup();

#endif /* UTIL_PROC_H */
