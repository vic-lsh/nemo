#ifndef HEMEM_UCM_PROC_MGR_H
#define HEMEM_UCM_PROC_MGR_H

#include "ds/fifo.h"
#include "type/process.h"

extern struct process_list processes_list;

#define PROCESS_FOR_EACH(iter) PROCESS_LIST_FOR_EACH(&processes_list, iter)

void proc_mgr_init();
void add_process(struct hemem_process *process);
void remove_process(struct hemem_process *process);
struct hemem_process *find_process(pid_t pid);

/**
 * Invoke this function to check aliveness of each process we track, and remove
 * the ones no longer alive.
 *
 * This is needed because we don't seem to be handling every path of app exit.
 */
size_t proc_mgr_prune();

#endif /* HEMEM_UCM_PROC_MGR_H */
