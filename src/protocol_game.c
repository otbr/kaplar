#include "server.h"

#include <stdlib.h>

struct protocol protocol_game = {
	.name = "game",
	.identifier = 0x00,
	.flags = PROTOCOL_SENDS_FIRST
		| PROTOCOL_USE_CHECKSUM,

	.create_handle = NULL,
	.release_handle = NULL,

	.on_connect = NULL,
	.on_recv_message = NULL,
	.on_recv_first_message = NULL,
};


static void *create_handle()
{
}