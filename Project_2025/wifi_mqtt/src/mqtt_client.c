#include "mqtt_client.h"
#include <mosquitto.h>
#include <stdio.h>
#include <string.h>

static struct mosquitto *mosq = NULL;

int mqtt_client_init(const char *host, int port, const char *client_id) 
{
    	mosquitto_lib_init();
    	mosq = mosquitto_new(client_id, true, NULL);
    	if (!mosq) 
    	{
        	fprintf(stderr, "Failed to create mosquitto client\n");
        	return -1;
    	}

    	if (mosquitto_connect(mosq, host, port, 60) != MOSQ_ERR_SUCCESS) 
    	{
        	fprintf(stderr, "Failed to connect to MQTT broker\n");
        	mosquitto_destroy(mosq);
        	mosq = NULL;
        	return -1;
    	}

    	mosquitto_loop_start(mosq);
    	return 0;
}

int mqtt_client_publish(const char *topic, const char *message) 
{
    	if (!mosq) 
    	{
	    	return -1;
    	}
    	int ret = mosquitto_publish(mosq, NULL, topic, strlen(message), message, 1, false);
    	return ret;
}

void mqtt_client_cleanup() 
{
    	if (mosq) 
    	{
        	mosquitto_loop_stop(mosq, true);
        	mosquitto_disconnect(mosq);
        	mosquitto_destroy(mosq);
        	mosq = NULL;
        	mosquitto_lib_cleanup();
    	}
}

