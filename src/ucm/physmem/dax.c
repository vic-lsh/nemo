#include "dax.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "util/log.h"

void* devdax_open_mmap(char* dax_path, size_t dax_size, int* dax_fd) {
    int _dax_fd = open(dax_path, O_RDWR);
    if (_dax_fd < 0) {
        LOG_ERR("err opening dax file '%s': %s\n", dax_path, strerror(errno));
        return NULL;
    }
    assert(_dax_fd >= 0);
    void* dax_mmap_loc = mmap(NULL, dax_size, PROT_READ | PROT_WRITE,
                              MAP_SHARED | MAP_POPULATE, _dax_fd, 0);
    if (dax_mmap_loc == MAP_FAILED) {
        LOG_ERR("err mmapping dax file '%s' len %lu: %s\n", dax_path, dax_size,
                strerror(errno));
        close(_dax_fd);
        return NULL;
    }
    if (dax_fd) {
        *dax_fd = _dax_fd;
    }

    return dax_mmap_loc;
}

void devdax_close_mmap(void* dax_mmap_region, size_t dax_size, int* dax_fd) {
    munmap(dax_mmap_region, dax_size);
    if (dax_fd) {
        close(*dax_fd);
    }
}
