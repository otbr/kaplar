#ifndef __SERVER_H__
#define __SERVER_H__

#define PROTOCOL_SENDS_FIRST	0x01
#define PROTOCOL_USE_CHECKSUM	0x02

extern struct protocol protocol_login;
extern struct protocol protocol_game;
extern struct protocol protocol_old_login;
extern struct protocol protocol_old_game;

struct protocol{
	const char *name;
	int identifier;
	int flags;

	// callbacks to initialize protocol internals
	void (*init)();
	void (*shutdown)();

	void *(*create_handle)(struct connection*);
	void (*release_handle)(void*);

	// event callbacks
	void (*on_connect)(void*);
	void (*on_recv_message)(void*, struct message*);
	void (*on_recv_first_message)(void*, struct message*);

	struct protocol *next;
};

void server_run();
void server_stop();
void server_add_protocol(int port, struct protocol *protocol);

#endif //__SERVER_H__