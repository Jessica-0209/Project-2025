#include "hostapd_listener.h"
#include "log.h"
#include "network.h"

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

int sockfd = -1;
char socket_path[128] = {0};
static char client_path[128] = {0}; 
char hostapd_config_ack[128] = {0};

command hostapd_cmds = {
    .commands = {
        [CMD_GET_CONFIG] = "GET_CONFIG",
        [CMD_STATUS]     = "STATUS",
        [CMD_ATTACH]     = "ATTACH"
    }
};

int send_hostapd_command(const int sockfd,const command_type type)
{
	struct sockaddr_un remote_addr;
    	memset(&remote_addr, 0, sizeof(remote_addr));
    	remote_addr.sun_family = AF_UNIX;
    	strncpy(remote_addr.sun_path, socket_path, sizeof(remote_addr.sun_path) - 1);
    
	const char *cmd = hostapd_cmds.commands[type];
	LOG_DEBUG("Sending hostapd command [%d]: %s", type, cmd);

	ssize_t sent = sendto(sockfd, cmd, strlen(cmd), 0, (struct sockaddr *)&remote_addr, sizeof(remote_addr));

	if (sent < 0)
	{
		LOG_ERROR("sendto() failed: %s", strerror(errno));
		LOG_DEBUG("Closing hostapd socket: fd=%d", sockfd);
		close(sockfd);

		if (unlink_socket_path(client_path) != 0)
		{
			LOG_ERROR("[SOCKET] Could not clean up old socket. Aborting...");
			return -1;
		}
		LOG_DEBUG("[SOCKET] CLI socket path cleaned");

		return -1;
	}
	LOG_DEBUG("%s command sent successfully", cmd);

	char resp[256] = {0};

	ssize_t resp_len = recv(sockfd, resp, sizeof(resp) - 1, 0);

	if (resp_len > 0)
	{
		resp[resp_len] = '\0';
		LOG_DEBUG("Response to %s: %s", cmd, resp);

		if (type == CMD_GET_CONFIG)
    		{
        		strncpy(hostapd_config_ack, resp, sizeof(hostapd_config_ack) - 1);
        		hostapd_config_ack[sizeof(hostapd_config_ack) - 1] = '\0';
    		}
	}
	else if (resp_len == 0)
	{
		LOG_WARN("recv(%s) returned 0: No data received", cmd);
		return -1;
	}
	else
	{
		LOG_WARN("recv(%s) failed: %s", cmd, strerror(errno));
		return -1;
	}

	return 0;
}

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

int hostapd_listener_init() 
{
	if (get_hostapd_socket_path(socket_path, sizeof(socket_path)) != 0)
        {
                LOG_ERROR("Failed to determine hostapd socket path");
                return -1;
        }

	const char *iface = strrchr(socket_path, '/');
	
	if (iface && *(iface + 1))
	{
    		LOG_DEBUG("Connected to wireless interface: %s", iface + 1);
	}
	else
	{
    		LOG_DEBUG("Connected to wireless interface (unknown)");
	}

	struct sockaddr_un local_addr;
	struct sockaddr_un remote_addr;
	
	LOG_DEBUG("Initializing hostapd listener...");

	sockfd = create_socket(AF_UNIX, SOCK_DGRAM, 0); 
	LOG_DEBUG("hostapd socket created: fd=%d", sockfd);

	if (sockfd < 0) 
	{
		LOG_ERROR("Failed to create UNIX socket");
		return -1;
	}
	LOG_DEBUG("Created socket with fd=%d", sockfd);

	memset(&local_addr, 0, sizeof(local_addr));
	local_addr.sun_family = AF_UNIX;
	snprintf(client_path, sizeof(client_path), "/tmp/wifi_mqtt_socket_%d", getpid());
	strncpy(local_addr.sun_path, client_path, sizeof(local_addr.sun_path) - 1);

	if (unlink_socket_path(client_path) != 0)
        {
                LOG_ERROR("[SOCKET] Could not clean up old socket. Aborting...");
                return -1;
        }
        LOG_DEBUG("[SOCKET] CLI socket path cleaned");

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

	if (send_hostapd_command(sockfd, CMD_ATTACH) != 0) 
	{
    		LOG_ERROR("Failed to send ATTACH command to hostapd");
    		return -1;
	}

        if (send_hostapd_command(sockfd, CMD_STATUS) != 0)
        {
                LOG_ERROR("Failed to send STATUS command to hostapd");
                return -1;
        }

        if (send_hostapd_command(sockfd, CMD_GET_CONFIG) != 0)
        {
                LOG_ERROR("Failed to send GET_CONFIG command to hostapd");
                return -1;
        }

	return 0;
}

/*
 * Function: get_connected_clients()
 * ------------------------------------------
 *
 *  Function to calculate the number of clients connected.
 *
 *  Returns: Returns the number of clients connected.
 *
 */

int get_connected_clients()
{
    	if (sockfd < 0)
    	{
        	LOG_ERROR("[SOCKET] Hostapd socket not initialized");
        	return 0;
    	}
	LOG_DEBUG("[SOCKET] Hostapd socket initialized!");

	if (get_hostapd_socket_path(socket_path, sizeof(socket_path)) != 0)
        {
                LOG_ERROR("Failed to send LIST_STA");
                return 0;
        }

	struct sockaddr_un remote_addr;	
	memset(&remote_addr, 0, sizeof(remote_addr));
        remote_addr.sun_family = AF_UNIX;
        strncpy(remote_addr.sun_path, socket_path, sizeof(remote_addr.sun_path) - 1);

	const char *cmd = "LIST_STA";
	LOG_DEBUG("[get_connected_clients] Sending LIST_STA to %s", remote_addr.sun_path);

	LOG_DEBUG("Sending hostapd command: [%s]", cmd);
        if (sendto(sockfd, cmd, strlen(cmd), 0, (struct sockaddr *)&remote_addr, sizeof(remote_addr)) < 0)
        {
                LOG_ERROR("Failed to send LIST_STA: %s", strerror(errno));
                return 0;
        }

    	char buf[2048] = {0};
	ssize_t len = recv(sockfd, buf, sizeof(buf) - 1, 0);

	if (len <= 0)
	{
		LOG_ERROR("recv(LIST_STA) failed: %s", strerror(errno));
		return 0;
	}

	buf[len] = '\0';
	LOG_DEBUG("LIST_STA response:\n%s", buf);

	int count = 0;
    	char *line = strtok(buf, "\n");
    	
	while (line)
    	{
        	if (strlen(line) > 0)
		{
            		count++;
		}
		line = strtok(NULL, "\n");
    	}

    	LOG_DEBUG("Total connected clients via hostapd: %d", count);
    	return count;
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
        	
		if (unlink_socket_path(client_path) != 0)
        	{
                	LOG_ERROR("[SOCKET] Could not clean up old socket. Aborting...");
                	return;
        	}
        	LOG_DEBUG("[SOCKET] CLI socket path cleaned");

		sockfd = -1;

		LOG_INFO("Hostapd listener socket closed and cleaned up");
    	}
	else
	{
		LOG_WARN("Socket was already closed");
	}
}

