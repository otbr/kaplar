#include "server.h"

#include "network.h"
#include "message.h"
#include "array.h"
#include "thread.h"
#include "log.h"
#include "scheduler.h"
#include "connection.h"

#include <stddef.h>

#define SERVICE_OPEN	0x00
#define SERVICE_CLOSED	0x01

#define SERVICE_MAX_REOPENS 16
struct service{
	long			port;
	long			flags;
	long			reopens;
	struct socket		*sock;
	struct protocol		*protocol_list;
};

#define MAX_SERVICES 4
static struct service		services[MAX_SERVICES];
static long			service_count = 0;
static long			running = 0;

static void service_reopen(struct service *service);
static void service_on_accept(struct socket *sock, int error, int bytes_transfered, void *udata)
{
	struct service *service = udata;

	// reopen service in case of error
	if(error != 0){
		LOG_ERROR("service_on_accept: operation failed (error = %d)", error);
		net_close(sock);
		if((service->flags & SERVICE_CLOSED) == 0){
			LOG_ERROR("service_on_accept: fatal socket error! re-opening service");
			service_reopen(service);
			return;
		}
	}

	// if service is closing, close it's socket and return
	// interrupting the accept chain
	if((service->flags & SERVICE_CLOSED) != 0){
		net_close(service->sock);
		service->sock = NULL;
		return;
	}

	// create new connection
	connection_accept(sock, service->protocol_list);

	// chain next accept
	if(net_async_accept(service->sock, service_on_accept, service) != 0){
		LOG_ERROR("service_on_accept: failed to chain next accept! trying to re-open service");
		service_reopen(service);
	}
}

static void service_reopen(struct service *service)
{
	if(service->sock != NULL)
		net_close(service->sock);

	if(service->reopens >= SERVICE_MAX_REOPENS){
		LOG_ERROR("service_reopen: maximum number of reopens reached (%d)", SERVICE_MAX_REOPENS);
		service->flags |= SERVICE_CLOSED;
		return;
	}

	service->sock = net_server_socket(service->port);
	if(service->sock == NULL){
		LOG_ERROR("service_reopen: failed to create service socket on port %d", service->port);
		return;
	}

	if(net_async_accept(service->sock, service_on_accept, service) != 0){
		LOG_ERROR("service_reopen: failed to restart accept chain on service port %d", service->port);
		net_close(service->sock);
		service->sock = NULL;
	}
}

void server_run()
{
	struct service *service;
	struct protocol *proto;

	// initialize services
	for(int i = 0; i < service_count; i++){
		service = &services[i];

		// check that port is valid
		if(service->port <= 0){
			LOG_ERROR("server_run: service with invalid port %d", service->port);
			continue;
		}

		// initialize protocol internals
		for(proto = service->protocol_list; proto != NULL; proto = proto->next)
			proto->init();

		// create service socket
		service->sock = net_server_socket(service->port);
		if(service->sock == NULL){
			LOG_ERROR("server_run: failed to start service on port %d", service->port);
			continue;
		}

		// start accept chain
		if(net_async_accept(service->sock, service_on_accept, service) != 0){
			LOG_ERROR("server_run: failed to start accept chain on service port %d", service->port);
			net_close(service->sock);
			service->sock = NULL;
		}
	}

	// network loop
	running = 1;
	while(running != 0){
		// net_work returning -1 means the net interface
		// is no longer usable and needs to be shutdown
		if(net_work() == -1)
			running = 0;
	}


	// close services
	for(int i = 0; i < service_count; i++){
		service = &services[i];

		// close service socket
		service->flags |= SERVICE_CLOSED;
		if(service->sock != NULL)
			net_close(service->sock);

		// shutdown protocol internals
		for(proto = service->protocol_list; proto != NULL; proto = proto->next)
			proto->shutdown();
	}
}

void server_stop()
{
	running = 0;
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
