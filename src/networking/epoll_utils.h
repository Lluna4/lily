#pragma once
#include <cstddef>
#ifdef __linux__
#include <sys/epoll.h>
#endif
#if defined(__APPLE__) || (__FreeBSD__)
#include <sys/event.h>
#endif


int add_to_epoll(int epfd, int fd);
int remove_from_epoll(int epfd, int fd);