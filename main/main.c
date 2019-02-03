/*  WiFi softAP Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_log.h"
#include "nvs_flash.h"

/*Includes from project*/
#include "wifi.h"
#include "statemachine.h"
#include "led_control.h"
#include "mqtt_client_component.h"

#include "esp_log.h"
#include "nvs_flash.h"

#include "cJSON.h"

/*Structs for mqtt subscribing topics and configs*/

#define BASE_TOPIC "/RGBController/"

#define TOPIC_1 "set_static"
#define TOPIC_2 "set_fade"
#define TOPIC_3 "set_random"

uint16_t globalFadeTime = 60 * 5;


led_strip_config_t led_config_s;

void callback(State st){
    switch(st){
        case STATE_INIT:
            break;
        case STATE_WIFI_CONNECTING:
            break;
        case STATE_WIFI_CONNECTED:
            ESP_LOGI("Main App", "Wifi Connected to station");
            mqtt_init();
            break;
        case STATE_AP_STARTED:
            ESP_LOGI("Main App", "ESP_WIFI_MODE_AP");
            break;
        case STATE_AP_GOT_CONFIG:
            break;
        case STATE_MQTT_CONNECTED:
            ESP_LOGI("Main App", "MQTT Connected and ready to receive messages");
            break;
        case STATE_RGB_STARTING:
            break;
        case STATE_RGB_STARTED:
            break;
        case STATE_UNDEFINED:
            break;
        default:
            ESP_LOGI("Main App", "Unhandled Event");
            break;
    }
}

void callback_set_static(esp_mqtt_event_handle_t event){
    /* TODO: Sanity check before parsing needed*/
    cJSON *root = cJSON_Parse(event->data);
    cJSON *format = cJSON_GetObjectItem(root,"colors");
    int red = cJSON_GetObjectItem(format,"red")->valueint;
    int green = cJSON_GetObjectItem(format,"green")->valueint;
    int blue = cJSON_GetObjectItem(format,"blue")->valueint;
    ESP_LOGI("Main App", "Got Colors r[%d]g[%d]b[%d]",red,green,blue);
    cJSON_Delete(root);

    led_config_s.mode = LED_MODE_STATIC;
    led_config_s.channel[LEDC_R].hex_val = red;
    led_config_s.channel[LEDC_G].hex_val = green;
    led_config_s.channel[LEDC_B].hex_val = blue;
    change_mode(&led_config_s);
}

void callback_set_fade(esp_mqtt_event_handle_t event){
    /*TODO: Sanity Check Before parsing needed*/
    cJSON *root = cJSON_Parse(event->data);
    cJSON *format = cJSON_GetObjectItem(root,"colors");
    int red = cJSON_GetObjectItem(format,"red")->valueint;
    int green = cJSON_GetObjectItem(format,"green")->valueint;
    int blue = cJSON_GetObjectItem(format,"blue")->valueint;
    int fade_time = cJSON_GetObjectItem(format,"time")->valueint;
    ESP_LOGI("Main App", "Got Colors r[%d]g[%d]b[%d] fadetime[%d]",red,green,blue,fade_time);
    cJSON_Delete(root);

    led_config_s.mode = LED_MODE_FADE;
    led_config_s.channel[LEDC_R].hex_val = red;
    led_config_s.channel[LEDC_G].hex_val = green;
    led_config_s.channel[LEDC_B].hex_val = blue;
    led_config_s.fadetime_s = fade_time;
    change_mode(&led_config_s);
}

void callback_set_random(esp_mqtt_event_handle_t event){
    ESP_LOGI("Main App", "Random Set");
}

mqtt_subscribers mqtt_configs={
    {
        .base_topic = BASE_TOPIC,
        .base_t_len = sizeof(BASE_TOPIC),
        .sub_topic = TOPIC_1,
        .sub_t_len = sizeof(TOPIC_1),
        .qos = 0,
        .callback = &callback_set_static
    },
    {
        .base_topic = BASE_TOPIC,
        .base_t_len = sizeof(BASE_TOPIC),
        .sub_topic = TOPIC_2,
        .sub_t_len = sizeof(TOPIC_2),
        .qos = 0,
        .callback = &callback_set_fade
    },
    {
        .base_topic = BASE_TOPIC,
        .base_t_len = sizeof(BASE_TOPIC),
        .sub_topic = TOPIC_3,
        .sub_t_len = sizeof(TOPIC_3),
        .qos = 0,
        .callback = &callback_set_random
    }
    };

void app_main(){
    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);


    /*Helper function to fill topics*/
    for(uint8_t i = 0; i < NUMBER_OF_SUBSCRIBERS; i++){
        constructTopic(&mqtt_configs[i]);
    }

    mqtt_set_config(&mqtt_configs);

    ESP_LOGI("Main App", "ESP_WIFI_MODE_AP");
    init_sm(&callback);
    wifi_config_init();
    led_control_init();


    
}
