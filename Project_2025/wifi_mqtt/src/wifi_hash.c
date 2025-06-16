#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cJSON.h>

#include "wifi_hash.h"
#include "log.h"

#define TABLE_SIZE 101

Wifi_Event* hash_table[TABLE_SIZE] = {NULL}; 

unsigned int hash(const char* str)
{
	unsigned long hash = 5381;
	int c;

	//converting each character of the MAC Address into ASCII to end up with a very big number, then doing mod
	while((c = *str++))
	{
		hash = ((hash << 5) + hash) + c;
	}

	return hash % TABLE_SIZE;
}

int insert_or_update(const char* mac, const char* ssid, const char* event_type, const char* timestamp)
{
    	unsigned int index = hash(mac);
    	Wifi_Event* current = hash_table[index];

    	while (current != NULL)
    	{
        	if (strcmp(current->mac, mac) == 0)
        	{
            		// Update existing entry
            		strncpy(current->ssid, ssid, sizeof(current->ssid));
            		strncpy(current->event_type, event_type, sizeof(current->event_type));
            		strncpy(current->timestamp, timestamp, sizeof(current->timestamp));

            		LOG_INFO("[HASH] Updated event for %s: %s on SSID %s\n", mac, event_type, ssid);
            		return 0;
        	}
        	current = current->next;
    	}

    	// Not found, insert new
    	Wifi_Event* new_node = (Wifi_Event* )malloc(sizeof(Wifi_Event));
    	if (!new_node)
    	{
        	LOG_ERROR("malloc failed");
        	return -1;
    	}

    	strncpy(new_node->mac, mac, sizeof(new_node->mac));
    	strncpy(new_node->ssid, ssid, sizeof(new_node->ssid));
    	strncpy(new_node->event_type, event_type, sizeof(new_node->event_type));
    	strncpy(new_node->timestamp, timestamp, sizeof(new_node->timestamp));
    	new_node->next = hash_table[index];
    	hash_table[index] = new_node;

    	LOG_INFO("[HASH] Inserted event for %s: %s on SSID %s\n", mac, event_type, ssid);
	return 1;
}

void parse_and_insert(const char* json_str)
{
	static int msg_count = 0;
	msg_count++;
	LOG_DEBUG("Processing MQTT message #%d\n", msg_count);

	cJSON* root = cJSON_Parse(json_str);
	if(!root)
	{
		LOG_WARN("Invalid!\n");
		return;
	}

	cJSON* mac = cJSON_GetObjectItemCaseSensitive(root, "mac");
	cJSON* ssid = cJSON_GetObjectItemCaseSensitive(root, "ssid");
	cJSON* event_type = cJSON_GetObjectItemCaseSensitive(root, "event_type");
	cJSON* timestamp = cJSON_GetObjectItemCaseSensitive(root, "timestamp");

	if (cJSON_IsString(mac) && cJSON_IsString(ssid) && cJSON_IsString(event_type) && cJSON_IsString(timestamp))
	{
		insert_or_update(mac->valuestring, ssid->valuestring, event_type->valuestring, timestamp->valuestring);	
	}
	else
	{
		LOG_WARN("Missing fields in JSON\n");
	}

	cJSON_Delete(root);
}

void display()
{
	printf("\n*****WIFI EVENTS TABLE*****\n");
	for (int i = 0; i < TABLE_SIZE; i++) 
	{
        	Wifi_Event* current = hash_table[i];
        	if (current) 
		{
            		printf("\nBucket %d:\n", i);
            		while (current) 
	    		{
                		printf("  MAC: %s | SSID: %s | Event: %s | Time: %s\n", current->mac, current->ssid, current->event_type, current->timestamp);
                		current = current->next;
            		}
        	}
    	}
}

void get_hash_table_as_string(char* output, size_t max_len) 
{
    	int offset = 0;
    	
	for (int i = 0; i < TABLE_SIZE; i++) 
    	{
        	Wifi_Event* current = hash_table[i];
        	
		if (current) 
		{
            		offset += snprintf(output + offset, max_len - offset, "\nBucket %d:\n", i);
            		
			while (current) 
	    		{
                		offset += snprintf(output + offset, max_len - offset, "  MAC: %s | SSID: %s | Event: %s | Time: %s\n", current->mac, current->ssid, current->event_type, current->timestamp);
                		current = current->next;
            		}
        	}
    	}
}

void free_table()
{
	for (int i = 0; i < TABLE_SIZE; i++) 
	{
        	Wifi_Event* current = hash_table[i];
        	while(current) 
		{
            		Wifi_Event* tmp = current;
            		current = current->next;
            		free(tmp);
        	}
        	hash_table[i] = NULL;
    	}
}
