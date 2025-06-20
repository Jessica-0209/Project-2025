#ifndef UTILS_H
#define UTILS_H

#define MAX_TOPICS 5
#define MAX_TOPIC_LENGTH 128

char *read_file(const char *filename);

typedef struct
{
        char *mqtt_host;
        char mqtt_topics[MAX_TOPICS][MAX_TOPIC_LENGTH];
        int topic_count;
        int mqtt_port;
} mqtt_json;

int parse_mqtt_config(const char *filename, mqtt_json *config);

char *build_event_json(const char *event_str, const char *ssid);

char *build_sysinfo_json(const char *hostapd_config);

#endif

