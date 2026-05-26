#include "shm.h"

#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "hemem-shared.h"
#include "mm.h"
#include "util/fs.h"
#include "util/log.h"

void* shm_create_mmap(char* shm_path, size_t shm_size) {
    void* shm_ptr = NULL;
    int shm_fd = 0;

    LOG("configuring shm region %s, size %lu\n", shm_path, shm_size);

    size_t default_page_sz = get_default_page_size();
    if (shm_size % default_page_sz != 0) {
        LOG_ERR("shm size %lu not a multiple of page size %lu\n", shm_size,
                default_page_sz);
        return NULL;
    }

    if (mk_parent_dirs(shm_path) == -1) {
        LOG_ERR("Failed to create directories for path %s\n", shm_path);
        return NULL;
    }

    // 1. Create the shared memory object.
    // - O_CREAT: Create the object if it doesn't exist.
    // - O_RDWR: Open for reading and writing.
    // - 0666: Permissions for owner, group, and others to read/write.
    shm_fd = open(shm_path, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("shm_open");
        goto cleanup_shm_open;
    }

    // 2. Configure the size of the shared memory object.
    if (ftruncate(shm_fd, shm_size) == -1) {
        perror("ftruncate");
        goto cleanup_shm_fd;
    }

    // 3. Map the shared memory object into this process's address space.
    shm_ptr = mmap(NULL, shm_size, PROT_READ | PROT_WRITE,
                   MAP_SHARED | MAP_HUGETLB, shm_fd, 0);
    if (shm_ptr == MAP_FAILED) {
        perror("mmap");
        if (errno == EINVAL) {
            LOG_ERR("failed to mmap shm region b/c EINVAL.\n");
            LOG_ERR(
                "One common cause is our use of MAP_HUGETLB. Did you run "
                "./scripts/post_boot_setup_shm.sh to set up shm directory as "
                "hugetlbfs?\n");
        }
        goto cleanup_shm_fd;
    }

    // The file descriptor is no longer needed after a successful mmap.
    // The mapping will persist until munmap is called or the process exits.
    close(shm_fd);

    return shm_ptr;

cleanup_shm_fd:
    close(shm_fd);
    shm_unlink(shm_path);
cleanup_shm_open:
    return NULL;
}

void shm_close_mmap(void* shm_region, size_t shm_size) {
    munmap(shm_region, shm_size);
}
