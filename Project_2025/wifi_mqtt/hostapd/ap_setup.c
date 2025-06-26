#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

char g_interface[32] = {0};
pid_t hostapd_pid = -1;

void start_hostapd(void)
{
    	pid_t pid = fork();

    	if (pid == 0)
    	{
               	execlp("sudo", "sudo", "./hostapd", "hostapd.conf", (char *)NULL);

              	perror("Failed to start hostapd");
        	exit(EXIT_FAILURE);
    	}
    	else if (pid > 0)
    	{
               	hostapd_pid = pid;
        	printf("hostapd started with PID %d\n", hostapd_pid);
    	}
    	else
    	{
        	perror("Failed to fork");
        	exit(EXIT_FAILURE);
    	}
}

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

void update_dns(void)
{
    	int ret = system("echo \"nameserver 8.8.8.8\" | sudo tee /etc/resolv.conf > /dev/null");

    	if (ret != 0)
    	{
        	printf("Failed to update DNS.\n");
    	}
}

void cleanup(int signum)
{
        printf("\n[INFO] Caught signal %d. Cleaning up...\n", signum);

        if (hostapd_pid > 0)
        {
                printf("[INFO] Terminating hostapd (PID %d)...\n", hostapd_pid);
                kill(hostapd_pid, SIGTERM);
                waitpid(hostapd_pid, NULL, 0);
        }

        char cmd[128] = {0};
        snprintf(cmd, sizeof(cmd), "sudo ip link set %s down", g_interface);
        system(cmd);

        system("sudo systemctl restart NetworkManager");
        sleep(10);

        update_dns();

        printf("[INFO] Clean-up complete. Exiting.\n");
        exit(EXIT_SUCCESS);
}

void generate_hostapd_conf(const char *interface)
{
    	FILE *fp = fopen("hostapd.conf", "w");
    	
    	if (fp == NULL)
    	{
        	perror("Failed to create hostapd.conf");
        	exit(EXIT_FAILURE);
    	}

    	fprintf(fp,
            "interface=%s\n"
            "driver=nl80211\n"
            "ssid=test2k25\n"
            "country_code=IN\n"
            "ieee80211d=1\n"
            "ieee80211n=1\n"
            "hw_mode=g\n"
            "channel=5\n"
            "beacon_int=100\n"
            "dtim_period=2\n"
            "ctrl_interface=/var/run/hostapd\n"
            "ctrl_interface_group=0\n"
            "logger_syslog=0\n"
            "logger_syslog_level=0\n"
            "logger_stdout=-1\n"
            "logger_stdout_level=2\n"
            "wpa=2\n"
            "wpa_passphrase=test@123\n"
            "wpa_key_mgmt=WPA-PSK\n"
            "rsn_pairwise=CCMP\n",
            interface);

    	fclose(fp);
    	printf("[INFO] Generated hostapd.conf with interface: %s\n", interface);
}

int main(void) 
{
	signal(SIGINT, cleanup);
	signal(SIGTERM, cleanup);

    	get_wireless_interface(g_interface, sizeof(g_interface));
    	printf("Using wireless interface: %s\n", g_interface);

	generate_hostapd_conf(g_interface);

	run_command("cp defconfig .config");
	run_command("make");

    	run_command("sudo systemctl stop NetworkManager");

    	char cmd[128] = {0};
    	snprintf(cmd, sizeof(cmd), "sudo ip link set %s down", g_interface);
    	run_command(cmd);

    	snprintf(cmd, sizeof(cmd), "sudo ip addr flush dev %s", g_interface);
    	run_command(cmd);

    	snprintf(cmd, sizeof(cmd), "sudo ip addr add 192.168.25.1/24 dev %s", g_interface);
    	run_command(cmd);

    	snprintf(cmd, sizeof(cmd), "sudo iw dev %s set type __ap", g_interface);
    	run_command(cmd);

    	snprintf(cmd, sizeof(cmd), "sudo ip link set %s up", g_interface);
    	run_command(cmd);

    	printf("Access Point setup complete on %s\n", g_interface);

    	printf("Starting hostapd...\n");
    	start_hostapd();
	pause();

    	return 0;
}

