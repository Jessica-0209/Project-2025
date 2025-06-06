#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <cJSON.h>
#include <unistd.h>

#include "hostapd_listener.h"
#include "mqtt_client.h"
#include "utils.h"
#include <mosquitto.h>

#define CONFIG "config/mqtt_config.json"

static volatile int keep_running = 1;

void int_handler(int dummy) 
{
    	keep_running = 0;
}

mqtt_json mqtt_config;

//subscriber callback
void on_message(struct mosquitto *mosq, void *userdata, const struct mosquitto_message *message) 
{
    	printf("[MQTT SUB] %s: %s\n", message->topic, (char *)message->payload);
}

//function to run the publisher
int run_publisher() 
{
    	if (hostapd_listener_init(HOSTAPD_SOCKET_PATH) < 0) 	
    	{
        	log_info("Failed to initialize hostapd listener");
        	return 1;
    	}

    	if (mqtt_client_init(mqtt_config.mqtt_host, mqtt_config.mqtt_port, "wifi_mqtt_publisher") < 0) 
    	{
        	log_info("Failed to initialize MQTT client");
        	hostapd_listener_cleanup();
        	return 1;
    	}

    	char event_buf[EVENT_BUF_SIZE];
    	while (keep_running) 
    	{
        	ssize_t len = hostapd_listener_receive(event_buf, sizeof(event_buf));
        	if (len > 0) 
		{
            		log_info("Received event: %s", event_buf);
				

			char *json_payload = build_event_json(event_buf);
                        if (json_payload)
                        {
                                mqtt_client_publish(mqtt_config.mqtt_topic, json_payload);
                                free(json_payload);
                        }
			else
                        {
                                log_info("Failed to parse event to JSON: %s", event_buf);
                        }
	
        	}
    	}

    	mqtt_client_cleanup();
    	hostapd_listener_cleanup();
    	return 0;
}

//function to run subscriber
int run_subscriber() 
{
    	mosquitto_lib_init();

    	struct mosquitto *mosq = mosquitto_new("wifi_mqtt_subscriber", true, NULL);
    	if (!mosq) 
    	{
        	fprintf(stderr, "Failed to create MQTT subscriber\n");
        	return 1;
    	}

    	mosquitto_message_callback_set(mosq, on_message);

    	if (mosquitto_connect(mosq, mqtt_config.mqtt_host, mqtt_config.mqtt_port, 60) != MOSQ_ERR_SUCCESS) 
    	{
        	fprintf(stderr, "Failed to connect to MQTT broker\n");
        	mosquitto_destroy(mosq);
        	return 1;
    	}

    	mosquitto_subscribe(mosq, NULL, mqtt_config.mqtt_topic, 0);
    	mosquitto_loop_start(mosq);

    	printf("[INFO] Subscribed to topic: %s\n", mqtt_config.mqtt_topic);
    	while (keep_running) 
    	{
        	sleep(1);
    	}

    	mosquitto_loop_stop(mosq, true);
    	mosquitto_disconnect(mosq);
    	mosquitto_destroy(mosq);
    	mosquitto_lib_cleanup();
    	return 0;
}

int main(int argc, char *argv[]) 
{
    	signal(SIGINT, int_handler);
    	signal(SIGTERM, int_handler);

	if (argc < 2) 
	{
        	fprintf(stderr, "Usage: %s [publisher|subscriber]\n", argv[0]);
        	return 1;
    	}

    	if (parse_mqtt_config(CONFIG, &mqtt_config) != 0) 
    	{
        	fprintf(stderr, "Failed to load MQTT config\n");
        	return 1;
    	}

 /*   	log_info("MQTT Broker: %s, Port: %d Topic: %s", mqtt_config.mqtt_host, mqtt_config.mqtt_port, mqtt_config.mqtt_topic);

    	if (hostapd_listener_init(HOSTAPD_SOCKET_PATH) < 0) 
    	{
        	log_info("Failed to initialize hostapd listener");
        	free(mqtt_config.mqtt_host);
        	free(mqtt_config.mqtt_topic);
        	return 1;
    	}

    	if (mqtt_client_init(mqtt_config.mqtt_host, mqtt_config.mqtt_port, "wifi_mqtt_client") < 0) 
    	{
        	log_info("Failed to initialize MQTT client");
        	hostapd_listener_cleanup();
        	free(mqtt_config.mqtt_host);
        	free(mqtt_config.mqtt_topic);
        	return 1;
    	}

 	char event_buf[EVENT_BUF_SIZE];
    	while (keep_running) 
    	{
        	ssize_t len = hostapd_listener_receive(event_buf, sizeof(event_buf));
        	if (len < 0) 
		{
        		perror("[ERROR] recv failed");
    		} 
		else if (len == 0) 
    		{
        		printf("[INFO] No data received (len == 0)\n");
    		} 
		else
		{
            		log_info("Received event: %s", event_buf);

			char *json_payload = build_event_json(event_buf);
			if (json_payload)
        		{
				mqtt_client_publish(mqtt_config.mqtt_topic, json_payload);
				free(json_payload);
			}
			else
			{
				log_info("Failed to parse event to JSON: %s", event_buf);
			}
        	}
	}
	
	mqtt_client_cleanup();
        hostapd_listener_cleanup();
	*/

	int result = 1;

    	if (strcmp(argv[1], "publisher") == 0) 
    	{
        	result = run_publisher();
    	} 	
    	else if (strcmp(argv[1], "subscriber") == 0) 
    	{
        	result = run_subscriber();
    	} 
    	else 
    	{
    	   	fprintf(stderr, "Invalid mode: %s. Use 'publisher' or 'subscriber'\n", argv[1]);
    	}

    	free(mqtt_config.mqtt_host);
    	free(mqtt_config.mqtt_topic);

    	log_info("Exiting...");

    	//return 0;
	return result;
}
