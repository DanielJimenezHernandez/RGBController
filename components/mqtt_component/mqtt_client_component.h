#ifndef MQTT_CLIENT_COMPONENT_H
#define MQTT_CLIENT_COMPONENT_H

#define DEFAULT_BROKER CONFIG_MQTT_DEF_BROKER
#define NUMBER_OF_SUBSCRIBERS CONFIG_MQTT_SUBSCRIBERS
#define BASE_TOPIC_MAX_LEN 64
#define SUB_TOPIC_MAX_LEN 32
#define FULL_TOPIC_MAX_LEN (BASE_TOPIC_MAX_LEN + SUB_TOPIC_MAX_LEN)

typedef void (*mqtt_callback_t)(char *payload);

typedef struct {
    char base_topic[BASE_TOPIC_MAX_LEN];
    uint8_t base_t_len;
    char sub_topic[SUB_TOPIC_MAX_LEN];
    uint8_t sub_t_len;
    uint8_t qos;
    char full_topic[FULL_TOPIC_MAX_LEN];
    uint8_t full_topic_len;
    mqtt_callback_t callback;
} mqtt_component_client_config;

typedef mqtt_component_client_config mqtt_subscribers[NUMBER_OF_SUBSCRIBERS];


void mqtt_set_config(mqtt_subscribers *config);
void mqtt_init();
void constructTopic(mqtt_component_client_config *config_struct);

#endif /* MQTT_CLIENT_H */