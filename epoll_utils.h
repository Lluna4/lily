#pragma once
#include <sys/epoll.h>

int add_to_epoll(int epfd, int fd);
int remove_from_epoll(int epfd, int fd);