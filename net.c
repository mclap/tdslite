#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "net.h"

struct net_s
{
	int fd;
};

net_t net_create(const char *addr, int port)
{
	struct sockaddr_in sa;

	struct net_s *cn = (net_t)calloc(1, sizeof(*cn));
	cn->fd = socket(AF_INET, SOCK_STREAM, 0);

	memset(&sa, 0, sizeof(sa));
	sa.sin_addr.s_addr = inet_addr(addr);
	sa.sin_port = htons(port);
	sa.sin_family = AF_INET;
	if (0 == connect(cn->fd, (struct sockaddr *)&sa, sizeof(sa)))
		return cn;

	close(cn->fd);
	free(cn);
	return 0;
}

int net_free(net_t cn)
{
	close(cn->fd);
	free(cn);
}

int net_send(net_t cn, const void *buf, size_t len)
{
	if (write(cn->fd, buf, len) == len)
		return len;

	return -1;
}

int net_read(net_t cn, void *buf, size_t len)
{
	return read(cn->fd, buf, len);
}
