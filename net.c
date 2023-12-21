#include "net.h"

static int try_listen(struct addrinfo *p, int qsize);

int
net_listen(struct addrinfo *infolist, int qsize)
{
        struct addrinfo *p;
        int fd;

        fd = -1;
        for (p = infolist; p; p = p->ai_next) {
                fd = try_listen(p, qsize);
                if (fd >= 0)
                        break;
        }

        return fd;
}

static int
try_listen(struct addrinfo *p, int qsize)
{
        int saved_errno;
        int fd;
        int on;

        fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (fd < 0)
                goto ret;

        on = 1;
        if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)))
                goto close_fd;

        if (bind(fd, p->ai_addr, p->ai_addrlen))
                goto close_fd;
        
        if (listen(fd, qsize))
                goto close_fd;

        goto ret;
close_fd:
        saved_errno = errno;
        close(fd);
        errno = saved_errno;
        fd = -1;
ret:
        return fd;
}

static int try_connect(struct addrinfo *p);

int
net_connect(struct addrinfo *infolist)
{
        struct addrinfo *p;
        int fd;

        fd = -1;
        for (p = infolist; p; p = p->ai_next) {
                fd = try_connect(p);
                if (fd >= 0)
                        break;
        }

        return fd;
}

static int
try_connect(struct addrinfo *p)
{
        int saved_errno;
        int fd;

        fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (fd < 0)
                goto ret;

        if (connect(fd, p->ai_addr, p->ai_addrlen))
                goto close_fd;

        goto ret;
close_fd:
        saved_errno = errno;
        close(fd);
        errno = saved_errno;
        fd = -1;
ret:
        return fd;
}
