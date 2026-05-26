#ifndef UCM_POLICY_INTERFACE_H
#define UCM_POLICY_INTERFACE_H

/**
 * This file defines the interface that each policy must implement.
 *
 * This interface is private to the policy submodule. Other modules should use
 * the APIs listed in policy.h.
 */

#include "type/page.h"
#include "type/process.h"

struct policy_opts;

typedef struct {
    /**
     * Type-erased pointer to policy-specific state.
     */
    void *opaque;
    /**
     * Handler function invoked every epoch.
     */
    void (*epoch_handler)(void *opaque, size_t epoch_num);
    /**
     * Returns a page to be allocated to an app's allocation request.
     */
    struct hemem_page *(*allocate_page)(void *opaque,
                                        struct hemem_process *process);
    /**
     * Finds a page within a process to promote.
     */
    struct hemem_page *(*find_promotion_candidate)(
        void *opaque, struct hemem_process *process);
    /**
     * Finds a page within a process to demote.
     */
    struct hemem_page *(*find_demotion_candidate)(
        void *opaque, struct hemem_process *process);
} policy_t;

#endif /* UCM_POLICY_INTERFACE_H */
