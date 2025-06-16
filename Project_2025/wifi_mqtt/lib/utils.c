#include "utils.h"
#include "log.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <cjson/cJSON.h> 

char *read_file(const char *filename) 
{
    	FILE *f = fopen(filename, "rb");
    	if (!f) 
	{
		return NULL;
	}

    	fseek(f, 0, SEEK_END);
    	long length = ftell(f);
    	rewind(f);

    	char *buffer = malloc(length + 1);
    	if (!buffer) 
    	{
        	fclose(f);
        	return NULL;
    	}

    	fread(buffer, 1, length, f);
    	buffer[length] = '\0';

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
	if (filename == NULL)
	{
		return -1;
	}

    	char *json_data = read_file(filename);
	if (!json_data) 
    	{
        	LOG_ERROR("Failed to read config file: %s\n", filename);
        	return -1;
    	}

    	cJSON *root = cJSON_Parse(json_data);
	free(json_data); 
    	if (!root) 
    	{
        	LOG_ERROR("Failed to parse JSON config\n");
        	return -1;
    	}

    	cJSON *jhost = cJSON_GetObjectItem(root, "host");
    	cJSON *jport = cJSON_GetObjectItem(root, "port");
    	cJSON *jtopic = cJSON_GetObjectItem(root, "topic");

    	if (!cJSON_IsString(jhost) || !cJSON_IsNumber(jport) || !cJSON_IsString(jtopic)) 
    	{
        	LOG_ERROR("Invalid config format\n");
        	cJSON_Delete(root);
        	return -1;
    	}

    	config->mqtt_host = strdup(jhost->valuestring);
    	config->mqtt_port = jport->valueint;
    	config->mqtt_topic = strdup(jtopic->valuestring);

    	cJSON_Delete(root);
    	return 0;
}

char *build_event_json(const char *event_str, const char *ssid)
{
    	char mac[18] = {0};
    	char event_type[64] = {0};

    	if (sscanf(event_str, "<%*d>%63s %17s", event_type, mac) != 2)
    	{
        	return NULL;
    	}

    	time_t now = time(NULL);
    	struct tm *t = localtime(&now);
    	char timestamp[64];
    	strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", t);

    	cJSON *root = cJSON_CreateObject();
    	cJSON_AddStringToObject(root, "event_type", event_type);
    	cJSON_AddStringToObject(root, "mac", mac);
	
	if (ssid)
	{
        	cJSON_AddStringToObject(root, "ssid", ssid);
	}	
	else
    	{	
        	cJSON_AddStringToObject(root, "ssid", "");
	}
    	
	cJSON_AddStringToObject(root, "timestamp", timestamp);

    	char *json_str = cJSON_Print(root);
    	cJSON_Delete(root);
    	return json_str;
}

