#ifndef __MESSAGE_H__
#define __MESSAGE_H__

#include "types.h"

#define MESSAGE_BUFFER_LEN 4096

#define MESSAGE_FREE 0x00
#define MESSAGE_BUSY 0x01

struct message{
	long readpos;
	long length;
	long state;
	uint8_t buffer[MESSAGE_BUFFER_LEN];

	struct message *next;
};

void message_reset(struct message *msg);
void message_goto(struct message *msg, long writepos);

uint8_t message_get_byte(struct message *msg);
uint16_t message_get_u16(struct message *msg);
uint32_t message_get_u32(struct message *msg);
const char *message_get_str(struct message *msg, uint16_t *len);

void message_add_byte(struct message *msg, uint8_t val);
void message_add_u16(struct message *msg, uint16_t val);
void message_add_u32(struct message *msg, uint32_t val);
void message_add_str(struct message *msg, const char *str, uint16_t len);

#endif //__MESSAGE_H__