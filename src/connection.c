#include "connection.h"

#include "message.h"
#include "array.h"
#include "server.h"
#include "network.h"
#include "thread.h"
#include "scheduler.h"
#include "log.h"
#include "util.h"

#include <stddef.h>


#define CONNECTION_OPEN			0x00
#define CONNECTION_CLOSED		0x01
#define CONNECTION_CLOSING		0x02
#define CONNECTION_FIRST_MSG		0x04
#define CONNECTION_RD_TIMEOUT_CANCEL	0x08
#define CONNECTION_WR_TIMEOUT_CANCEL	0x10

#define RD_TIMEOUT 30000 // 30sec
#define WR_TIMEOUT 30000 // 30sec
#define MAX_OUTPUT 8
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
static void on_read_length(struct socket *sock, int error, int transfered, void *udata);
static void on_read_body(struct socket *sock, int error, int transfered, void *udata);
static void on_write(struct socket *sock, int error, int transfered, void *udata);

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
}

static void on_read_length(struct socket *sock, int error, int transfered, void *udata)
{
	struct connection	*conn = udata;
	struct message		*msg = &conn->input;

	mutex_lock(conn->lock);
	// the message length on a read operation
	// will have only the length of the body
	msg->readpos = 0;
	msg->length = message_get_u16(msg);
	if((conn->flags & (CONNECTION_CLOSED | CONNECTION_CLOSING)) == 0
			&& error == 0 && transfered > 0 && msg->length > 0
			&& msg->length+2 <= MESSAGE_BUFFER_LEN){

		// NOTE: need to reschedule read timeout only
		// after reading the body and not here

		// chain body read
		if(net_async_read(sock, msg->buffer+2, msg->length,
				on_read_body, conn) == 0){
			mutex_unlock(conn->lock);
			return;
		}

	}

	if(scheduler_remove(conn->rd_timeout) == -1)
		conn->flags |= CONNECTION_RD_TIMEOUT_CANCEL;
	else
		conn->ref_count -= 1;
	mutex_unlock(conn->lock);
	connection_close(conn, 0);
	internal_release(conn);
}

static void on_read_body(struct socket *sock, int error, int transfered, void *udata)
{
	struct connection	*conn = udata;
	struct message		*msg = &conn->input;
	struct protocol		*proto;
	long			proto_id;
	uint32_t		checksum;

	mutex_lock(conn->lock);
	// check for errors or if the connection is closed/closing
	if((conn->flags & (CONNECTION_CLOSED | CONNECTION_CLOSING)) == 0
			&& error == 0 && transfered > 0){
		// check if the message has a checksum
		checksum = adler32(msg->buffer + 6, msg->length - 4);
		if(checksum != message_get_u32(msg))
			msg->readpos -= 4;
		else
			LOG("valid checksum: %lu", checksum);

		// check if it's the first message
		if((conn->flags & CONNECTION_FIRST_MSG) == 0){
			conn->flags |= CONNECTION_FIRST_MSG;

			// if handle is still NULL the service has multiple
			// protocols and we need to choose it now
			if(conn->handle == NULL){
				proto = conn->protocol;
				proto_id = message_get_byte(msg);
				while(proto != NULL && proto->identifier != proto_id)
					proto = proto->next;

				// if the requested protocol wasn't found, abort connection
				if(proto == NULL)
					goto close;

				// create protocol handle
				conn->handle = proto->handle_create(conn);
				conn->protocol = proto;
			}
			conn->protocol->on_recv_first_message(conn->handle, msg);
		}
		else{
			conn->protocol->on_recv_message(conn->handle, msg);
		}

		// chain next length read and reschedule read timeout
		scheduler_reschedule(RD_TIMEOUT, conn->rd_timeout);
		if(net_async_read(sock, msg->buffer, 2, on_read_length, conn) == 0){
			mutex_unlock(conn->lock);
			return;
		}
	}

close:
	if(scheduler_remove(conn->rd_timeout) == -1)
		conn->flags |= CONNECTION_RD_TIMEOUT_CANCEL;
	else
		conn->ref_count -= 1;
	mutex_unlock(conn->lock);
	connection_close(conn, 0);
	internal_release(conn);
}

static void on_write(struct socket *sock, int error, int transfered, void *udata)
{
	struct connection	*conn = udata;
	struct message		*msg, *next;
	int			close = 1;

	mutex_lock(conn->lock);
	// check for errors or if the connection is closed
	if((conn->flags & CONNECTION_CLOSED) == 0
			&& error == 0 && transfered > 0){
		// pop message from queue and free it
		msg = conn->output_queue;
		next = msg->next;
		conn->output_queue = next;
		msg->state = MESSAGE_FREE;

		if(next != NULL){
			//reschedule write timeout
			scheduler_reschedule(WR_TIMEOUT, conn->wr_timeout);

			// chain next write
			if(net_async_write(conn->sock, next->buffer, next->length,
					on_write, conn) == 0){
				mutex_unlock(conn->lock);
				return;
			}
		}
		else if((conn->flags & CONNECTION_CLOSING) == 0){
			close = 0;
		}
	}

	// cancel write timeout
	if(scheduler_remove(conn->wr_timeout) == -1)
		conn->flags |= CONNECTION_WR_TIMEOUT_CANCEL;
	else
		conn->ref_count -= 1;
	mutex_unlock(conn->lock);
	if(close != 0)
		connection_close(conn, 1);
	internal_release(conn);
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

	// initialize connection
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

	// lock here because the protocol callbacks, read timeout
	// or read handler may use the connection before this returns
	mutex_lock(conn->lock);

	// if the protocol sends first, create handle now
	if(protocol->flags & PROTOCOL_SENDS_FIRST){
		conn->handle = protocol->handle_create(conn);
		protocol->on_connect(conn->handle);
	}

	// schedule read timeout
	conn->ref_count += 1;
	conn->rd_timeout = scheduler_add(RD_TIMEOUT, read_timeout_handler, conn);
	if(conn->rd_timeout != NULL){
		conn->ref_count += 1;
		if(net_async_read(sock, conn->input.buffer, 2, on_read_length, conn) == 0){
			mutex_unlock(conn->lock);
			return;
		}

		// if the async read failed, cancel the read timeout
		if(scheduler_remove(conn->rd_timeout) == -1)
			conn->flags |= CONNECTION_RD_TIMEOUT_CANCEL;
	}

	// if the scheduler print a warning we know we need
	// to bump it's capacity else it was a network error
	mutex_unlock(conn->lock);
	connection_close(conn, 1);
	internal_release(conn);
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
	void *tmp;

	mutex_lock(conn->lock);
	// release protocol handle
	if(conn->handle != NULL){
		// handle_release SHOULDN'T call connection_close
		// but just in case...
		tmp = conn->handle;
		conn->handle = NULL;
		conn->protocol->handle_release(tmp);
	}

	if((conn->flags & CONNECTION_CLOSED) == 0){
		if(conn->output_queue == NULL || abort != 0){
			conn->flags |= CONNECTION_CLOSED;
			net_socket_shutdown(conn->sock, NET_SHUT_RDWR);
			net_close(conn->sock);
			conn->sock = NULL;
		}
		else if((conn->flags & CONNECTION_CLOSING) == 0){
			conn->flags |= CONNECTION_CLOSING;
			net_socket_shutdown(conn->sock, NET_SHUT_RD);
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
	return msg;
}

void connection_send(struct connection *conn, struct message *msg)
{
	struct message **it;

	mutex_lock(conn->lock);
	if(conn->output_queue == NULL){
		// add message to output queue
		msg->next = NULL;
		conn->output_queue = msg;

		// schedule write timeout
		conn->ref_count += 1;
		conn->wr_timeout = scheduler_add(WR_TIMEOUT, write_timeout_handler, conn);
		if(conn->wr_timeout != NULL){
			// restart write chain
			conn->ref_count += 1;
			if(net_async_write(conn->sock, msg->buffer, msg->length,
					on_write, conn) == 0){
				mutex_unlock(conn->lock);
				return;
			}

			// if the async write failed, cancel the write timeout
			if(scheduler_remove(conn->wr_timeout) == -1)
				conn->flags |= CONNECTION_WR_TIMEOUT_CANCEL;
		}

		mutex_unlock(conn->lock);
		connection_close(conn, 1);
		internal_release(conn);
	}
	else{
		// push msg to the end of queue so it will
		// be processed by the write chain
		it = &conn->output_queue;
		while(*it != NULL)
			it = &(*it)->next;
		*it = msg;

		mutex_unlock(conn->lock);
	}
}
