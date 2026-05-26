#include "uffd.h"

#include <assert.h>
#include <errno.h>
#include <linux/userfaultfd.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/epoll.h>
#include <sys/ioctl.h>

#include "core.h"
#include "epoll-util.h"
#include "ipc/ipc.h"
#include "mm.h"
#include "physmem/physmem.h"
#include "policy/policy.h"
#include "proc-mgr.h"
#include "stats/stats.h"
#include "type/process.h"
#include "ucm-config.h"
#include "ucm.h"
#include "util/log.h"
#include "util/thread.h"
#include "util/timer.h"

#define MAX_UFFD_MSGS (3)

int fault_epoll_fd = -1;

// Maintains mapping from uffd to process pointers.
struct hemem_process *uffd_to_proc[1024];

#define SECS_TO_NANOS(secs) ((secs)*1000000000UL)

static bool bounded_migration_wait(struct hemem_page *page) {
    const size_t kCheckTimeInterval = 10000;
    const size_t kTimeoutSecs = 1;
    const size_t kTimeoutNanos = SECS_TO_NANOS(kTimeoutSecs);

    bool success = true;

    struct timespec start, current;
    clock_gettime(CLOCK_MONOTONIC, &start);
    size_t check_counter = 0;

    while (page->migrating) {
        if (++check_counter >= kCheckTimeInterval) {
            check_counter = 0;

            clock_gettime(CLOCK_MONOTONIC, &current);

            size_t elapsed_nanos =
                SECS_TO_NANOS(current.tv_sec - start.tv_sec) +
                (current.tv_nsec - start.tv_nsec);
            if (elapsed_nanos >= kTimeoutNanos) {
                LOG_ERR(
                    "Page migration took longer than %lu secs. This is likely "
                    "a bug.\n",
                    kTimeoutSecs);
                success = false;
                break;
            }
        }
    }

    return success;
}

static void handle_wp_fault_inner(struct hemem_process *process,
                                  uint64_t page_boundary) {
    struct hemem_page *page;
    // struct hemem_page_app page_app;
    assert(process);

    page = mm_find_page(&process->mm, page_boundary);
    if (page == NULL) {
        LOG_ERR("Page not found, va %p\n", (void *)page_boundary);
        print_stacktrace();
        assert(page != NULL);
    }

    wall_and_cpu_time_t wait_time;

    bool success;
    TIME_OP(wait_time, { success = bounded_migration_wait(page); });
    // LOG("hemem: handle_wp_fault: done waiting for page %p\n", (void
    // *)page->va);
    if (!success) {
        LOG_ERR("wp fault took more than 1 sec for page %p\n",
                (void *)page->va);
        assert(success);
    }

    // LOG("migration waiting end page %p\n", (void *)page->va);
    ucm_stats_record_migration_wait(process, wait_time.wall_time_us);
}

static void handle_wp_fault(struct hemem_process *process,
                            uint64_t page_boundary, uint64_t fault_addr) {
    physmem_mode_t mode = get_physmem_mode();
    if (mode == USE_SHM && fault_addr == page_boundary) {
        // BUG: this is likely a bug. for some reason, when using SHM mode,
        // when the app remaps a migrated page, the act of remapping (via
        // an mmap() call) triggers a write-protection fault. this always
        // happens at the page boundary. If we don't mark the fault as
        // handled here, the ucm and the app would deadlock.
        //
        // This is a disaster waiting to happen, because we cannot tell if
        // this is a fault induced by the mmap(), or a fault induced by the
        // application itself. We also have no idea why the mmap() would
        // cause a wp fault (this doesn't happen for mode == USE_DAX).

        return;
    }

    handle_wp_fault_inner(process, page_boundary);
}

static void handle_missing_fault_unallocated(struct hemem_process *process,
                                             uint64_t page_boundary) {
    void *addr;
    struct timeval missing_start, missing_end;
    struct timeval start, end;
    struct hemem_page *page;
    uint64_t offset;
    bool in_dram;
    uint64_t pagesize;
    struct hemem_page_app page_app;

    assert(page_boundary != 0);

    gettimeofday(&missing_start, NULL);

    gettimeofday(&start, NULL);
    page = ucm_allocate_page(process);
    assert(page != NULL);

    gettimeofday(&end, NULL);
    LOG_TIME("page_fault: %f s\n", elapsed_secs(&start, &end));

    offset = page->devdax_offset;
    in_dram = page->in_dram;
    pagesize = pt_to_pagesize(page->pt);

    addr = devdax_offset_to_addr(offset, in_dram);
    if (process->zero) {
        memset(addr, 0, pagesize);
    }
    ucm_stats_get()->mem.memsets++;

    // use mmap return addr to track new page's virtual address
    page->uffd = process->uffd;
    page->pid = process->pid;
    page->va = page_boundary;
    assert(page->va != 0);
    assert(page->va % HUGEPAGE_SIZE == 0);
    page->migrations_up = page->migrations_down = 0;

    ucm_stats_get()->pagefaults.missing_faults_handled++;
    ucm_stats_record_page_allocated(process, pagesize);

    mm_add_page(&process->mm, page);

    gettimeofday(&missing_end, NULL);
    LOG_TIME("hemem_missing_fault: %f s\n",
             elapsed_secs(&missing_start, &missing_end));

    page_app.va = page_boundary;
    page_app.devdax_offset = page->devdax_offset;
    page_app.in_dram = page->in_dram;
    page_app.pt = page->pt;

    enum ucm_ipc_result status = ipc_remap_pages(process, &page_app, 1);
    if (likely(status == IPC_SUCCESS)) {
        LOG_DEBUG("handling_missing_fault_unallocated done for page %p\n",
                  (void *)page_boundary);
        return;
    }

    switch (status) {
        case IPC_APP_EXITED:
            ucm_remove_process(process);
            break;
        default:
            LOG_ERR("ipc_remap_pages erred %d\n", status);
            assert(0);
    }
}

__maybe_unused static void handle_missing_fault_allocated(
    struct hemem_process *process, struct hemem_page *found_page) {
    process->stats.pf.miss_but_allocated_faults++;

    // TODO: revisit whether this mutex is still needed
    // NOTE: add unlock if this is needed
    // wall_and_cpu_time_t lock_time;
    // TIME_OP(lock_time, { pthread_mutex_lock(&found_page->remap_lock); });
    // if (lock_time.wall_time_us > 200) {
    //     LOG_WARN("handle_fault lock time %f us page %p\n",
    //              lock_time.wall_time_us, (void *)found_page->va);
    // }

    struct hemem_page_app page_app;
    page_app.va = found_page->va;
    page_app.devdax_offset = found_page->devdax_offset;
    page_app.in_dram = found_page->in_dram;
    page_app.pt = found_page->pt;

    enum ucm_ipc_result status = ipc_remap_pages(process, &page_app, 1);
    if (likely(status == IPC_SUCCESS)) {
        LOG_DEBUG("handle_fault done for page %p\n", (void *)found_page->va);
        return;
    }

    switch (status) {
        case IPC_APP_EXITED:
            ucm_remove_process(process);
            break;
        default:
            LOG_ERR("ipc_remap_pages erred %d\n", status);
            assert(0);
    }
}

static void handle_missing_fault(struct hemem_process *process,
                                 uint64_t page_boundary) {
    process->stats.pf.miss_faults++;
    struct hemem_page *found_page = NULL;
    if ((found_page = mm_find_page(&process->mm, page_boundary))) {
        LOG_DEBUG("handle missing allocated pagefault for page %p\n",
                  (void *)page_boundary);
        // Hemem already mapped this page. Let the app know
        // about this mapping and remap it.
        //
        // Hemem maps the pages when they're allocated to
        // the app. However, a mapping may be valid (the
        // app hasn't deallocated this), but not present in
        // the OS page table. This can happen if the app
        // calls madvise with MADV_DONTNEED.

        // FIXME: this is only needed in Redis so far.
        // Enabling UFFD_FEATURE_EVENT_UNMAP seems to result in a pagefault
        // event when the application remap thread remaps. Doing any handling
        // here would cause the UCM to deadlock with the app-side remap thread.
        // handle_missing_fault_allocated(process, found_page);

        // NOTE: this is the prior solution, which works if
        // UFFD_FEATURE_EVENT_UNMAP is not enabled.
        // However, we need the UNMAP feature to support QEMU.
        // handle_wp_fault(process, page_boundary);
    } else {
        LOG_DEBUG("handle missing UNallocated pagefault for page %p\n",
                  (void *)page_boundary);
        // Hemem didn't map this page. Allocate new page
        // to the app.
        handle_missing_fault_unallocated(process, page_boundary);
    }
}

// This is effectively the pagefault handler in UCM.
static void handle_page_fault(uint64_t fault_addr, uint64_t fault_flags,
                              struct hemem_process *process) {
    // align faulting address to page boundary
    // huge page boundary in this case due to dax allignment
    uint64_t page_boundary = fault_addr & ~(get_default_page_size() - 1);
    assert(page_boundary != 0);

    bool is_wp = fault_flags & UFFD_PAGEFAULT_FLAG_WP;
    bool is_minor = fault_flags & UFFD_PAGEFAULT_FLAG_MINOR;
    bool is_write = fault_flags & UFFD_PAGEFAULT_FLAG_WRITE;
    bool is_missing = !is_minor && !is_wp;

    LOG_DEBUG(
        "handle pagefault for page %p is_wp %d is_minor %d is_write %d "
        "is_missing %d\n",
        (void *)page_boundary, is_wp, is_minor, is_write, is_missing);

    if (is_wp) {
        LOG_DEBUG("handle wp pagefault for page %p\n", (void *)page_boundary);
        handle_wp_fault(process, page_boundary, fault_addr);
    } else if (is_missing) {
        LOG_DEBUG("handle missing pagefault for page %p\n",
                  (void *)page_boundary);
        handle_missing_fault(process, page_boundary);
    } else {
        // we don't support other fault types yet.
        assert(0);
    }

    // wake the faulting thread
    struct uffdio_range range;
    range.start = (uint64_t)page_boundary;
    range.len = get_default_page_size();

    int ret = ioctl(process->uffd, UFFDIO_WAKE, &range);

    if (ret < 0) {
        if (errno == EBADF || errno == ENOENT) {
            if (!process->exited) {
                perror("uffdio wake");
                assert(0);
            }
        } else {
            perror("uffdio wake");
            assert(0);
        }
    }
}

// handles UFFD_EVENT_UNMAP
static void handle_page_unmap(uint64_t start, uint64_t end,
                              struct hemem_process *process) {
    uint64_t pagesize;
    struct hemem_page *page;
    LOG_DEBUG("Received an unmap event for addr %p to %p len %ld\n",
              (void *)start, (void *)end, end - start);

    uint64_t page_boundary = start & ~(get_default_page_size() - 1);
    while (page_boundary < end) {
        page = mm_find_page(&process->mm, page_boundary);
        if (page != NULL) {
            pagesize = pt_to_pagesize(page->pt);

            // TODO: the unmap pending events should maybe be one enum, because
            // we don't expect multiple such pending events to be present? Or
            // rather, when we have a way to know the cause of the unmap event
            // when we recieve them.
            if (atomic_exchange(&page->migration_uffd_unmap_pending, false)) {
                // just migrated
            } else if (atomic_exchange(&page->map_fixed_uffd_unmap_pending,
                                       false)) {
                // just added another UFFD mapping
                LOG_DEBUG("page %p just MAP_FIXED\n", (void *)page->va);
            } else {
                LOG_DEBUG("Process %d unmapped page %p\n", process->pid,
                          (void *)page->va);
                mm_remove_page(&process->mm, page);
                ucm_stats_record_page_freed(process, pagesize);
            }
            page_boundary += pagesize;
        } else {
            page_boundary += get_default_page_size();
        }
    }
}

// handles UFFD_EVENT_REMOVE
static void handle_page_remove(
    uint64_t start, uint64_t end,
    __attribute__((unused)) struct hemem_process *process) {
    LOG_WARN("Received an remove event for addr %p to %p len %ld\n",
             (void *)start, (void *)end, end - start);
}

// handles UFFD_EVENT_REMAP
static void handle_page_remap(
    uint64_t from, uint64_t to, uint64_t len,
    __attribute__((unused)) struct hemem_process *process) {
    LOG_WARN("Received an REMAP event for addr %p to %p len %ld\n",
             (void *)from, (void *)to, len);
}

static char *uffd_event_str(char event) {
    switch (event) {
        case UFFD_EVENT_UNMAP:
            return "UFFD_EVENT_UNMAP";
        case UFFD_EVENT_REMOVE:
            return "UFFD_EVENT_REMOVE";
        case UFFD_EVENT_REMAP:
            return "UFFD_EVENT_REMAP";
        case UFFD_EVENT_PAGEFAULT:
            return "UFFD_EVENT_PAGEFAULT";
        case UFFD_EVENT_FORK:
            return "UFFD_EVENT_FORK";
        default:
            return "unknown";
    }
}

static void handle_uffd_msg(struct uffd_msg *msg,
                            struct hemem_process *process) {
    LOG_DEBUG("handling fault event 0x%x '%s'\n", msg->event,
              uffd_event_str(msg->event));

    switch (msg->event) {
        case UFFD_EVENT_UNMAP:
            // note: unmap also uses the `arg.remove`, like UFFD_EVENT_REMOVE.
            handle_page_unmap(msg->arg.remove.start, msg->arg.remove.end,
                              process);
            break;
        case UFFD_EVENT_REMOVE:
            handle_page_remove(msg->arg.remove.start, msg->arg.remove.end,
                               process);
            break;
        case UFFD_EVENT_REMAP:
            handle_page_remap(msg->arg.remap.from, msg->arg.remap.to,
                              msg->arg.remap.len, process);
            break;
        case UFFD_EVENT_PAGEFAULT:
            handle_page_fault((uint64_t)msg->arg.pagefault.address,
                              msg->arg.pagefault.flags, process);
            break;
        default:
            LOG_ERR("received an unexpected uffd event %d\n", msg->event);
            assert(0);
    }
}

void *handle_fault_thread() {
    static struct uffd_msg msg[MAX_UFFD_MSGS];
    ssize_t nread;
    int nmsgs;
    int i;
    int num_ready_fds, ready_fd;
    struct epoll_event epoll_events[MAX_EPOLL_EVENTS];
    struct hemem_process *process;

    thread_pin_self(FAULT_THREAD_CPU);
    pthread_setname_np(pthread_self(), "uffdfault-thr");

    for (;;) {
        num_ready_fds =
            epoll_wait(fault_epoll_fd, epoll_events, MAX_EPOLL_EVENTS, -1);
        for (i = 0; i < num_ready_fds; i++) {
            ready_fd = epoll_events[i].data.fd;

            if ((epoll_events[i].events & EPOLLERR) ||
                (epoll_events[i].events & EPOLLHUP) ||
                (!(epoll_events[i].events & EPOLLIN))) {
                fprintf(stderr, "epoll not EPOLLIN in for uffd\n");
                continue;
            }

            process = uffd_to_proc[ready_fd];

            nread = read(ready_fd, &msg[0],
                         MAX_UFFD_MSGS * sizeof(struct uffd_msg));
            if (nread == 0) {
                fprintf(stderr, "EOF on userfaultfd\n");
                assert(0);
            }

            if (nread < 0) {
                if (errno == EAGAIN) {
                    continue;
                }
                perror("read");
                assert(0);
            }

            if ((nread % sizeof(struct uffd_msg)) != 0) {
                fprintf(stderr, "invalid msg size: [%ld]\n", nread);
                assert(0);
            }

            nmsgs = nread / sizeof(struct uffd_msg);
            for (i = 0; i < nmsgs; i++) {
                LOG_DEBUG("handling fault %d of %d\n", i, nmsgs);
                handle_uffd_msg(&msg[i], process);
            }
        }
    }
}

void add_process_uffd(struct hemem_process *process, long uffd) {
    uffd_to_proc[uffd] = process;
    add_epoll_ctl(fault_epoll_fd, uffd);
}

pthread_t uffd_thread;

int uffd_init() {
    fault_epoll_fd = epoll_create1(0);
    if (fault_epoll_fd == -1) {
        perror("epoll_create1");
        return -1;
    }

    int ret = pthread_create(&uffd_thread, NULL, handle_fault_thread, 0);
    if (ret != 0) {
        perror("uffd_thread pthread_create");
        return -1;
    }
    return 0;
}
