#ifndef WIFI_HASH_H
#define WIFI_HASH_H

// ========== Wi-Fi Event Hash Table ==========

typedef struct Wifi_Event {
    char mac[32];
    char ssid[64];
    char event_type[32];
    char timestamp[64];
    struct Wifi_Event* next;
} Wifi_Event;

extern Wifi_Event* wifi_hash_table[];

int insert_or_update_the_hash(const char* mac, const char* ssid, const char* event_type, const char* timestamp);
void display_wifi_table();
void get_wifi_table_as_string(char* output, size_t max_len);
void free_wifi_table();


// ========== System Info Hash Table ==========

typedef struct SysInfo {
    char hostname[64];
    double cpu_usage;
    double memory_usage;
    char timestamp[64];
    struct SysInfo* next;
} SysInfo;

extern SysInfo* sysinfo_hash_table[];

int insert_or_update_sysinfo(const char* hostname, double cpu, double mem, const char* timestamp);
void display_sysinfo_table();
void get_sysinfo_table_as_string(char* output, size_t max_len);
void free_sysinfo_table();

#endif
