#include "server.h"

#include "network.h"
#include "message.h"
#include "thread.h"

struct connection{
	int fd;
	int bound;
	long pending_ops;
	struct mutex *lock;

	struct message input[16];
	struct message output[16];
	struct message *output_queue;

	struct protocol *prot;
	void *handle;

	struct connection *next;
};

int server_init()
{
	return 0;
}

void server_shutdown()
{
}

void server_run()
{
	//
}