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

mqtt_subscribers *config_ptr;

mqtt_callback_t mqtt_callback;

void constructTopic(mqtt_component_client_config *config_struct){
    uint8_t count = 0;
    if ((config_struct->base_t_len + config_struct->sub_t_len) > FULL_TOPIC_MAX_LEN){
        ESP_LOGE(TAG,"Topic Too Long to use...");
    }

    for (uint8_t i = 0; i < config_struct->base_t_len - 1; i++){
        config_struct->full_topic[count] = config_struct->base_topic[i];
        count++;
    }

    for (uint8_t i = 0; i < config_struct->sub_t_len - 1; i++){
        config_struct->full_topic[count] = config_struct->sub_topic[i];
        count++;
    }
    config_struct->full_topic[count+1] = '\0';
    config_struct->full_topic_len = count;

}

static esp_err_t mqtt_event_handler(esp_mqtt_event_handle_t event)
{
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    // your_context_t *context = event->context;
    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
            for(int i = 0; i < NUMBER_OF_SUBSCRIBERS; i++){
                // ESP_LOGI(TAG, "Subscribing to %s with qos = %d",
                //         (*config_ptr)[i].full_topic,(*config_ptr)[i].qos);
                
                msg_id = esp_mqtt_client_subscribe(client, (*config_ptr)[i].full_topic, (*config_ptr)[i].qos);
                ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);
            }
            set_system_state(STATE_MQTT_CONNECTED);
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
            break;

        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
            msg_id = esp_mqtt_client_publish(client, "/topic/qos0", "data", 0, 0, 0);
            ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
            break;
        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_PUBLISHED:
            ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "MQTT_EVENT_DATA");

            for (uint8_t i = 0; i < NUMBER_OF_SUBSCRIBERS; i++){
                int ret = memcmp((*config_ptr)[i].full_topic,event->topic,event->topic_len);
                if(ret == 0){
                    ESP_LOGI(TAG,"Topic = %s",(*config_ptr)[i].full_topic);
                    if((*config_ptr)[i].callback != NULL){
                        (*config_ptr)[i].callback(event);
                    }
                }
            }
            break;
        case MQTT_EVENT_ERROR:
            ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
            break;
        default:
            ESP_LOGI(TAG, "Other event id:%d", event->event_id);
            break;
    }
    return ESP_OK;
}


static void mqtt_app_start(void)
{
    /*TODO: get config of the broker address*/
    esp_mqtt_client_config_t mqtt_cfg = {
        .uri = "http://192.168.2.2",
        .event_handle = mqtt_event_handler,
        .transport = MQTT_TRANSPORT_OVER_TCP
        // .user_context = (void *)your_context
    };

    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_start(client);
}

void mqtt_init()
{
    ESP_LOGI(TAG, "[APP] Startup..");
    ESP_LOGI(TAG, "[APP] Free memory: %d bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());

    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("MQTT_CLIENT", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT_TCP", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT_SSL", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT", ESP_LOG_VERBOSE);
    esp_log_level_set("OUTBOX", ESP_LOG_VERBOSE);

    mqtt_app_start();
}

void mqtt_set_config(mqtt_subscribers *config){
    config_ptr = config;
}