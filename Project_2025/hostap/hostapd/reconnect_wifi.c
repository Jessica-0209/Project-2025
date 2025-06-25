#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAX_BUFFER 128

void get_wifi_interface(char *interface, size_t size) 
{    	
	FILE *fp = popen("iw dev | awk '$1==\"Interface\"{print $2}' | head -n 1", "r");
    	if (fp == NULL) 
    	{
        	perror("Failed to run command");
        	exit(1);
    	}

    	if (fgets(interface, size, fp) == NULL)
   	{
        	printf("No wireless interface found.\n");
        	pclose(fp);
    		exit(1);
    	}

    	interface[strcspn(interface, "\n")] = 0;
    	pclose(fp);
}

void restart_network_manager(void) 
{
    	printf("Restarting NetworkManager...\n");
    	int ret = system("sudo systemctl restart NetworkManager");
    	
    	if (ret != 0) 
    	{
        	printf("Failed to restart NetworkManager.\n");
        	exit(1);
    	}
    	sleep(10);
}

int check_wifi_status(const char *interface) 
{
    	char cmd[256] = {0};
    	snprintf(cmd, sizeof(cmd), "nmcli -t -f GENERAL.STATE device show %s | awk -F: '{print $2}' | tr -d ' '", interface);

    	FILE *fp = popen(cmd, "r");
    	if (fp == NULL) 
    	{
        	perror("Failed to run command");
        	exit(1);
    	}

    	char status[MAX_BUFFER] = {0};
    	
    	if (fgets(status, sizeof(status), fp) != NULL) 
    	{
        	status[strcspn(status, "\n")] = 0;
    	}
    	pclose(fp);

    	if (strstr(status, "100") != NULL) 
    	{
        	printf("Wi-Fi successfully connected on %s.\n", interface);
        	return 1;
    	} 
    	else 
    	{
        	printf("Wi-Fi connection failed on %s.\n", interface);
        	return 0;
    	}
}

void update_dns(void) 
{
    	int ret = system("echo \"nameserver 8.8.8.8\" | sudo tee /etc/resolv.conf > /dev/null");
    	
    	if (ret != 0) 
    	{
        	printf("Failed to update DNS.\n");
    	}
}

int main(void) 
{
    	char interface[MAX_BUFFER] = {0};

    	get_wifi_interface(interface, sizeof(interface));
    	restart_network_manager();

    	if (!check_wifi_status(interface)) 
    	{
        	exit(1);
    	}

    	update_dns();

    	return 0;
}

