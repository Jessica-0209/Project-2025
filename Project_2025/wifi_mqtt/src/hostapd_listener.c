#include "hostapd_listener.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

static int sockfd = -1;
static char client_path[108]; 
/*
 *
 *
 */
int hostapd_listener_init(const char *socket_path) 
{
	struct sockaddr_un local_addr;
	struct sockaddr_un remote_addr;
	
	sockfd = socket(AF_UNIX, SOCK_DGRAM, 0);
	if (sockfd < 0) 
	{
		perror("socket");
		return -1;
	}

	memset(&local_addr, 0, sizeof(local_addr));
	local_addr.sun_family = AF_UNIX;
	snprintf(client_path, sizeof(client_path), "/tmp/wifi_mqtt_socket_%d", getpid());
	strncpy(local_addr.sun_path, client_path, sizeof(local_addr.sun_path) - 1);
	unlink(client_path); 

	if (bind(sockfd, (struct sockaddr *)&local_addr, sizeof(local_addr)) < 0) 
	{
		perror("bind");
		close(sockfd);
		return -1;
	}

	memset(&remote_addr, 0, sizeof(remote_addr));
	remote_addr.sun_family = AF_UNIX;
	strncpy(remote_addr.sun_path, socket_path, sizeof(remote_addr.sun_path) - 1);

	const char *attach_cmd = "ATTACH";
    	if (sendto(sockfd, attach_cmd, strlen(attach_cmd), 0, (struct sockaddr *)&remote_addr, sizeof(remote_addr)) < 0)
    	{
        	perror("sendto ATTACH");
        	close(sockfd);
        	unlink(client_path);
        	return -1;
    	}

    	char attach_resp[100] = {0};
    	
	ssize_t resp_len = recv(sockfd, attach_resp, sizeof(attach_resp) - 1, 0);
    	if (resp_len > 0)
    	{
        	attach_resp[resp_len] = '\0';
        	printf("[DEBUG] ATTACH Response: %s\n", attach_resp);
    	}
    	else
    	{
        	perror("recv ATTACH");
    	}

	const char *cmd = "STATUS";
	ssize_t sent = sendto(sockfd, cmd, strlen(cmd), 0, (struct sockaddr *)&remote_addr, sizeof(remote_addr));
	if (sent < 0)
	{
		perror("sendto STATUS");
		close(sockfd);
		unlink(client_path);
		sockfd = -1;
		return -1;
	}

	printf("[INFO] Connected to hostapd socket and sent STATUS\n");

	char ack[100] = {0};
	ssize_t ack_len = recv(sockfd, ack, sizeof(ack)-1, 0);
	if (ack_len > 0) 
	{
		ack[ack_len] = '\0';
		printf("[DEBUG] Received after STATUS: %s\n", ack);
	} 
	else 
	{
		perror("recv");
	}

	return 0;
}

ssize_t hostapd_listener_receive(char *buffer, size_t bufsize) 
{
    	if (sockfd < 0) 
    	{
	    	return -1;
    	}

    	ssize_t len = recv(sockfd, buffer, bufsize - 1, 0);
    	if (len > 0) 
	{
		buffer[len] = '\0';
	}
    	return len;
}

void hostapd_listener_cleanup() 
{
    	if (sockfd >= 0) 
    	{
        	close(sockfd);
		unlink(client_path);
        	sockfd = -1;
    	}
}

