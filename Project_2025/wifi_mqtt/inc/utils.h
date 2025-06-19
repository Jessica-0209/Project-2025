#ifndef UTILS_H
#define UTILS_H

char *read_file(const char *filename);

#define MAX_TOPICS 5
#define MAX_TOPIC_LEN 128

typedef struct
{
        char mqtt_host[128];
        int mqtt_port;
	char mqtt_topics[MAX_TOPICS][128];
    	int topic_count;
} mqtt_json;

int parse_mqtt_config(const char *filename, mqtt_json *config);

char *build_event_json(const char *event_str, const char *ssid);

char* build_sysinfo_json(const char *hostapd_config);

#endif

