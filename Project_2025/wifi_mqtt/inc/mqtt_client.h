#ifndef MQTT_CLIENT_H
#define MQTT_CLIENT_H

int mqtt_client_init(const char *host, int port, const char *client_id);

int mqtt_client_publish(const char *topic, const char *message);

void mqtt_client_cleanup();

#endif

