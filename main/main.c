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
#include "mbedtls/md5.h"
/*Includes from project*/
#include "wifi.h"
#include "statemachine.h"
#include "led_control.h"
#include "mqtt_client_component.h"
#include "sntp_component.h"
#include "http_component.h"
#include "mdns_component.h"
#include "storage_component.h"

#include "global.h"
#include "i2cdev.h"

/*led finished callbacks*/
void set_static_task_done_cb(void);

static const char *TAG = "Main";

struct tm system_time;
char system_time_str[64];

/*Structs for mqtt subscribing topics and configs*/
#define DEVICE_NAME CONFIG_DEVICE_DEF_NAME
#define BASE_TOPIC CONFIG_MQTT_BASE_TOPIC

#define TOPIC_STATE "state"

#define TOPIC_LED_SET_STATIC  "set_static"
#define TOPIC_LED_SET_FADE    "set_fade"
#define TOPIC_LED_SET_RANDOM  "set_random"
#define TOPIC_LED_BRIGHTNESS  "brightness"

#define FULL_SUB_STATIC_TOPIC BASE_TOPIC "/" DEVICE_NAME "/" TOPIC_LED_SET_STATIC
#define FULL_PUB_STATIC_TOPIC BASE_TOPIC "/" DEVICE_NAME "/" TOPIC_LED_SET_STATIC "/" TOPIC_STATE

#define FULL_SUB_FADE_TOPIC BASE_TOPIC "/" DEVICE_NAME "/" TOPIC_LED_SET_FADE
#define FULL_PUB_FADE_TOPIC BASE_TOPIC "/" DEVICE_NAME "/" TOPIC_LED_SET_FADE "/" TOPIC_STATE

#define FULL_SUB_RANDOM_TOPIC BASE_TOPIC "/" DEVICE_NAME "/" TOPIC_LED_SET_RANDOM
#define FULL_PUB_RANDOM_TOPIC BASE_TOPIC "/" DEVICE_NAME "/" TOPIC_LED_SET_RANDOM "/" TOPIC_STATE

#define FULL_SUB_BRIGHTNESS_TOPIC BASE_TOPIC "/" DEVICE_NAME "/" TOPIC_LED_BRIGHTNESS
#define FULL_PUB_BRIGHTNESS_TOPIC BASE_TOPIC "/" DEVICE_NAME "/" TOPIC_LED_BRIGHTNESS "/" TOPIC_STATE

#define FULL_SUB_STATE_TOPIC BASE_TOPIC "/" DEVICE_NAME 
#define FULL_PUB_STATE_TOPIC BASE_TOPIC "/" DEVICE_NAME "/" TOPIC_STATE

#define SET_STATIC_INDEX 0
#define SET_FADE_INDEX 1
#define SET_RANDOM_INDEX 2
#define SET_BRIGHTNESS_INDEX 3
#define SET_STATE_INDEX 4

void callback_set_static(esp_mqtt_event_handle_t event);
void callback_set_fade(esp_mqtt_event_handle_t event);
void callback_set_random(esp_mqtt_event_handle_t event);
void callback_set_bightness(esp_mqtt_event_handle_t event);
void callback_set_state(esp_mqtt_event_handle_t event);

mqtt_subscribers_t mqtt_configs={
    {
        .qos = 0,
        .full_sub_topic = FULL_SUB_STATIC_TOPIC,
        .full_sub_topic_len = sizeof(FULL_SUB_STATIC_TOPIC),
        .full_pub_topic = FULL_PUB_STATIC_TOPIC,
        .full_pub_topic_len = sizeof(FULL_PUB_STATIC_TOPIC),
        .callback = &callback_set_static
    },
    {
        .qos = 0,
        .full_sub_topic = FULL_SUB_FADE_TOPIC,
        .full_sub_topic_len = sizeof(FULL_SUB_FADE_TOPIC),
        .full_pub_topic = FULL_PUB_FADE_TOPIC,
        .full_pub_topic_len = sizeof(FULL_PUB_FADE_TOPIC),
        .callback = &callback_set_fade
    },
    {
        .qos = 0,
        .full_sub_topic = FULL_SUB_RANDOM_TOPIC,
        .full_sub_topic_len = sizeof(FULL_SUB_RANDOM_TOPIC),
        .full_pub_topic = FULL_PUB_RANDOM_TOPIC,
        .full_pub_topic_len = sizeof(FULL_PUB_RANDOM_TOPIC),
        .callback = &callback_set_random
    },
    {
        .qos = 0,
        .full_sub_topic = FULL_SUB_BRIGHTNESS_TOPIC,
        .full_sub_topic_len = sizeof(FULL_SUB_BRIGHTNESS_TOPIC),
        .full_pub_topic = FULL_PUB_BRIGHTNESS_TOPIC,
        .full_pub_topic_len = sizeof(FULL_PUB_BRIGHTNESS_TOPIC),
        .callback = &callback_set_bightness
    },
    {
        .qos = 0,
        .full_sub_topic = FULL_SUB_STATE_TOPIC,
        .full_sub_topic_len = sizeof(FULL_SUB_STATE_TOPIC),
        .full_pub_topic = FULL_PUB_STATE_TOPIC,
        .full_pub_topic_len = sizeof(FULL_PUB_STATE_TOPIC),
        .callback = &callback_set_state
    }
};

uint16_t globalFadeTime = 60 * 5;


sLedStripConfig_t led_config_s;


void callback(State st){
    switch(st){
        case STATE_INIT:
            led_control_init();
            initialize_external_rtc();
            break;
        case STATE_WIFI_CONNECTING:
            ESP_LOGI(TAG, "Setting mode to LED_MODE_CONNECTING_TO_AP");
            /*Colors for the config mode*/
            // led_config_s.channel[LEDC_R].hex_val = 35;
            // led_config_s.channel[LEDC_G].hex_val = 67;
            // led_config_s.channel[LEDC_B].hex_val = 88;
            // led_config_s.mode = LED_MODE_CONNECTING_TO_AP;

            // change_mode(&led_config_s);
            break;
        case STATE_WIFI_CONNECTED:
            ESP_LOGI(TAG, "Wifi Connected to station");
            initialize_sntp("CST6CDT,M4.1.0,M10.5.0");
            start_webserver();
            initialise_mdns((char*)&_global_configs.deviceID_str_global[0]);
            mqtt_init();
            break;
        case STATE_AP_STARTED:
            ESP_LOGI(TAG, "ESP_WIFI_MODE_AP ");
            /*Colors for the config mode*/
            // led_config_s.channel[LEDC_R].hex_val = 5;
            // led_config_s.channel[LEDC_G].hex_val = 61;
            // led_config_s.channel[LEDC_B].hex_val = 15;
            // led_config_s.mode = LED_MODE_CONNECTING_TO_AP;
            // change_mode(&led_config_s);
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
            mqtt_pub(mqtt_configs[SET_STATIC_INDEX].full_pub_topic,hass_colors(),0);
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

void callback_set_state(esp_mqtt_event_handle_t event){
    char data[4];
    memcpy(data,event->data,event->data_len);
    //ESP_LOGW(TAG,"HERE IT IS");
    sLedStripConfig_t current_led_state;
    current_led_state = get_led_state();
    data[event->data_len] = '\0';
    if(strcmp(data,"ON") == 0){
        if(!current_led_state.channel[LEDC_R].hex_val && !current_led_state.channel[LEDC_G].hex_val && !current_led_state.channel[LEDC_B].hex_val){

            led_config_s.mode = LED_MODE_STATIC;
            led_config_s.brightness = 0;
            led_config_s.channel[LEDC_R].hex_val = 255;
            led_config_s.channel[LEDC_G].hex_val = 255;
            led_config_s.channel[LEDC_B].hex_val = 255;
            change_mode(&led_config_s);
            mqtt_pub(mqtt_configs[SET_STATE_INDEX].full_pub_topic,"ON",0);
        }
    }
    else if(strcmp(data,"OFF") == 0){
        led_config_s.mode = LED_MODE_STATIC;
        led_config_s.brightness = 0;
        led_config_s.channel[LEDC_R].hex_val = 0;
        led_config_s.channel[LEDC_G].hex_val = 0;
        led_config_s.channel[LEDC_B].hex_val = 0;
        change_mode(&led_config_s);
        mqtt_pub(mqtt_configs[SET_STATE_INDEX].full_pub_topic,"OFF",0);
        
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
    led_config_s.brightness = 0;
    led_config_s.channel[LEDC_R].hex_val = red;
    led_config_s.channel[LEDC_G].hex_val = green;
    led_config_s.channel[LEDC_B].hex_val = blue;
    change_mode(&led_config_s);
}

#define isspace(c)           (c == ' ' || c == '\f' || c == '\n' || c == '\r' || c == '\t' || c == '\v')
int str2int(int *out, char *s, int base) {
    char *end;
    /*STR2INT_INCONVERTIBLE*/
    if (s[0] == '\0' || isspace(s[0]))
        return -4;
    long l = strtol(s, &end, base);
    if (*end != '\0')
        return -3;
    *out = l;
    return 0;
}
void callback_set_bightness(esp_mqtt_event_handle_t event){
    int brightness;
    char data[4];
    memcpy(data,event->data,event->data_len);
    data[event->data_len] = '\0';
    str2int(&brightness,data,10);
    ESP_LOGE(TAG, "Setting brightness to [%d]",brightness);
    set_global_brightness(brightness);


    led_config_s = get_led_state();
    led_config_s.mode = LED_MODE_STATIC;
    led_config_s.brightness = 1;
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

/* Declaration of led tasks done*/
void set_static_task_done_cb(void){
    ESP_LOGI(TAG,"set_static_task_done_cb");
    mqtt_pub(mqtt_configs[SET_STATIC_INDEX].full_pub_topic,hass_colors(),0);
}

/* Declaration of led tasks done*/
void brightness_task_done_cb(void){
    ESP_LOGI(TAG,"brightness_task_done_cb");
    char brightness[4];
    sprintf(brightness,"%d",global_brightness);

    mqtt_pub(mqtt_configs[SET_BRIGHTNESS_INDEX].full_pub_topic,brightness,0);
}

void app_main(){
    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    get_global_configs();

    if(_global_configs.deviceID_str_global[0] == 0){
        /*Device id global variable*/
        /*16 bytes of the md5 digest*/
        unsigned char deviceID[16];
        /*32 bytes of the hex string representation + the null strin terminator*/
        char deviceID_str[33];
        /*Generate a unique device id from the device mac address*/
        uint8_t mac[6];
        esp_read_mac(mac, ESP_MAC_WIFI_STA);
    
        mbedtls_md5_ret( (unsigned char *)mac,6,deviceID );

        sprintf(deviceID_str,"%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",deviceID[0],
            deviceID[1],deviceID[2],deviceID[3],deviceID[4],deviceID[5],deviceID[6],deviceID[7],deviceID[8],
            deviceID[9],deviceID[10],deviceID[11],deviceID[12],deviceID[13],deviceID[14],deviceID[15]);

        ESP_LOGI(TAG,"STA mac address Digest:[%s]",(char *)deviceID_str);

        set_global_deviceID(&deviceID[0]);
        set_global_deviceID_str(&deviceID_str[0]);

        save_global_configs();
    }

    /* Initialization of state machine*/
    time_set_flag = false;
    init_sm(&callback);
    set_system_state(STATE_UNDEFINED);

    /* Initialize i2c */

    while (i2cdev_init() != ESP_OK)
    {
        printf("Could not init I2Cdev library\n");
        vTaskDelay(250 / portTICK_PERIOD_MS);
    }

    /*register all the callbacks for the led tasks to execute when done*/
    led_register_done_cb(set_static_task_done_cb,LED_MODE_STATIC);
    led_register_done_cb(brightness_task_done_cb,LED_MODE_BRIGHTNESS);
    /* Set state machine to init*/
    set_system_state(STATE_INIT);
   
    /*Set mqtt configs and callbacks the actual init will be done in state machine*/
    mqtt_set_config(&mqtt_configs);
    
}
