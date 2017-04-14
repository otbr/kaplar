#ifndef __PROTOCOL_H__
#define __PROTOCOL_H__

struct protocol{
	void (*parse_msg)(struct message *msg, void *handle);
};

#endif //__PROTOCOL_H__