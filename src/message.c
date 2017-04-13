#include "message.h"

#include <string.h>

/*

todo: create message with different sizes (small, medium, large)

*/

#ifdef __BIG_ENDIAN__
// the compiler should optimize these away
static inline uint16_t swap_u16(uint16_t u16)
{
	return u16;
}
static inline uint32_t swap_u32(uint32_t u32)
{
	return u32;
}
#else
static inline uint16_t swap_u16(uint16_t x)
{
	return (x & 0xFF00) >> 8
		| (x & 0x00FF) << 8;
}
static inline uint32_t swap_u32(uint32_t x)
{
	return (x & 0xFF000000) >> 24
		| (x & 0x00FF0000) >> 8
		| (x & 0x0000FF00) << 8
		| (x & 0x000000FF) << 24;
}
#endif

void message_reset(struct message *msg)
{
	msg->readpos = 0;
	msg->length = 0;
	msg->state = MESSAGE_FREE;
	msg->next = NULL;
}

uint8_t message_get_byte(struct message *msg)
{
	uint8_t val = *(msg->buffer + msg->readpos);
	msg->readpos++;
	return val;
}

uint16_t message_get_u16(struct message *msg)
{
	uint16_t val = *(uint16_t*)(msg->buffer + msg->readpos);
	msg->readpos += 2;
	return swap_u16(val);
}

uint32_t message_get_u32(struct message *msg)
{
	uint32_t val = *(uint32_t*)(msg->buffer + msg->readpos);
	msg->readpos += 4;
	return swap_u32(val);
}

const char *message_get_str(struct message *msg, uint16_t *len)
{
	const char *str = NULL;

	//assert(len != NULL || "message_get_str: parameter \"len\" can't be NULL");
	if(len == NULL) return NULL;
	*len = message_get_u16(msg);
	if(*len != 0){
		str = (const char*)(msg->buffer + msg->readpos);
		msg->readpos += *len;
	}

	return str;
}

void message_add_byte(struct message *msg, uint8_t val)
{
	*(msg->buffer + msg->readpos) = val;
	msg->readpos++;
	msg->length++;
}

void message_add_u16(struct message *msg, uint16_t val)
{
	*(uint16_t*)(msg->buffer + msg->readpos) = swap_u16(val);
	msg->readpos += 2;
	msg->length += 2;
}

void message_add_u32(struct message *msg, uint32_t val)
{
	*(uint32_t*)(msg->buffer + msg->readpos) = swap_u32(val);
	msg->readpos += 4;
	msg->length += 4;
}

void message_add_str(struct message *msg, const char *str, uint16_t len)
{
	message_add_u16(msg, len);
	memcpy(msg->buffer + msg->readpos, str, len);
	msg->readpos += len;
	msg->length += len;
}