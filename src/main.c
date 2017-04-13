#include "cmdline.h"
#include "work.h"
#include "scheduler.h"
#include "network.h"
#include "thread.h"

#include <stdio.h>
#include <stdlib.h>

struct socket *server;
struct thread *thread;

static void on_read(struct socket *sock, int error, int bytes_transfered, void *udata)
{
	char *data = udata;

	printf("on_read: sock = %p, error = %d, bytes_transfered = %d, udata = %p\n",
		sock, error, bytes_transfered, udata);
	if(bytes_transfered > 0)
		printf("%.*s\n", bytes_transfered, data);

	free(data);
	net_close(sock);
}

static void accept();
static void on_accept(struct socket *sock, int error, int bytes_trasnfered, void *udata)
{
	char *data;

	printf("got it! reading message...\n");
	data = malloc(1024);
	net_async_read(sock, data, 1024, on_read, data);
	accept();
}

static void accept()
{
	printf("waiting on connection...\n");
	net_async_accept(server, on_accept, NULL);
}

int main(int argc, char **argv)
{
	cmdl_init(argc, argv);
	work_init();
	scheduler_init();

	//
	net_init();
	server = net_server_socket(7171);
	accept();
	while(1) printf("work: %d\n", net_work());

	getchar();
	net_shutdown();
	scheduler_shutdown();
	work_shutdown();
	return 0;
}