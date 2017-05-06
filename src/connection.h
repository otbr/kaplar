#ifndef CONNECTION_H_
#define CONNECTION_H_

struct connection;
struct socket;
struct protocol;

void		connection_init(void);
void		connection_shutdown(void);

void        connection_accept(struct socket *sock, struct protocol *protocol);
void		connection_close(struct connection *conn, int abort);

struct message	*connection_get_output_message(struct connection *conn);
void		connection_send(struct connection *conn, struct message *msg);

#endif //CONNECTION_H_
