#ifndef TYPES_H_
#define TYPES_H_

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

// redefine integer types without the *_t
typedef int8_t int8;
typedef uint8_t uint8;
typedef int16_t int16;
typedef uint16_t uint16;
typedef int32_t int32;
typedef uint32_t uint32;
typedef int64_t int64;
typedef uint64_t uint64;

// boolean type
#include <stdbool.h>

#endif //TYPES_H_
