#include "epoll_utils.h"

#if defined(__linux__)
int add_to_epoll(int epfd, int fd)
{
	struct epoll_event epoll_ev = {0};
	epoll_ev.data.fd = fd;
	epoll_ev.events = EPOLLIN;
	return epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &epoll_ev);
}
#elif defined (__FreeBSD) || defined(__APPLE__)
int add_to_epoll(int epfd, int fd)
{
	struct kevent ev;
	EV_SET(&ev, fd, EVFILT_READ, EV_ADD, 0, 0, 0);
	return kevent(epfd, &ev, 1, NULL, 0, NULL);
}
#endif

#if defined(__linux__)
int remove_from_epoll(int epfd, int fd)
{
	return epoll_ctl(epfd, EPOLL_CTL_DEL, fd, nullptr);
}
#elif defined (__FreeBSD) || defined(__APPLE__)
int remove_from_epoll(int epfd, int fd)
{
	struct kevent ev;
	EV_SET(&ev, fd, EVFILT_READ, EV_DELETE, 0, 0, 0);
	return kevent(epfd, &ev, 1, NULL, 0, NULL);
}
#endif