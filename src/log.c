#include "log.h"
#include "types.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>

#define MAX_FILE_NAME_SIZE 64

// local variables
static char	filename[MAX_FILE_NAME_SIZE];
static FILE	*file = NULL;
static int	saving = 0;

bool log_start()
{
	time_t curTime;
	struct tm *timeptr;

	if(!saving){
		// name example: "Jan-01-1999-133700.log"
		curTime = time(NULL);
		timeptr = localtime(&curTime);
		strftime(filename, MAX_FILE_NAME_SIZE, "%b-%d-%Y-%H%M%S.log", timeptr);

		// open file
		file = fopen(filename, "a");
		if(file)
			saving = 1;
	}
	return saving;
}

void log_stop()
{
	if(!saving)
		return;

	saving = 0;
	fclose(file);
	file = NULL;
}

void log_add(const char *tag, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	log_add0(tag, fmt, ap);
	va_end(ap);
}

void log_add0(const char *tag, const char *fmt, va_list ap)
{
	time_t curTime;
	struct tm *timeptr;
	char timeStr[64];
	char logMessage[256];
	char logEntry[256];
	size_t entrySize;

	// time str
	curTime = time(NULL);
	timeptr = localtime(&curTime);
	// format example: "Jan 01 1999 13:37:00"
	strftime(timeStr, 64, "%b %d %Y %H:%M:%S", timeptr);

	// concatenate log message
	vsnprintf(logMessage, 256, fmt, ap);

	// concatenate log entry
	entrySize = snprintf(logEntry, 256, "[%s] %s\t%s\n", timeStr, tag, logMessage);
	if(entrySize <= 0){
		printf("<ERROR> Failed to concatenate log entry of type %s!\n", tag);
		return;
	}

	// output to console
	printf("%s", logEntry);

	// if saving, output to log file
	if(saving){
		if(fwrite(logEntry, 1, entrySize, file) != 1)
			printf("<ERROR> Failed to write log entry to file! (%s)\n", filename);
	}
}