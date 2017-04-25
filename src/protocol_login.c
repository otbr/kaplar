#include "server.h"

#include "message.h"
#include "connection.h"

#include <string.h>

#define OTSERV_CLIENT_VERSION "8.60"
#define STRING_CLIENT_VERSION "This server requires client version "OTSERV_CLIENT_VERSION"."

// no-op for this protocol
static void init(){}
static void shutdown(){}
static void destroy_handle(void *handle){}
static void on_connect(void *handle){}
static void on_recv_message(void *handle, struct message *msg){}

// forward decl
static void *create_handle(struct connection *conn);
static void on_recv_first_message(void *handle, struct message *msg);

struct protocol protocol_login = {
	.name = "login",
	.identifier = 0x01,
	.flags = PROTOCOL_USE_CHECKSUM,

	.init = init,
	.shutdown = shutdown,

	.create_handle = create_handle,
	.destroy_handle = destroy_handle,

	.on_connect = on_connect,
	.on_recv_message = on_recv_message,
	.on_recv_first_message = on_recv_first_message,
};

static void disconnect(struct connection *conn, const char *message)
{
	struct message *output = connection_get_output_message(conn);
	if(output){
		message_add_byte(output, 0x0A);
		message_add_str(output, message, (uint16_t)strlen(message));
		connection_send(conn, output);
	}
	connection_close(conn);
}

static void *create_handle(struct connection *conn)
{
	return conn;
}

static void on_recv_first_message(void *handle, struct message *msg)
{
	struct connection *conn = handle;

	uint16_t version;
	char accname[32];
	char password[32];
	struct message *output;

	message_get_u16(msg); // client_os
	version = message_get_u16(msg);

	// there was no RSA encryption <= 760
	if(version <= 760){
		disconnect(conn, STRING_CLIENT_VERSION);
		return;
	}
	// rsa_decrypt

	message_get_u32(msg); // xtea key[0]
	message_get_u32(msg); // xtea key[1]
	message_get_u32(msg); // xtea key[2]
	message_get_u32(msg); // xtea key[3]

	message_get_str(msg, accname, 32); // accname
	message_get_str(msg, password, 32); //password

	// check version
	// check acc/password
	// etc...

	output = connection_get_output_message(conn);
	if(output){
		// add motd
		//message_add_byte(output, 0x14);
		//message_add_str(output, motd, strlen(motd)); // motd

		// add char list
		message_add_byte(output, 0x64);
		message_add_u16(output, 1); // charlist length
		//for(each character){
			message_add_str(output, "Player", 6); // character's name
			message_add_str(output, "Kaplar", 6); // world's name
			message_add_u32(output, 16777343); // server ip
			message_add_u16(output, 7172); // server port
		//}
		message_add_u16(output, 0); // premium days

		//send_message(conn, output);
		connection_send(conn, output);
	}
	connection_close(conn);
}