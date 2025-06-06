#ifndef HOSTAPD_LISTENER_H
#define HOSTAPD_LISTENER_H

#include <stddef.h>   
#include <sys/types.h>

#define HOSTAPD_SOCKET_PATH "/var/run/hostapd/wlp44s0"
#define EVENT_BUF_SIZE 1024

int hostapd_listener_init(const char *socket_path);

ssize_t hostapd_listener_receive(char *buffer, size_t bufsize);

void hostapd_listener_cleanup();

#endif

