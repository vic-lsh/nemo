#ifndef HEMEM_IPC_H
#define HEMEM_IPC_H
#define _GNU_SOURCE

#include "ipc-shared.h"
#include "type/process.h"

/**
 * Result of an IPC communication, from the UCM's point of view.
 */
enum ucm_ipc_result {
    /**
     * IPC was successful.
     */
    IPC_SUCCESS,
    /**
     * The app exited when the UCM attempted to send or receive from the
     * application.
     */
    IPC_APP_EXITED,
    /**
     * The UCM received an invalid message from the application.
     */
    IPC_INVALID_MSG,
    /**
     * IPC erred for reasons other than the ones explicitly listed in this enum.
     */
    IPC_ERRED,
};

int ipc_init_ucm();

[[nodiscard]] enum ucm_ipc_result ipc_remap_pages(
    struct hemem_process *process, struct hemem_page_app *fault_pages,
    int num_fault_pages);

#endif /* HEMEM_IPC_H */
