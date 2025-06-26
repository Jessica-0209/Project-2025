#ifndef UTILS_H
#define UTILS_H

#define MAX_TOPICS 5
#define MAX_TOPIC_LENGTH 128

#include <stdlib.h>

char *read_file(const char *filename);

typedef struct
{
        char *mqtt_host;
        char mqtt_topics[MAX_TOPICS][MAX_TOPIC_LENGTH];
        int topic_count;
        int mqtt_port;
} mqtt_json;

int parse_mqtt_json_config_file(const char *filename, mqtt_json *config);

int get_hostname_from_mac(const char *mac, char *hostname, size_t hostname_len);

char *build_wifi_event_json(const char *event_str, const char *ssid);

char *build_sysinfo_json(const char *hostapd_config);

#endif

