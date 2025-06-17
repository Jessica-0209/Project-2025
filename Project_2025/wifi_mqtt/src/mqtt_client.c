#include "mqtt_client.h"
#include "log.h"

#include <mosquitto.h>
#include <stdio.h>
#include <string.h>

static struct mosquitto *mosq = NULL;

/* Function: mqtt_client_init()
 * ------------------------------------------
 *
 * Initializes and connects an MQTT client to the specified broker.
 * Starts the Mosquitto loop in the background.
 *
 * host: MQTT broker hostname or IP address
 * port: MQTT broker port number
 * client_id: Unique identifier for the MQTT client
 *
 * Returns: 0 on success, -1 on failure
 */

int mqtt_client_init(const char *host, int port, const char *client_id) 
{
	LOG_DEBUG("Initializing MQTT client with ID: %s", client_id);
        LOG_DEBUG("Connecting to host: %s, port: %d", host, port);

    	if (mosquitto_lib_init() != MOSQ_ERR_SUCCESS) 
	{
	    	LOG_ERROR("Failed to initialize Mosquitto library\n");
	    	return 1;
	}
	LOG_DEBUG("Mosquitto Library Initialized!");

    	mosq = mosquitto_new(client_id, true, NULL);
    	if (!mosq) 
    	{
        	LOG_ERROR("Failed to create mosquitto client\n");
        	return -1;
    	}
	LOG_DEBUG("Mosquitto client created");

    	if (mosquitto_connect(mosq, host, port, 60) != MOSQ_ERR_SUCCESS) 
    	{
        	LOG_ERROR("Failed to connect to MQTT broker\n");
        	mosquitto_destroy(mosq);
        	mosq = NULL;
        	return -1;
    	}
	LOG_DEBUG("Mosquitto client connected to broker");

    	mosquitto_loop_start(mosq);
    	LOG_INFO("Connected to MQTT broker");

	return 0;
}

/* Function: mqtt_client_publish()
 * ------------------------------------------
 *
 * Publishes a message to the specified MQTT topic using the initialized client.
 *
 * topic: MQTT topic string to publish to
 * message: Message content to be published
 *
 * Returns: 0 on success, or Mosquitto error code on failure
 */

int mqtt_client_publish(const char *topic, const char *message) 
{
    	if (!mosq) 
    	{
		LOG_ERROR("MQTT client not initialized");
	    	return -1;
    	}

	LOG_DEBUG("Publishing to topic: %s", topic);
        LOG_DEBUG("Message: %s", message);

    	int ret = mosquitto_publish(mosq, NULL, topic, strlen(message), message, 1, false);
    	
	if (ret != MOSQ_ERR_SUCCESS)
        {
                LOG_WARN("Failed to publish message to topic: %s (Error: %d)", topic, ret);
        }
        else
        {
                LOG_DEBUG("Message published successfully to topic: %s", topic);
        }

	return ret;
}

/* Function: mqtt_client_cleanup()
 * ------------------------------------------
 *
 * Cleans up the MQTT client by stopping the loop, disconnecting, and freeing resources.
 *
 * Takes no arguments.
 *
 * Returns: void
 */

void mqtt_client_cleanup() 
{
	LOG_DEBUG("Cleaning up MQTT client");

    	if (mosq) 
    	{
        	mosquitto_loop_stop(mosq, true);
		LOG_DEBUG("Mosquitto loop stopped");

        	mosquitto_disconnect(mosq);
		LOG_DEBUG("Mosquitto disconnected");

        	mosquitto_destroy(mosq);
		LOG_DEBUG("Mosquitto client destroyed");

        	mosq = NULL;
        	
		mosquitto_lib_cleanup();
		LOG_DEBUG("Mosquitto library cleaned up");

		LOG_INFO("MQTT client cleanup done");
    	}
	else
        {
                LOG_DEBUG("MQTT client already NULL, nothing to clean up");
        }
}

