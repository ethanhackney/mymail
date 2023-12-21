#ifndef NET_H
#define NET_H

#include <errno.h>
#include <netdb.h>
#include <unistd.h>

/**
 * Create listening socket:
 *
 * args:
 *      @infolist:      list of addrinfo
 *      @qsize:         connection queue size
 * ret:
 *      @success:       listening socket descriptor
 *      @failure:       -1
 */
extern int net_listen(struct addrinfo *infolist, int qsize);

/**
 * Create a connected socket:
 *
 * args:
 *      @infolist:      list of addrinfo
 * ret:
 *      @success:       connected socket descriptor
 *      @failure:       -1
 */
extern int net_connect(struct addrinfo *infolist);

#endif
