#include <sys/ioctl.h>
#include <net/if.h>
#include <linux/wireless.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <ctype.h>
#include <stdio.h>
#include <errno.h>

#include "network.h"
#include "log.h"

/* Function: get_wireless_interface()
 * ------------------------------------------
 *
 * Scans the available network interfaces and finds the first active wireless interface.
 * Uses `ioctl` with `SIOCGIFCONF` to list interfaces and `SIOCGIWNAME` to detect wireless.
 *
 * iface_name: Buffer to store the name of the detected wireless interface.
 * max_len:    Maximum length of the buffer `iface_name`.
 *
 * Returns: 0 on success (wireless interface found), -1 on failure.
 */

int get_wireless_interface(char *iface_name, size_t max_len)
{
    	int sock = socket(AF_INET, SOCK_DGRAM, 0);
    	
    	if (sock < 0)
    	{
        	return -1;
	}
    	struct ifconf ifc;
    	struct ifreq ifr[16] = {0}; 
    	ifc.ifc_buf = (char *)ifr;
    	ifc.ifc_len = sizeof(ifr);

    	if (ioctl(sock, SIOCGIFCONF, &ifc) == -1)
    	{
        	close(sock);
        	return -1;
    	}

    	int num_ifaces = ifc.ifc_len / sizeof(struct ifreq);
    	
    	for (int i = 0; i < num_ifaces; i++)
    	{	
        	struct iwreq pwrq;
        	memset(&pwrq, 0, sizeof(pwrq));
        	strncpy(pwrq.ifr_name, ifr[i].ifr_name, IFNAMSIZ);

        	if (ioctl(sock, SIOCGIWNAME, &pwrq) != -1)  
        	{
            		strncpy(iface_name, ifr[i].ifr_name, max_len);
            		close(sock);
            		return 0;
        	}
    	}

    	close(sock);
    	return -1;
}

/* Function: get_hostapd_socket_path()
 * ------------------------------------------
 *
 * Constructs the full path to the hostapd control socket based on the active wireless interface.
 * First detects the wireless interface using `get_wireless_interface()`, then combines it
 * with the hostapd control directory path (defined by HOSTAPD_DIR).
 *
 * socket_path: Buffer to store the resulting hostapd socket path.
 * max_len:     Maximum length of the buffer `socket_path`.
 *
 * Returns: 0 on success (socket path constructed), -1 on failure (no wireless interface found).
 */

int get_hostapd_socket_path(char *socket_path, size_t max_len)
{
    	char iface[IFNAMSIZ] = {0};

    	if (get_wireless_interface(iface, sizeof(iface)) != 0)
    	{
        	LOG_ERROR("No wireless interface found");
        	return -1;
    	}

    	snprintf(socket_path, max_len, "%s/%s", HOSTAPD_DIR, iface);
    	return 0;
}

/*void get_hostapd_config(cJSON *root)
{
    	DIR *proc_dir = opendir("/proc");
    	
	if (!proc_dir)
    	{
        	LOG_ERROR("Failed to open /proc directory");
        	return;
    	}

    	char config_path[256] = {0};
	char binary_path[512] = {0};
    	struct dirent *entry;

    	while ((entry = readdir(proc_dir)) != NULL)
    	{
        	if (!isdigit(entry->d_name[0]))
            	{
			continue;
		}

        	char cmdline_path[512] = {0};
        	snprintf(cmdline_path, sizeof(cmdline_path), "/proc/%s/cmdline", entry->d_name);

        	FILE *fp = fopen(cmdline_path, "r");
        	
		if (!fp)
		{
            		continue;
		}
        
		char cmdline[1024] = {0};
       		size_t len = fread(cmdline, 1, sizeof(cmdline) - 1, fp);
        	fclose(fp);

        	if (len == 0)
		{
            		continue;
    		}
        	cmdline[len] = '\0';

		char *arg = cmdline;
		int arg_index = 0;

		while (arg < cmdline + len)
            	{
			if (arg_index == 0 && (strcmp(arg, "hostapd") == 0 || strstr(arg, "/hostapd")))
            		{
                		strncpy(binary_path, arg, sizeof(binary_path) - 1);
            		}
			
			if (strstr(arg, ".conf"))
                	{
            			if (arg[0] == '/')
                    		{
                        		strncpy(config_path, arg, sizeof(config_path) - 1);
                    		}
                   		else if (strlen(binary_path) > 0)
                    		{
                       			char *last_slash = strrchr(binary_path, '/');
                       		
					if (last_slash)
                       			{
                        			*last_slash = '\0'; 
                            			snprintf(config_path, sizeof(config_path), "%s/%s", binary_path, arg);
                        		}
                		}
				else
               			{
                    			char cwd[256] = {0};
                    			if (getcwd(cwd, sizeof(cwd)))
                    			{
                        			snprintf(config_path, sizeof(config_path), "%s/%s", cwd, arg);
                    			}
                		}
				break;
            		}
			arg += strlen(arg) + 1;
            		arg_index++;
		}		

       		if (strlen(config_path) > 0)
    		{ 
			break;
		}
       	}

    	closedir(proc_dir);

    	if (strlen(config_path) == 0)
    	{
        	LOG_WARN("Could not detect hostapd config path from running process");
        	return;
    	}

    	LOG_DEBUG("Detected hostapd config file: %s", config_path);

    	FILE *fp = fopen(config_path, "r");
    	
	if (!fp)
    	{
        	LOG_ERROR("Failed to open detected hostapd config: %s", config_path);
        	return;
    	}

    	cJSON *config_obj = cJSON_CreateObject();
    	char line[256] = {0};

    	while (fgets(line, sizeof(line), fp))
    	{
        	if (line[0] == '#' || line[0] == '\n') 
		{
			continue;
		}

        	char *eq = strchr(line, '=');
        	
		if (!eq)
		{
			continue;
		}

        	*eq = '\0';
        	char *key = line;
        	char *val = eq + 1;
        	val[strcspn(val, "\r\n")] = '\0';

        	cJSON_AddStringToObject(config_obj, key, val);
        	LOG_DEBUG("Parsed config key=%s, value=%s", key, val);
    	}

    	fclose(fp);
    	cJSON_AddItemToObject(root, "hostapd_config", config_obj);
    	LOG_DEBUG("Appended hostapd config to sysinfo JSON from: %s", config_path);
}*/

/* Function: unlink_socket_path()
 * -------------------------------------------
 * This function checks if the provided socket file path exists on the filesystem.
 * If it does, it unlinks (removes) it.
 *
 * path: The path of the Unix domain socket file
 *
 * Returns: 0 on success, -1 on failure
 *
 */

int unlink_socket_path(const char *path)
{
    	if (!path)
    	{
        	fprintf(stderr, "[UNLINK] Invalid path: NULL\n");
        	return -1;
    	}

    	if (unlink(path) == 0)
    	{
        	printf("[UNLINK] Successfully unlinked socket path: %s\n", path);
        	return 0;
    	}
    	else if (errno == ENOENT)
    	{
        	printf("[UNLINK] Path does not exist, nothing to unlink: %s\n", path);
        	return 0;
    	}
    	else
    	{
        	fprintf(stderr, "[UNLINK] Failed to unlink %s: %s\n", path, strerror(errno));
        	return -1;
    	}
}
