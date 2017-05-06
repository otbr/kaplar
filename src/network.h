#ifndef NETWORK_H_
#define NETWORK_H_

#include <errno.h>

#define NET_SHUT_RD	0x00
#define NET_SHUT_WR	0x01
#define NET_SHUT_RDWR	0x02

struct socket;

int	net_init(void);
void	net_shutdown(void);

struct socket	*net_socket(void);
struct socket	*net_server_socket(int port);
unsigned long	net_get_remote_address(struct socket *sock);

void	net_socket_shutdown(struct socket *sock, int how);
void	net_close(struct socket *sock);

int	net_async_accept(struct socket *sock,
		void (*fp)(struct socket*, int, int, void*), void *udata);
int	net_async_read(struct socket *sock, char *buf, int len,
		void (*fp)(struct socket*, int, int, void*), void *udata);
int	net_async_write(struct socket *sock, char *buf, int len,
		void (*fp)(struct socket*, int, int, void*), void *udata);

int	net_work(void);

#endif //NETWORK_H_
