#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cJSON.h>

#include "wifi_hash.h"
#include "log.h"

#define TABLE_SIZE 101

Wifi_Event* hash_table[TABLE_SIZE] = {NULL}; 

/* Function: hash()
 * ------------------------------------------
 * Computes a hash value for the given MAC address using the djb2 algorithm.
 * This is used to index into the hash table.
 *
 * str: The MAC address as a string.
 * Returns: The computed hash index (modulo TABLE_SIZE).
 */

unsigned int hash(const char* str)
{
    	unsigned long hash = 5381;
    	int c = 0;

    	while ((c = *str++))
    	{
        	hash = ((hash << 5) + hash) + c;
    	}

    	unsigned int index = hash % TABLE_SIZE;
    	LOG_DEBUG("[HASH] Computed index %u for key %s", index, str);

    	return index;
}

/* Function: insert_or_update_the_hash_table()
 * ------------------------------------------
 * Inserts a new Wi-Fi event into the hash table or updates an existing entry 
 * based on the MAC address. Maintains collision chains using linked lists.
 *
 * mac:        MAC address of the device.
 * hostname:   Hostname of the device.
 * ssid:       SSID associated with the event.
 * event_type: Event type (e.g., AP-STA-CONNECTED).
 * timestamp:  Time the event occurred.
 *
 * Returns: 0 if updated, 1 if inserted, -1 on malloc failure.
 */

int insert_or_update_the_hash_table(const char* mac, const char* hostname, const char* ssid, const char* event_type, const char* timestamp)
{
    	unsigned int index = hash(mac);
    	LOG_DEBUG("[HASH] Inserting/updating MAC %s at index %u", mac, index);

    	Wifi_Event* current = hash_table[index];

    	while (current != NULL)
    	{
        	if (strcmp(current->mac, mac) == 0)
        	{
            		LOG_DEBUG("[HASH] Found existing MAC %s — updating values", mac);
	
            		strncpy(current->ssid, ssid, sizeof(current->ssid));
            		strncpy(current->hostname, hostname, sizeof(current->hostname));
            		strncpy(current->event_type, event_type, sizeof(current->event_type));
            		strncpy(current->timestamp, timestamp, sizeof(current->timestamp));

            		LOG_INFO("[HASH] Updated event for %s: %s on SSID %s (Hostname: %s)", mac, event_type, ssid, hostname);
            		return 0;
        	}
        	current = current->next;
    	}

    	LOG_DEBUG("[HASH] MAC %s not found. Inserting new node", mac);

    	Wifi_Event* new_node = (Wifi_Event*)malloc(sizeof(Wifi_Event));
    	if (!new_node)
    	{
        	LOG_ERROR("[HASH] malloc failed");
        	return -1;
    	}

    	strncpy(new_node->mac, mac, sizeof(new_node->mac));
    	strncpy(new_node->hostname, hostname, sizeof(new_node->hostname));
    	strncpy(new_node->ssid, ssid, sizeof(new_node->ssid));
    	strncpy(new_node->event_type, event_type, sizeof(new_node->event_type));
    	strncpy(new_node->timestamp, timestamp, sizeof(new_node->timestamp));
    	new_node->next = hash_table[index];
    	hash_table[index] = new_node;

    	LOG_INFO("[HASH] Inserted event for %s: %s on SSID %s (Hostname: %s)", mac, event_type, ssid, hostname);
    	return 1;
}

/* Function: parse_json_and_insert()
 * ------------------------------------------
 * Parses an MQTT JSON string containing Wi-Fi event data and stores it in the hash table.
 *
 * json_str: The JSON-formatted string containing keys: mac, hostname, ssid, event_type, and timestamp.
 */

void parse_json_and_insert(const char* json_str)
{
    	static int msg_count = 0;
    	msg_count++;
    	LOG_DEBUG("[HASH] Processing MQTT message #%d", msg_count);

    	cJSON* root = cJSON_Parse(json_str);
    	if (!root)
    	{
        	LOG_WARN("[HASH] JSON parsing failed: Invalid format");
        	return;
    	}

    	cJSON* mac = cJSON_GetObjectItemCaseSensitive(root, "mac");
    	cJSON* hostname = cJSON_GetObjectItemCaseSensitive(root, "hostname");
    	cJSON* ssid = cJSON_GetObjectItemCaseSensitive(root, "ssid");
    	cJSON* event_type = cJSON_GetObjectItemCaseSensitive(root, "event_type");
    	cJSON* timestamp = cJSON_GetObjectItemCaseSensitive(root, "timestamp");

    	if (cJSON_IsString(mac) && cJSON_IsString(hostname) && cJSON_IsString(ssid) && cJSON_IsString(event_type) && cJSON_IsString(timestamp))
    	{
        	LOG_DEBUG("[HASH] Extracted MAC: %s, Hostname: %s, SSID: %s, Event: %s, Time: %s",
            mac->valuestring, hostname->valuestring, ssid->valuestring, event_type->valuestring, timestamp->valuestring);

        	insert_or_update_the_hash_table(mac->valuestring, hostname->valuestring, ssid->valuestring, event_type->valuestring, timestamp->valuestring);
    	}
    	else
    	{
        	LOG_WARN("[HASH] Missing fields in received JSON: %s", json_str);
    	}

    	cJSON_Delete(root);
}

/* Function: display_wifi_table()
 * ------------------------------------------
 * Prints all current entries in the Wi-Fi events hash table to stdout.
 */

void display_wifi_table(void)
{
    	LOG_DEBUG("[HASH] Displaying full Wi-Fi table...");

    	printf("\n***** WIFI EVENTS TABLE *****\n");

    	for (int i = 0; i < TABLE_SIZE; i++)
    	{
        	Wifi_Event* current = hash_table[i];
        	if (current)
        	{
            		printf("\nBucket %d:\n", i);
            		while (current)
            		{
                		printf("  MAC: %s | Hostname: %s | SSID: %s | Event: %s | Time: %s\n", current->mac, current->hostname, current->ssid, current->event_type, current->timestamp);
                		current = current->next;
            		}
        	}
    	}
}

/* Function: get_wifi_table_as_string()
 * ------------------------------------------
 * Serializes the Wi-Fi event hash table into a string for use in command-line output.
 *
 * output:  Buffer to store the resulting string.
 * max_len: Maximum length of the output buffer.
 */

void get_wifi_table_as_string(char* output, size_t max_len)
{
    	LOG_DEBUG("[HASH] Creating Wi-Fi table string output...");
    	int offset = 0;

    	for (int i = 0; i < TABLE_SIZE; i++)
    	{
        	Wifi_Event* current = hash_table[i];

        	if (current)
        	{
            		offset += snprintf(output + offset, max_len - offset, "\nBucket %d:\n", i);

            		while (current)
            		{
                		offset += snprintf(output + offset, max_len - offset, "  MAC: %s | Hostname: %s | SSID: %s | Event: %s | Time: %s\n", current->mac, current->hostname, current->ssid, current->event_type, current->timestamp);
                		current = current->next;
            		}
        	}
    	}

    	LOG_DEBUG("[HASH] Wi-Fi table string built");
}

/* Function: free_wifi_table()
 * ------------------------------------------
 * Frees all dynamically allocated memory in the hash table and resets the table.
 */

void free_wifi_table(void)
{
    	LOG_DEBUG("[HASH] Freeing all hash table entries...");

    	for (int i = 0; i < TABLE_SIZE; i++)
    	{
        	Wifi_Event* current = hash_table[i];
        	while (current)
        	{
            		LOG_DEBUG("[HASH] Freeing node for MAC: %s", current->mac);

            		Wifi_Event* tmp = current;
            		current = current->next;

            		free(tmp);
        	}
        	hash_table[i] = NULL;
    	}

    	LOG_INFO("[HASH] All hash table entries cleared");
}

