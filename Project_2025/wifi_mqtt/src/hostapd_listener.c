#include "hostapd_listener.h"
#include "log.h"
#include "utils.h"
#include "network.h"

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <stdio.h>
#include <cjson/cJSON.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

int sockfd = -1;
static char client_path[128] = {0}; 
char hostapd_config_ack[128] = {0};

/* Function: hostapd_listener_init()
 * ------------------------------------------
 *
 * Initializes a UNIX datagram socket to communicate with the hostapd control interface.
 * Binds to a unique client socket path and sends ATTACH and STATUS commands to hostapd.
 *
 * socket_path: path to the hostapd control socket (usually /var/run/hostapd/wlan0)
 *
 * Returns: 0 on success, -1 on failure
 */

int hostapd_listener_init()
{
        char socket_path[128] = {0};
        if (get_hostapd_socket_path(socket_path, sizeof(socket_path)) != 0)
        {
                LOG_ERROR("Failed to determine hostapd socket path");
                return -1;
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

        unlink_socket_path(client_path);
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
        if (sendto(sockfd, attach_cmd, strlen(attach_cmd), 0,
                   (struct sockaddr *)&remote_addr, sizeof(remote_addr)) < 0)
        {
                LOG_ERROR("sendto(ATTACH) failed: %s", strerror(errno));
                close(sockfd);
                unlink_socket_path(client_path);
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

        const char *cmd = "GET_CONFIG";
        LOG_DEBUG("Sending GET_CONFIG command");
        if (sendto(sockfd, cmd, strlen(cmd), 0, (struct sockaddr *)&remote_addr, sizeof(remote_addr)) < 0)
        {
                LOG_ERROR("sendto(GET_CONFIG) failed: %s", strerror(errno));
                close(sockfd);
                unlink_socket_path(client_path);
                sockfd = -1;
                return -1;
        }
        LOG_INFO("Connected to hostapd socket and sent GET_CONFIG");

	ssize_t ack_len = recv(sockfd, hostapd_config_ack, sizeof(hostapd_config_ack) - 1, 0);
        if (ack_len > 0)
        {
                hostapd_config_ack[ack_len] = '\0';
    		LOG_DEBUG("Received after GET_CONFIG:\n%s", hostapd_config_ack);
	}
        else
        {
                LOG_WARN("recv(GET_CONFIG ACK) failed: %s", strerror(errno));
        }

        return 0;
}

int get_connected_clients()
{
        LOG_DEBUG("[get_connected_clients] sockfd = %d", sockfd);

        if (sockfd < 0)
        {
                LOG_ERROR("[SOCKET] Hostapd socket not initialized");
                return -1;
        }
        LOG_DEBUG("[SOCKET] Hostapd socket initialized!");

	char socket_path[128] = {0}; 

    	if (get_hostapd_socket_path(socket_path, sizeof(socket_path)) != 0)
    	{
        	LOG_ERROR("Failed to get hostapd socket path");
        	return -1;
    	}

    	LOG_DEBUG("[get_connected_clients] Using socket path: %s", socket_path);
    
	struct sockaddr_un remote_addr;
    	memset(&remote_addr, 0, sizeof(remote_addr));
    	remote_addr.sun_family = AF_UNIX;
    	strncpy(remote_addr.sun_path, socket_path, sizeof(remote_addr.sun_path) - 1);
        
	const char *cmd = "STA-FIRST";

        LOG_DEBUG("[get_connected_clients] Sending STA-FIRST to %s", remote_addr.sun_path);

        if (sendto(sockfd, cmd, strlen(cmd), 0, (struct sockaddr *)&remote_addr, sizeof(remote_addr)) < 0)
        {
                LOG_ERROR("Failed to send STA-FIRST: %s", strerror(errno));
                return -1;
        }

        char buf[64] = {0};
        int count = 0;

        while (1)
        {
                ssize_t len = recv(sockfd, buf, sizeof(buf) - 1, 0);
                if (len <= 0)
                {
                        LOG_WARN("recv failed during STA iteration");
                        break;
                }

                buf[len] = '\0';

                if (strncmp(buf, "FAIL", 4) == 0)
                {
                        break;
                }

                count++;

                char cmd_next[128] = {0};
                snprintf(cmd_next, sizeof(cmd_next), "STA-NEXT %s", buf);

                if (sendto(sockfd, cmd_next, strlen(cmd_next), 0, (struct sockaddr *)&remote_addr, sizeof(remote_addr)) < 0)
                {
                        LOG_ERROR("send(STA-NEXT) failed");
                        break;
                }
        }

        LOG_DEBUG("Total connected clients via hostapd: %d", count);
        return count;
}

/*int get_hostapd_config_json_object(cJSON *parent)
{
    	if (sockfd < 0) 
    	{
        	LOG_ERROR("Socket not initialized");
        	return -1;
    	}

    	char socket_path[128] = {0};
    	if (get_hostapd_socket_path(socket_path, sizeof(socket_path)) != 0) 
    	{
        	LOG_ERROR("Failed to get hostapd socket path");
        	return -1;
    	}

    	struct sockaddr_un remote_addr = {0};
    	remote_addr.sun_family = AF_UNIX;
    	strncpy(remote_addr.sun_path, socket_path, sizeof(remote_addr.sun_path) - 1);

    	const char *cmd = "GET_CONFIG";
    	if (sendto(sockfd, cmd, strlen(cmd), 0, (struct sockaddr *)&remote_addr, sizeof(remote_addr)) < 0) 
    	{
        	LOG_ERROR("Failed to send GET_CONFIG: %s", strerror(errno));
        	return -1;
    	}

    	char buf[512] = {0};
    	cJSON *config = cJSON_CreateObject();

    	while (1) 	
    	{
        	ssize_t len = recv(sockfd, buf, sizeof(buf) - 1, 0);
        	if (len <= 0)
		{
			break;
		}

        	buf[len] = '\0';
        	LOG_DEBUG("[GET_CONFIG] Received: %s", buf);

        	if (strncmp(buf, "bssid=", 6) == 0)
		{
            		cJSON_AddStringToObject(config, "bssid", buf + 6);
		}
		else if (strncmp(buf, "ssid=", 5) == 0)
		{
			cJSON_AddStringToObject(config, "ssid", buf + 5);
		}
		else if (strncmp(buf, "wpa=", 4) == 0)
		{
			cJSON_AddStringToObject(config, "wpa", buf + 4);
		}
		else if (strncmp(buf, "key_mgmt=", 9) == 0)
		{
			cJSON_AddStringToObject(config, "key_mgmt", buf + 9);
		}
		else if (strncmp(buf, "group_cipher=", 13) == 0)
		{
			cJSON_AddStringToObject(config, "group_cipher", buf + 13);
		}
		else if (strncmp(buf, "rsn_pairwise_cipher=", 21) == 0)
		{
			cJSON_AddStringToObject(config, "rsn_pairwise_cipher", buf + 21);
    		}	
    	}

    	cJSON_AddItemToObject(parent, "hostapd_config", config);
    	return 0;
}*/

/* Function: hostapd_listener_receive()
 * ------------------------------------------
 *
 * Receives data (hostapd event messages) from the initialized listener socket.
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
		unlink_socket_path(client_path);
		sockfd = -1;

		LOG_INFO("Hostapd listener socket closed and cleaned up");
	}
	else 
	{
		LOG_WARN("Socket was already closed");
	}
}

