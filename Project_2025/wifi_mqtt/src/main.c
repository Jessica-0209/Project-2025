#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <cJSON.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <pthread.h>

#include "hostapd_listener.h"
#include "mqtt_client.h"
#include "utils.h"
#include "wifi_hash.h"
#include <mosquitto.h>

#define CONFIG "config/mqtt_config.json"
#define CLI_SOCKET_PATH "/tmp/wifi_mqtt_cli.sock"

static volatile int keep_running = 1;

void int_handler(int dummy) 
{
    	keep_running = 0;
}

mqtt_json mqtt_config;

void* handle_cli_commands(void* arg)
{
    	int server_fd;
	int client_fd;
    	struct sockaddr_un addr;
    	char buf[64];

    	unlink(CLI_SOCKET_PATH);

    	server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    	if (server_fd < 0) 
    	{
        	perror("socket");
        	return NULL;
    	}

    	memset(&addr, 0, sizeof(addr));
    	addr.sun_family = AF_UNIX;
    	strncpy(addr.sun_path, CLI_SOCKET_PATH, sizeof(addr.sun_path)-1);

    	if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) 
    	{
        	perror("bind");
        	close(server_fd);
        	return NULL;
    	}

    	listen(server_fd, 5);

    	while (keep_running) 
    	{
        	client_fd = accept(server_fd, NULL, NULL);
        	if (client_fd < 0) 
		{
			continue;
		}

        	int len = read(client_fd, buf, sizeof(buf)-1);
        	buf[len] = '\0';

        	if (strcmp(buf, "show") == 0) 
		{
            		char response[4096];
    			get_hash_table_as_string(response, sizeof(response));
    			write(client_fd, response, strlen(response)); 
        	}

        	close(client_fd);
    	}

    	close(server_fd);
    	unlink(CLI_SOCKET_PATH);
    	return NULL;
}

//subscriber callback
void on_message(struct mosquitto *mosq, void *userdata, const struct mosquitto_message *message) 
{
    	printf("[MQTT SUB] %s: %s\n", message->topic, (char *)message->payload);
	
	cJSON *root = cJSON_Parse((char *)message->payload);
    	if (!root) 
    	{
        	fprintf(stderr, "Failed to parse JSON\n");
        	return;
    	}
	
	Wifi_Event event;
    	cJSON *mac = cJSON_GetObjectItem(root, "mac");
    	cJSON *ssid = cJSON_GetObjectItem(root, "ssid");
    	cJSON *event_type = cJSON_GetObjectItem(root, "event_type");
    	cJSON *timestamp = cJSON_GetObjectItem(root, "timestamp");

    	if (mac && ssid && event_type && timestamp) 
    	{
        	strncpy(event.mac, mac->valuestring, sizeof(event.mac));
        	strncpy(event.ssid, ssid->valuestring, sizeof(event.ssid));
        	strncpy(event.event_type, event_type->valuestring, sizeof(event.event_type));
        	strncpy(event.timestamp, timestamp->valuestring, sizeof(event.timestamp));

        	insert_or_update(event.mac, event.ssid, event.event_type, event.timestamp);
    	}

    	else 
    	{
        	fprintf(stderr, "Incomplete event JSON\n");
    	}

    	cJSON_Delete(root);
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
				
                        char *json_payload = build_event_json(event_buf, "test2025");
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

	pthread_t cli_thread;
	pthread_create(&cli_thread, NULL, handle_cli_commands, NULL);

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

	//display();
	free_table();

    	return 0;
}

int main(int argc, char *argv[]) 
{
    	signal(SIGINT, int_handler);
    	signal(SIGTERM, int_handler);

	if (argc < 2) 
	{
        	fprintf(stderr, "Usage: %s [publisher|subscriber|show]\n", argv[0]);
        	return 1;
    	}

    	if (parse_mqtt_config(CONFIG, &mqtt_config) != 0) 
    	{
        	fprintf(stderr, "Failed to load MQTT config\n");
        	return 1;
    	}

	int result = 1;

    	if (strcmp(argv[1], "publisher") == 0) 
    	{
        	result = run_publisher();
    	} 	
    	else if (strcmp(argv[1], "subscriber") == 0) 
    	{
        	result = run_subscriber();
    	}
	else if (strcmp(argv[1], "show") == 0)
	{
    		display();  
    		free_table();
    		result = 0;
	}
    	else 
    	{
    	   	fprintf(stderr, "Invalid mode: %s. Use 'publisher' or 'subscriber', or 'show'\n", argv[1]);
    	}

    	free(mqtt_config.mqtt_host);
    	free(mqtt_config.mqtt_topic);

    	log_info("Exiting...");

	return result;
}
