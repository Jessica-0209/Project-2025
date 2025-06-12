#ifndef UTILS_H
#define UTILS_H

char *read_file(const char *filename);

void log_info(const char *format, ...);

typedef struct
{
        char *mqtt_host;
        char *mqtt_topic;
        int mqtt_port;
} mqtt_json;

int parse_mqtt_config(const char *filename, mqtt_json *config);

char *build_event_json(const char *event_str, const char *ssid);

#endif

