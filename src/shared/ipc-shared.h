#ifndef HEMEM_IPC_SHARED_H
#define HEMEM_IPC_SHARED_H

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include "hemem-shared.h"

enum operation {
    ALLOC_SPACE = 0,
    FREE_SPACE = 1,
    ADD_PROCESS = 2,
    REMOVE_PROCESS = 3,
    RECORD_UFFD = 4,
    REMAP_PAGES = 5,
    RECORD_REMAP_FD = 6
};

enum status_code { SUCCESS = 0, FAILED = 1, INVALID_REQUEST = 2, NO_SPACE = 3 };

struct hemem_page_app {
    uint64_t va;
    uint64_t ucm_va;
    uint64_t devdax_offset;
    bool in_dram;
    enum pagetypes pt;
};

struct msg_header {
    int status;
    pid_t pid;
    enum operation operation;
    size_t msg_size;
};

struct alloc_request {
    struct msg_header header;
    void* addr;
    size_t length;
    /**
     * Whether the application is requesting mmap with MAP_FIXED.
     * If set, we expect `addr` to fall under memory we allocated.
     */
    bool map_fixed;
};

struct alloc_response {
    struct msg_header header;
    size_t num_pages;
    struct hemem_page_app pages[];
};

struct free_request {
    struct msg_header header;
    void* addr;
    size_t length;
};

struct free_response {
    struct msg_header header;
};

struct add_process_request {
    struct msg_header header;
    double target_miss_ratio;
    uint64_t req_dram;
    /**
     * Pid from the client's perspective. This is the source of truth for the
     * requested pid.
     */
    pid_t pid;
    /**
     * Provide the namespace of the process's pid, found in /proc/self/ns/pid.
     * If set, we assume this is a container process.
     */
    long pid_namespace;
    /**
     * Deprecated.
     */
    bool zero;
};

#define MAX_MEM_PATH_LEN 64

struct add_process_response {
    struct msg_header header;
    pid_t host_pid;
    char fastmem_path[MAX_MEM_PATH_LEN];
    char slowmem_path[MAX_MEM_PATH_LEN];
};

struct remove_process_request {
    struct msg_header header;
};

struct remove_process_response {
    struct msg_header header;
};

struct record_uffd_request {
    struct msg_header header;
    /**
     * UFFD for the calling process.
     */
    long uffd;
};

struct record_uffd_response {
    struct msg_header header;
};

struct remap_request {
    struct msg_header header;
    size_t num_pages;
    struct hemem_page_app pages[];
};

struct remap_response {
    struct msg_header header;
    /**
     * va of the first page that has been remapped, as a sanity check.
     */
    uint64_t va;
};

struct record_remap_fd_request {
    struct msg_header header;
};

struct record_remap_fd_response {
    struct msg_header header;
};

#endif /* HEMEM_IPC_SHARED_H */
