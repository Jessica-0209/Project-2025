#ifndef WIFI_HASH_H
#define WIFI_HASH_H

typedef struct Wifi_Event {
    	char mac[32];
    	char ssid[64];
    	char event_type[32];
    	char timestamp[64];
    	struct Wifi_Event* next;
} Wifi_Event;

extern Wifi_Event* hash_table[];

int insert_or_update(const char* mac, const char* ssid, const char* event_type, const char* timestamp);

void parse_and_insert(const char* json_str);

void display();
void get_hash_table_as_string(char* output, size_t max_len);
void free_table();

#endif
