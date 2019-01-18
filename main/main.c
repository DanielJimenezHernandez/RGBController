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



static const char *TAG = "main app";


void main_task(void *pvParameter)
{
    esp_err_t ret;
    set_state(STATE_INIT);
    
    while(1){
        switch(get_state()){
            case STATE_INIT:
                //Initialize NVS
                ret = nvs_flash_init();
                if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
                    ESP_ERROR_CHECK(nvs_flash_erase());
                    ret = nvs_flash_init();
                }
                ESP_ERROR_CHECK(ret);

                ESP_LOGI(TAG, "ESP_WIFI_MODE_AP");
                wifi_init_softap();
                break;
            case STATE_WIFI_CONNECTING:
                break;
            case STATE_WIFI_CONNECTED:
                break;
            case STATE_WIFI_DISCONNECTED:
                break;
            case STATE_AP_STARTED:
                break;
            case STATE_AP_GOT_CONFIG:
                break;
            case STATE_MQTT_CONNECTED:
                break;
            case STATE_MQTT_DISCONNECTED:
                break;
            case STATE_FULLY_OPERATIONAL:
                break;
            case STATE_UNDEFINED:
                break;
        }
    }

}

void app_main()
{
    xTaskCreate(&main_task, "main_task", configMINIMAL_STACK_SIZE, NULL, 5, NULL);
}
