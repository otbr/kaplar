#include "connection.h"

#include "message.h"
#include "array.h"
#include "server.h"
#include "network.h"
#include "thread.h"


#define CONNECTION_OPEN		0x00
#define CONNECTION_CLOSED	0x01
#define CONNECTION_CLOSING	0x02
#define CONNECTION_READ_DONE	0x04
#define CONNECTION_WRITE_DONE	0x08

#define MAX_OUTPUT 32
struct connection{
	struct socket		*sock;
	long			flags;
	struct message		input;
	struct message		output[MAX_OUTPUT];
	struct message		*output_queue;
	struct mutex		*lock;

	struct protocol		*protocol;
	void			*handle;

	struct connection	*next;
};

#define MAX_CONNECTIONS 4096
static struct array		*conn_array;
static struct connection	*conn_list;
static struct mutex		*lock;


// async op callbacks
static void on_read_header(struct socket *sock, int error, int bytes_transfered, void *udata);
static void on_read_body(struct socket *sock, int error, int bytes_transfered, void *udata);
static void on_write(struct socket *sock, int error, int bytes_transfered, void *udata);

static void on_read_header(struct socket *sock, int error, int bytes_transfered, void *udata)
{
	struct connection *conn = udata;
	struct message *msg = &conn->input;

	mutex_lock(conn->lock);
	// check for errors, if message length is greater than the limit
	// or if the connection is closed/closing
	message_decode_length(msg);
	if((conn->flags & (CONNECTION_CLOSED | CONNECTION_CLOSING)) != 0
			|| error != 0 || msg->length > MESSAGE_BUFFER_LEN)
		goto close;

	// chain the body read and close connection in case of error
	if(net_async_read(sock, msg->buffer+2, msg->length-2, on_read_body, conn) != 0)
		goto close;

	mutex_unlock(conn->lock);
	return;

close:
	conn->flags |= CONNECTION_READ_DONE;
	mutex_unlock(conn->lock);
	connection_close(conn);
}

static void on_read_body(struct socket *sock, int error, int bytes_transfered, void *udata)
{
	struct connection *conn = udata;
	struct message *msg = &conn->input;
	void *handle = conn->handle;

	mutex_lock(conn->lock);
	// check for errors or if the connection is closed/closing
	if((conn->flags & (CONNECTION_CLOSED | CONNECTION_CLOSING)) != 0
			|| error != 0)
		goto close;

	// check if it's the first message
	if(handle == NULL){
		// get protocol identifier
		// conn->protocol->on_recv_first_message(handle, msg);
	}
	else{
		conn->protocol->on_recv_message(handle, msg);
	}

	// chain the header read and close connection in case of error
	if(net_async_read(sock, msg->buffer, 2, on_read_header, conn) != 0)
		goto close;

	mutex_unlock(conn->lock);
	return;

close:
	conn->flags |= CONNECTION_READ_DONE;
	mutex_unlock(conn->lock);
	connection_close(conn);
}

static void on_write(struct socket *sock, int error, int bytes_transfered, void *udata)
{
	struct connection *conn = udata;
	struct message *msg, *next;

	mutex_lock(conn->lock);
	// check for errors or if the connection is closed
	if((conn->flags & CONNECTION_CLOSED) != 0 || error != 0)
		goto close;

	// pop message from queue and free it
	msg = conn->output_queue;
	next = msg->next;
	conn->output_queue = next;
	msg->state = MESSAGE_FREE;

	// chain an async write to the next output message if any
	if(next != NULL){
		if(net_async_write(conn->sock, next->buffer, next->length,
				on_write, conn) != 0)
			goto close;
	}
	// if connection is closing and there is no more messages
	// to send we can close it
	else if((conn->flags & CONNECTION_CLOSING) != 0){
		goto close;
	}

	mutex_unlock(conn->lock);
	return;

close:
	conn->flags |= CONNECTION_WRITE_DONE;
	mutex_unlock(conn->lock);
	connection_close(conn);
}

// connection functions
void connection_init()
{
	conn_array = array_create(MAX_CONNECTIONS, sizeof(struct connection));
	conn_list = NULL;
	mutex_create(&lock);
}

void connection_shutdown()
{
	struct connection *it, *tmp;

	it = conn_list;
	while(it != NULL){
		tmp = it;
		it = it->next;
		net_close(tmp->sock);
		mutex_destroy(tmp->lock);
		array_locked_del(conn_array, tmp);
	}

	array_destroy(conn_array);
}

void connection_accept(struct socket *sock, struct protocol *protocol)
{
	struct connection *conn;

	// allocate new connection and add it to the connection list
	mutex_lock(lock);
	conn = array_new(conn_array);
	conn->next = conn_list;
	conn_list = conn;
	mutex_unlock(lock);

	// initialize connection values
	conn->sock = sock;
	conn->flags = CONNECTION_OPEN;
	conn->output_queue = NULL;
	conn->protocol = protocol;
	conn->handle = NULL;

	// set connection message states
	conn->input.state = MESSAGE_BUSY;
	for(int i = 0; i < MAX_OUTPUT; i++)
		conn->output[i].state = MESSAGE_FREE;

	// create connection lock
	mutex_create(&conn->lock);

	// if the protocol needs to send first, it will be
	// alone and we need to create the handle now
	if(protocol->flags & PROTOCOL_SENDS_FIRST){
		conn->handle = protocol->create_handle(conn);
		protocol->on_connect(conn->handle);
	}

	// start connection read loop
	if(net_async_read(sock, conn->input.buffer, 2, on_read_header, conn) != 0){
		conn->flags |= CONNECTION_READ_DONE;
		connection_close(conn);
	}
}

struct message *connection_get_output_message(struct connection *conn)
{
	struct message *msg;
	int i;

	msg = NULL;
	mutex_lock(conn->lock);
	for(i = 0; i < MAX_OUTPUT; i++){
		if(conn->output[i].state == MESSAGE_FREE){
			msg = &conn->output[i];
			msg->state = MESSAGE_BUSY;
			break;
		}
	}
	mutex_unlock(conn->lock);
	return msg;
}

void connection_send(struct connection *conn, struct message *msg)
{
	struct message **it;

	mutex_lock(conn->lock);
	if(conn->output_queue == NULL){
		// restart write chain
		conn->output_queue = msg;
		net_async_write(conn->sock, msg->buffer, msg->length, on_write, conn);
	}
	else{
		// push msg to the end of queue so it will
		// be processed by the write chain
		it = &conn->output_queue;
		while(*it != NULL)
			it = &(*it)->next;
		*it = msg;
	}
	mutex_unlock(conn->lock);
}

void connection_close(struct connection *conn)
{
	struct connection **it;

	mutex_lock(conn->lock);
	// release protocol handle so no further operations
	// will take effects
	if(conn->handle != NULL){
		conn->protocol->destroy_handle(conn->handle);
		conn->handle = NULL;
	}

	// shutdown socket reading
	if((conn->flags & CONNECTION_CLOSING) == 0){
		conn->flags |= CONNECTION_CLOSING;
		net_socket_shutdown(conn->sock, NET_SHUT_RD);
	}

	// if there is no output_queue then there is no current
	// writing operation
	if(conn->output_queue == NULL)
		conn->flags |= CONNECTION_WRITE_DONE;

	// if both reading and writing chains are done
	// we may safelly release the connection
	if(conn->flags & (CONNECTION_READ_DONE | CONNECTION_WRITE_DONE)){
		// destroy connection lock
		mutex_unlock(conn->lock);
		mutex_destroy(conn->lock);

		// remove it from the connection list and free it
		mutex_lock(lock);
		it = &conn_list;
		while(*it != NULL && *it != conn)
			it = &(*it)->next;
		// (*it) must be the connection here or else
		// there is a bug somewhere
		if(*it != NULL)
			(*it) = conn->next;
		array_del(conn_array, conn);
		mutex_unlock(lock);
		return;
	}
	mutex_unlock(conn->lock);
}