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

#include "esp_log.h"
#include "nvs_flash.h"

void callback(State st){
    switch(st){
        case STATE_INIT:
            break;
        case STATE_WIFI_CONNECTING:
            break;
        case STATE_WIFI_CONNECTED:
            break;
        case STATE_AP_STARTED:
            ESP_LOGI("Main App", "ESP_WIFI_MODE_AP");
            break;
        case STATE_AP_GOT_CONFIG:
            break;
        case STATE_MQTT_CONNECTED:
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

void app_main(){
    // //Initialize NVS
    // esp_err_t ret = nvs_flash_init();
    // if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    //   ESP_ERROR_CHECK(nvs_flash_erase());
    //   ret = nvs_flash_init();
    // }
    // ESP_ERROR_CHECK(ret);
    
    // ESP_LOGI("Main App", "ESP_WIFI_MODE_AP");
    // init_sm(&callback);
    // wifi_config_init();
    led_control_init();
}
