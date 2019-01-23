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

/* The examples use WiFi configuration that you can set via 'make menuconfig'.

   If you'd rather not, just change the below entries to strings with
   the config you want - ie #define EXAMPLE_WIFI_SSID "mywifissid"
*/
#define AP_ESP_WIFI_SSID      CONFIG_RGB_CONTROLLER_AP_SSID
#define AP_ESP_WIFI_PASS      CONFIG_RGB_CONTROLLER_AP_PWD
#define AP_MAX_STA_CONN       CONFIG_RGB_CONTROLLER_MAX_STA_CON

/*HARDCODED STUFF MUST REMOVE*/
#define HARDCODED_SSID        "LaFinK"
#define HARDCODED_PASSWD      "Lagertha123"
#define HARDCODED_AUTH      

typedef struct {
    uint8_t ssid[32];           /**< SSID of ESP32 soft-AP */
    uint8_t password[64];       /**< Password of ESP32 soft-AP */
    wifi_auth_mode_t authmode;  /**< Auth mode of ESP32 soft-AP. Do not support AUTH_WEP in soft-AP mode */
} wifi_params_t;

/*struct to hold received wifi credentials*/
wifi_params_t wifi_global_params;


/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;
/* Struct to control wifi configs*/
wifi_config_t wifi_config;

static const char *TAG = "WiFi_Config";

static int s_retry_num = 0;

void dummy_get_params(wifi_params_t * wifi_params){
    strncpy((char *)&wifi_params->ssid[0],HARDCODED_SSID,strlen(HARDCODED_SSID));
    strncpy((char *)&wifi_params->password[0],HARDCODED_PASSWD,strlen(HARDCODED_PASSWD));
    wifi_params->authmode = 0;
}

static esp_err_t event_handler(void *ctx, system_event_t *event)
{
    switch(event->event_id) {
    case SYSTEM_EVENT_STA_START:
        esp_wifi_connect();
        break;
    case SYSTEM_EVENT_STA_GOT_IP:
        ESP_LOGI(TAG, "got ip:%s",
                 ip4addr_ntoa(&event->event_info.got_ip.ip_info.ip));
        s_retry_num = 0;
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        {
            if (s_retry_num < 3) {
                esp_wifi_connect();
                s_retry_num++;
                ESP_LOGI(TAG,"retry to connect to the AP");
            }
            ESP_LOGI(TAG,"connect to the AP fail\n");
            break;
        }
    case SYSTEM_EVENT_AP_STACONNECTED:
        ESP_LOGI(TAG, "station:"MACSTR" join, AID=%d",
                 MAC2STR(event->event_info.sta_connected.mac),
                 event->event_info.sta_connected.aid);
        dummy_get_params(&wifi_global_params);
        /*Once we get wifi params connect to the access point configured in the wifi_global_params*/
        esp_wifi_stop();
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
        ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
        ESP_ERROR_CHECK(esp_wifi_start() );
        break;
    case SYSTEM_EVENT_AP_STADISCONNECTED:
        ESP_LOGI(TAG, "station:"MACSTR"leave, AID=%d",
                 MAC2STR(event->event_info.sta_disconnected.mac),
                 event->event_info.sta_disconnected.aid);
        break;
    default:
        break;
    }
    return ESP_OK;
}

void wifi_config_init(){
    /*create an LwIP core task and initialize LwIP-related work*/
    tcpip_adapter_init();
    /*create a system Event task and initialize an application event’s callback function*/
    ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL));
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_get_config(ESP_IF_WIFI_STA,&wifi_config));
    ESP_LOGI(TAG,"wifi_config ap.ssid: [%s]",wifi_config.ap.ssid);
    /* If initial config from NVS is empty start ap and get gredentials*/
    if (strcmp((char *)wifi_config.sta.ssid, "") == 0){
        /* FIll needed configs for starting the AP*/
        strncpy((char *)&wifi_config.ap.ssid[0],CONFIG_RGB_CONTROLLER_AP_SSID,sizeof(wifi_config.ap.ssid));
        wifi_config.ap.ssid_len = strlen(CONFIG_RGB_CONTROLLER_AP_SSID);
        strncpy((char *)&wifi_config.ap.password[0],CONFIG_RGB_CONTROLLER_AP_PWD,sizeof(wifi_config.ap.password));
        wifi_config.ap.max_connection = CONFIG_RGB_CONTROLLER_MAX_STA_CON;
        wifi_config.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;

        if (strlen(CONFIG_RGB_CONTROLLER_AP_PWD) == 0){
            wifi_config.ap.authmode = WIFI_AUTH_OPEN;
        }
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
        ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config));
        ESP_ERROR_CHECK(esp_wifi_start());

        ESP_LOGI(TAG, "wifi_init_softap finished.SSID:%s password:%s",
             CONFIG_RGB_CONTROLLER_AP_SSID, CONFIG_RGB_CONTROLLER_AP_PWD);
    }
    else{
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
        ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
        ESP_ERROR_CHECK(esp_wifi_start() );
        ESP_LOGI(TAG, "wifi_init_sta finished.");
        ESP_LOGI(TAG, "connect to ap SSID:%s password:%s",
             wifi_config.ap.ssid, wifi_config.ap.password);
    }
}


