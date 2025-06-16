#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <cJSON.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <pthread.h>
#include <mosquitto.h>
#include <netlink/netlink.h>
#include <netlink/genl/genl.h>
#include <netlink/genl/ctrl.h>
#include <netlink/msg.h>
#include <netlink/attr.h>
#include <linux/nl80211.h>

#include "hostapd_listener.h"
#include "mqtt_client.h"
#include "utils.h"
#include "wifi_hash.h"
#include "log.h"

#define CONFIG "config/mqtt_config.json"
#define CLI_SOCKET_PATH "/tmp/wifi_mqtt_cli.sock"

static volatile int keep_running = 1;

void int_handler(int dummy) 
{
    	keep_running = 0;
}

mqtt_json mqtt_config;

char ssid_buf[256];

int callback_handler(struct nl_msg *msg, void *arg) 
{
    	struct nlattr *attrs[NL80211_ATTR_MAX + 1];
    	struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));

    	nla_parse(attrs, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0), genlmsg_attrlen(gnlh, 0), NULL);

    	if (attrs[NL80211_ATTR_SSID]) 
    	{
        	int len = nla_len(attrs[NL80211_ATTR_SSID]);
        	memcpy(ssid_buf, nla_data(attrs[NL80211_ATTR_SSID]), len);
        	ssid_buf[len] = '\0';
        	return NL_OK;
    	}
	
    	return NL_SKIP;
}

char *get_ssid() 
{
    	struct nl_sock *sock = nl_socket_alloc();
    	genl_connect(sock);
    	int driver_id = genl_ctrl_resolve(sock, "nl80211");

    	struct nl_msg *msg = nlmsg_alloc();
    	genlmsg_put(msg, 0, 0, driver_id, 0, NLM_F_DUMP, NL80211_CMD_GET_INTERFACE, 0);
    	nl_socket_modify_cb(sock, NL_CB_VALID, NL_CB_CUSTOM, callback_handler, NULL);

    	nl_send_auto(sock, msg);
    	nl_recvmsgs_default(sock);

    	nlmsg_free(msg);
    	nl_socket_free(sock);

    	return strlen(ssid_buf) > 0 ? ssid_buf : NULL;
}

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
        	LOG_ERROR("socket");
        	return NULL;
    	}

    	memset(&addr, 0, sizeof(addr));
    	addr.sun_family = AF_UNIX;
    	strncpy(addr.sun_path, CLI_SOCKET_PATH, sizeof(addr.sun_path)-1);

    	if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) 
    	{
        	LOG_ERROR("bind");
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
    	LOG_INFO("[MQTT SUB] %s: %s\n", message->topic, (char *)message->payload);
	
	cJSON *root = cJSON_Parse((char *)message->payload);
    	if (!root) 
    	{
        	LOG_WARN("Failed to parse JSON\n");
        	return;
    	}
	LOG_DEBUG("Successfully parsed JSON!");
	
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
        	LOG_WARN("Incomplete event JSON\n");
    	}

    	cJSON_Delete(root);
	LOG_DEBUG("Parsed cJSON object tree freed!");
}

//function to run the publisher
int run_publisher() 
{
    	if (hostapd_listener_init(HOSTAPD_SOCKET_PATH) < 0) 	
    	{
        	LOG_ERROR("Failed to initialize hostapd listener");
        	return 1;
    	}
	LOG_DEBUG("Hostapd listener Initialized!");

    	if (mqtt_client_init(mqtt_config.mqtt_host, mqtt_config.mqtt_port, "wifi_mqtt_publisher") < 0) 
    	{
        	LOG_ERROR("Failed to initialize MQTT client");
        	hostapd_listener_cleanup();
        	return 1;
    	}
	LOG_DEBUG("MQTT Client Initialized!");

    	char event_buf[EVENT_BUF_SIZE];
    	while (keep_running) 
    	{
        	ssize_t len = hostapd_listener_receive(event_buf, sizeof(event_buf));
        	if (len > 0) 
		{
            		LOG_INFO("Received event: %s", event_buf);
			
			char *ssid = get_ssid();
			if (!ssid)
			{
				ssid = "unknown_ssid";
			}

                        char *json_payload = build_event_json(event_buf, ssid);
			if (json_payload)
                       	{
                               	mqtt_client_publish(mqtt_config.mqtt_topic, json_payload);
                               	free(json_payload);
                        }
			else
                        {
                              	LOG_WARN("Failed to parse event to JSON: %s", event_buf);
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
        	LOG_ERROR("Failed to create MQTT subscriber\n");
        	return 1;
    	}
	LOG_DEBUG("Mosquitto created successfully!");

    	mosquitto_message_callback_set(mosq, on_message);

    	if (mosquitto_connect(mosq, mqtt_config.mqtt_host, mqtt_config.mqtt_port, 60) != MOSQ_ERR_SUCCESS) 
    	{
        	LOG_ERROR("Failed to connect to MQTT broker\n");
        	mosquitto_destroy(mosq);
        	return 1;
    	}

    	mosquitto_subscribe(mosq, NULL, mqtt_config.mqtt_topic, 0);
    	mosquitto_loop_start(mosq);

    	LOG_INFO("Subscribed to topic: %s\n", mqtt_config.mqtt_topic);
    	while (keep_running) 
    	{
        	sleep(1);
    	}

    	mosquitto_loop_stop(mosq, true);
    	mosquitto_disconnect(mosq);
    	mosquitto_destroy(mosq);
    	mosquitto_lib_cleanup();

	free_table();

    	return 0;
}

LogLevel get_log_level_from_user(const char *arg)
{
	if (strcmp(arg, "error") == 0)
        {
		return LOG_LEVEL_ERROR;
	}
	if (strcmp(arg, "warn")  == 0)
	{
		return LOG_LEVEL_WARNING;
	}
	if (strcmp(arg, "info")  == 0)
	{
		return LOG_LEVEL_INFO;
	}
	if (strcmp(arg, "debug") == 0)
	{
		return LOG_LEVEL_DEBUG;
	}
  
	return LOG_LEVEL_INFO;
}

int log_level = -1;

int main(int argc, char *argv[]) 
{
    	signal(SIGINT, int_handler);
    	signal(SIGTERM, int_handler);

	if (argc < 2) 
	{
        	LOG_ERROR("Usage: %s [publisher|subscriber] [log_level]\n", argv[0]);
		LOG_ERROR("Log levels: error, warn, info, debug\n");
        	return 1;
    	}
	LOG_DEBUG("Argument 1 passed successfully!");

	if (argc >= 3) 
	{
	 	log_level = get_log_level_from_user(argv[2]);
        	set_log_level(log_level);
		LOG_DEBUG("Log level set to %d", log_level);
    	} 
	else 
    	{
        	set_log_level(LOG_LEVEL_INFO);
		LOG_DEBUG("Default Log level INFO");
    	}
	
    	if (parse_mqtt_config(CONFIG, &mqtt_config) != 0) 
    	{
        	LOG_ERROR("Failed to load MQTT config\n");
        	return 1;
    	}

	int result = 1;

    	if (strcmp(argv[1], "publisher") == 0) 
    	{
		LOG_DEBUG("Publisher mode ON");
        	result = run_publisher();
    	} 	
    	else if (strcmp(argv[1], "subscriber") == 0) 
    	{
		LOG_DEBUG("Subscriber mode ON");
        	result = run_subscriber();
    	}
    	else 
    	{
		LOG_DEBUG("Use 'publisher' or 'subscriber'");
    	   	LOG_ERROR("Invalid mode: %s\n", argv[1]);
    	}

    	free(mqtt_config.mqtt_host);
	LOG_DEBUG("Host freed!");
    	free(mqtt_config.mqtt_topic);
	LOG_DEBUG("Topic freed!");

    	LOG_INFO("Exiting...");
	LOG_DEBUG("Exited!");

	return result;
}
