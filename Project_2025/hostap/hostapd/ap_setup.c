#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void run_command(const char *command) 
{
    	printf("[CMD] %s\n", command);
    	int ret = system(command);
    	
	if (ret != 0) 
	{
        	fprintf(stderr, "Command failed: %s\n", command);
        	exit(EXIT_FAILURE);
    	}
}

void get_wireless_interface(char *interface, size_t size) 
{
    	FILE *fp = popen("iw dev | awk '$1==\"Interface\"{print $2}' | head -n 1", "r");
    	
	if (fp == NULL) 
	{
        	perror("Failed to run command to get interface");
        	exit(EXIT_FAILURE);
    	}

    	if (fgets(interface, size, fp) == NULL) 
	{
        	fprintf(stderr, "No wireless interface found.\n");
        	pclose(fp);
        	exit(EXIT_FAILURE);
    	}

    	interface[strcspn(interface, "\n")] = '\0';

    	pclose(fp);
}

int main() 
{
	char interface[32] = {0};

    	get_wireless_interface(interface, sizeof(interface));
    	printf("Using wireless interface: %s\n", interface);

    	run_command("sudo systemctl stop NetworkManager");

    	char cmd[128] = {0};
    	snprintf(cmd, sizeof(cmd), "sudo ip link set %s down", interface);
    	run_command(cmd);

    	snprintf(cmd, sizeof(cmd), "sudo ip addr flush dev %s", interface);
    	run_command(cmd);

    	snprintf(cmd, sizeof(cmd), "sudo ip addr add 192.168.25.1/24 dev %s", interface);
    	run_command(cmd);

    	snprintf(cmd, sizeof(cmd), "sudo iw dev %s set type __ap", interface);
    	run_command(cmd);

    	snprintf(cmd, sizeof(cmd), "sudo ip link set %s up", interface);
    	run_command(cmd);

    	printf("Access Point setup complete on %s\n", interface);

    	printf("Starting hostapd...\n");
    	run_command("sudo ./hostapd hostapd.conf");

    	return 0;
}

