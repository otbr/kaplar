#include "../network.h"

#include "../array.h"
#include "../log.h"
#include "../thread.h"

#include <stddef.h>
#include <unistd.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/epoll.h>

#define WORK_TIMEOUT	1000	// 1sec

#define OP_FREE		0x00
#define OP_ACCEPT	0x01
#define OP_READ		0x02
#define OP_WRITE	0x03
struct async_op{
	long opcode;
	long length;
	void *buf;
	void (*complete)(struct socket*, int, int, void*);
	void *udata;
};

#define MAX_SOCKETS	2048
#define SOCKET_MAX_OPS	16
struct socket{
	int			fd;
	struct epoll_event	event;
	struct async_op		ops[SOCKET_MAX_OPS];
	struct mutex		*lock;
	struct sockaddr_in	*remote_addr;
};

static int		epoll_fd = -1;
static struct array	*sock_array = NULL;

int net_init(void)
{
	// create epoll fd
	epoll_fd = epoll_create1(0);
	if(epoll_fd == -1){
		LOG_ERROR("net_init: failed to create epoll fd (error = %d)", errno);
		return -1;
	}

	// create socket array
	sock_array = array_create(MAX_SOCKETS, sizeof(struct socket));
	array_init_lock(sock_array);
	return 0;
}

void net_shutdown(void)
{
	if(sock_array != NULL){
		array_destroy(sock_array);
		sock_array = NULL;
	}

	if(epoll_fd != -1){
		close(epoll_fd);
		epoll_fd = -1;
	}
}

struct socket *net_socket(void)
{
	int fd, flags;
	struct linger linger;
	struct epoll_event event;
	struct socket *sock;

	fd = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
	if(fd == -1){
		LOG_ERROR("net_socket: failed to create socket (error = %d)", errno);
		return NULL;
	}

	// set linger
	linger.l_onoff = 0;
	linger.l_linger = 0;
	if(setsockopt(fd, SOL_SOCKET, SO_LINGER, (void *)&linger, sizeof(struct linger)) == -1){
		LOG_ERROR("net_socket: failed to set socket linger option (error = %d", errno);
		close(fd);
		return NULL;
	}

	// set nonblocking
	flags = fcntl(fd, F_GETFL);
	if(flags == -1){
		LOG_ERROR("net_socket: failed to get socket flags (error = %d)", errno);
		close(fd);
		return NULL;
	}
	if(fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1){
		LOG_ERROR("net_socket: failed to set nonblocking mode (error = %d)", errno);
		close(fd);
		return NULL;
	}

	// create socket handle
	sock = array_locked_new(sock_array);
	if(sock == NULL){
		LOG_ERROR("net_socket: socket array is full");
		close(fd);
		return NULL;
	}

	// initialize socket
	sock->fd = fd;
	sock->event.events = 0;
	sock->event.data.ptr = sock;
	sock->remote_addr = NULL;
	for(i = 0; i < SOCKET_MAX_OPS; i++)
		sock->ops[i].opcode = OP_FREE;
	mutex_create(&sock->lock);


	// add fd to epoll set
	if(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &sock->event) == -1){
		LOG_ERROR("net_socket: failed to register socket to epoll (error = %d)", errno);
		net_close(sock);
		return NULL;
	}

	return sock;
}

struct socket *net_server_socket(int port)
{
	return NULL;
}

unsigned long net_get_remote_address(struct socket *sock)
{
	if(sock->remote_addr == NULL) return 0;
	return sock->remote_addr->sin_addr.s_addr;
}

void net_socket_shutdown(struct socket *sock, int how)
{
	shutdown(sock->fd, how);

/*
	if(how == NET_SHUT_RD || how == NET_SHUTRDWR){
		for(i = 0; i < SOCKET_MAX_OPS; i++){
			if(sock->ops[i].opcode == OP_READ){
				//cancel op
			}
		}
	}
*/
}

void net_close(struct socket *sock)
{
	// cancel ops

	// release resource
	close(sock->fd);
	mutex_destroy(sock->lock);
	array_locked_del(sock_array, sock);
}

int net_async_accept(struct socket *sock,
		void (*fp)(struct socket*, int, int, void*), void *udata)
{
	return -1;
}
int net_async_read(struct socket *sock, char *buf, int len,
		void (*fp)(struct socket*, int, int, void*), void *udata)
{
	return -1;
}
int net_async_write(struct socket *sock, char *buf, int len,
		void (*fp)(struct socket*, int, int, void*), void *udata)
{
	return -1;
}

int net_work(void)
{
	int ret;
	struct epoll_event event;

	ret = epoll_wait(epoll_fd, event, 1, WORK_TIMEOUT);

	if(ret == 0){
		// timeout
	}
	else if(ret == -1){
		// error
	}

	return -1;
}
