#include "../network.h"

#include "../thread.h"
#include "../log.h"
#include "../mmblock.h"

#define WIN32_MEAN_AND_LEAN 1
#include <winsock2.h>
#include <mswsock.h>

#include <stdio.h>

#define OP_NONE		0x00
#define OP_ACCEPT	0x01
#define OP_READ		0x02
#define OP_WRITE	0x03
struct async_op{
	OVERLAPPED	overlapped;
	int		opcode;
	struct socket	*socket;
	void		(*complete)(struct socket*, int, int, void*);
	void		*udata;
};

#define MAX_SOCKETS 2048
#define SOCKET_MAX_OPS 8
struct socket{
	SOCKET			fd;
	struct async_op		ops[SOCKET_MAX_OPS];
	struct mutex		*lock;
	char			addr_buffer[(sizeof(struct sockaddr_in) + 16) * 2];
	struct sockaddr_in	*local_addr;
	struct sockaddr_in	*remote_addr;
};


// windows specific handles
static LPFN_ACCEPTEX			_AcceptEx;
static LPFN_GETACCEPTEXSOCKADDRS	_GetAcceptExSockaddrs;
static struct WSAData	wsa_data;
static HANDLE		iocp = NULL;
static struct mmblock	*sockblk = NULL;


static int posix_error(int error)
{
	switch(error){
		case WSA_OPERATION_ABORTED:	return ECANCELED;
		case ERROR_NETNAME_DELETED:
		case ERROR_CONNECTION_ABORTED:
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

static int load_extensions()
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
		closesocket(fd);
		return -1;
	}

	// load GetAcceptExSockaddrs
	ret = WSAIoctl(fd, SIO_GET_EXTENSION_FUNCTION_POINTER,
		&guid1, sizeof(GUID), &_GetAcceptExSockaddrs, sizeof(LPFN_GETACCEPTEXSOCKADDRS),
		&dummy, NULL, NULL);
	if(ret == SOCKET_ERROR){
		LOG_ERROR("server_init: failed to retrieve GetAcceptExSockaddrs extension (error = %d)", GetLastError());
		closesocket(fd);
		return -1;
	}

	closesocket(fd);
	return 0;
}

// NOTE: must be used OUTSIDE the socket lock
static struct async_op *socket_op(struct socket *sock, int opcode)
{
	struct async_op *op = NULL;
	mutex_lock(sock->lock);
	for(int i = 0; i < SOCKET_MAX_OPS; i++){
		if(sock->ops[i].opcode == OP_NONE){
			op = &sock->ops[i];
			op->opcode = opcode;
			break;
		}
	}
	mutex_unlock(sock->lock);
	return op;
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
	if(load_extensions() != 0){
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
	sockblk = mmblock_create(MAX_SOCKETS, sizeof(struct socket));
	mmblock_init_lock(sockblk);
	return 0;
}

void net_shutdown()
{
	// release resources
	if(sockblk != NULL){
		mmblock_release(sockblk);
		sockblk = NULL;
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
		closesocket(fd);
		return NULL;
	}

	sock = mmblock_xalloc(sockblk);
	if(sock == NULL){
		LOG_ERROR("net_socket: socket memory block is at maximum capacity (%d)", MAX_SOCKETS);
		closesocket(fd);
		return NULL;
	}

	// initialize socket
	sock->fd = fd;
	sock->local_addr = NULL;
	sock->remote_addr = NULL;
	for(int i = 0; i < SOCKET_MAX_OPS; i++)
		sock->ops[i].opcode = OP_NONE;
	mutex_create(&sock->lock);
	return sock;
}

struct socket *net_server_socket(int port)
{
	struct sockaddr_in addr;
	struct socket *sock;

	// create socket
	sock = net_socket();
	if(sock == NULL){
		LOG_ERROR("net_server_socket: failed to create socket");
		return NULL;
	}

	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = INADDR_ANY;
	if(bind(sock->fd, (struct sockaddr*)&addr, sizeof(struct sockaddr_in)) == SOCKET_ERROR){
		LOG_ERROR("net_server_socket: failed to bind server socket to port %d (error = %d)", port, GetLastError());
		net_close(sock);
		return NULL;
	}

	if(listen(sock->fd, SOMAXCONN) == SOCKET_ERROR){
		LOG_ERROR("net_server_socket: failed to listen to port %d (error = %d)", port, GetLastError());
		net_close(sock);
		return NULL;
	}
	return sock;
}

void net_socket_shutdown(struct socket *sock, int how)
{
	struct async_op *op;

	shutdown(sock->fd, how);
	for(int i = 0; i < SOCKET_MAX_OPS; i++){
		// operations will be canceled here but
		// will be released by the work function
		op = &sock->ops[i];
		if((how == NET_SHUT_RDWR && op->opcode != OP_NONE)
			|| (how == NET_SHUT_RD && op->opcode == OP_READ)
			|| (how == NET_SHUT_WR && op->opcode == OP_WRITE))
			CancelIoEx((HANDLE)sock->fd, (OVERLAPPED*)op);
	}
}

void net_close(struct socket *sock)
{
	// cancel any pending io operations on the socket
	CancelIoEx((HANDLE)sock->fd, NULL);

	// release socket resources
	closesocket(sock->fd);
	mutex_destroy(sock->lock);
	mmblock_xfree(sockblk, sock);
}

int net_async_accept(struct socket *sock,
		void (*fp)(struct socket*, int, int, void*), void *udata)
{
	BOOL ret;
	DWORD transfered;
	DWORD error;
	struct async_op *op;

	op = socket_op(sock, OP_ACCEPT);
	if(op == NULL){
		LOG_ERROR("net_async_accept: maximum simultaneous operations reached (%d)", SOCKET_MAX_OPS);
		return -1;
	}
	memset(&op->overlapped, 0, sizeof(OVERLAPPED));
	op->socket = net_socket();
	op->complete = fp;
	op->udata = udata;
	ret = _AcceptEx(sock->fd, op->socket->fd, op->socket->addr_buffer, 0,
		sizeof(struct sockaddr_in) + 16, sizeof(struct sockaddr_in) + 16,
		&transfered, (OVERLAPPED*)op);
	if(ret == FALSE){
		error = GetLastError();
		if(error != WSA_IO_PENDING){
			LOG_ERROR("net_async_accept: AcceptEx failed (error = %d)", error);
			op->opcode = OP_NONE;
			return -1;
		}
	}
	return 0;
}

int net_async_read(struct socket *sock, char *buf, int len,
		void (*fp)(struct socket*, int, int, void*), void *udata)
{
	WSABUF data;
	DWORD transfered;
	DWORD flags;
	DWORD error;
	struct async_op *op;

	data.buf = buf;
	data.len = len;
	flags = 0;

	op = socket_op(sock, OP_READ);
	if(op == NULL){
		LOG_ERROR("net_async_read: maximum simultaneous operations reached (%d)", SOCKET_MAX_OPS);
		return -1;
	}
	memset(&op->overlapped, 0, sizeof(OVERLAPPED));
	op->socket = sock;
	op->complete = fp;
	op->udata = udata;
	if(WSARecv(sock->fd, &data, 1, &transfered, &flags, (OVERLAPPED*)op, NULL) == SOCKET_ERROR){
		error = GetLastError();
		if(error != WSA_IO_PENDING){
			LOG_ERROR("net_async_read: WSARecv failed (error = %d)", error);
			op->opcode = OP_NONE;
			return -1;
		}
	}
	return 0;
}

int net_async_write(struct socket *sock, char *buf, int len,
		void (*fp)(struct socket*, int, int, void*), void *udata)
{
	WSABUF data;
	DWORD transfered;
	DWORD error;
	struct async_op *op;

	data.buf = buf;
	data.len = len;

	op = socket_op(sock, OP_WRITE);
	if(op == NULL){
		LOG_ERROR("net_async_write: maximum simultaneous operations reached (%d)", SOCKET_MAX_OPS);
		return -1;
	}
	memset(&op->overlapped, 0, sizeof(OVERLAPPED));
	op->socket = sock;
	op->complete = fp;
	op->udata = udata;
	if(WSASend(sock->fd, &data, 1, &transfered, 0, (OVERLAPPED*)op, NULL) == SOCKET_ERROR){
		error = GetLastError();
		if(error != WSA_IO_PENDING){
			LOG_ERROR("net_async_write: WSASend failed (error = %d)", error);
			op->opcode = OP_NONE;
			return -1;
		}
	}
	return 0;
}

int net_work()
{
	DWORD transfered, error;
	ULONG_PTR completion_key;
	struct async_op *op;
	int dummy;
	BOOL ret;

	error = NO_ERROR;
	ret = GetQueuedCompletionStatus(iocp, &transfered,
		&completion_key, (OVERLAPPED **)&op, NET_WORK_TIMEOUT);

	if(ret == FALSE){
		error = GetLastError();
		LOG_ERROR("net_work: failed to retrieve queued status (error = %d)", error);
	}

	if(op != NULL){
		// retrieve remote and local addresses on accept
		if(op->opcode == OP_ACCEPT){
			_GetAcceptExSockaddrs(op->socket->addr_buffer, 0,
				sizeof(struct sockaddr_in) + 16, sizeof(struct sockaddr_in) + 16,
				(struct sockaddr**)&op->socket->local_addr, &dummy,
				(struct sockaddr**)&op->socket->remote_addr, &dummy);
		}

		op->complete(op->socket, posix_error(error), transfered, op->udata);
		op->opcode = OP_NONE;
		return 1;
	}

	return 0;
}

unsigned long net_remote_address(struct socket *sock)
{
	if(sock->remote_addr == NULL) return 0;
	return sock->remote_addr->sin_addr.s_addr;
}
