#include "network.h"
#include "log.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <net/if.h>
#include <linux/wireless.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/un.h>

#define HOSTAPD_DIR "/var/run/hostapd"
#define MAX_PATH_LEN 128
#define LEASES_FILE "/tmp/dhcp.leases"

/* Function: create_unix_socket()
 * --------------------------------------------------
 *
 * This function can be used to create a UNIX socket for both stream (SOCK_STREAM)
 * and datagram (SOCK_DGRAM) communication.
 *
 * type: Type of socket - SOCK_STREAM or SOCK_DGRAM.
 * path: Filesystem path for the UNIX socket.
 * bind_socket: Whether to bind the socket to the path (1 = yes, 0 = no).
 *
 * Returns: File descriptor on success, -1 on error.
 *
 */

int create_socket(const int domain, const int type, const int protocol)
{
    	int fd = socket(domain, type, protocol);
    	
    	if (fd < 0) 
    	{
        	LOG_ERROR("Socket creation failed [domain=%d, type=%d]: %s", domain, type, strerror(errno));
        	return -1;
    	}

	if (domain != AF_UNIX && domain != AF_INET)
	{
    		LOG_WARN("Unusual socket domain passed: %d", domain);
	}

    	LOG_DEBUG("Socket created successfully [fd=%d, domain=%d, type=%d]", fd, domain, type);
    	return fd;
}

/* Function: get_wireless_interface()
 * ------------------------------------------
 *
 * Scans the available network interfaces and finds the first active wireless interface.
 * Uses `ioctl` with `SIOCGIFCONF` to list interfaces and `SIOCGIWNAME` to detect wireless.
 *
 * iface_name: Buffer to store the name of the detected wireless interface.
 * max_len:    Maximum length of the buffer `iface_name`.
 *
 * Returns: 0 on success, -1 on failure.
 */

int get_wireless_interface(char *iface_name, size_t max_len)
{
	LOG_DEBUG("[WIFI] Attempting to find wireless interface...");

	int sock = socket(AF_INET, SOCK_DGRAM, 0);
    	if (sock < 0)
    	{
		LOG_ERROR("[WIFI] Failed to create socket");
        	return -1;
	}

    	struct ifconf ifc;
    	struct ifreq ifr[16] = {0};
	ifc.ifc_len = sizeof(ifr);
	ifc.ifc_buf = (char *)ifr;

    	if (ioctl(sock, SIOCGIFCONF, &ifc) == -1)
    	{
		LOG_ERROR("[WIFI] ioctl SIOCGIFCONF failed");
        	close(sock);
        	return -1;
    	}

    	int num_ifaces = ifc.ifc_len / sizeof(struct ifreq);
	LOG_DEBUG("[WIFI] Number of interfaces found: %d", num_ifaces);

    	for (int i = 0; i < num_ifaces; i++)
    	{
		LOG_DEBUG("[WIFI] Checking interface: %s", ifr[i].ifr_name);

        	struct iwreq pwrq;
        	memset(&pwrq, 0, sizeof(pwrq));
        	strncpy(pwrq.ifr_name, ifr[i].ifr_name, IFNAMSIZ);

        	if (ioctl(sock, SIOCGIWNAME, &pwrq) != -1)
        	{
			LOG_INFO("[WIFI] Wireless interface found: %s", ifr[i].ifr_name);

            		strncpy(iface_name, ifr[i].ifr_name, max_len);
            		close(sock);
            		return 0;
        	}
		else
                {
                        LOG_DEBUG("[WIFI] %s is not a wireless interface", ifr[i].ifr_name);
                }
    	}

	LOG_WARN("[WIFI] No wireless interface found");
    	close(sock);
    	return -1;
}

/* Function: get_hostapd_socket_path()
 * ------------------------------------------
 *
 * Constructs the full path to the hostapd control socket based on the active wireless interface.
 *
 * socket_path: Buffer to store the resulting hostapd socket path.
 * max_len:     Maximum length of the buffer `socket_path`.
 *
 * Returns: 0 on successful construction of socket, -1 on failure.
 */

int get_hostapd_socket_path(char *socket_path, size_t max_len)
{
	LOG_DEBUG("[HOSTAPD] Getting hostapd control socket path...");

	char iface[IFNAMSIZ] = {0};

    	if (get_wireless_interface(iface, sizeof(iface)) != 0)
    	{
        	LOG_ERROR("[HOSTAPD] No wireless interface found");
        	return -1;
    	}

	LOG_DEBUG("[HOSTAPD] Found wireless interface: %s", iface);

    	snprintf(socket_path, max_len, "%s/%s", HOSTAPD_DIR, iface);
	LOG_INFO("[HOSTAPD] Computed socket path: %s", socket_path);

    	return 0;
}

int get_hostname_from_mac(const char *mac, char *hostname, size_t hostname_len)
{

        FILE *file = fopen(LEASES_FILE, "r");

        if (!file)
        {
                perror("Failed to open DHCP leases file");
                return -1;
        }

        char line[256] = {0};
        while (fgets(line, sizeof(line), file))
        {
                char client_mac[32] = {0};
                char client_hostname[128] = {0};
                if (sscanf(line, "%*s %31s %*s %127s %*s", client_mac, client_hostname) == 2)
                {
                        if (strcasecmp(mac, client_mac) == 0)
                        {
                                strncpy(hostname, client_hostname, hostname_len - 1);
                                hostname[hostname_len - 1] = '\0';
                                fclose(file);
                                return 0;
                        }
                }
        }

        fclose(file);
        return -1;
}

/*
 * Function: unlink_socket_path()
 * ----------------------------------------
 *
 * Function to check if the provided socket file path exists on the filesystem.
 * If it does, it unlinks (removes) it.
 *
 * path: The path of the UNIX domain socket file
 *
 * Returns: 0 on success, -1 on failure
 *
 */

int unlink_socket_path(const char *path) 
{
  	if (!path) 
  	{
    		LOG_ERROR("[UNLINK] Invalid path: NULL");
    		return -1;
  	}

  	if (unlink(path) == 0) 
  	{
    		LOG_INFO("[UNLINK] Successfully unlinked socket path: %s", path);
    		return 0;
  	} 	
  	else if (errno == ENOENT) 
  	{
    		LOG_DEBUG("[UNLINK] Path does not exist, nothing to unlink: %s", path);
    		return 0;
  	} 
  	else 
  	{
    		LOG_ERROR("[UNLINK] Failed to unlink %s: %s", path, strerror(errno));
  		return -1;
  	}
}
