#include "server.h"

#include "network.h"
#include "message.h"
#include "array.h"
#include "thread.h"
#include "log.h"
#include "scheduler.h"
#include "connection.h"

struct service{
	int			port;
	struct socket		*sock;
	struct protocol		*protocol_list;
};

#define MAX_SERVICES 4
static struct service		services[MAX_SERVICES];
static long			service_count = 0;
static long			running = 0;

static void service_open(struct service *service);
static void service_close(struct service *service);
static void service_on_accept(struct socket *sock, int error, int bytes_transfered, void *udata)
{
	struct service *service = udata;
	if(error != 0){
		// create new connection
		connection_accept(sock, service->protocol_list);

		// chain another accept
		net_async_accept(service->sock, service_on_accept, service);
	}
	else{
		LOG_ERROR("service_on_accept: operation failed with error %d", error);
		net_close(sock);
		if(error != ECANCELED){
			LOG_ERROR("service_on_accept: fatal socket error! re-opening service");
			service_close(service);
			service_open(service);
		}
	}
}

static void service_open(struct service *service)
{
	service->sock = net_server_socket(service->port);
	if(service->sock != NULL)
		net_async_accept(service->sock, service_on_accept, service);
}

static void service_close(struct service *service)
{
	if(service->sock != NULL)
		net_close(service->sock);
}

void server_init()
{
}

void server_shutdown()
{
}

void server_run()
{
	// initialize services
	for(int i = 0; i < service_count; i++){
		if(services[i].port <= 0){
			LOG_ERROR("server_run: service with invalid port %d", services[i].port);
			continue;
		}

		service_open(&services[i]);
	}

	// network loop
	running = 1;
	while(running) net_work();
}

void server_stop()
{
	running = 0;
	// close service sockets
	for(int i = 0; i < service_count; i++)
		net_close(services[i].sock);
}

void server_add_protocol(int port, struct protocol *protocol)
{
	int i;
	struct service *service;

	if(running != 0){
		LOG_ERROR("server_add_protocol: server already running");
		return;
	}

	service = NULL;
	for(i = 0; i < service_count; i++){
		if(services[i].port == port){
			service = &services[i];
			break;
		}
	}

	if(service == NULL){
		if(service_count >= MAX_SERVICES){
			LOG_ERROR("server_add_protocol: reached maximum number of services %d", MAX_SERVICES);
			return;
		}
		service = &services[service_count];
		service->port = port;
		service->protocol_list = protocol;
		service_count++;
	}
	else{
		if(service->protocol_list->flags & PROTOCOL_SENDS_FIRST ||
				protocol->flags & PROTOCOL_SENDS_FIRST){
			LOG_ERROR("server_add_protocol: protocols \"%s\" and \"%s\" cannot use the same port",
				service->protocol_list->name, protocol->name);
			return;
		}

		protocol->next = service->protocol_list;
		service->protocol_list = protocol;
	}
}

void server_cleanup()
{
	// close all services
	// cleanup protocols
}