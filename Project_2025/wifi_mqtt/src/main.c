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
#include "network.h"
#include "sysinfo_publisher.h"

#define CONFIG "config/mqtt_config.json"
#define CLI_SOCKET_PATH "/tmp/wifi_mqtt_cli.sock"

static volatile int g_keep_running = 1;

void int_handler(int dummy) 
{
    	g_keep_running = 0;
}

mqtt_json mqtt_config;

char ssid_buf[256] = {0};

/* Function: callback_handler()
 * ------------------------------------------
 *
 * Netlink callback handler to extract the SSID from the received message.
 *
 * msg: pointer to the Netlink message
 * arg: optional argument (unused here)
 *
 * Returns: NL_OK if SSID was successfully extracted, NL_SKIP otherwise
 */

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
		LOG_DEBUG("Extracted SSID from Netlink: %s", ssid_buf);
        	return NL_OK;
    	}
	
	LOG_DEBUG("SSID attribute not found in Netlink message.");
    	return NL_SKIP;
}

/* Function: get_ssid()
 * ------------------------------------------
 *
 * Uses Netlink to get the SSID of the current wireless interface using nl80211.
 *
 * Takes no arguments.
 *
 * Returns: pointer to SSID string if found, NULL otherwise
 */

char *get_ssid() 
{
    	struct nl_sock *sock = nl_socket_alloc();
    	
	if (!sock) 
	{
		LOG_ERROR("Failed to allocate netlink socket");
		return NULL;
	}

	if (genl_connect(sock) != 0) 
	{
		LOG_ERROR("Failed to connect netlink socket");
		nl_socket_free(sock);
		return NULL;
	}
	LOG_DEBUG("Netlink socket connected.");

    	int driver_id = genl_ctrl_resolve(sock, "nl80211");

	if (driver_id < 0)
	{
		LOG_ERROR("nl80211 not found");
		nl_socket_free(sock);
		return NULL;
	}
	LOG_DEBUG("nl80211 driver ID resolved: %d", driver_id);

    	struct nl_msg *msg = nlmsg_alloc();
    	
	if (!msg)
	{
		LOG_ERROR("Failed to allocate nlmsg");
		nl_socket_free(sock);
		return NULL;
	}

	genlmsg_put(msg, 0, 0, driver_id, 0, NLM_F_DUMP, NL80211_CMD_GET_INTERFACE, 0);
    	nl_socket_modify_cb(sock, NL_CB_VALID, NL_CB_CUSTOM, callback_handler, NULL);

    	nl_send_auto(sock, msg);
    	nl_recvmsgs_default(sock);

    	nlmsg_free(msg);
    	nl_socket_free(sock);

	if (strlen(ssid_buf) > 0)
	{
		LOG_DEBUG("Final SSID: %s", ssid_buf);
		return ssid_buf;
	}

	LOG_DEBUG("SSID not found, returning NULL");

    	return NULL;
}

/* Function: handle_cli_commands()
 * ------------------------------------------
 *
 * Thread function to handle CLI socket commands from clients.
 * Responds to "show" command by writing Wi-Fi event hash table to the client.
 *
 * arg: unused (can be NULL)
 *
 * Returns: NULL
 */

void* handle_cli_commands(void* arg)
{
    	int server_fd;
	int client_fd;
    	struct sockaddr_un addr;
    	char buf[64];

    	unlink_socket_path(CLI_SOCKET_PATH);
	LOG_DEBUG("CLI socket path cleaned");

    	server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    	
	if (server_fd < 0) 
    	{
        	LOG_ERROR("CLI socket creation failed");
        	return NULL;
    	}
	LOG_DEBUG("CLI server socket created");

    	memset(&addr, 0, sizeof(addr));
    	addr.sun_family = AF_UNIX;
    	strncpy(addr.sun_path, CLI_SOCKET_PATH, sizeof(addr.sun_path)-1);

    	if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) 
    	{
        	LOG_ERROR("CLI socket bind failed");
        	close(server_fd);
        	return NULL;
    	}
	LOG_DEBUG("CLI socket bound to path");

    	listen(server_fd, 5);
	LOG_DEBUG("CLI server listening");

    	while (g_keep_running) 
    	{
        	client_fd = accept(server_fd, NULL, NULL);
        	if (client_fd < 0) 
		{
			LOG_WARN("CLI client accept failed");
			continue;
		}

        	int len = read(client_fd, buf, sizeof(buf)-1);

		if (len < 0)
		{
			LOG_WARN("CLI read error");
			close(client_fd);
			continue;
		}

        	buf[len] = '\0';
		LOG_DEBUG("CLI received: %s", buf);

        	if (strcmp(buf, "show") == 0) 
		{
            		char response[4096];
    			get_hash_table_as_string(response, sizeof(response));
    			write(client_fd, response, strlen(response)); 
			LOG_DEBUG("Sent CLI response");
        	}

        	close(client_fd);
    	}

    	close(server_fd);
    	unlink_socket_path(CLI_SOCKET_PATH);
	LOG_DEBUG("CLI server shutdown complete");

    	return NULL;
}

/* Function: on_message()
 * ------------------------------------------
 *
 * MQTT message callback for subscriber. Parses the message payload as JSON and
 * stores event or system info based on topic.
 *
 * mosq: pointer to the Mosquitto client
 * userdata: user-defined pointer (unused here)
 * message: pointer to received MQTT message
 *
 * Returns: void
 */

void on_message(struct mosquitto *mosq, void *userdata, const struct mosquitto_message *message)
{
    	const char *topic = message->topic;
    	const char *payload = (char *)message->payload;

    	LOG_INFO("[MQTT SUB] %s: %s\n", topic, payload);

    	cJSON *root = cJSON_Parse(payload);
	
	if (!root)
    	{
        	LOG_WARN("Failed to parse JSON\n");
        	return;
    	}

    	LOG_DEBUG("Successfully parsed JSON!");
	
    	if (strcmp(topic, "wifi/events") == 0)
    	{
        	LOG_DEBUG("Handling topic: wifi/events");

        	Wifi_Event event;
        	cJSON *mac = cJSON_GetObjectItem(root, "mac");
        	cJSON *ssid = cJSON_GetObjectItem(root, "ssid");
        	cJSON *event_type = cJSON_GetObjectItem(root, "event_type");
        	cJSON *timestamp = cJSON_GetObjectItem(root, "timestamp");

        	if (mac && ssid && event_type && timestamp)
        	{
            		LOG_DEBUG("Parsed fields - MAC: %s, SSID: %s, Event Type: %s, Timestamp: %s",
                      mac->valuestring, ssid->valuestring, event_type->valuestring, timestamp->valuestring);

            		strncpy(event.mac, mac->valuestring, sizeof(event.mac));
            		strncpy(event.ssid, ssid->valuestring, sizeof(event.ssid));
            		strncpy(event.event_type, event_type->valuestring, sizeof(event.event_type));
            		strncpy(event.timestamp, timestamp->valuestring, sizeof(event.timestamp));

            		LOG_DEBUG("Inserting event into hash table...");
            		insert_or_update_the_hash(event.mac, event.ssid, event.event_type, event.timestamp);
            		LOG_DEBUG("Event inserted into hash table.");
        	}
        	else
        	{
        	    	LOG_WARN("Incomplete Wi-Fi event JSON\n");
        	}
    	}
    	else if (strcmp(topic, "system/info") == 0)
    	{
        	LOG_DEBUG("Handling topic: system/info");

        	cJSON *hostname = cJSON_GetObjectItem(root, "hostname");
        	cJSON *cpu = cJSON_GetObjectItem(root, "cpu");
        	cJSON *memory = cJSON_GetObjectItem(root, "memory");

        	if (hostname && cpu && memory)
        	{
            		LOG_DEBUG("Parsed system info - Hostname: %s, CPU: %.2f, Memory: %.2f",
                      hostname->valuestring, cpu->valuedouble, memory->valuedouble);

            		LOG_INFO("[SYSINFO] Hostname: %s, CPU: %.2f%%, Memory: %.2f%%\n",
                     hostname->valuestring, cpu->valuedouble, memory->valuedouble);
        	}
        	else
        	{
        	    	LOG_WARN("Incomplete system info JSON\n");
        	}
    	}
    	else
    	{
        	LOG_WARN("Unknown topic: %s\n", topic);
    	}

    	cJSON_Delete(root);
    	LOG_DEBUG("Parsed cJSON object tree freed!");
}

/* Function: run_publisher()
 * ------------------------------------------
 *
 * Initializes the hostapd event listener and MQTT client.
 * Continuously publishes Wi-Fi events and periodic system info to MQTT topics.
 *
 * Takes no arguments.
 *
 * Returns: 0 on success, 1 on failure
 */

static int run_publisher() 
{
    	if (hostapd_listener_init() < 0) 	
    	{
        	LOG_ERROR("Failed to initialize hostapd listener");
        	return 1;
    	}
	LOG_DEBUG("Hostapd listener initialized");

    	if (mqtt_client_init(mqtt_config.mqtt_host, mqtt_config.mqtt_port, "wifi_mqtt_publisher") < 0) 
    	{
        	LOG_ERROR("Failed to initialize MQTT client");
        	hostapd_listener_cleanup();
        	return 1;
    	}
	LOG_DEBUG("MQTT client initialized");

    	char event_buf[EVENT_BUF_SIZE];
    	while (g_keep_running) 
    	{
        	ssize_t len = hostapd_listener_receive(event_buf, sizeof(event_buf));
        	if (len > 0) 
		{
            		LOG_INFO("Received event: %s", event_buf);
			
			char *ssid = get_ssid();
			if (!ssid)
			{
				ssid = "unknown_ssid";
				LOG_DEBUG("SSID not found, using default");
			}

                        char *json_payload = build_event_json(event_buf, ssid);
			if (json_payload)
                       	{
                               	mqtt_client_publish(mqtt_config.mqtt_topics[0], json_payload);
				LOG_DEBUG("Published Wi-Fi event JSON to topic: %s", mqtt_config.mqtt_topics[0]);
                               	free(json_payload);
                        }
			else
                        {
                              	LOG_WARN("Failed to parse event to JSON: %s", event_buf);
                      	}
        	}
		
		static time_t last_sysinfo_sent = 0;
		time_t now = time(NULL);

		if (now - last_sysinfo_sent >= 10)
		{
			char *sysinfo_json = build_sysinfo_json(hostapd_config_ack);
			if (sysinfo_json)
			{
				mqtt_client_publish(mqtt_config.mqtt_topics[1], sysinfo_json);
				LOG_DEBUG("Published system info JSON to topic: %s", mqtt_config.mqtt_topics[1]);
				free(sysinfo_json);
			}
			else
			{
				LOG_WARN("Failed to build system info JSON");
			}
			last_sysinfo_sent = now;
		}
    	}

    	mqtt_client_cleanup();
    	hostapd_listener_cleanup();
	LOG_DEBUG("Publisher cleanup complete");

    	return 0;
}

/* Function: run_subscriber()
 * ------------------------------------------
 *
 * Initializes MQTT subscriber and CLI socket handler thread.
 * Subscribes to topics and processes incoming MQTT messages.
 *
 * Takes no arguments.
 *
 * Returns: 0 on success, 1 on failure
 */

static int run_subscriber() 
{
    	if (mosquitto_lib_init() != MOSQ_ERR_SUCCESS) 
	{
    		LOG_ERROR("Failed to initialize Mosquitto library\n");
    		return 1;
	}	
	LOG_DEBUG("Mosquitto library initialized!");

	pthread_t cli_thread, sysinfo_thread;
	pthread_create(&cli_thread, NULL, handle_cli_commands, NULL);
	pthread_create(&sysinfo_thread, NULL, run_sysinfo_publisher, NULL);

    	struct mosquitto *mosq = mosquitto_new("wifi_mqtt_subscriber", true, NULL);
    	if (!mosq) 
    	{
        	LOG_ERROR("Failed to create MQTT subscriber\n");
        	return 1;
    	}

    	mosquitto_message_callback_set(mosq, on_message);

    	if (mosquitto_connect(mosq, mqtt_config.mqtt_host, mqtt_config.mqtt_port, 60) != MOSQ_ERR_SUCCESS) 
    	{
        	LOG_ERROR("Failed to connect to MQTT broker\n");
        	mosquitto_destroy(mosq);
        	return 1;
    	}

	for (int i = 0; i < mqtt_config.topic_count; i++) 
	{
    		mosquitto_subscribe(mosq, NULL, mqtt_config.mqtt_topics[i], 0);
		LOG_INFO("Subscribed to topic: %s\n", mqtt_config.mqtt_topics[i]);
	}

    	mosquitto_loop_start(mosq);

	for (int i = 0; i < mqtt_config.topic_count; i++) 
	{
    		mosquitto_subscribe(mosq, NULL, mqtt_config.mqtt_topics[i], 0);
    		LOG_INFO("Subscribed to topic: %s\n", mqtt_config.mqtt_topics[i]);
	}

    	while (g_keep_running) 
    	{
        	sleep(1);
    	}

    	mosquitto_loop_stop(mosq, true);
    	mosquitto_disconnect(mosq);
    	mosquitto_destroy(mosq);
    	mosquitto_lib_cleanup();

	free_wifi_table();

    	return 0;
}

/* Function: get_log_level_from_user()
 * ------------------------------------------
 *
 * Converts user-supplied log level string to corresponding enum value.
 *
 * arg: string representation of log level (error, warn, info, debug)
 *
 * Returns: matching LogLevel enum value, defaults to LOG_LEVEL_INFO
 */

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

/* Function: main()
 * ------------------------------------------
 *
 * Entry point of the program. Parses command-line arguments,
 * sets log level, loads MQTT config, and runs publisher or subscriber mode.
 *
 * argc: argument count
 * argv: argument vector
 *
 * Returns: 0 or 1 depending on success or failure
 */

int main(int argc, char *argv[]) 
{
    	signal(SIGINT, int_handler);
    	signal(SIGTERM, int_handler);

	if (argc < 2) 
	{
        	fprintf(stderr, "Usage: %s [publisher|subscriber] [log_level]\n", argv[0]);
		fprintf(stderr, "Log levels: error, warn, info, debug\n");
        	return 1;
    	}

	if (argc >= 3) 
	{
		int log_level = get_log_level_from_user(argv[2]);
        	set_log_level(log_level);
		LOG_DEBUG("log level set to %d", log_level); //this will come only for debug no then, what about the others??????????
    	} 
	else 
    	{
        	set_log_level(LOG_LEVEL_INFO);
    	}
	
    	if (parse_mqtt_config(CONFIG, &mqtt_config) != 0) 
    	{
        	LOG_ERROR("Failed to load MQTT config\n");
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
    	else 
    	{
    	   	LOG_ERROR("Invalid mode: %s. Use 'publisher' or 'subscriber'\n", argv[1]);
    	}


    	LOG_INFO("Exiting...");

	return result;
}
