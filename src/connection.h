#ifndef __CONNECTION_H__
#define __CONNECTION_H__

struct connection;

void		connection_init();
void		connection_shutdown();
void		connection_accept(struct socket *sock, struct protocol *protocol);

struct message	*connection_get_output_message(struct connection *conn);
void		connection_send(struct connection *conn, struct message *msg);
void		connection_close(struct connection *conn);

#endif //__CONNECTION_H__