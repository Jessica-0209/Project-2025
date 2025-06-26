#ifndef NETWORK_H
#define NETWORK_H

#include <stddef.h>
#include <cjson/cJSON.h>

#define HOSTAPD_DIR "/var/run/hostapd"

int create_socket(const int domain,const int type,const int protocol);

int get_wireless_interface(char *iface_name, size_t max_len);

int get_hostapd_socket_path(char *socket_path, size_t max_len);

void get_hostapd_config(cJSON *root);

int unlink_socket_path(const char *path);

#endif 
