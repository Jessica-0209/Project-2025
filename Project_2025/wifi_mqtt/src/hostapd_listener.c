#include "hostapd_listener.h"
#include "log.h"

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

static int sockfd = -1;
static char client_path[108]; 

/* Function: hostapd_listener_init()
 * ------------------------------------------
 *
 * Initializes a UNIX datagram socket to communicate with the hostapd control interface.
 * Binds to a unique client socket path and sends ATTACH and STATUS commands to hostapd.
 *
 * socket_path: path to the hostapd control socket
 *
 * Returns: 0 on success, -1 on failure
 */

int hostapd_listener_init(const char *socket_path) 
{

	if (NULL == *socket_path)
	{
		return  -1;
	}
	struct sockaddr_un local_addr;
	struct sockaddr_un remote_addr;
	
	LOG_DEBUG("Initializing hostapd listener...");

	sockfd = socket(AF_UNIX, SOCK_DGRAM, 0);

	if (sockfd < 0) 
	{
		LOG_ERROR("socket() failed: %s", strerror(errno));
		return -1;
	}
	LOG_DEBUG("Created socket with fd=%d", sockfd);

	memset(&local_addr, 0, sizeof(local_addr));
	local_addr.sun_family = AF_UNIX;
	snprintf(client_path, sizeof(client_path), "/tmp/wifi_mqtt_socket_%d", getpid());
	strncpy(local_addr.sun_path, client_path, sizeof(local_addr.sun_path) - 1);
	// check failed or sucess 
	// goto  
	unlink(client_path); 

	LOG_DEBUG("Client path set to %s", client_path);

	if (bind(sockfd, (struct sockaddr *)&local_addr, sizeof(local_addr)) < 0) 
	{
		LOG_ERROR("bind() failed: %s", strerror(errno));
		close(sockfd);
		return -1;
	}
	LOG_DEBUG("Socket bind successful");

	memset(&remote_addr, 0, sizeof(remote_addr));
	remote_addr.sun_family = AF_UNIX;
	strncpy(remote_addr.sun_path, socket_path, sizeof(remote_addr.sun_path) - 1);

	LOG_DEBUG("Attempting to send ATTACH command to %s", socket_path);

	const char *attach_cmd = "ATTACH";

    	if (sendto(sockfd, attach_cmd, strlen(attach_cmd), 0, (struct sockaddr *)&remote_addr, sizeof(remote_addr)) < 0)
    	{
        	LOG_ERROR("sendto(ATTACH) failed: %s", strerror(errno));
        	close(sockfd);
        	unlink(client_path);
        	return -1;
    	}
	LOG_DEBUG("ATTACH command sent successfully");

    	char attach_resp[128] = {0};
    	
	ssize_t resp_len = recv(sockfd, attach_resp, sizeof(attach_resp) - 1, 0);
    	
	if (resp_len > 0)
    	{
        	attach_resp[resp_len] = '\0';
        	LOG_DEBUG("ATTACH Response: %s", attach_resp);
    	}
    	else
    	{
        	LOG_WARN("recv(ATTACH) failed: %s", strerror(errno));
    	}

	const char *cmd = "STATUS";
	LOG_DEBUG("Sending STATUS command");

	ssize_t sent = sendto(sockfd, cmd, strlen(cmd), 0, (struct sockaddr *)&remote_addr, sizeof(remote_addr));
	
	if (sent < 0)
	{
		LOG_ERROR("sendto(STATUS) failed: %s", strerror(errno));
		close(sockfd);
		unlink(client_path);
		
		sockfd = -1;
		return -1;
	}

	LOG_INFO("Connected to hostapd socket and sent STATUS");

	char ack[128] = {0};
	
	ssize_t ack_len = recv(sockfd, ack, sizeof(ack)-1, 0);
	
	if (ack_len > 0) 
	{
		ack[ack_len] = '\0';
		LOG_DEBUG("Received after STATUS: %s", ack);
	} 
	else 
	{
		LOG_WARN("recv(STATUS ACK) failed: %s", strerror(errno));
	}

	return 0;
}

/* Function: hostapd_listener_receive()
 * ------------------------------------------
 *
 * Receives hostapd event messages from the initialized listener socket.
 *
 * buffer: destination buffer to store received data
 * bufsize: size of the destination buffer
 *
 * Returns: number of bytes received on success, -1 on failure
 */

ssize_t hostapd_listener_receive(char *buffer, size_t bufsize) 
{
    	if (sockfd < 0) 
    	{
		LOG_ERROR("Listener socket not initialized");
	    	return -1;
    	}

	LOG_DEBUG("Waiting to receive data from hostapd...");

    	ssize_t len = recv(sockfd, buffer, bufsize - 1, 0);
    	if (len > 0) 
	{
		buffer[len] = '\0';
		LOG_DEBUG("Received message: %s", buffer);
	}
	else
	{
		LOG_WARN("recv() failed: %s", strerror(errno));
	}

    	return len;
}

/* Function: hostapd_listener_cleanup()
 * ------------------------------------------
 *
 * Cleans up the hostapd listener by closing the socket and removing the client socket file.
 *
 * Takes no arguments.
 *
 * Returns: void
 */

void hostapd_listener_cleanup() 
{
	LOG_DEBUG("Cleaning up hostapd listener...");

    	if (sockfd >= 0) 
    	{
        	close(sockfd);
		unlink(client_path);
        	sockfd = -1;

		LOG_INFO("Hostapd listener socket closed and cleaned up");
    	}
	else
	{
		LOG_WARN("Socket was already closed");
	}
}

