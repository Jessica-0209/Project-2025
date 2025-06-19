#include "log.h"
#include <time.h>
#include <stdarg.h>
#include <stdio.h>

static LogLevel current_level = LOG_LEVEL_INFO;

static const char* level_to_string(LogLevel level);

/* Function: set_log_level()
 * ------------------------------------------
 *
 * Sets the current global log level. Messages below this level will be ignored.
 *
 * level: The minimum log level to display (e.g., LOG_LEVEL_INFO, LOG_LEVEL_DEBUG).
 *
 * Returns: void
 */

void set_log_level(LogLevel level)
{
	current_level = level;
}

/* Function: level_to_string()
 * ------------------------------------------
 *
 * Converts a LogLevel enum value into its corresponding string representation.
 * Used internally for displaying log prefixes.
 *
 * level: Log level enum value.
 *
 * Returns: String representation of the log level ("ERROR", "WARNING", etc.).
 */

static const char* level_to_string(LogLevel level)
{
	switch (level)
	{
		case LOG_LEVEL_ERROR:
			return "ERROR";
		case LOG_LEVEL_WARNING:
			return "WARNING";
		case LOG_LEVEL_INFO:
			return "INFO";
		case LOG_LEVEL_DEBUG:
			return "DEBUG";
		default:
			return "UNKNOWN";
	}
}

/* Function: log_message()
 * ------------------------------------------
 *
 * Logs a formatted message with timestamp and severity level if it meets the
 * current log level threshold.
 *
 * level:  The severity level of the message.
 * format: printf-style format string for the log message.
 * ...   : Additional arguments matching the format string.
 *
 * Returns: void
 */

void log_message(LogLevel level, const char *format, ...)
{
	if (level > current_level)
	{
		if (current_level >= LOG_LEVEL_DEBUG)
		{
			fprintf(stderr, "[LOG][DEBUG] Skipping message at level %s due to current_level %s\n",
				level_to_string(level), level_to_string(current_level));
		}
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

