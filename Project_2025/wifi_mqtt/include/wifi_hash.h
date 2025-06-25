#ifndef WIFI_HASH_H
#define WIFI_HASH_H

typedef struct Wifi_Event {
    char mac[32];
    char hostname[128];
    char ssid[64];
    char event_type[32];
    char timestamp[64];
    struct Wifi_Event* next;
} Wifi_Event;

extern Wifi_Event* wifi_hash_table[];

int insert_or_update_the_hash_table(const char* mac, const char* hostname, const char* ssid, const char* event_type, const char* timestamp);
void display_wifi_table(void);
void get_wifi_table_as_string(char* output, size_t max_len);
void free_wifi_table(void);

#endif
