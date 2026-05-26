#include "container.h"

#include <assert.h>
#include <dirent.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "util/log.h"

#define MAX_PATH 256
#define MAX_LINE 512

typedef struct {
    int pid;
    int host_pid;
    int container_pid;
} pid_info_t;

int is_numeric(const char *str) {
    if (!str || *str == '\0') return 0;
    while (*str) {
        if (*str < '0' || *str > '9') return 0;
        str++;
    }
    return 1;
}

int is_pid_in_target_ns(int pid, char *target_ns) {
    char path[MAX_PATH];
    char link_target[MAX_PATH];
    ssize_t len;

    snprintf(path, sizeof(path), "/proc/%d/ns/pid", pid);

    len = readlink(path, link_target, sizeof(link_target) - 1);
    if (len == -1) {
        return 0;  // Can't read link
    }

    link_target[len] = '\0';
    return strcmp(link_target, target_ns) == 0;
}

// Parses the `NSpid` line in /proc/<pid>/status file.
//
// Based on the `NSpid` line, we deduce whether the process is running inside
// a container or not. Processes running in a docker container run in their
// pid namespace, giving them at least 2 pids: one in the host namespace, and
// another in the container pid namespace.
//
// We set the pid outptrs according to what we find in the `NSpid` line:
// - if we see 1 pid, then we deduce that this is a proc running in the host.
// - if we see 2 pids, then we deduce that this proc is in a container.
// - more than 2 pids are not supported right now.
bool parse_nspid_line(int host_pid, int target_pid, int *host_pid_o,
                      int *container_pid_o) {
    char path[MAX_PATH];
    FILE *fp;
    char line[MAX_LINE];
    char *token;
    int pid_count = 0;
    int pids[16];  // Should be enough for nested namespaces

    snprintf(path, sizeof(path), "/proc/%d/status", host_pid);

    fp = fopen(path, "r");
    if (!fp) {
        return false;
    }

    bool found = false;
    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "NSpid:", 6) == 0) {
            // Parse the NSpid line to extract all PIDs
            token = strtok(line + 6, " \t\n");  // Skip "NSpid:" prefix
            while (token != NULL && pid_count < 16) {
                if (is_numeric(token)) {
                    int pid = atoi(token);
                    if (pid == target_pid) {
                        found = true;
                    }
                    pids[pid_count++] = pid;
                }
                token = strtok(NULL, " \t\n");
            }
            break;
        }
    }

    fclose(fp);

    if (found) {
        switch (pid_count) {
            case 1:
                assert(pids[0] == host_pid);
                *host_pid_o = pids[0];
                break;
            case 2:
                *host_pid_o = pids[0];
                *container_pid_o = pids[1];
                break;
            default:
                LOG_WARN(
                    "we don't support processes with more than 2 pids, found "
                    "%d\n",
                    pid_count);
        }
    }

    return found;
}

/**
 * Given a pid, as well as the pid_namespace in which the pid is obtained,
 * return the pid of the same process in the host pid namespace.
 *
 * The `target_pid` is provided by the application; we do not yet know whether
 * this is its pid in the host pid namespace.
 *
 * If the returned pid does not equal the provided pid, then we deduce that the
 * process is running in a container.
 */
int map_nspid_to_pid(long pid_namespace, int target_pid) {
    DIR *proc_dir;
    struct dirent *entry;
    char target_ns[64];

    // Format target namespace string: "pid:[namespace_id]"
    snprintf(target_ns, sizeof(target_ns), "pid:[%ld]", pid_namespace);

    proc_dir = opendir("/proc");
    if (!proc_dir) {
        perror("opendir /proc");
        return -1;
    }

    LOG_DEBUG("looking for PID %d in namespace %ld:\n", target_pid,
              pid_namespace);

    // Scan /proc for numeric PID directories
    while ((entry = readdir(proc_dir)) != NULL) {
        if (!is_numeric(entry->d_name)) {
            continue;
        }

        int host_pid_entry = atoi(entry->d_name);
        if (is_pid_in_target_ns(host_pid_entry, target_ns)) {
            // found the pid that we're looking for

            int host_pid = 0;
            int container_pid = 0;
            if (parse_nspid_line(host_pid_entry, target_pid, &host_pid,
                                 &container_pid)) {
                // found that pid_entry matches target_pid.

                closedir(proc_dir);

                bool is_container = container_pid != 0;
                if (is_container) {
                    assert(container_pid == target_pid);
                    LOG_DEBUG("Found container PID %d maps to host PID %d\n",
                              container_pid, host_pid);
                } else {
                    assert(host_pid == target_pid);
                }

                return host_pid;
            }
        }
    }

    closedir(proc_dir);
    LOG_ERR("failed to find target pid %d under pid namespace %ld\n",
            target_pid, pid_namespace);
    return -1;
}
