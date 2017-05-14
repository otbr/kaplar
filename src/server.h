#ifndef SERVER_H_
#define SERVER_H_

#define PROTOCOL_SENDS_FIRST	0x01

extern struct protocol protocol_login;
extern struct protocol protocol_game;
extern struct protocol protocol_old_login;
extern struct protocol protocol_old_game;

struct connection;
struct message;

struct protocol{
	const char *name;
	long identifier;
	long flags;

	void (*init)(void);
	void (*shutdown)(void);

	void *(*handle_create)(struct connection*);
	void (*handle_release)(void*);

	void (*message_begin)(void*, struct message*);
	void (*message_end)(void*, struct message*);

	void (*on_connect)(void*);
	void (*on_recv_message)(void*, struct message*);
	void (*on_recv_first_message)(void*, struct message*);

	struct protocol *next;
};

void server_run(void);
void server_stop(void);
void server_add_protocol(int port, struct protocol *protocol);

#endif //SERVER_H_
