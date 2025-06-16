#include "log.h"
#include <time.h>
#include <stdarg.h>
#include <stdio.h>

static LogLevel current_level = LOG_LEVEL_INFO;

void set_log_level(LogLevel level) 
{
    	current_level = level;
}

static const char* level_to_string(LogLevel level) 
{
    	switch (level) 
    	{
        	case LOG_LEVEL_ERROR:
		     	return "ERROR";
			break;
        	
		case LOG_LEVEL_WARNING: 
			return "WARNING";
			break;
        	
		case LOG_LEVEL_INFO:
			return "INFO";
			break;
        	
		case LOG_LEVEL_DEBUG:
			return "DEBUG";
			break;
        	
		default:
			return "UNKNOWN";
    	}
}

void log_message(LogLevel level, const char *format, ...) 
{
    	if (level > current_level) 
	{
        	return; 
    	}
	time_t now = time(NULL);
    	struct tm *t = localtime(&now);
    	char time_buf[20];
    	strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", t);

    	fprintf(stderr, "[%s] [%s] ", time_buf, level_to_string(level));

    	va_list args;
    	va_start(args, format);
    	vfprintf(stderr, format, args);
    	va_end(args);

	fprintf(stderr, "\n");
}
