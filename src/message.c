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

void message_start(struct message *msg)
{
	msg->length = 0;
	msg->readpos = 2;
}

void message_decode_length(struct message *msg)
{
	uint16_t length = *(uint16_t*)(msg->buffer);
	msg->length = swap_u16(length) + 2;
}

void message_add_header(struct message *msg)
{
	*(uint16_t*)(msg->buffer) = swap_u16((uint16_t)msg->length);
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

void message_get_str(struct message *msg, char *buf, uint16_t buflen)
{
	uint16_t len = message_get_u16(msg);
	if(len == 0) return;
	if(len > buflen - 1)
		len = buflen - 1;

	memcpy(buf, (msg->buffer + msg->readpos), len);
	buf[len] = 0x00;
	msg->readpos += len;
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

void message_add_str(struct message *msg, const char *buf, uint16_t buflen)
{
	message_add_u16(msg, buflen);
	memcpy((msg->buffer + msg->readpos), buf, buflen);
	msg->readpos += buflen;
	msg->length += buflen;
}