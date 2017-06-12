#include "../network.h"

#include "../mmblock.h"
#include "../log.h"
#include "../thread.h"

#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#include <netinet/in.h>

#define OP_NONE		0x00
#define OP_ACCEPT	0x01
#define OP_READ		0x02
#define OP_WRITE	0x03
struct async_op{
	long		opcode;
	struct socket	*socket;
	void		*buf;
	long		len;

	int		error;
	int		transfered;
	void		(*complete)(struct socket*, int, int, void*);
	void		*udata;

	struct async_op	*next;
};

#define MAX_SOCKETS		2048
#define SOCKET_MAX_OPS		8
struct socket{
	int			fd;
	struct sockaddr		addr;
	struct async_op		ops[SOCKET_MAX_OPS];
	struct async_op		*usr_queue;
	struct async_op		*rd_queue;
	struct async_op		*wr_queue;
	struct mutex		*lock;
};

static int		kq = -1;
static struct mmblock	*sockblk = NULL;

static int setoptions(int fd)
{
	int flags;
	struct linger linger;

	// set linger
	linger.l_onoff = 0;
	linger.l_linger = 0;
	if(setsockopt(fd, SOL_SOCKET, SO_LINGER, (void *)&linger, sizeof(struct linger)) == -1){
		LOG_ERROR("setoptions: failed to set socket linger option (error = %d)", errno);
		return -1;
	}

	// set nonblocking
	flags = fcntl(fd, F_GETFL);
	if(flags == -1){
		LOG_ERROR("setoptions: failed to get socket flags (error = %d)", errno);
		return -1;
	}
	if(fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1){
		LOG_ERROR("setoptions: failed to set nonblocking mode (error = %d)", errno);
		return -1;
	}

	return 0;
}

static struct socket *socket_handle(int fd)
{
	struct socket *sock;

	// create handle
	sock = mmblock_xalloc(sockblk);
	if(sock == NULL){
		LOG_ERROR("net_socket: socket memory block is at maximum capacity (%d)", MAX_SOCKETS);
		return NULL;
	}

	// initialize handle
	sock->fd = fd;
	sock->usr_queue = NULL;
	sock->rd_queue = NULL;
	sock->wr_queue = NULL;
	//memset(&sock->addr, 0, sizeof(struct sockaddr));
	for(int i = 0; i < SOCKET_MAX_OPS; i++)
		sock->ops[i].opcode = OP_NONE;
	mutex_create(&sock->lock);
	return sock;
}

static void socket_release(struct socket *sock)
{
	close(sock->fd);
	mutex_destroy(sock->lock);
	mmblock_xfree(sockblk, sock);
}

static inline
void trigger_usr(struct socket *sock)
{
	struct kevent event = {
		.ident	= sock->fd,
		.filter	= EVFILT_USER,
		.flags	= 0,
		.fflags	= NOTE_TRIGGER,
		.data	= 0,
		.udata	= sock,
	};

	if(kevent(kq, &event, 1, NULL, 0, NULL) == -1)
		LOG_ERROR("trigger_usr: failed to trigger user event (error = %d)", errno);
}

static void cancel_rd_ops(struct socket *sock)
{
	struct async_op **op;
	mutex_lock(sock->lock);
	// get usr_queue tail
	op = &sock->usr_queue;
	while(*op != NULL)
		op = &(*op)->next;

	// push rd_queue into usr_queue
	*op = sock->rd_queue;
	sock->rd_queue = NULL;

	// cancel operations
	while(*op != NULL){
		(*op)->error = ECANCELED;
		op = &(*op)->next;
	}
	mutex_unlock(sock->lock);
}

static void cancel_wr_ops(struct socket *sock)
{
	struct async_op **op;
	mutex_lock(sock->lock);
	// get usr_queue tail
	op = &sock->usr_queue;
	while(*op != NULL)
		op = &(*op)->next;

	// push wr_queue into usr_queue
	*op = sock->wr_queue;
	sock->wr_queue = NULL;

	// cancel operations
	while(*op != NULL){
		(*op)->error = ECANCELED;
		op = &(*op)->next;
	}
	mutex_unlock(sock->lock);
}

// NOTE: must be used INSIDE the socket lock
static struct async_op *socket_op(struct socket *sock, int opcode)
{
	struct async_op *op = NULL;
	for(int i = 0; i < SOCKET_MAX_OPS; i++){
		if(sock->ops[i].opcode == OP_NONE){
			op = &sock->ops[i];
			op->opcode = opcode;
			op->next = NULL;
			break;
		}
	}
	return op;
}

// NOTE: must be used INSIDE the socket lock
static void defer_completion(struct socket *sock, struct async_op *op)
{
	struct async_op **it;

	// trigger user event if this is the first op
	// (it doesn't matter the order here because of the lock)
	if(sock->usr_queue == NULL)
		trigger_usr(sock);

	it = &sock->usr_queue;
	while(*it != NULL)
		it = &(*it)->next;
	*it = op;
}

// NOTE: must be used INSIDE the socket lock
static int try_complete_accept(struct socket *sock, struct async_op *op)
{
	int fd, error;
	struct kevent events[3];
	struct sockaddr addr;
	socklen_t addrlen;

	addrlen = sizeof(struct sockaddr);
	fd = accept(sock->fd, &addr, &addrlen);
	if(fd == -1){
		error = errno;
		if(error == EWOULDBLOCK)
			return -1;
		op->error = error;
		return 0;
	}

	// set socket options
	if(setoptions(fd) == -1){
		LOG_ERROR("try_complete_accept: failed to set new socket options");
		close(fd);
		op->error = EAGAIN;
		return 0;
	}

	op->socket = socket_handle(fd);
	if(op->socket == NULL){
		LOG_ERROR("try_complete_accept: failed to create new socket handle");
		close(fd);
		op->error = EAGAIN;
		return 0;
	}

	// copy address to socket
	memcpy(&op->socket->addr, &addr, sizeof(struct sockaddr));

	// add new socket to kqueue
	EV_SET(events+0, fd, EVFILT_USER, EV_ADD | EV_CLEAR, 0, 0, op->socket);
	EV_SET(events+1, fd, EVFILT_READ, EV_ADD | EV_CLEAR, 0, 0, op->socket);
	EV_SET(events+2, fd, EVFILT_WRITE, EV_ADD | EV_CLEAR, 0, 0, op->socket);
	if(kevent(kq, events, 3, NULL, 0, NULL) == -1){
		LOG_ERROR("try_complete_accept: failed to add new socket to kqueue (error = %d)", errno);
		socket_release(op->socket);
		op->socket = NULL;
		op->error = EAGAIN;
	}
	return 0;
}

// NOTE: must be used INSIDE the socket lock
static int try_complete(struct socket *sock, struct async_op *op)
{
	int ret, error;
	while(op->len > 0){
		if(op->opcode == OP_READ)
			ret = recv(sock->fd, op->buf, op->len, 0);
		else /*if(op->opcode == OP_WRITE)*/
			ret = send(sock->fd, op->buf, op->len, 0);

		if(ret == -1){
			error = errno;
			if(error == EWOULDBLOCK)
				return -1;
			op->error = error;
			return 0;
		}
		else if(ret == 0){
			// connection closed by peer
			op->transfered = 0;
			//op->error = ECONNRESET;
			return 0;
		}
		op->buf += ret;
		op->len -= ret;
		op->transfered += ret;
	}
	return 0;
}

int net_init(void)
{
	// create kqueue
	if((kq = kqueue()) == -1){
		LOG_ERROR("net_init: failed to create kqueue (error = %d)", errno);
		return -1;
	}

	// create socket memory block
	sockblk = mmblock_create(MAX_SOCKETS, sizeof(struct socket));
	mmblock_init_lock(sockblk);
	return 0;
}

void net_shutdown(void)
{
	if(sockblk != NULL){
		mmblock_release(sockblk);
		sockblk = NULL;
	}

	if(kq != -1){
		close(kq);
		kq = -1;
	}
}

struct socket *net_socket(void)
{
	int fd;
	struct kevent events[3];
	struct socket *sock;

	fd = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
	if(fd == -1){
		LOG_ERROR("net_socket: failed to create socket (error = %d)", errno);
		return NULL;
	}

	if(setoptions(fd) == -1){
		LOG_ERROR("net_socket: failed to set socket options");
		close(fd);
		return NULL;
	}

	sock = socket_handle(fd);
	if(sock == NULL){
		LOG_ERROR("net_socket: failed to create socket handle");
		close(fd);
		return NULL;
	}

	// add socket to kqueue
	EV_SET(events+0, fd, EVFILT_USER, EV_ADD | EV_CLEAR, 0, 0, sock);
	EV_SET(events+1, fd, EVFILT_READ, EV_ADD | EV_CLEAR, 0, 0, sock);
	EV_SET(events+2, fd, EVFILT_WRITE, EV_ADD | EV_CLEAR, 0, 0, sock);
	if(kevent(kq, events, 3, NULL, 0, NULL) == -1){
		LOG_ERROR("net_socket: failed to add socket to kqueue (error = %d)", errno);
		socket_release(sock);
		return NULL;
	}

	return sock;
}

struct socket *net_server_socket(int port)
{
	int fd;
	struct socket *sock;
	struct kevent events[2];
	struct sockaddr_in addr;

	fd = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
	if(fd == -1){
		LOG_ERROR("net_server_socket: failed to create socket (error = %d)", errno);
		return NULL;
	}

	if(setoptions(fd) == -1){
		LOG_ERROR("net_server_socket: failed to initialize socket");
		close(fd);
		return NULL;
	}

	// bind to port
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = INADDR_ANY;
	if(bind(fd, (struct sockaddr*)&addr, sizeof(struct sockaddr_in)) == -1){
		LOG_ERROR("net_server_socket: failed to bind socket to port %d (error = %d)", port, errno);
		close(fd);
		return NULL;
	}

	// listen
	if(listen(fd, SOMAXCONN) == -1){
		LOG_ERROR("net_server_socket: failed to listen to port %d (error = %d)", port, errno);
		close(fd);
		return NULL;
	}

	// create socket handle
	sock = socket_handle(fd);
	if(sock == NULL){
		LOG_ERROR("net_server_socket: failed to create socket handle");
		close(fd);
		return NULL;
	}

	// add socket to epoll
	EV_SET(events+0, fd, EVFILT_USER, EV_ADD | EV_CLEAR, 0, 0, sock);
	EV_SET(events+1, fd, EVFILT_READ, EV_ADD | EV_CLEAR, 0, 0, sock);
	if(kevent(kq, events, 2, NULL, 0, NULL) == -1){
		LOG_ERROR("net_server_socket: failed to add socket to kqueue (error = %d)", errno);
		socket_release(sock);
		return NULL;
	}
	return sock;
}

void net_socket_shutdown(struct socket *sock, int how)
{
	shutdown(sock->fd, how);
	if(how == NET_SHUT_RD || how == NET_SHUT_RDWR)
		cancel_rd_ops(sock);
	if(how == NET_SHUT_WR || how == NET_SHUT_RDWR)
		cancel_wr_ops(sock);

	trigger_usr(sock);
}

static void release_operation(struct socket *sock, int error, int transfered, void *udata)
{
	LOG("release operation");
	socket_release(sock);
}

void net_close(struct socket *sock)
{
	struct async_op *op;
	struct kevent events[2];

	if(sock == NULL) return;

	// NOTE: after calling net_close, the socket
	// will be invalid and further calls to async_*
	// will have undefined behaviour (probably crash)

	// remove read, write filters
	EV_SET(events+0, sock->fd, EVFILT_READ, EV_DELETE, 0, 0, NULL);
	EV_SET(events+1, sock->fd, EVFILT_WRITE, EV_DELETE, 0, 0, NULL);
	if(kevent(kq, events, 2, NULL, 0, NULL) == -1)
		LOG_ERROR("net_close: failed to remove socket from kqueue (error = %d)", errno);

	// cancel queued operations
	cancel_rd_ops(sock);
	cancel_wr_ops(sock);

	// adding the release operation last to the deferred list
	// will make so every previous operation will be completed
	// before actually releasing the socket resources
	mutex_lock(sock->lock);
	op = socket_op(sock, OP_READ);
	op->socket = sock;
	op->complete = release_operation;
	defer_completion(sock, op);
	trigger_usr(sock);
	mutex_unlock(sock->lock);
}

int net_async_accept(struct socket *sock,
		void (*fp)(struct socket*, int, int, void*), void *udata)
{
	struct async_op *op, **it;

	mutex_lock(sock->lock);
	op = socket_op(sock, OP_ACCEPT);
	if(op == NULL){
		mutex_unlock(sock->lock);
		LOG_ERROR("net_async_accept: maximum simultaneous operations reached (%d)", SOCKET_MAX_OPS);
		return -1;
	}
	op->socket = NULL;
	op->error = 0;
	op->complete = fp;
	op->udata = udata;

	if(sock->rd_queue == NULL){
		if(try_complete_accept(sock, op) == 0)
			defer_completion(sock, op);
		else
			sock->rd_queue = op;
	}
	else{
		it = &sock->rd_queue;
		while(*it != NULL)
			it = &(*it)->next;
		*it = op;
	}
	mutex_unlock(sock->lock);
	return 0;
}

int net_async_read(struct socket *sock, char *buf, int len,
		void (*fp)(struct socket*, int, int, void*), void *udata)
{
	struct async_op *op, **it;

	mutex_lock(sock->lock);
	op = socket_op(sock, OP_READ);
	if(op == NULL){
		mutex_unlock(sock->lock);
		LOG_ERROR("net_async_read: maximum simultaneous operations reached (%d)", SOCKET_MAX_OPS);
		return -1;
	}
	op->socket = sock;
	op->buf = buf;
	op->len = len;
	op->error = 0;
	op->transfered = 0;
	op->complete = fp;
	op->udata = udata;

	// if there are no pending read operations, we may try to
	// complete the read operation now
	if(sock->rd_queue == NULL){
		// if the operation completed, defer the completion
		// routine to net_work, else add it to the read queue
		// so it will be completed when the socket is ready
		// to read
		if(try_complete(sock, op) == 0)
			defer_completion(sock, op);
		else
			sock->rd_queue = op;
	}
	else{
		// insert into read queue tail
		it = &sock->rd_queue;
		while(*it != NULL)
			it = &(*it)->next;
		*it = op;
	}
	mutex_unlock(sock->lock);
	return 0;
}
int net_async_write(struct socket *sock, char *buf, int len,
		void (*fp)(struct socket*, int, int, void*), void *udata)
{
	struct async_op *op, **it;

	mutex_lock(sock->lock);
	op = socket_op(sock, OP_WRITE);
	if(op == NULL){
		mutex_unlock(sock->lock);
		LOG_ERROR("net_async_write: maximum simultaneous operations reached (%d)", SOCKET_MAX_OPS);
		return -1;
	}
	op->socket = sock;
	op->buf = buf;
	op->len = len;
	op->error = 0;
	op->transfered = 0;
	op->complete = fp;
	op->udata = udata;

	if(sock->wr_queue == NULL){
		if(try_complete(sock, op) == 0)
			defer_completion(sock, op);
		else
			sock->wr_queue = op;
	}
	else{
		// insert into write queue tail
		it = &sock->wr_queue;
		while(*it != NULL)
			it = &(*it)->next;
		*it = op;
	}
	mutex_unlock(sock->lock);
	return 0;
}

int net_work(void)
{
	// set the kevent timeout to 1 sec
	static struct timespec timeout = {
		.tv_sec		= (NET_WORK_TIMEOUT / 1000),
		.tv_nsec	= (NET_WORK_TIMEOUT % 1000) * 1000000,
	};

	int ret;
	struct kevent events[64];
	struct socket *sock;
	struct async_op *op;

	// retrieve events
	ret = kevent(kq, NULL, 0, events, 64, &timeout);
	if(ret == -1){
		LOG_ERROR("net_work: kevent failed (error = %d)", errno);
		return -1;
	}

	// process events
	for(int i = 0; i < ret; i++){
		sock = events[i].udata;

		// user event (complete deferred operations)
		if(events[i].filter == EVFILT_USER){
			while(1){
				mutex_lock(sock->lock);
				if((op = sock->usr_queue) == NULL){
					mutex_unlock(sock->lock);
					break;
				}
				sock->usr_queue = op->next;
				mutex_unlock(sock->lock);

				// complete and release op
				op->complete(op->socket, op->error, op->transfered, op->udata);
				op->opcode = OP_NONE;
			}
		}

		// socket ready to read
		else if(events[i].filter == EVFILT_READ){
			while(1){
				mutex_lock(sock->lock);
				// get next operation from the queue
				if((op = sock->rd_queue) == NULL){
					mutex_unlock(sock->lock);
					break;
				}

				// try to complete the operation
				if(op->opcode == OP_ACCEPT)
					ret = try_complete_accept(sock, op);
				else /*if(op->opcode == OP_READ)*/
					ret = try_complete(sock, op);

				// if operation is not ready for completion,
				// break the loop
				if(ret == -1){
					mutex_unlock(sock->lock);
					break;
				}

				// advance read queue if the operation completed
				sock->rd_queue = op->next;
				mutex_unlock(sock->lock);

				// complete and release op
				op->complete(op->socket, op->error, op->transfered, op->udata);
				op->opcode = OP_NONE;
			}
		}

		// socket ready to write
		else if(events[i].filter == EVFILT_WRITE){
			while(1){
				mutex_lock(sock->lock);
				// get next operation from the queue
				if((op = sock->wr_queue) == NULL){
					mutex_unlock(sock->lock);
					break;
				}

				// try to complete write
				ret = try_complete(sock, op);

				// if operation is not ready for completion,
				// break the loop
				if(ret == -1){
					mutex_unlock(sock->lock);
					break;
				}

				// advance write queue if the operation completed
				sock->wr_queue = op->next;
				mutex_unlock(sock->lock);

				// complete and release op
				op->complete(op->socket, op->error, op->transfered, op->udata);
				op->opcode = OP_NONE;
			}
		}
	}

	return 0;
}

unsigned long net_remote_address(struct socket *sock)
{
	if(sock->addr.sa_family != AF_INET) return 0;
	return ((struct sockaddr_in*)&sock->addr)->sin_addr.s_addr;
}
