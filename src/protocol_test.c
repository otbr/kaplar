#include "server.h"

#include "connection.h"
#include "message.h"
#include "log.h"
#include "util.h"

#include <stddef.h>

static void init(void);
static void shutdown(void);

static void *handle_create(struct connection *conn);
static void handle_release(void *handle);

static void message_begin(void *handle, struct message *msg);
static void message_end(void *handle, struct message *msg);

static void on_connect(void *handle);
static void on_recv_message(void *handle, struct message *msg);
static void on_recv_first_message(void *handle, struct message *msg);

struct protocol protocol_test = {
	.name			= "test",
	.identifier		= 0x00,
	.flags			= PROTOCOL_SENDS_FIRST,

	.init			= init,
	.shutdown		= shutdown,

	.handle_create		= handle_create,
	.handle_release		= handle_release,

	.message_begin		= message_begin,
	.message_end		= message_end,

	.on_connect		= on_connect,
	.on_recv_message	= on_recv_message,
	.on_recv_first_message	= on_recv_first_message,

	.next			= NULL,
};

static void send_hello(struct connection *conn)
{
	struct message *msg;

	msg = connection_get_output_message(conn);
	message_begin(conn, msg);
	message_add_str(msg, "Hello World", 11);
	message_end(conn, msg);
	connection_send(conn, msg);
}

static void init()
{
}

static void shutdown()
{
}

static void *handle_create(struct connection *conn)
{
	return conn;
}

static void handle_release(void *handle)
{
}

static void message_begin(void *handle, struct message *msg)
{
	msg->length = 0;
	msg->readpos = 6;
}

static void message_end(void *handle, struct message *msg)
{
	// add message header
	msg->readpos = 2;
	message_add_u32(msg, (uint32_t)adler32(msg->buffer+6, msg->length));
	msg->readpos = 0;
	message_add_u16(msg, (uint16_t)msg->length);
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
	message_get_str(msg, buf, 64);
	LOG("on_recv_message: %s", buf);
	send_hello(conn);
}

static void on_recv_first_message(void *handle, struct message *msg)
{
	LOG("on_recv_first message: protocol id = %02X", message_get_byte(msg));
	on_recv_message(handle, msg);
}