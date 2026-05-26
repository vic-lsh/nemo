#ifndef HEMEM_APP_IPC_H
#define HEMEM_APP_IPC_H

#include <stddef.h>
#include <stdint.h>

#include "hemem-app.h"
#include "hemem-shared.h"
#include "ipc-shared.h"

enum status_code ipc_add_process(app_ctx_t* ctx);
enum status_code ipc_remove_process(app_ctx_t* ctx);
struct alloc_response* ipc_alloc_space(app_ctx_t* ctx, void* addr,
                                       size_t length, bool map_fixed);
enum status_code ipc_free_space(app_ctx_t* ctx, void* addr, size_t length);

#endif /* HEMEM_APP_IPC_H */
