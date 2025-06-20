#ifndef HOSTAPD_LISTENER_H
#define HOSTAPD_LISTENER_H

#include <stddef.h>   
#include <sys/types.h>

#define EVENT_BUF_SIZE 1024

extern int sockfd;
extern char hostapd_config_ack[128];

int hostapd_listener_init();

int get_connected_clients();

ssize_t hostapd_listener_receive(char *buffer, size_t bufsize);

void hostapd_listener_cleanup();

#endif

