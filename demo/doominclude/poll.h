#ifndef _POLL_H
#define _POLL_H
#define POLLIN 1
struct pollfd { int fd; short events; short revents; };
int poll(struct pollfd *fds, unsigned int nfds, int timeout);
#endif
