#include "../network.h"

#include "../thread.h"
#include "../log.h"
#include "../array.h"

#define WIN32_MEAN_AND_LEAN 1
#include <winsock2.h>
#include <mswsock.h>

#include <stdio.h>

// windows specific handles
static LPFN_ACCEPTEX _AcceptEx;
static LPFN_GETACCEPTEXSOCKADDRS _GetAcceptExSockaddrs;
static struct WSAData wsa_data;
static HANDLE iocp = NULL;

// socket handle
struct socket{
	SOCKET fd;

	// used when accepting a new connection
	char addr_buffer[(sizeof(struct sockaddr_in) + 16) * 2];
	struct sockaddr_in *local_addr;
	struct sockaddr_in *remote_addr;
};

#define MAX_SOCKETS 4096
static struct array *sock_array = NULL;
static struct mutex *sock_lock = NULL;


// async op handle
struct async_op{
	OVERLAPPED overlapped;
	int opcode;
	struct socket *socket;
	void (*complete)(struct socket*, int, int, void*);
	void *udata;
};

#define OP_ACCEPT	0x01
#define OP_READ		0x02
#define OP_WRITE	0x03

#define MAX_ASYNC_OPS 4096
static struct array *op_array = NULL;
static struct mutex *op_lock = NULL;


// local helper functions
static int posix_error(int error)
{
	switch(error){
		case WSA_OPERATION_ABORTED:	return ECANCELED;
		case WSAECONNABORTED:		return ECONNABORTED;
		case WSAECONNRESET:		return ECONNRESET;
		case WSAENETRESET:		return ENETRESET;
		case WSAENETDOWN:		return ENETDOWN;
		case WSAENOTCONN:		return ENOTCONN;
		case WSAEWOULDBLOCK:		return EWOULDBLOCK;
		case NO_ERROR:			return 0;

		// generic error
		default:			return -1;
	}
}

static void *array_locked_new(struct array *array, struct mutex *mtx)
{
	void *ptr;

	mutex_lock(mtx);
	ptr = array_new(array);
	mutex_unlock(mtx);
	return ptr;
}

static void array_locked_del(struct array *array, struct mutex *mtx, void *ptr)
{
	mutex_lock(mtx);
	array_del(array, ptr);
	mutex_unlock(mtx);
}

static int ws_load_extensions()
{
	int ret;
	DWORD dummy;
	SOCKET fd;
	GUID guid0 = WSAID_ACCEPTEX;
	GUID guid1 = WSAID_GETACCEPTEXSOCKADDRS;

	// create dummy socket
	if((fd = socket(AF_INET, SOCK_STREAM, IPPROTO_IP)) == INVALID_SOCKET){
		LOG_ERROR("server_init: failed to create dummy socket (error = %d)", GetLastError());
		return -1;
	}

	// load AcceptEx
	ret = WSAIoctl(fd, SIO_GET_EXTENSION_FUNCTION_POINTER,
		&guid0, sizeof(GUID), &_AcceptEx, sizeof(LPFN_ACCEPTEX),
		&dummy, NULL, NULL);
	if(ret == SOCKET_ERROR){
		LOG_ERROR("server_init: failed to retrieve AcceptEx extension (error = %d)", GetLastError());
		goto err;
	}

	// load GetAcceptExSockaddrs
	ret = WSAIoctl(fd, SIO_GET_EXTENSION_FUNCTION_POINTER,
		&guid1, sizeof(GUID), &_GetAcceptExSockaddrs, sizeof(LPFN_GETACCEPTEXSOCKADDRS),
		&dummy, NULL, NULL);
	if(ret == SOCKET_ERROR){
		LOG_ERROR("server_init: failed to retrieve GetAcceptExSockaddrs extension (error = %d)", GetLastError());
		goto err;
	}

	return 0;

err:
	closesocket(fd);
	return -1;
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

	// load windows sockets extensions
	if(ws_load_extensions() != 0){
		LOG_ERROR("server_init: failed to load ws extensions");
		return -1;
	}

	// create io completion port
	iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	if(iocp == NULL){
		LOG_ERROR("server_init: failed to create completion port %d", GetLastError());
		return -1;
	}

	// memory for the socket structs
	sock_array = array_create(MAX_SOCKETS, sizeof(struct socket));
	mutex_create(&sock_lock);

	// memory for the operation structs
	op_array = array_create(MAX_ASYNC_OPS, sizeof(struct async_op));
	mutex_create(&op_lock);
	return 0;
}

void net_shutdown()
{
	// release resources
	if(sock_array != NULL){
		array_destroy(sock_array);
		sock_array = NULL;
	}
	if(sock_lock != NULL){
		mutex_destroy(sock_lock);
		sock_lock = NULL;
	}

	if(op_array != NULL){
		array_destroy(op_array);
		op_array = NULL;
	}
	if(op_lock != NULL){
		mutex_destroy(op_lock);
		op_lock = NULL;
	}

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
	if(fd == INVALID_SOCKET){
		LOG_ERROR("net_socket: failed to create socket (error = %d)", GetLastError());
		return NULL;
	}

	if(CreateIoCompletionPort((HANDLE)fd, iocp, 0, 0) != iocp){
		LOG_ERROR("net_socket: failed to register socket to completion port (error = %d)", GetLastError());
		return NULL;
	}

	sock = array_locked_new(sock_array, sock_lock);
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

	if(CreateIoCompletionPort((HANDLE)fd, iocp, 0, 0) == NULL){
		LOG_ERROR("net_server_socket: failed to register server socket to completion port (error = %d)", GetLastError());
		goto err;
	}

	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = INADDR_ANY;
	if(bind(fd, (struct sockaddr*)&addr, sizeof(struct sockaddr_in)) == SOCKET_ERROR){
		LOG_ERROR("net_server_socket: failed to bind server socket to port %d (error = %d)", port, GetLastError());
		goto err;
	}

	if(listen(fd, SOMAXCONN) == SOCKET_ERROR){
		LOG_ERROR("net_server_socket: failed to listen on server socket (error = %d)", GetLastError());
		goto err;
	}

	sock = array_locked_new(sock_array, sock_lock);
	sock->fd = fd;
	return sock;

err:
	closesocket(fd);
	return NULL;
}

void net_close(struct socket *sock)
{
	closesocket(sock->fd);
	array_locked_del(sock_array, sock_lock, sock);
}

int net_async_accept(struct socket *sock,
		void (*fp)(struct socket*, int, int, void*), void *udata)
{
	BOOL ret;
	DWORD bytes_transfered;
	DWORD error;
	struct async_op *op;

	op = array_locked_new(op_array, op_lock);
	memset(op, 0, sizeof(struct async_op));
	op->opcode = OP_ACCEPT;
	op->socket = net_socket();
	op->complete = fp;
	op->udata = udata;
	ret = _AcceptEx(sock->fd, op->socket->fd, op->socket->addr_buffer, 0,
		sizeof(struct sockaddr_in) + 16, sizeof(struct sockaddr_in) + 16,
		&bytes_transfered, (OVERLAPPED*)op);
	if(ret == FALSE){
		error = GetLastError();
		if(error != WSA_IO_PENDING){
			LOG_ERROR("net_async_accept: AcceptEx failed (error = %d)", error);
			array_locked_del(op_array, op_lock, op);
			return -1;
		}
	}
	return 0;
}

int net_async_read(struct socket *sock, char *buf, int len,
		void (*fp)(struct socket*, int, int, void*), void *udata)
{
	WSABUF data;
	DWORD bytes_transfered;
	DWORD flags;
	DWORD error;
	struct async_op *op;

	data.buf = buf;
	data.len = len;
	flags = 0;

	op = array_locked_new(op_array, op_lock);
	memset(op, 0, sizeof(struct async_op));
	op->opcode = OP_READ;
	op->socket = sock;
	op->complete = fp;
	op->udata = udata;
	if(WSARecv(sock->fd, &data, 1, &bytes_transfered, &flags, (OVERLAPPED*)op, NULL) == SOCKET_ERROR){
		error = GetLastError();
		if(error != WSA_IO_PENDING){
			LOG_ERROR("net_async_read: WSARecv failed (error = %d)", error);
			array_locked_del(sock_array, sock_lock, op);
			return -1;
		}
	}
	return 0;
}

int net_async_write(struct socket *sock, char *buf, int len,
		void (*fp)(struct socket*, int, int, void*), void *udata)
{
	WSABUF data;
	DWORD bytes_transfered;
	DWORD error;
	struct async_op *op;

	data.buf = buf;
	data.len = len;

	op = array_locked_new(op_array, op_lock);
	memset(op, 0, sizeof(struct async_op));
	op->opcode = OP_WRITE;
	op->socket = sock;
	op->complete = fp;
	op->udata = udata;
	if(WSASend(sock->fd, &data, 1, &bytes_transfered, 0, (OVERLAPPED*)op, NULL) == SOCKET_ERROR){
		error = GetLastError();
		if(error != WSA_IO_PENDING){
			LOG_ERROR("net_async_write: WSASend failed (error = %d)", error);
			array_locked_del(op_array, op_lock, op);
			return -1;
		}
	}
	return 0;
}

int net_work()
{
	DWORD bytes_transfered, error;
	ULONG_PTR completion_key;
	struct async_op *op;
	int dummy;
	BOOL ret;

	error = NO_ERROR;
	ret = GetQueuedCompletionStatus(iocp, &bytes_transfered,
		&completion_key, (OVERLAPPED **)&op, INFINITE);

	if(ret == FALSE){
		error = GetLastError();
		LOG_ERROR("net_work: GetQueuedCompletionStatus failed (error = %d)", error);
	}

	if(op != NULL){
		// retrieve remote and local addresses on accept
		if(op->opcode == OP_ACCEPT){
			_GetAcceptExSockaddrs(op->socket->addr_buffer, 0,
				sizeof(struct sockaddr_in) + 16, sizeof(struct sockaddr_in) + 16,
				(struct sockaddr**)&op->socket->local_addr, &dummy,
				(struct sockaddr**)&op->socket->remote_addr, &dummy);
		}

		op->complete(op->socket, posix_error(error), bytes_transfered, op->udata);
		array_locked_del(op_array, op_lock, op);
		return 1;
	}

	return 0;
}