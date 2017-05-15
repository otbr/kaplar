#include "server.h"

#include <stddef.h>

struct protocol protocol_game = {
	.name = "game",
	.identifier = 0x00,
	.flags = PROTOCOL_SENDS_FIRST,

	.handle_create = NULL,
	.handle_release = NULL,

	.message_begin = NULL,
	.message_end = NULL,

	.on_connect = NULL,
	.on_recv_message = NULL,
	.on_recv_first_message = NULL,
};
