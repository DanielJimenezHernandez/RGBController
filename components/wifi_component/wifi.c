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
#define STA_MAX_RETRIES       CONFIG_RGB_CONTROLLER_MAX_RETRIES

/*HARDCODED STUFF MUST REMOVE*/
#define HARDCODED_SSID        "LaFinK"
#define HARDCODED_PASSWD      "Lagertha123"
#define HARDCODED_AUTH      

/* Struct to control wifi configs*/
wifi_config_t wifi_config;

static const char *TAG = "WiFi_Config";

static int s_retry_num = 0;

void switch_to_ap_config();

void dummy_get_params(wifi_config_t * wifi_params){
    strncpy((char *)&wifi_params->sta.ssid[0],HARDCODED_SSID,strlen(HARDCODED_SSID));
    strncpy((char *)&wifi_params->sta.password[0],HARDCODED_PASSWD,strlen(HARDCODED_PASSWD));
    /* wen stored in flash this configs are modified*/
    wifi_params->sta.scan_method = 0;
    wifi_params->sta.bssid_set = 0;
}

static esp_err_t event_handler(void *ctx, system_event_t *event)
{
    switch(event->event_id) {
    case SYSTEM_EVENT_STA_START:
        esp_wifi_connect();
        break;
    case SYSTEM_EVENT_STA_STOP:
        ESP_LOGI(TAG,"STA STOP");
        switch_to_ap_config();
        break;
    case SYSTEM_EVENT_STA_GOT_IP:
        ESP_LOGI(TAG, "got ip:%s",
                 ip4addr_ntoa(&event->event_info.got_ip.ip_info.ip));
        s_retry_num = 0;
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        {
            if (s_retry_num < STA_MAX_RETRIES) {
                esp_wifi_connect();
                s_retry_num++;
                ESP_LOGI(TAG,"retry to connect to the AP");
            }
            else{
                ESP_LOGI(TAG,"connect to the AP fail\n");
                /*TODO: Init AP config again*/
                esp_wifi_stop();
                s_retry_num = 0;
            }
            
            break;
        }
    case SYSTEM_EVENT_AP_STACONNECTED:
        ESP_LOGI(TAG, "station:"MACSTR" join, AID=%d",
                 MAC2STR(event->event_info.sta_connected.mac),
                 event->event_info.sta_connected.aid);
        /*TOD: Replace this with udp service to get configs*/
        dummy_get_params(&wifi_config);
        /*Once we get wifi params connect to the access point configured in the wifi_global_params*/
        esp_wifi_stop();
        break;
    case SYSTEM_EVENT_AP_STADISCONNECTED:
        ESP_LOGI(TAG, "station:"MACSTR"leave, AID=%d",
                 MAC2STR(event->event_info.sta_disconnected.mac),
                 event->event_info.sta_disconnected.aid);
        break;
    case SYSTEM_EVENT_AP_STOP:
        ESP_LOGI(TAG,"AP STOP");
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
        ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
        ESP_ERROR_CHECK(esp_wifi_start() );
        break;
    default:
        break;
    }
    return ESP_OK;
}

void switch_to_ap_config(){
    /* fill config struct*/
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

void wifi_config_init(){
    /*create an LwIP core task and initialize LwIP-related work*/
    tcpip_adapter_init();
    /*create a system Event task and initialize an application event’s callback function*/
    ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL));
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_get_config(ESP_IF_WIFI_STA,&wifi_config));
    /*workaround for scan unsuccesful*/
    wifi_config.sta.scan_method = 0;
    wifi_config.sta.bssid_set = 0;
    ESP_LOGI(TAG,"wifi_config sta.ssid: [%s]",wifi_config.sta.ssid);
    /* If initial config from NVS is empty start ap and get gredentials*/
    if (strcmp((char *)wifi_config.sta.ssid, "") == 0){
        switch_to_ap_config();
    }
    else{
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
        ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
        ESP_ERROR_CHECK(esp_wifi_start() );
        ESP_LOGI(TAG, "wifi_init_sta finished.");
        ESP_LOGI(TAG, "connect to sta SSID:%s password:%s",
             wifi_config.sta.ssid, wifi_config.sta.password);
    }
}


