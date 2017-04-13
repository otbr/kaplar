#ifndef __TYPES_H__
#define __TYPES_H__

// integer types
#if defined(_MSC_VER) && (_MSC_VER < 1600)
	typedef	signed __int8 int8_t;
	typedef	unsigned __int8 uint8_t;
	typedef signed __int16 int16_t;
	typedef	unsigned __int16 uint16_t;
	typedef signed __int32 int32_t;
	typedef unsigned __int32 uint32_t;
	typedef signed __int64 int64_t;
	typedef unsigned __int64 uint64_t;
#else
	#include <stdint.h>
#endif

// boolean type
#include <stdbool.h>

#endif //__TYPES_H__