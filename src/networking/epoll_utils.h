#pragma once
#include <cstddef>
#ifdef __linux__
#include <sys/epoll.h>
#endif
#ifdef __APPLE__
#include <sys/event.h>
#endif


int add_to_epoll(int epfd, int fd);
int remove_from_epoll(int epfd, int fd);