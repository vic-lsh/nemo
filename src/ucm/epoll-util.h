#ifndef HEMEM_EPOLL_UTIL_H
#define HEMEM_EPOLL_UTIL_H

#include <sys/epoll.h>

int add_epoll_ctl(int epoll, int fd);
int delete_epoll_ctl(int epoll, int fd);

#endif /* HEMEM_EPOLL_UTIL_H */
