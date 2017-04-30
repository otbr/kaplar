#include "server.h"

#include "connection.h"
#include "message.h"
#include "log.h"

#include <stdlib.h>

// no-op for this protocol
static void init(){}
static void shutdown(){}
static void release_handle(void *handle){}

// forward decl
static void *create_handle(struct connection *conn);
static void on_connect(void *handle);
static void on_disconnect(void *handle);
static void on_recv_message(void *handle, struct message *msg);


struct protocol protocol_test = {
	.name = "test",
	.identifier = 0x00,
	.flags = PROTOCOL_SENDS_FIRST,

	.init = init,
	.shutdown = shutdown,

	.create_handle = create_handle,
	.release_handle = release_handle,

	.on_connect = on_connect,
	.on_recv_message = on_recv_message,
	.on_recv_first_message = on_recv_message,

	.next = NULL,
};

static void *create_handle(struct connection *conn)
{
	return conn;
}

static void send_hello(struct connection *conn)
{
	struct message *msg;

	// send hello message
	msg = connection_get_output_message(conn);
	message_add_str(msg, "Hello World", 11);
	connection_send(conn, msg);
}

static void on_connect(void *handle)
{
	struct connection *conn = handle;
	LOG("on_connect");
	send_hello(conn);
}

static void on_recv_message(void *handle, struct message *msg)
{
	char buf[64];
	struct connection *conn = handle;
	msg->readpos = 2;
	message_get_str(msg, buf, 64);
	LOG("on_recv_message: %s", buf);
	send_hello(conn);
}