/*  WiFi softAP Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
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
#include "dht.h"
#include "sound_component.h"
//ws1228b
#include "dled_pixel.h"
#include "dled_strip.h"
#include "esp32_rmt_dled.h"

#include "global.h"
#include "i2cdev.h"

/*led finished callbacks*/
void set_static_task_done_cb(void);

TimerHandle_t sensor_data_timer_h;

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
            ESP_LOGD(TAG, "STATE_INIT");
            led_control_init();
#ifdef CONFIG_DHT_ENABLE
            dht_init();
#endif
            initialize_external_rtc();
            break;
        case STATE_WIFI_CONNECTING:
            ESP_LOGD(TAG, "STATE_WIFI_CONNECTING");
            /*Colors for the config mode*/
            led_config_s.channel[LEDC_R].hex_val = 35;
            led_config_s.channel[LEDC_G].hex_val = 67;
            led_config_s.channel[LEDC_B].hex_val = 88;
            led_config_s.mode = LED_MODE_CONNECTING_TO_AP;

            change_mode(&led_config_s);
            break;
        case STATE_WIFI_CONNECTED:
            ESP_LOGD(TAG, "STATE_WIFI_CONNECTED");
            /*ToDo Grab timezone from client config*/
            initialize_sntp("CST6CDT,M4.1.0,M10.5.0");
            start_webserver();
            initialise_mdns((char*)&_global_configs.deviceID_str_global[0]);
            mqtt_init();
            break;
        case STATE_AP_STARTED:
            ESP_LOGD(TAG, "STATE_AP_STARTED");
            /*Colors for the config mode*/
            // led_config_s.channel[LEDC_R].hex_val = 5;
            // led_config_s.channel[LEDC_G].hex_val = 61;
            // led_config_s.channel[LEDC_B].hex_val = 15;
            // led_config_s.mode = LED_MODE_CONNECTING_TO_AP;
            // change_mode(&led_config_s);
            start_webserver();
            break;
        case STATE_AP_GOT_CONFIG:
            ESP_LOGD(TAG, "STATE_AP_GOT_CONFIG");
            wifi_init_sta_new(&global_ssid[0],&global_passwd[0]);
            break;
        case STATE_MQTT_CONNECTED:
            ESP_LOGD(TAG, "STATE_MQTT_CONNECTED");
            led_config_s.mode = LED_MODE_STOPPED;
            change_mode(&led_config_s);
            char color_response[16];
            hass_colors(&color_response[0]);
            mqtt_pub(mqtt_configs[SET_STATIC_INDEX].full_pub_topic,color_response,0);
            break;
        case STATE_RGB_STARTING:
            break;
        case STATE_RGB_STARTED:
            wifi_component_init();
            break;
        case STATE_UNDEFINED:
            ESP_LOGD(TAG,"STATE_UNDEFINED");
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
            char brightness[8];
            sprintf(brightness,"%d",get_global_brightness());
            mqtt_pub(mqtt_configs[SET_BRIGHTNESS_INDEX].full_pub_topic,brightness,0);
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
    char colors_hass[16];
    memset(colors_hass,0,sizeof(colors_hass));
    hass_colors(&colors_hass[0]);
    ESP_LOGD(TAG, "static colors for publish %s",colors_hass);
    mqtt_pub(mqtt_configs[SET_STATIC_INDEX].full_pub_topic,colors_hass,0);
    if(strcmp(colors_hass,"0,0,0") == 0){
        mqtt_pub(mqtt_configs[SET_STATE_INDEX].full_pub_topic,"OFF",0);
    }
    else{
        mqtt_pub(mqtt_configs[SET_STATE_INDEX].full_pub_topic,"ON",0);
    }

}

#ifdef CONFIG_DHT_ENABLE
sensor_data_t data;
void publish_sensor_data( TimerHandle_t xTimer ){
    char temp[16];
    char hum[16];
    get_dht_data(&data);

    sprintf(temp,"%.2f",data.temperature);
    sprintf(hum,"%.2f",data.humidity);

    mqtt_pub("RGB/temp",temp,0);
    mqtt_pub("RGB/hum",hum,0);
}
#endif

/* Declaration of led tasks done*/
void brightness_task_done_cb(void){
    ESP_LOGI(TAG,"brightness_task_done_cb");
    char brightness[4];
    sprintf(brightness,"%d",global_brightness);

    mqtt_pub(mqtt_configs[SET_BRIGHTNESS_INDEX].full_pub_topic,brightness,0);
}

void delay_ms(uint32_t ms)
{
    if (ms == 0) return;
    vTaskDelay(ms / portTICK_PERIOD_MS);
}




void test_ws1228b(void *pvParameter){
    esp_err_t err;
    rmt_pixel_strip_t rps;
    pixel_strip_t strip;

    dled_strip_init(&strip);
    dled_strip_create(&strip, DLED_WS281x, 10, 255);

    err = rmt_dled_create(&rps, &strip);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "[0x%x] rmt_dled_init failed", err);
        while(true) { }
    }

    err = rmt_dled_config(&rps, 18, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "[0x%x] rmt_dled_config failed", err);
        while(true) { }
    }
    while(1){
        uint16_t step;
        step = 0;
        // dled_pixel_set(&strip.pixels[0],255,0,0);
        // dled_pixel_set(&strip.pixels[1],0,255,0);
        // dled_pixel_set(&strip.pixels[2],0,0,255);
        // dled_strip_fill_buffer(&strip);
        // rmt_dled_send(&rps);



        // strip.pixels->r = 0;
        // strip.pixels->g = 255;
        // strip.pixels->b = 0;
        // dled_strip_fill_buffer(&strip);
        // rmt_dled_send(&rps);
        // strip.pixels++;
        //
        // strip.pixels->r = 0;
        // strip.pixels->g = 0;
        // strip.pixels->b = 255;
        // dled_strip_fill_buffer(&strip);
        // rmt_dled_send(&rps);
        // strip.pixels++;

        while (step < 6 * strip.length) {
            dled_pixel_move_pixel(strip.pixels, strip.length, strip.max_cc_val, step);
            dled_strip_fill_buffer(&strip);
            rmt_dled_send(&rps);
            step++;
            delay_ms(20);
        }
        
        step = 0;
        while (true) {
            dled_pixel_rainbow_step(strip.pixels, strip.length, strip.max_cc_val, step);
            dled_strip_fill_buffer(&strip);
            rmt_dled_send(&rps);
            step++;
            delay_ms(50);
        }
    }
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


    /*Set mqtt configs and callbacks the actual init will be done in state machine*/
    mqtt_set_config(&mqtt_configs);

/*If DHT enabled start a timer to publish every 5 minutes*/
#ifdef CONFIG_DHT_ENABLE

    sensor_data_timer_h = xTimerCreate("sensor_data timer",
                            60000 * 1 / portTICK_PERIOD_MS,
                            pdTRUE,
                            NULL,
                            publish_sensor_data);
    xTimerStart(sensor_data_timer_h,0);

#endif
    led_control_init();
    //sound_init();
    xTaskCreate(&test_ws1228b,"test_ws1228b",2048,NULL,5,NULL);
    //set_system_state(STATE_INIT);
}
