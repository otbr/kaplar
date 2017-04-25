#include "cmdline.h"
#include "work.h"
#include "scheduler.h"
#include "network.h"
#include "server.h"
#include "log.h"

#include <stdlib.h>

#define OTSERV_NAME "Kaplar"
#define OTSERV_VERSION "0.0.1"

static void on_read(struct socket *sock, int error, int bytes_transfered, void *udata)
{
	char *buf = udata;
	LOG("on_read: sock=%p, error=%d, bytes_transfered=%d, udata=%p\n",
		sock, error, bytes_transfered, udata);

	free(buf);
}

static void on_accept(struct socket *sock, int error, int bytes_transfered, void *udata)
{
	char *buff;
	struct socket *server = udata;

	buff = malloc(1024);
	net_async_read(sock, buff, 1024, on_read, buff);
	net_socket_shutdown(sock, NET_SHUT_RDWR);
	net_async_accept(server, on_accept, udata);
}

int main(int argc, char **argv)
{
	struct socket *server;

	cmdl_init(argc, argv);
	// parse command line here

	LOG(OTSERV_NAME " Version " OTSERV_VERSION);
	LOG("================================");

	work_init();
	scheduler_init();
	//server_init();

	// run server
	//server_add_protocol(7171, &protocol_login);
	//server_add_protocol(7171, &protocol_old_login);
	//server_add_protocol(7172, &protocol_old_game);
	//server_add_protocol(7172, &protocol_game);
	//server_run();

	net_init();
	server = net_server_socket(7171);
	net_async_accept(server, on_accept, server);
	while(1) net_work();
	net_shutdown();

	//server_shutdown();
	scheduler_shutdown();
	work_shutdown();
	return 0;
}