#pragma once

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <cstdint>
#include <ctime>

namespace sylar {
bool is_hook_enable();

void set_hook_enable(bool flag);
}  // namespace sylar

extern "C" {
// sleep
using sleep_func = unsigned int (*)(unsigned int seconds);
extern sleep_func sleep_f;

using usleep_func = int (*)(useconds_t usec);
extern usleep_func usleep_f;

using nanosleep_func = int (*)(const struct timespec *req, struct timespec *rem);
extern nanosleep_func nanosleep_f;

// socket
using socket_func = int (*)(int domain, int type, int protocol);
extern socket_func socket_f;

using connect_func = int (*)(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
extern connect_func connect_f;

using accept_func = int (*)(int s, struct sockaddr *addr, socklen_t *addrlen);
extern accept_func accept_f;

// read
using read_func = ssize_t (*)(int fd, void *buf, size_t count);
extern read_func read_f;

using readv_func = ssize_t (*)(int fd, const struct iovec *iov, int iovcnt);
extern readv_func readv_f;

using recv_func = ssize_t (*)(int sockfd, void *buf, size_t len, int flags);
extern recv_func recv_f;

using recvfrom_func = ssize_t (*)(int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr,
                                  socklen_t *addrlen);
extern recvfrom_func recvfrom_f;

using recvmsg_func = ssize_t (*)(int sockfd, struct msghdr *msg, int flags);
extern recvmsg_func recvmsg_f;

// write
using write_func = ssize_t (*)(int fd, const void *buf, size_t count);
extern write_func write_f;

using writev_func = ssize_t (*)(int fd, const struct iovec *iov, int iovcnt);
extern writev_func writev_f;

using send_func = ssize_t (*)(int s, const void *msg, size_t len, int flags);
extern send_func send_f;

using sendto_func = ssize_t (*)(int s, const void *msg, size_t len, int flags, const struct sockaddr *to,
                                socklen_t tolen);
extern sendto_func sendto_f;

using sendmsg_func = ssize_t (*)(int s, const struct msghdr *msg, int flags);
extern sendmsg_func sendmsg_f;

using close_func = int (*)(int fd);
extern close_func close_f;

using fcntl_func = int (*)(int fd, int cmd, ...);
extern fcntl_func fcntl_f;

using ioctl_func = int (*)(int d, unsigned long int request, ...);
extern ioctl_func ioctl_f;

using getsockopt_func = int (*)(int sockfd, int level, int optname, const void *optval, socklen_t* optlen);
extern getsockopt_func getsockopt_f;

using setsockopt_func = int (*)(int sockfd, int level, int optname, const void *optval, socklen_t optlen);
extern setsockopt_func setsockopt_f;

extern int connect_with_timeout(int fd, const struct sockaddr *addr, socklen_t addrlen, uint64_t timeoutMs);
};