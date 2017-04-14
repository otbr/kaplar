#ifndef __NETWORK_H__
#define __NETWORK_H__

#include <errno.h>

struct socket;

int	net_init();
void	net_shutdown();

struct socket	*net_socket();
struct socket	*net_server_socket(int port);
void		net_close(struct socket *sock);


int	net_async_accept(struct socket *sock,
		void (*fp)(struct socket*, int, int, void*), void *udata);
int	net_async_read(struct socket *sock, char *buf, int len,
		void (*fp)(struct socket*, int, int, void*), void *udata);
int	net_async_write(struct socket *sock, char *buf, int len,
		void (*fp)(struct socket*, int, int, void*), void *udata);

int	net_work();

#endif //__NETWORK_H__