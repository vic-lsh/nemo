#include "ipc-chan/server.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ipc-chan/common.h"
#include "util/fs.h"

int channel_server_init() {
    int fd = -1;
    struct sockaddr_un server_addr;
    int ret;

    /* unix socket server */
    fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket");
        return fd;
    }

    if (mk_parent_dirs(CHAN_SOCKET_PATH) != 0) {
        LOG_ERR("failed to create parent directories for channel socket '%s'\n",
                CHAN_SOCKET_PATH);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sun_family = AF_UNIX;
    snprintf(server_addr.sun_path, sizeof(server_addr.sun_path),
             CHAN_SOCKET_PATH);

    // delete previously open socket, if it exists.
    unlink(server_addr.sun_path);

    ret = bind(fd, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (ret != 0) {
        close(fd);
        perror("bind");
        return -1;
    }

    ret = listen(fd, MAX_BACKLOG);
    if (ret != 0) {
        close(fd);
        perror("listen");
        return -1;
    }

    return fd;
}
