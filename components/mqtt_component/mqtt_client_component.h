#ifndef MQTT_CLIENT_COMPONENT_H
#define MQTT_CLIENT_COMPONENT_H

#include "mqtt_client.h"

#define DEFAULT_BROKER CONFIG_MQTT_DEF_BROKER
#define NUMBER_OF_SUBSCRIBERS CONFIG_MQTT_SUBSCRIBERS
#define BASE_TOPIC_MAX_LEN 64
#define SUB_TOPIC_MAX_LEN 64
#define DEVICE_NAME_MAX_LEN 32
#define PUB_TOPIC_MAX_LEN 32
#define FULL_TOPIC_MAX_LEN (BASE_TOPIC_MAX_LEN + SUB_TOPIC_MAX_LEN + DEVICE_NAME_MAX_LEN + PUB_TOPIC_MAX_LEN)

typedef void (*mqtt_callback_t)(esp_mqtt_event_handle_t event);

typedef struct {
    uint8_t qos;
    char full_sub_topic[FULL_TOPIC_MAX_LEN];
    uint8_t full_sub_topic_len;
    char full_pub_topic[FULL_TOPIC_MAX_LEN];
    uint8_t full_pub_topic_len;
    mqtt_callback_t callback;
} mqtt_component_client_config;

typedef mqtt_component_client_config mqtt_subscribers[NUMBER_OF_SUBSCRIBERS];


void mqtt_set_config(mqtt_subscribers *config);
void mqtt_init();
void constructTopic(mqtt_component_client_config *config_struct);

void mqtt_pub(char *topic, char *payload, uint8_t retain);

#endif /* MQTT_CLIENT_H */