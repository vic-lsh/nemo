#include "proc-mgr.h"

#include <pthread.h>
#include <string.h>

#include "ds/fifo.h"
#include "ucm.h"
#include "util/log.h"

struct process_list processes_list;

/**
 * Root of a hashmap that maps from PID to process struct.
 *
 * Protected by `proc_map_lock`.
 */
struct hemem_process volatile *proc_map = NULL;
pthread_mutex_t proc_map_lock = PTHREAD_MUTEX_INITIALIZER;

void add_process(struct hemem_process *process) {
    struct hemem_process *p;

    pthread_mutex_lock(&proc_map_lock);
    HASH_FIND(phh, proc_map, &(process->pid), sizeof(pid_t), p);
    assert(p == NULL);
    HASH_ADD(phh, proc_map, pid, sizeof(pid_t), process);
    pthread_mutex_unlock(&proc_map_lock);

    enqueue_process(&processes_list, process);
}

void remove_process(struct hemem_process *process) {
    pthread_mutex_lock(&proc_map_lock);
    HASH_DELETE(phh, proc_map, process);
    pthread_mutex_unlock(&proc_map_lock);

    process_list_remove(&processes_list, process);
    pthread_mutex_lock(&(process->process_lock));
    process->mm.current_fastmem = 0;
    process->mm.current_slowmem = 0;
    pthread_mutex_unlock(&(process->process_lock));
}

struct hemem_process *find_process(pid_t pid) {
    struct hemem_process *process;
    pthread_mutex_lock(&proc_map_lock);
    HASH_FIND(phh, proc_map, &pid, sizeof(pid_t), process);
    pthread_mutex_unlock(&proc_map_lock);
    return process;
}

bool is_process_alive(pid_t pid) {
    // probe process aliveness by checking the /proc file system.
    char proc_path[64];
    snprintf(proc_path, sizeof(proc_path), "/proc/%d", pid);
    return (access(proc_path, F_OK) == 0);
}

size_t proc_mgr_prune() {
    size_t n_removed = 0;
    PROCESS_FOR_EACH(process) {
        if (!is_process_alive(process->pid)) {
            LOG_WARN("proc pid = %d no longer alive\n", process->pid);
            ucm_remove_process(process);
            n_removed++;
        }
    }
    return n_removed;
}

void proc_mgr_init() {
    memset(&processes_list, 0, sizeof(struct process_list));
    pthread_mutex_init(&(processes_list.list_lock), NULL);
}
