#include "connection.h"

#include "message.h"
#include "array.h"
#include "server.h"
#include "network.h"
#include "thread.h"
#include "scheduler.h"
#include "log.h"

#include <stdlib.h>


#define CONNECTION_OPEN			0x00
#define CONNECTION_CLOSED		0x01
#define CONNECTION_CLOSING		0x02
#define CONNECTION_FIRST_MSG		0x04
#define CONNECTION_RD_TIMEOUT_CANCEL	0x08
#define CONNECTION_WR_TIMEOUT_CANCEL	0x10

#define RD_TIMEOUT 30000 // 30sec
#define WR_TIMEOUT 30000 // 30sec
#define MAX_OUTPUT 32
struct connection{
	struct socket		*sock;
	long			flags;
	long			ref_count;
	struct message		input;
	struct message		output[MAX_OUTPUT];
	struct message		*output_queue;
	struct mutex		*lock;

	struct protocol		*protocol;
	void			*handle;

	struct sch_entry	*rd_timeout;
	struct sch_entry	*wr_timeout;
};

#define MAX_CONNECTIONS 2048
static struct array		*conn_array;

static void internal_release(struct connection *conn);
static void on_read_header(struct socket *sock, int error, int bytes_transfered, void *udata);
static void on_read_body(struct socket *sock, int error, int bytes_transfered, void *udata);
static void on_write(struct socket *sock, int error, int bytes_transfered, void *udata);

static void read_timeout_handler(void *arg)
{
	struct connection *conn = arg;
	mutex_lock(conn->lock);
	conn->rd_timeout = NULL;
	if((conn->flags & CONNECTION_RD_TIMEOUT_CANCEL) != 0){
		conn->flags &= ~(CONNECTION_RD_TIMEOUT_CANCEL);
		mutex_unlock(conn->lock);
	}
	else{
		mutex_unlock(conn->lock);
		connection_close(conn, 1);
	}
	internal_release(conn);
	LOG("read timeout returned");
}
static void write_timeout_handler(void *arg)
{
	struct connection *conn = arg;
	mutex_lock(conn->lock);
	conn->wr_timeout = NULL;
	if((conn->flags & CONNECTION_WR_TIMEOUT_CANCEL) != 0){
		conn->flags &= ~(CONNECTION_WR_TIMEOUT_CANCEL);
		mutex_unlock(conn->lock);
	}
	else{
		mutex_unlock(conn->lock);
		connection_close(conn, 1);
	}
	internal_release(conn);
	LOG("write timeout returned");
}

static void on_read_header(struct socket *sock, int error, int bytes_transfered, void *udata)
{
	struct connection *conn = udata;
	struct message *msg = &conn->input;
	int abort = 0;

	mutex_lock(conn->lock);
	message_decode_length(msg);
	if((conn->flags & (CONNECTION_CLOSED | CONNECTION_CLOSING)) != 0
			|| error != 0 || msg->length > MESSAGE_BUFFER_LEN
			|| msg->length == 0){
		goto close;
	}
	// chain the body read and abort connection in case of error
	else if(net_async_read(sock, msg->buffer+2,
			msg->length-2, on_read_body, conn) != 0){
		abort = 1;
		goto close;
	}

	mutex_unlock(conn->lock);
	return;

close:
	conn->flags |= CONNECTION_RD_TIMEOUT_CANCEL;
	scheduler_pop(conn->rd_timeout);
	mutex_unlock(conn->lock);
	connection_close(conn, abort);
	internal_release(conn);
	LOG("read header returned");
}

static void on_read_body(struct socket *sock, int error, int bytes_transfered, void *udata)
{
	struct connection *conn = udata;
	struct message *msg = &conn->input;
	struct protocol *proto;
	int protocol_id;
	int abort = 0;

	mutex_lock(conn->lock);
	// check for errors or if the connection is closed/closing
	if((conn->flags & (CONNECTION_CLOSED | CONNECTION_CLOSING)) != 0
			|| error != 0)
		goto close;

	// calc checksum
	msg->readpos = 2;

	// check if it's the first message
	if((conn->flags & CONNECTION_FIRST_MSG) == 0){
		conn->flags |= CONNECTION_FIRST_MSG;

		// if handle is still NULL the service has multiple
		// protocols and we need to choose it now
		protocol_id = message_get_byte(msg);
		if(conn->handle == NULL){
			proto = conn->protocol;
			while(proto != NULL && proto->identifier != protocol_id)
				proto = proto->next;

			// if no valid protocol was found, abort connection
			if(proto == NULL){
				abort = 1;
				goto close;
			}

			// create protocol handle
			conn->handle = proto->create_handle(conn);
			conn->protocol = proto;
		}

		conn->protocol->on_recv_first_message(conn->handle, msg);
	}
	else{
		conn->protocol->on_recv_message(conn->handle, msg);
	}

	// chain the header read and and reschedule read timeout
	scheduler_reschedule(RD_TIMEOUT, conn->rd_timeout);
	if(net_async_read(sock, msg->buffer, 2, on_read_header, conn) != 0){
		abort = 1;
		goto close;
	}

	mutex_unlock(conn->lock);
	return;

close:
	conn->flags |= CONNECTION_RD_TIMEOUT_CANCEL;
	scheduler_pop(conn->rd_timeout);
	mutex_unlock(conn->lock);
	connection_close(conn, abort);
	internal_release(conn);
	LOG("read body returned");
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


	if(next != NULL){
		//reschedule write timeout
		scheduler_reschedule(WR_TIMEOUT, conn->wr_timeout);
		if(net_async_write(conn->sock, next->buffer, next->length+2,
				on_write, conn) != 0)
			goto close;
	}
	else{
		if((conn->flags & CONNECTION_CLOSING) != 0)
			goto close;

		// remove reference from the write handler
		conn->ref_count -= 1;
		// this will cancel the write timeout and remove its
		// reference to the connection
		conn->flags |= CONNECTION_WR_TIMEOUT_CANCEL;
		scheduler_pop(conn->wr_timeout);
	}

	mutex_unlock(conn->lock);
	return;

close:
	// cancel write timeout
	conn->flags |= CONNECTION_WR_TIMEOUT_CANCEL;
	scheduler_pop(conn->wr_timeout);
	mutex_unlock(conn->lock);
	connection_close(conn, 1);
	internal_release(conn);
	LOG("write returned");
}

// connection functions
void connection_init()
{
	conn_array = array_create(MAX_CONNECTIONS, sizeof(struct connection));
	array_init_lock(conn_array);
}

void connection_shutdown()
{
	struct connection *conn;
	int i;

	for(i = 0; i < MAX_CONNECTIONS; i++){
		conn = array_locked_get(conn_array, i);
		if(conn != NULL)
			connection_close(conn, 1);
	}

	// this may release the connection memory before
	// the read/write handlers release the connection
	// but it shouldn't be a problem since this will
	// only be called on cleanup
	array_destroy(conn_array);
}

void connection_accept(struct socket *sock, struct protocol *protocol)
{
	struct connection *conn;

	// initialize connection values
	conn = array_locked_new(conn_array);
	conn->sock = sock;
	conn->flags = CONNECTION_OPEN;
	conn->ref_count = 0;
	conn->output_queue = NULL;
	conn->protocol = protocol;
	conn->handle = NULL;
	conn->rd_timeout = NULL;
	conn->wr_timeout = NULL;

	// set connection message states
	conn->input.state = MESSAGE_BUSY;
	for(int i = 0; i < MAX_OUTPUT; i++)
		conn->output[i].state = MESSAGE_FREE;

	// create connection lock
	mutex_create(&conn->lock);

	// if the protocol needs to send first, create handle now
	// and call on_connect
	if(protocol->flags & PROTOCOL_SENDS_FIRST){
		conn->handle = protocol->create_handle(conn);
		protocol->on_connect(conn->handle);
	}

	// start connection read loop
	conn->ref_count += 1;
	conn->rd_timeout = scheduler_add(RD_TIMEOUT, read_timeout_handler, conn);

	conn->ref_count += 1;
	if(net_async_read(sock, conn->input.buffer, 2, on_read_header, conn) != 0){
		connection_close(conn, 1);
		internal_release(conn);
	}
}


static void internal_release(struct connection *conn)
{
	mutex_lock(conn->lock);
	conn->ref_count -= 1;
	if(conn->ref_count <= 0){
		if(conn->sock != NULL){
			net_close(conn->sock);
			conn->sock = NULL;
		}

		// destroy connection lock
		mutex_unlock(conn->lock);
		mutex_destroy(conn->lock);

		// release connection memory
		array_locked_del(conn_array, conn);
		LOG("connection released");
	}
	else{
		mutex_unlock(conn->lock);
	}
}

void connection_close(struct connection *conn, int abort)
{
	void *handle;

	LOG("connection_close: %p, %d", conn, abort);
	mutex_lock(conn->lock);
	if(conn->handle != NULL){
		// this is necessary because if the release_handle
		// calls connection_close it would cause an infinite loop
		handle = conn->handle;
		conn->handle = NULL;
		conn->protocol->release_handle(handle);
	}

	if((conn->flags & CONNECTION_CLOSED) == 0){
		// close socket if aborting
		if(abort != 0){
			conn->flags |= CONNECTION_CLOSED;
			net_socket_shutdown(conn->sock, NET_SHUT_RDWR);
			net_close(conn->sock);
			conn->sock = NULL;
		}
		// else start closing the connection
		else if((conn->flags & CONNECTION_CLOSING) == 0){
			conn->flags |= CONNECTION_CLOSING;
			if(conn->output_queue != NULL)
				net_socket_shutdown(conn->sock, NET_SHUT_RD);
			else
				net_socket_shutdown(conn->sock, NET_SHUT_RDWR);
		}
	}
	mutex_unlock(conn->lock);
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

	// setup message to start writing
	message_start(msg);
	return msg;
}

void connection_send(struct connection *conn, struct message *msg)
{
	struct message **it;

	mutex_lock(conn->lock);
	message_add_header(msg);
	if(conn->output_queue == NULL){
		// add message to output queue
		msg->next = NULL;
		conn->output_queue = msg;

		// schedule write timeout
		conn->ref_count += 1;
		conn->wr_timeout = scheduler_add(WR_TIMEOUT, write_timeout_handler, conn);

		// retart write chain
		conn->ref_count += 1;
		if(net_async_write(conn->sock, msg->buffer, msg->length+2,
				on_write, conn) != 0){
			mutex_unlock(conn->lock);
			connection_close(conn, 1);
			internal_release(conn);
			return;
		}
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
