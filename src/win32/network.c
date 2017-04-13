#include "../thread.h"
#include "../log.h"
#include "../mem.h"

#define WIN32_MEAN_AND_LEAN 1
#include <winsock2.h>
#include <mswsock.h>

#include <stdio.h>

// AcceptEx
// WSARecv
// WSASend
// GetAcceptExSockaddrs
// GetQueuedCompletionStatus
// PostQueuedCompletionStatus

// windows specific handles
static struct WSAData wsa_data;
static HANDLE iocp;

// socket handle
struct socket{
	SOCKET fd;

	// used when accepting a new connection
	char addr_buffer[(sizeof(struct sockaddr_in) + 16) * 2];
	struct sockaddr_in *local_addr;
	struct sockaddr_in *remote_addr;
};

#define MAX_SOCKETS 4096
static struct mem_pool *sock_pool;
static struct mutex *sock_lock;


// async op handle
static struct async_op{
	OVERLAPPED overlapped;
	struct socket *socket;
	void (*complete)(int, int, void*);
	void *udata;
};

#define MAX_ASYNC_OPS 4096
static struct mem_pool *op_pool;
static struct mutex *op_lock;


// local helper functions
static void *locked_alloc(struct mem_pool *pool, struct mutex *mtx)
{
	void *ptr;
	mutex_lock(mtx);
	ptr = mem_pool_alloc(pool);
	mutex_unlock(mtx);
	return ptr;
}

static void locked_free(struct mem_pool *pool, struct mutex *mtx, void *ptr)
{
	mutex_lock(mtx);
	mem_pool_free(pool, ptr);
	mutex_unlock(mtx);
}

int net_init()
{
	int ret;

	// initialize WSA
	ret = WSAStartup(MAKEWORD(2, 2), &wsa_data);
	if(ret != 0){
		LOG_ERROR("server_init: WSAStartup failed with error %d", ret);
		return -1;
	}

	// check version
	if(LOBYTE(wsa_data.wVersion) != 2 || HIBYTE(wsa_data.wVersion) != 2){
		LOG_ERROR("server_init: WSA version error");
		return -1;
	}

	// create io completion port
	iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	if(iocp == NULL){
		LOG_ERROR("server_init: failed to create completion port %d", WSAGetLastError());
		return -1;
	}

	// memory for the socket structs
	mem_create_pool(MAX_SOCKETS, sizeof(struct socket), &sock_pool);
	mutex_create(&sock_lock);

	// memory for the operation structs
	mem_create_pool(MAX_ASYNC_OPS, sizeof(struct async_op), &op_pool);
	mutex_create(&op_lock);
	return 0;
}

void net_shutdown()
{
	// release resources
	mem_destroy_pool(sock_pool);
	mutex_destroy(sock_lock);

	mem_destroy_pool(op_pool);
	mutex_destroy(op_lock);

	// close iocp
	if(iocp != NULL){
		CloseHandle(iocp);
		iocp = NULL;
	}

	WSACleanup();
}

struct socket *net_socket()
{
	SOCKET fd;
	struct socket *sock;

	fd = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_IP, NULL, 0, WSA_FLAG_OVERLAPPED);
	if(fd == INVALID_SOCKET)
		return NULL;

	sock = locked_alloc(sock_pool, sock_lock);
	sock->fd = fd;
	return sock;
}

struct socket *net_server_socket(int port)
{
	SOCKET fd;
	struct sockaddr_in addr;
	struct socket *sock;

	fd = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_IP, NULL, 0, WSA_FLAG_OVERLAPPED);
	if(fd == INVALID_SOCKET){
		LOG_ERROR("net_server_socket: failed to create socket (error = %d)", GetLastError());
		goto err;
	}

	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = INADDR_ANY;
	if(bind(sock->fd, (struct sockaddr*)&addr, sizeof(struct sockaddr_in)) == SOCKET_ERROR){
		LOG_ERROR("net_server_socket: failed to bind server socket to port %d (error = %d)", port, GetLastError());
		goto err;
	}

	if(listen(sock->fd, SOMAXCONN) == SOCKET_ERROR){
		LOG_ERROR("net_server_socket: failed to listen on server socket (error = %d)", GetLastError());
		goto err;
	}

	sock = locked_alloc(sock_pool, sock_lock);
	sock->fd = fd;
	return sock;

err:
	closesocket(sock->fd);
	return NULL;
}

void net_close(struct socket *sock)
{
	closesocket(sock->fd);
	locked_free(sock_pool, sock_lock, sock);
}

int net_async_accept(struct socket *sock, struct socket **new_sock,
		void (*fp)(int, int, void*), void *udata)
{
	AcceptEx(sock->fd, (*new_sock)->fd, (*new_sock)->addr_buffer, 0, sizeof(struct sockaddr_in) + 16,
		sizeof(struct sockaddr_in) + 16, NULL, NULL);
}

int net_async_read(struct socket *sock, char *buf, int len,
		void (*fp)(int, int, void*), void *udata)
{
	WSABUF data;
	DWORD bytes_transfered;
	DWORD flags;
	DWORD error;
	struct async_op *op;

	data.buf = buf;
	data.len = len;
	flags = 0;

	op = op_alloc();
	memset(op, 0, sizeof(struct async_op));
	op->complete = fp;
	op->udata = udata;
	if(WSARecv(sock->fd, &data, 1, &bytes_transfered, &flags, op, NULL) == SOCKET_ERROR){
		error = GetLastError();
		if(error != WSA_IO_PENDING){
			op_free(op);
			return 0;
		}
	}
	return 1;
}

int net_async_write(struct socket *sock, char *buf, int len,
		void (*fp)(int, int, void*), void *udata)
{
	WSABUF data;
	DWORD bytes_transfered;
	DWORD error;
	struct async_op *op;

	data.buf = buf;
	data.len = len;

	op = locked_alloc(op_pool, op_lock);
	memset(op, 0, sizeof(struct async_op));
	op->complete = fp;
	op->udata = udata;
	if(WSASend(sock->fd, &data, 1, &bytes_transfered, 0, op, NULL) == SOCKET_ERROR){
		error = GetLastError();
		if(error != WSA_IO_PENDING){
			op_free(op);
			return 0;
		}
	}
	return 1;
}

int net_work()
{
	DWORD bytes_transfered, error;
	ULONG_PTR completion_key;
	struct async_op *op;
	BOOL ret;

	ret = GetQueuedCompletionStatus(iocp, &bytes_transfered,
		&completion_key, (OVERLAPPED **)&op, 0);
	error = GetLastError();

	if(ret == FALSE)
		LOG_ERROR("net_work: failed to dequeue work from the completion port (%d)", error);

	if(op != NULL){
		op->complete(error, bytes_transfered, op->udata);
		locked_free(op_mem, op_lock, op);
		return 1;
	}

	return 0;
}