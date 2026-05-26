#ifndef HEMEM_UCM_IPC_QEMU_H
#define HEMEM_UCM_IPC_QEMU_H

#define _GNU_SOURCE

/**
 * Start server for receiving connection from our QEMU fork.
 */
int ipc_qemu_init();

#endif /* HEMEM_UCM_IPC_QEMU_H */
