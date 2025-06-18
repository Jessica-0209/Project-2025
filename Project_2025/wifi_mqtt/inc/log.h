#ifndef LOG_H
#define LOG_H

#include <stdio.h>

extern int log_level;

typedef enum {
    LOG_LEVEL_ERROR = 0,
    LOG_LEVEL_WARNING,
    LOG_LEVEL_INFO,
    LOG_LEVEL_DEBUG
} LogLevel;

void set_log_level(LogLevel level);

const char* level_to_string(LogLevel level);

void log_message(LogLevel level, const char *format, ...);

#define LOG_ERROR(...)   log_message(LOG_LEVEL_ERROR, __VA_ARGS__)
#define LOG_WARN(...)    log_message(LOG_LEVEL_WARNING, __VA_ARGS__)
#define LOG_INFO(...)    log_message(LOG_LEVEL_INFO, __VA_ARGS__)
#define LOG_DEBUG(...)   log_message(LOG_LEVEL_DEBUG, __VA_ARGS__)

#endif

