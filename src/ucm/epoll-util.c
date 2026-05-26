#include "epoll-util.h"

#include <errno.h>
#include <stddef.h>
#include <stdio.h>

int add_epoll_ctl(int epoll, int fd) {
    int ret;
    struct epoll_event event;

    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET;

    ret = epoll_ctl(epoll, EPOLL_CTL_ADD, fd, &event);
#ifdef HEMEM_DEBUG
    printf("add_epoll_ctl, fd=%d\n", fd);
#endif

    if (ret != 0) {
        perror("epoll_ctl");
        return -1;
    }

    return 0;
}

int delete_epoll_ctl(int epoll, int fd) {
    int ret;
    ret = epoll_ctl(epoll, EPOLL_CTL_DEL, fd, NULL);

#ifdef HEMEM_DEBUG
    printf("delete socket fd:%d\n", fd);
#endif

    if (ret != 0) {
        perror("epoll_ctl");
        return -1;
    }

    return 0;
}
