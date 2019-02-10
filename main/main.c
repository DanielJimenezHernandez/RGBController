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
#include "cJSON.h"
/*Includes from project*/
#include "wifi.h"
#include "statemachine.h"
#include "led_control.h"
#include "mqtt_client_component.h"
#include "sntp_component.h"
#include "http_component.h"
#include "mdns_component.h"

#include "global.h"
#include "i2cdev.h"

static const char *TAG = "Main";

struct tm system_time;
char system_time_str[64];

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
            led_control_init();
            initialize_external_rtc();
            break;
        case STATE_WIFI_CONNECTING:
            ESP_LOGI(TAG, "Setting mode to LED_MODE_CONNECTING_TO_AP");
            /*Colors for the config mode*/
            led_config_s.channel[LEDC_R].hex_val = 35;
            led_config_s.channel[LEDC_G].hex_val = 67;
            led_config_s.channel[LEDC_B].hex_val = 88;
            led_config_s.mode = LED_MODE_CONNECTING_TO_AP;

            change_mode(&led_config_s);
            break;
        case STATE_WIFI_CONNECTED:
            ESP_LOGI(TAG, "Wifi Connected to station");
            initialize_sntp("UTC-6");
            start_webserver();
            initialise_mdns();
            mqtt_init();
            break;
        case STATE_AP_STARTED:
            ESP_LOGI(TAG, "ESP_WIFI_MODE_AP ");
            /*Colors for the config mode*/
            led_config_s.channel[LEDC_R].hex_val = 5;
            led_config_s.channel[LEDC_G].hex_val = 61;
            led_config_s.channel[LEDC_B].hex_val = 15;
            led_config_s.mode = LED_MODE_CONNECTING_TO_AP;
            change_mode(&led_config_s);
            start_webserver();
            break;
        case STATE_AP_GOT_CONFIG:
            ESP_LOGI(TAG, "STATE_AP_GOT_CONFIG try to connect to new ap");
            wifi_init_sta_new(&global_ssid[0],&global_passwd[0]);
            break;
        case STATE_MQTT_CONNECTED:
            ESP_LOGI(TAG, "MQTT Connected and ready to receive messages");
            led_config_s.mode = LED_MODE_STOPPED;
            change_mode(&led_config_s);
            break;
        case STATE_RGB_STARTING:
            break;
        case STATE_RGB_STARTED:
            wifi_component_init();
            break;
        case STATE_UNDEFINED:
            break;
        default:
            ESP_LOGI(TAG, "Unhandled Event");
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
    ESP_LOGI(TAG, "Got Colors r[%d]g[%d]b[%d]",red,green,blue);
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
    ESP_LOGI(TAG, "Got Colors r[%d]g[%d]b[%d] fadetime[%d]",red,green,blue,fade_time);
    cJSON_Delete(root);

    led_config_s.mode = LED_MODE_FADE;
    led_config_s.channel[LEDC_R].hex_val = red;
    led_config_s.channel[LEDC_G].hex_val = green;
    led_config_s.channel[LEDC_B].hex_val = blue;
    led_config_s.fadetime_s = fade_time;
    change_mode(&led_config_s);
}

void callback_set_random(esp_mqtt_event_handle_t event){
    ESP_LOGI(TAG, "Random Set");
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
    /* Initialization of state machine*/
    time_set_flag = false;
    init_sm(&callback);
    set_system_state(STATE_UNDEFINED);
    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    /* Initialize i2c */

    while (i2cdev_init() != ESP_OK)
    {
        printf("Could not init I2Cdev library\n");
        vTaskDelay(250 / portTICK_PERIOD_MS);
    }

    /* Set state machine to init*/
    set_system_state(STATE_INIT);


    /*Helper function to fill topics*/
    for(uint8_t i = 0; i < NUMBER_OF_SUBSCRIBERS; i++){
        constructTopic(&mqtt_configs[i]);
    }
    
    /*Set mqtt configs and callbacks the actual init will be done in state machine*/
    mqtt_set_config(&mqtt_configs);
    
}
