#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "wifi_hash.h"
#include "log.h"

#define HASH_TABLE_SIZE 128

SysInfo* sysinfo_hash_table[HASH_TABLE_SIZE];

static unsigned int hash_hostname(const char* hostname)
{
    	unsigned long hash = 5381;
    	int c;
    	while ((c = *hostname++))
    	{
        	hash = ((hash << 5) + hash) + c;
    	}
    	return hash % HASH_TABLE_SIZE;
}

int insert_or_update_sysinfo(const char* hostname, double cpu, double mem, const char* timestamp)
{
    	if (!hostname || !timestamp)
    	{
	    return -1;
	}

    	unsigned int index = hash_hostname(hostname);
    	SysInfo* entry = sysinfo_hash_table[index];

    	while (entry)
    	{
        	if (strcmp(entry->hostname, hostname) == 0)
        	{
            		entry->cpu_usage = cpu;
            		entry->memory_usage = mem;
            		strncpy(entry->timestamp, timestamp, sizeof(entry->timestamp));
            		return 0; 
		}
        	entry = entry->next;
    	}

    	SysInfo* new_entry = (SysInfo*)malloc(sizeof(SysInfo));
    	if (!new_entry)
    	{
        	return -1;
	}

    	strncpy(new_entry->hostname, hostname, sizeof(new_entry->hostname));
    	new_entry->cpu_usage = cpu;
    	new_entry->memory_usage = mem;
    	strncpy(new_entry->timestamp, timestamp, sizeof(new_entry->timestamp));
    	new_entry->next = sysinfo_hash_table[index];
    	sysinfo_hash_table[index] = new_entry;

    	return 0;
}

void display_sysinfo_table()
{
    	LOG_INFO("==== System Info Table ====\n");
    	for (int i = 0; i < HASH_TABLE_SIZE; ++i)
    	{
        	SysInfo* entry = sysinfo_hash_table[i];
        	while (entry)
        	{
            		printf("Hostname: %s\n", entry->hostname);
            		printf("CPU Usage: %.2f%%\n", entry->cpu_usage);
            		printf("Memory Usage: %.2f%%\n", entry->memory_usage);
            		printf("Timestamp: %s\n\n", entry->timestamp);
            		entry = entry->next;
        	}
    	}
}

void get_sysinfo_table_as_string(char* output, size_t max_len)
{
    	size_t offset = 0;
    	offset += snprintf(output + offset, max_len - offset, "==== System Info Table ====\n");

    	for (int i = 0; i < HASH_TABLE_SIZE && offset < max_len; ++i)
    	{
        	SysInfo* entry = sysinfo_hash_table[i];
        	while (entry && offset < max_len)
        	{
            		offset += snprintf(output + offset, max_len - offset, "Hostname: %s\nCPU Usage: %.2f%%\nMemory Usage: %.2f%%\nTimestamp: %s\n\n", entry->hostname, entry->cpu_usage, entry->memory_usage, entry->timestamp);
            		entry = entry->next;
        	}
    	}
}

void free_sysinfo_table()
{
    	for (int i = 0; i < HASH_TABLE_SIZE; ++i)
    	{
        	SysInfo* entry = sysinfo_hash_table[i];
        	while (entry)
        	{
            		SysInfo* temp = entry;
            		entry = entry->next;
            		free(temp);
        	}
        	sysinfo_hash_table[i] = NULL;
    	}
}
