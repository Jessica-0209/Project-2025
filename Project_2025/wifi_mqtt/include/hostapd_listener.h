#ifndef HOSTAPD_LISTENER_H
#define HOSTAPD_LISTENER_H

#include <stddef.h>   
#include <sys/types.h>

#define EVENT_BUF_SIZE 1024

extern int sockfd;
extern char hostapd_config_ack[128];

typedef enum {
    CMD_GET_CONFIG,
    CMD_STATUS,
    CMD_ATTACH,
    CMD_COUNT 
} command_type;

typedef struct {

    const char *commands[CMD_COUNT];

} command;

extern command hostapd_cmds;

int send_hostapd_command(const int sockfd, const command_type type);

int hostapd_listener_init(void);

int get_connected_clients(void);

ssize_t hostapd_listener_receive(char *buffer, size_t bufsize);

void hostapd_listener_cleanup(void);

#endif

