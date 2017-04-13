#ifndef __LOG_H__
#define __LOG_H__

#include "types.h"
#include <stdarg.h>

bool	log_start();
void	log_stop();

void	log_add(const char *tag, const char *fmt, ...);
void	log_add0(const char *tag, const char *fmt, va_list ap);

// LOG*(fmt, ...)
#define LOG(...)		log_add("INFO", __VA_ARGS__)
#define LOG_WARNING(...)	log_add("WARNING", __VA_ARGS__)
#define LOG_ERROR(...)		log_add("ERROR", __VA_ARGS__)

// debug logging
#ifndef _DEBUG
	#define LOG_DEBUG(fmt, ...)
	#define DEBUG_CHECK(cond, fmt, ...)
#else
	// LOG_DEBUG(fmt, ...)
	#define LOG_DEBUG(...) log_add("DEBUG", __VA_ARGS__)

	// DEBUG_CHECK(cond, fmt, ...)
	#define DEBUG_CHECK(cond, ...)			\
			if(!(cond)) { LOG_DEBUG(__VA_ARGS__); }
#endif

#endif //__LOG_H__