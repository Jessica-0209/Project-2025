#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <mosquitto.h>
#include <cjson/cJSON.h>
#include <sys/utsname.h>

#include "utils.h"
#include "log.h"

extern mqtt_json mqtt_config;

/* Function: get_hostname()
 * ------------------------------------------
 *
 * Retrieves the current system's hostname.
 *
 * Returns: Pointer to a static buffer containing the hostname.
 */

char *get_hostname()
{
    	static char hostname[64];

    	if (gethostname(hostname, sizeof(hostname)) != 0)
	{
		LOG_WARN("[SYSINFO] gethostname failed");
		strcpy(hostname, "unknown");
	}

	LOG_DEBUG("[SYSINFO] Hostname: %s", hostname);
	
	return hostname;
}

/* Function: get_cpu_usage()
 * ------------------------------------------
 *
 * Calculates CPU usage percentage over a 1-second interval.
 * Uses values from /proc/stat before and after 1 second to compute usage.
 *
 * Returns: CPU usage as a percentage, or -1 on error.
 */

double get_cpu_usage()
{
    	FILE *fp = fopen("/proc/stat", "r");

    	if (!fp)
	{
		LOG_ERROR("[SYSINFO] Failed to open /proc/stat for CPU usage");
		return -1;
	}

    	long user, nice, system, idle;
    	fscanf(fp, "cpu %ld %ld %ld %ld", &user, &nice, &system, &idle);
    	fclose(fp);

	LOG_DEBUG("[SYSINFO] CPU Snapshot 1 - user: %ld, nice: %ld, system: %ld, idle: %ld", user, nice, system, idle);

    	sleep(1);

    	FILE *fp2 = fopen("/proc/stat", "r");
    	
	if (!fp2)
	{
		LOG_ERROR("[SYSINFO] Failed to reopen /proc/stat for CPU usage");
		return -1;
	}

    	long user2, nice2, system2, idle2;
    	fscanf(fp2, "cpu %ld %ld %ld %ld", &user2, &nice2, &system2, &idle2);
    	fclose(fp2);
	LOG_DEBUG("[SYSINFO] CPU Snapshot 2 - user: %ld, nice: %ld, system: %ld, idle: %ld", user2, nice2, system2, idle2);

    	long total_diff = (user2 + nice2 + system2 + idle2) - (user + nice + system + idle);
    	long idle_diff = idle2 - idle;
	
    	if (total_diff == 0) 
	{
		return 0;
	}
	
	double usage = 100.0 * (total_diff - idle_diff) / total_diff;
	
	LOG_DEBUG("[SYSINFO] CPU Usage: %.2f%%", usage);
	
	return usage;
}

/* Function: get_memory_usage()
 * ------------------------------------------
 *
 * Computes memory usage by parsing /proc/meminfo.
 * Considers total, free, buffers, and cached memory to estimate used memory.
 *
 * Returns: Memory usage as a percentage (double), or -1 on error.
 */

double get_memory_usage()
{
    	FILE *fp = fopen("/proc/meminfo", "r");
    	
	if (!fp) 
	{
		LOG_ERROR("[SYSINFO] Failed to open /proc/meminfo");
		return -1;
	}

    	long total = 0, free = 0, buffers = 0, cached = 0;
    	char key[32];
    	long value;

    	while (fscanf(fp, "%s %ld kB\n", key, &value) == 2)
    	{
        	if (strcmp(key, "MemTotal:") == 0) 
		{
			total = value;
		}
        	else if (strcmp(key, "MemFree:") == 0) 
		{
			free = value;
		}
        	else if (strcmp(key, "Buffers:") == 0) 
		{
			buffers = value;
		}
        	else if (strcmp(key, "Cached:") == 0) 
		{
			cached = value;
		}
    	}
    	fclose(fp);

    	long used = total - free - buffers - cached;
	
	double usage = used * 1024;
	LOG_DEBUG("[SYSINFO] Memory Usage: %ld (total: %ld kB)", usage, total);
	
	return usage;	
}

/* Function: run_sysinfo_publisher()
 * ------------------------------------------
 *
 * Thread function that continuously publishes system information (hostname, CPU, memory)
 * to the MQTT topic "system/info" every 5 seconds using the cJSON format.
 *
 * arg: argument
 *
 * Returns: NULL
 */

void *run_sysinfo_publisher(void *arg)
{
	LOG_DEBUG("[SYSINFO] Starting system info publisher thread...");

    	struct mosquitto *mosq = mosquitto_new("sysinfo_pub", true, NULL);
    	if (!mosq)
    	{
        	LOG_ERROR("[SYSINFO] Failed to create Mosquitto client");
        	return NULL;
    	}

    	if (mosquitto_connect(mosq, mqtt_config.mqtt_host, mqtt_config.mqtt_port, 60) != MOSQ_ERR_SUCCESS)
    	{
        	LOG_ERROR("[SYSINFO] Failed to connect to broker");
        	mosquitto_destroy(mosq);
        	return NULL;
    	}

    	LOG_INFO("[SYSINFO] Connected to broker %s:%d", mqtt_config.mqtt_host, mqtt_config.mqtt_port);

    	while (1)
    	{
        	const char *hostname = get_hostname();
        	double cpu = get_cpu_usage();
        	double mem = get_memory_usage();
	
		LOG_DEBUG("[SYSINFO] Preparing JSON payload");

        	cJSON *root = cJSON_CreateObject();
        	cJSON_AddStringToObject(root, "hostname", hostname);
        	cJSON_AddNumberToObject(root, "cpu", cpu);
        	cJSON_AddNumberToObject(root, "memory", mem);

        	char *json_str = cJSON_Print(root);
        	LOG_DEBUG("[SYSINFO] JSON Payload: %s", json_str);

		int pub_status = mosquitto_publish(mosq, NULL, "system/info", strlen(json_str), json_str, 0, false);
		
		if (pub_status == MOSQ_ERR_SUCCESS)
		{
			LOG_INFO("[SYSINFO] Published system info successfully");
		}
		else
		{
			LOG_WARN("[SYSINFO] Failed to publish system info: %s", mosquitto_strerror(pub_status));
		}

        	LOG_INFO("[SYSINFO] Published: %s", json_str);

        	cJSON_Delete(root);
        	free(json_str);

        	sleep(5);
    	}

    	mosquitto_disconnect(mosq);
    	mosquitto_destroy(mosq);
    	LOG_DEBUG("[SYSINFO] Publisher thread exiting");

	return NULL;
}
