#include "cmdline.h"
#include "work.h"
#include "scheduler.h"
#include "network.h"
#include "server.h"
#include "log.h"
#include "connection.h"

#include <stdlib.h>
#include <stdio.h>

#define OTSERV_NAME "Kaplar"
#define OTSERV_VERSION "0.0.1"

extern struct protocol protocol_test;

int main(int argc, char **argv)
{
	cmdl_init(argc, argv);
	// parse command line here

	// start logging
	//log_start();

	LOG(OTSERV_NAME " Version " OTSERV_VERSION);
	LOG("================================");

	work_init();
	scheduler_init();
	net_init();
	connection_init();

	//server_add_protocol(7171, &protocol_login);
	//server_add_protocol(7171, &protocol_old_login);
	//server_add_protocol(7171, &protocol_old_game);
	//server_add_protocol(7172, &protocol_game);
	server_add_protocol(7171, &protocol_test);

	LOG("server running...");
	server_run();

	LOG("cleaning up...");
	connection_shutdown();
	net_shutdown();
	scheduler_shutdown();
	work_shutdown();
	//log_stop();
	return 0;
}
