#include "uffd.h"

#include <fcntl.h>
#include <linux/userfaultfd.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>

#include "hemem-app.h"
#include "ipc.h"

int uffd_register_page(long uffd, uint64_t page_boundary, size_t page_size) {
    struct uffdio_register uffdio_register;
    uffdio_register.range.start = page_boundary;
    uffdio_register.range.len = page_size;
    uffdio_register.mode =
        UFFDIO_REGISTER_MODE_MISSING | UFFDIO_REGISTER_MODE_WP;
    uffdio_register.ioctls = 0;
    if (ioctl(uffd, UFFDIO_REGISTER, &uffdio_register) == -1) {
        return -1;
    }
    return 0;
}

int uffd_unregister_page(long uffd, uint64_t page_boundary, size_t page_size) {
    struct uffdio_range uffdio_range;
    uffdio_range.start = page_boundary;
    uffdio_range.len = page_size;
    if (ioctl(uffd, UFFDIO_UNREGISTER, &uffdio_range) == -1) {
        perror("ioctl uffdio_unregister");
        return -1;
    }
    return 0;
}

long uffd_open() {
    long userfault_fd = syscall(__NR_userfaultfd, O_CLOEXEC | O_NONBLOCK);
    if (userfault_fd == -1) {
        perror("uffd");
        return -1;
    }

    struct uffdio_api uffdio_api;
    uffdio_api.api = UFFD_API;
    uffdio_api.features =
        UFFD_FEATURE_PAGEFAULT_FLAG_WP | UFFD_FEATURE_MISSING_SHMEM |
        UFFD_FEATURE_MISSING_HUGETLBFS | UFFD_FEATURE_EVENT_FORK |
        UFFD_FEATURE_EVENT_REMAP | UFFD_FEATURE_EVENT_UNMAP;
    uffdio_api.ioctls = 0;
    if (ioctl(userfault_fd, UFFDIO_API, &uffdio_api) == -1) {
        perror("ioctl uffdio_api");
        close(userfault_fd);
        return -1;
    }

    return userfault_fd;
}
