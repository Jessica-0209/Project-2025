#include "utils.h"
#include "log.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <cjson/cJSON.h> 

/* Function: read_json_file()
 * ------------------------------------------
 *
 * Function to read the contents of JSON file into a dynamically allocated buffer
 *
 * filename: path to the json file
 *
 * Returns: On success, returns a pointer to the buffer containing the json content, and NULL on failure
 */

char *read_json_file(const char *filename) 
{
	LOG_DEBUG("Attempting to open JSON file: %s", filename);
    	FILE *f = fopen(filename, "rb");

    	if (!f) 
	{
		LOG_ERROR("Failed to open file: %s", filename);
		return NULL;
	}

    	if (fseek(f, 0, SEEK_END) != 0)
        {
		LOG_ERROR("Failed to seek to end of file: %s", filename);
                fclose(f);
                return NULL;
        }

    	long length = ftell(f);
	if (length < 0)
        {
		LOG_ERROR("Failed to get file length: %s", filename);
                fclose(f);
                return NULL;
	}

	LOG_DEBUG("File length: %ld bytes", length);
    	rewind(f);

    	char *buffer = malloc(length + 1);
    	if (!buffer) 
    	{
		LOG_ERROR("Memory allocation failed for file buffer");
        	fclose(f);
        	return NULL;
    	}

    	size_t read_count = fread(buffer, 1, length, f);
        if (read_count != length)
        {
		LOG_ERROR("Failed to read file contents properly. Expected: %ld, Read: %zu", length, read_count);
                free(buffer);
                fclose(f);
                return NULL;
        }

    	buffer[length] = '\0';
	LOG_DEBUG("Successfully read file: %s", filename);

    	fclose(f);
    	return buffer;
}


/* Function: parse_mqtt_config()
 * ------------------------------
 *
 * Function to parse the mqtt configuration from json
 *
 * filename: path to the JSON file
 * config: pointer to mqtt_json structure where the parsed configuration will be stored
 *
 * Returns: success or failure of parsing the configuration file
 *
 */


int parse_mqtt_config(const char *filename, mqtt_json *config) 
{
	LOG_DEBUG("Starting MQTT config parsing from file: %s", filename);

	if (filename == NULL)
	{
		LOG_ERROR("Filename is NULL");
		return -1;
	}

    	char *json_data = read_json_file(filename);

	if (!json_data) 
    	{
        	LOG_ERROR("Failed to read config file: %s\n", filename);
        	return -1;
    	}

	LOG_DEBUG("Successfully read JSON config file");

    	cJSON *root = cJSON_Parse(json_data);
	
	free(json_data);

    	if (!root) 
    	{
        	LOG_ERROR("Failed to parse JSON config\n");
        	return -1;
    	}

	LOG_DEBUG("Successfully parsed JSON root object");

    	cJSON *jhost = cJSON_GetObjectItem(root, "host");
    	cJSON *jport = cJSON_GetObjectItem(root, "port");
    	cJSON *jtopics = cJSON_GetObjectItem(root, "topics");

	if (!cJSON_IsString(jhost))
        {
                LOG_ERROR("Missing or invalid 'host' in config");
        }

        if (!cJSON_IsNumber(jport))
        {
                LOG_ERROR("Missing or invalid 'port' in config");
        }

        if (!cJSON_IsArray(jtopics))
        {
                LOG_ERROR("Missing or invalid 'topics' array in config");
        }

    	if (!cJSON_IsString(jhost) || !cJSON_IsNumber(jport) || !cJSON_IsArray(jtopics)) 
    	{
        	LOG_ERROR("Invalid config format\n");
        	cJSON_Delete(root);
        	return -1;
    	}

    	config->mqtt_host = strdup(jhost->valuestring);
    	config->mqtt_port = jport->valueint;
    	config->topic_count = 0;

	LOG_DEBUG("MQTT host: %s", config->mqtt_host);
        LOG_DEBUG("MQTT port: %d", config->mqtt_port);

    	cJSON *topic_item = NULL;
    	cJSON_ArrayForEach(topic_item, jtopics) 
    	{
        	if (cJSON_IsString(topic_item) && config->topic_count < MAX_TOPICS) 
		{
            		strncpy(config->mqtt_topics[config->topic_count], topic_item->valuestring, MAX_TOPIC_LENGTH - 1);
            		config->mqtt_topics[config->topic_count][MAX_TOPIC_LENGTH - 1] = '\0';

            		LOG_DEBUG("Added topic [%d]: %s", config->topic_count, config->mqtt_topics[config->topic_count]);

			config->topic_count++;
        	}
		else if (!cJSON_IsString(topic_item))
                {
                        LOG_WARN("Skipping non-string topic entry");
                }
                else
                {
                        LOG_WARN("Topic limit reached (%d)", MAX_TOPICS);
                }
    	}

    	cJSON_Delete(root);
	LOG_DEBUG("Completed parsing config file: %s", filename);

    	return 0;
}

/* Function: build_event_json()
 * ------------------------------------------
 *
 * Function to convert a raw Wi-Fi event string and optional SSID into a JSON-formatted string.
 *
 * event_str: the raw event string containing event type and MAC address
 * ssid: the SSID associated with the event (can be NULL)
 *
 * Returns: pointer to a dynamically allocated JSON string representing the event, or NULL on failure to parse the input
 *
 */

char *build_event_json(const char *event_str, const char *ssid)
{
	LOG_DEBUG("Building event JSON from string: %s", event_str);

    	char mac[18] = {0};
    	char event_type[64] = {0};

    	if (sscanf(event_str, "<%*d>%63s %17s", event_type, mac) != 2)
    	{
		LOG_ERROR("Failed to parse event string: %s", event_str);
        	return NULL;
    	}

	LOG_DEBUG("Parsed event_type: %s, MAC: %s", event_type, mac);

    	time_t now = time(NULL);
    	struct tm *t = localtime(&now);
    	char timestamp[64];
    	strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", t);

    	cJSON *root = cJSON_CreateObject();

	if (!root)
        {
                LOG_ERROR("Failed to create cJSON root object");
                return NULL;
        }

    	cJSON_AddStringToObject(root, "event_type", event_type);
    	cJSON_AddStringToObject(root, "mac", mac);
	
	if (ssid)
	{
        	cJSON_AddStringToObject(root, "ssid", ssid);
		LOG_DEBUG("SSID added to JSON: %s", ssid);
	}	
	else
    	{	
        	cJSON_AddStringToObject(root, "ssid", "");
		LOG_DEBUG("SSID not provided; added empty string");
	}
    	
	cJSON_AddStringToObject(root, "timestamp", timestamp);

    	char *json_str = cJSON_Print(root);

	if (!json_str)
        {
                LOG_ERROR("Failed to print JSON string");
                cJSON_Delete(root);
                return NULL;
        }

	LOG_DEBUG("Built JSON: %s", json_str);

    	cJSON_Delete(root);
    	return json_str;
}

/* Function: build_sysinfo_json()
 * ------------------------------------------
 *
 * Function to gather system information including hostname, CPU usage, and memory usage, and convert it into a JSON-formatted string.
 *
 * Returns: pointer to a dynamically allocated JSON string containing system stats, or NULL if creation fails at any stage.
 *
 */

char *build_sysinfo_json()
{
    	char hostname[128] = {0};
    	gethostname(hostname, sizeof(hostname));
	
    	FILE *fp;
    	char buf[128];
    	double cpu_usage = 0.0;
    	double mem_usage = 0.0;

    	fp = fopen("/proc/stat", "r");
    	if (fp)
    	{
        	unsigned long long int user, nice, system, idle;
        	fscanf(fp, "cpu %llu %llu %llu %llu", &user, &nice, &system, &idle);
        	fclose(fp);
		LOG_DEBUG("First read - user: %llu, nice: %llu, system: %llu, idle: %llu", user, nice, system, idle);

        	sleep(1);
        	
		fp = fopen("/proc/stat", "r");
        	unsigned long long int user2, nice2, system2, idle2;
        	fscanf(fp, "cpu %llu %llu %llu %llu", &user2, &nice2, &system2, &idle2);
        	fclose(fp);
		LOG_DEBUG("Second read - user: %llu, nice: %llu, system: %llu, idle: %llu", user2, nice2, system2, idle2);

        	double total1 = user + nice + system + idle;
        	double total2 = user2 + nice2 + system2 + idle2;
        	double delta_total = total2 - total1;
        	double delta_idle = idle2 - idle;

        	cpu_usage = 100.0 * (delta_total - delta_idle) / delta_total;
    		LOG_DEBUG("CPU Usage: %.2f%%", cpu_usage);
	}
	else
        {
                LOG_ERROR("Failed to open /proc/stat for CPU usage");
        }

    	// Memory usage
    	unsigned long mem_total = 0, mem_free = 0;
    	fp = fopen("/proc/meminfo", "r");
    	if (fp)
    	{
        	while (fgets(buf, sizeof(buf), fp))
        	{
            		if (sscanf(buf, "MemTotal: %lu kB", &mem_total)) 
			{
				continue;	
			}
			if (sscanf(buf, "MemAvailable: %lu kB", &mem_free))
			{
				break;
			}
		}
        	fclose(fp);

		if (mem_total > 0)
                {
                        mem_usage = 100.0 * (mem_total - mem_free) / mem_total;
                        LOG_DEBUG("Memory Usage: %.2f%% (Total: %lu KB, Available: %lu KB)", mem_usage, mem_total, mem_free);
                }
                else
                {
                        LOG_ERROR("Failed to read total memory from /proc/meminfo");
                }

        	mem_usage = 100.0 * (mem_total - mem_free) / mem_total;
    	}
	else
        {
                LOG_ERROR("Failed to open /proc/meminfo for memory usage");
        }

    	cJSON *root = cJSON_CreateObject();
    	cJSON_AddStringToObject(root, "hostname", hostname);
    	cJSON_AddNumberToObject(root, "cpu", cpu_usage);
    	cJSON_AddNumberToObject(root, "memory", mem_usage);

    	char *json_str = cJSON_Print(root);
    	LOG_DEBUG("Built sysinfo JSON: %s", json_str);

	cJSON_Delete(root);
    	return json_str;
}
