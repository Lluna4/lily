#include "epoll_utils.h"

int add_to_epoll(int epfd, int fd)
{
	struct epoll_event epoll_ev = {0};
	epoll_ev.data.fd = fd;
	epoll_ev.events = EPOLLIN;
	return epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &epoll_ev);
}

int remove_from_epoll(int epfd, int fd)
{
	return epoll_ctl(epfd, EPOLL_CTL_DEL, fd, nullptr);
}