#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "esp_wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event_loop.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"

#include "esp_log.h"
#include "mqtt_client.h"

#include "mqtt_client_component.h"
#include "statemachine.h"

static const char *TAG = "MQTT component";

mqtt_subscribers_t *config_ptr;

esp_mqtt_client_handle_t client_handle;

mqtt_callback_t mqtt_callback;

static esp_err_t mqtt_event_handler(esp_mqtt_event_handle_t event)
{
    ESP_LOGD(TAG, "New MQTT Event");
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    // your_context_t *context = event->context;
    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGD(TAG, "---MQTT_EVENT_CONNECTED");
            for(int i = 0; i < NUMBER_OF_SUBSCRIBERS; i++){
                ESP_LOGI(TAG,"Subscribing to %s",(*config_ptr)[i].full_sub_topic);
                msg_id = esp_mqtt_client_subscribe(client, (*config_ptr)[i].full_sub_topic, (*config_ptr)[i].qos);
            }
            set_system_state(STATE_MQTT_CONNECTED);
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGD(TAG, "---MQTT_EVENT_DISCONNECTED");
            break;

        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGD(TAG, "---MQTT_EVENT_SUBSCRIBED");
            break;
        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGD(TAG, "---MQTT_EVENT_UNSUBSCRIBED");
            break;
        case MQTT_EVENT_PUBLISHED:
            ESP_LOGD(TAG, "---MQTT_EVENT_PUBLISHED");
            break;
        case MQTT_EVENT_DATA:
            ESP_LOGD(TAG, "---MQTT_EVENT_DATA");
            char topic[128];
            snprintf(topic,event->topic_len + 1,"%s",event->topic);
            for (uint8_t i = 0; i < NUMBER_OF_SUBSCRIBERS; i++){
                int ret = strcmp((*config_ptr)[i].full_sub_topic,topic);
                if(ret == 0){
                    ESP_LOGI(TAG,"Topic = %s",(*config_ptr)[i].full_sub_topic);
                    if((*config_ptr)[i].callback != NULL){
                        (*config_ptr)[i].callback(event);
                    }
                }
            }
            break;
        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "---MQTT_EVENT_ERROR");
            break;
        default:
            ESP_LOGI(TAG, "----MQTT_EVENT_ERROR id:%d", event->event_id);
            break;
    }
    return ESP_OK;
}


void mqtt_init(void)
{
    ESP_LOGD(TAG, "Initializing MQTT component");

#ifdef CONFIG_MQTT_EXTERNAL_BROKER
    /*TODO: get config of the broker address*/
    esp_mqtt_client_config_t mqtt_cfg = {
        .uri = CONFIG_MQTT_DEF_BROKER,
        .event_handle = mqtt_event_handler,
        .transport = MQTT_TRANSPORT_OVER_TCP,
        .username = CONFIG_MQTT_BROKER_USERNAME,
        .password = CONFIG_MQTT_BROKER_PASSWD,
        .port = CONFIG_MQTT_PORT
        // .user_context = (void *)your_context
    };
#else
    esp_mqtt_client_config_t mqtt_cfg = {
        .uri = CONFIG_MQTT_DEF_BROKER,
        .event_handle = mqtt_event_handler,
        .transport = MQTT_TRANSPORT_OVER_TCP,
        // .user_context = (void *)your_context
    };  
#endif
    client_handle = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_start(client_handle);
}


void mqtt_set_config(mqtt_subscribers_t *config){
    config_ptr = config;
}

void mqtt_pub(char *topic, char *payload, uint8_t retain){
    ESP_LOGE(TAG, "Publishing [%s] to [%s]",payload, topic);
    esp_mqtt_client_publish(client_handle, topic, payload, 0, 0, retain);
}