#include "util/proc.h"

#include <assert.h>
#include <sys/stat.h>

#include "util/log.h"

int get_cgroup() {
    // TODO: actually return cgroup rather than printing things out.
    FILE* file = fopen("/proc/self/cgroup", "r");
    if (!file) {
        printf("Cannot read cgroup info - cgroups may be disabled\n");
        return -1;
    }

    char line[256];
    while (fgets(line, sizeof(line), file)) {
        // Format: hierarchy-ID:controller-list:cgroup-path
        printf("cgroup %s", line);
    }
    fclose(file);
    return 0;
}

long get_pid_namespace() {
    struct stat st;

    if (stat("/proc/self/ns/pid", &st) == -1) {
        perror("stat");
        return -1;
    }

    // The inode number is the namespace identifier
    return (long)st.st_ino;
}
