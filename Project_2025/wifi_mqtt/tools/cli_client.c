#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <string.h>

#include "log.h"

#define CLI_SOCKET_PATH "/tmp/wifi_mqtt_cli.sock"

int main() 
{
    	int sock;
    	struct sockaddr_un addr;
    	char *message = "show";
	char response[4096]; 

    	sock = socket(AF_UNIX, SOCK_STREAM, 0);
    	if (sock < 0) 
    	{
        	perror("socket");
        	return 1;
    	}

    	memset(&addr, 0, sizeof(addr));
    	addr.sun_family = AF_UNIX;
    	strncpy(addr.sun_path, CLI_SOCKET_PATH, sizeof(addr.sun_path)-1);

    	if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) 
    	{
        	perror("connect");
        	close(sock);
        	return 1;
    	}

    	write(sock, message, strlen(message));
    	
	int len = read(sock, response, sizeof(response) - 1);
    	if (len > 0)
    	{
        	response[len] = '\0';  
        	LOG_INFO("%s\n", response);
    	}
    	else
    	{
        	LOG_INFO("No response received.\n");
    	}

	close(sock);
    	return 0;
}
