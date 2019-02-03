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
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#include "statemachine.h"

static EventGroupHandle_t s_wifi_event_group;

/* The examples use WiFi configuration that you can set via 'make menuconfig'.

   If you'd rather not, just change the below entries to strings with
   the config you want - ie #define EXAMPLE_WIFI_SSID "mywifissid"
*/
#define AP_ESP_WIFI_SSID      CONFIG_RGB_CONTROLLER_AP_SSID
#define AP_ESP_WIFI_PASS      CONFIG_RGB_CONTROLLER_AP_PWD
#define AP_MAX_STA_CONN       CONFIG_RGB_CONTROLLER_MAX_STA_CON
#define STA_MAX_RETRIES       CONFIG_RGB_CONTROLLER_MAX_RETRIES

/*HARDCODED STUFF MUST REMOVE*/
// #define HARDCODED_SSID        "LaFinK"
// #define HARDCODED_PASSWD      "Lagertha123"
#define HARDCODED_SSID        "INFINITUM8652_2.4"
#define HARDCODED_PASSWD      "oEaUtLiKCr"
  

/* Struct to control wifi configs*/
wifi_config_t wifi_config;
const static int AP_CONNECTED_BIT = BIT0;
const static int AP_GOT_CONFIG_BIT = BIT1;
const static int STA_CONNECTED_BIT = BIT2;
const static int STA_TIMEOUT = BIT3;
const static int WIFI_FIRST_INIT = BIT4;

/*static functions*/
void wifi_init_ap();
void wifi_init_sta();

static const char *TAG = "WiFi component";

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

static int s_retry_num = 0;

static esp_err_t event_handler(void *ctx, system_event_t *event)
{
    esp_err_t err;

    switch(event->event_id) {
    case SYSTEM_EVENT_STA_START:
        ESP_LOGI(TAG,"SYSTEM_EVENT_STA_START");
        esp_wifi_connect();
        set_system_state(STATE_WIFI_CONNECTING);
        break;
    case SYSTEM_EVENT_STA_STOP:
        ESP_LOGI(TAG,"SYSTEM_EVENT_STA_STOP");
        wifi_init_ap();
        break;
    case SYSTEM_EVENT_STA_GOT_IP:
        set_system_state(STATE_WIFI_CONNECTED);
        ESP_LOGI(TAG,"SYSTEM_EVENT_STA_GOT_IP");
        ESP_LOGI(TAG, "got ip:%s",
                 ip4addr_ntoa(&event->event_info.got_ip.ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, STA_CONNECTED_BIT);
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        {
            ESP_LOGI(TAG,"SYSTEM_EVENT_STA_DISCONNECTED");
            if (s_retry_num < STA_MAX_RETRIES) {
                esp_wifi_connect();
                xEventGroupClearBits(s_wifi_event_group, STA_CONNECTED_BIT);
                s_retry_num++;
                ESP_LOGI(TAG,"retry to connect to the AP");
            }
            else{
                /*After 5 retries init accespoint as config*/
                err = esp_wifi_stop();
                ESP_LOGI(TAG,"esp_wifi_stop() err = %d",err);
            }
            ESP_LOGI(TAG,"connect to the AP fail");
            break;
        }
    default:
        break;
    }
    return ESP_OK;
}

void wifi_init_sta(){
    s_wifi_event_group = xEventGroupCreate();

    tcpip_adapter_init();
    ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL) );

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = HARDCODED_SSID,
            .password = HARDCODED_PASSWD
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_sta finished.");
    ESP_LOGI(TAG, "connect to ap SSID:%s password:%s",
             HARDCODED_SSID, HARDCODED_PASSWD);
}

void wifi_init_ap(){
    wifi_config_t wifi_config = {
        .ap = {
            .ssid = AP_ESP_WIFI_SSID,
            .ssid_len = strlen(AP_ESP_WIFI_SSID),
            .password = AP_ESP_WIFI_PASS,
            .max_connection = AP_MAX_STA_CONN,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_ap finished.");
    ESP_LOGI(TAG, "ap SSID:%s password:%s",
             AP_ESP_WIFI_SSID, AP_ESP_WIFI_PASS);
}

void wifi_component_init()
{
    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");
    wifi_init_sta();
}


