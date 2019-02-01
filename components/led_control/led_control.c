
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "driver/ledc.h"
#include "esp_err.h"
#include "esp_log.h"
#include "led_control.h"


const uint8_t gamma8[] = {
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  1,  1,  1,  1,
    1,  1,  1,  1,  1,  1,  1,  1,  1,  2,  2,  2,  2,  2,  2,  2,
    2,  3,  3,  3,  3,  3,  3,  3,  4,  4,  4,  4,  4,  5,  5,  5,
    5,  6,  6,  6,  6,  7,  7,  7,  7,  8,  8,  8,  9,  9,  9, 10,
   10, 10, 11, 11, 11, 12, 12, 13, 13, 13, 14, 14, 15, 15, 16, 16,
   17, 17, 18, 18, 19, 19, 20, 20, 21, 21, 22, 22, 23, 24, 24, 25,
   25, 26, 27, 27, 28, 29, 29, 30, 31, 32, 32, 33, 34, 35, 35, 36,
   37, 38, 39, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 50,
   51, 52, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 66, 67, 68,
   69, 70, 72, 73, 74, 75, 77, 78, 79, 81, 82, 83, 85, 86, 87, 89,
   90, 92, 93, 95, 96, 98, 99,101,102,104,105,107,109,110,112,114,
  115,117,119,120,122,124,126,127,129,131,133,135,137,138,140,142,
  144,146,148,150,152,154,156,158,160,162,164,167,169,171,173,175,
  177,180,182,184,186,189,191,193,196,198,200,203,205,208,210,213,
  215,218,220,223,225,228,231,233,236,239,241,244,247,249,252,255 };

#define LEDC_HS_TIMER          LEDC_TIMER_0
#define LEDC_HS_MODE           LEDC_HIGH_SPEED_MODE
#define LEDC_HS_R_GPIO         (5)
#define LEDC_HS_R_CHANNEL      LEDC_CHANNEL_0
#define LEDC_HS_G_GPIO         (18)
#define LEDC_HS_G_CHANNEL      LEDC_CHANNEL_1
#define LEDC_HS_B_GPIO         (19)
#define LEDC_HS_B_CHANNEL      LEDC_CHANNEL_2
#define LEDC_R                 0
#define LEDC_G                 1
#define LEDC_B                 2

#define LEDC_TEST_CH_NUM       (3)
#define LEDC_TEST_DUTY         (4000)
#define LEDC_TEST_FADE_TIME    (3000)

#define MODE_CHANGE_BIT         BIT0

static EventGroupHandle_t s_ledc_event_group;

uint32_t brightness_limit = 8191;
//uint32_t brightness_limit = 8191;
static sLed_state global_led_state;
uint8_t new_r = 0;
uint8_t new_g = 0;
uint8_t new_b = 0;

static const char *TAG = "Led_Control";

/*
    * Prepare and set configuration of timers
    * that will be used by LED Controller
    */
ledc_timer_config_t ledc_timer = {
    .duty_resolution = LEDC_TIMER_13_BIT, // resolution of PWM duty
    .freq_hz = 5000,                      // frequency of PWM signal
    .speed_mode = LEDC_HS_MODE,           // timer mode
    .timer_num = LEDC_HS_TIMER            // timer index
};

/*
    * Prepare individual configuration
    * for each channel of LED Controller
    * by selecting:
    * - controller's channel number
    * - output duty cycle, set initially to 0
    * - GPIO number where LED is connected to
    * - speed mode, either high or low
    * - timer servicing selected channel
    *   Note: if different channels use one timer,
    *         then frequency and bit_num of these channels
    *         will be the same
    */
ledc_channel_config_t ledc_channel[LEDC_TEST_CH_NUM] = {
    {
        .channel    = LEDC_HS_R_CHANNEL,
        .duty       = 0,
        .gpio_num   = LEDC_HS_R_GPIO,
        .speed_mode = LEDC_HS_MODE,
        .hpoint     = 0,
        .timer_sel  = LEDC_HS_TIMER
    },
    {
        .channel    = LEDC_HS_G_CHANNEL,
        .duty       = 0,
        .gpio_num   = LEDC_HS_G_GPIO,
        .speed_mode = LEDC_HS_MODE,
        .hpoint     = 0,
        .timer_sel  = LEDC_HS_TIMER
    },
    {
        .channel    = LEDC_HS_B_CHANNEL,
        .duty       = 0,
        .gpio_num   = LEDC_HS_B_GPIO,
        .speed_mode = LEDC_HS_MODE,
        .hpoint     = 0,
        .timer_sel  = LEDC_HS_TIMER
    },
};


sLed_state get_led_state(){
    return global_led_state;
}

void set_color(uint8_t r,uint8_t g,uint8_t b){
    /* color sanity check*/
    ESP_LOGI(TAG, "Setting - r[%d] g[%d] b[%d]",r,g,b);


    /* convert color to duty*/
    uint32_t r_duty = gamma8[r] * brightness_limit / 255;
    uint32_t g_duty = gamma8[g] * brightness_limit / 255;
    uint32_t b_duty = gamma8[b] * brightness_limit / 255;
        
    if(ledc_set_duty(ledc_channel[LEDC_R].speed_mode, ledc_channel[LEDC_R].channel, r_duty) == ESP_OK){
            if (ledc_update_duty(ledc_channel[LEDC_R].speed_mode, ledc_channel[LEDC_R].channel) == ESP_OK){
                global_led_state.r.duty = r_duty;
                global_led_state.r.hex_val = r;
            }
            else{
                ESP_LOGE(TAG, "Error updating PWM Value");
            }
    }
    else{
        ESP_LOGE(TAG, "Error setting PWM Value");
    }
    if(ledc_set_duty(ledc_channel[LEDC_G].speed_mode, ledc_channel[LEDC_G].channel, g_duty) == ESP_OK){
            if (ledc_update_duty(ledc_channel[LEDC_G].speed_mode, ledc_channel[LEDC_G].channel) == ESP_OK){
                global_led_state.g.duty = g_duty;
                global_led_state.g.hex_val = g;
            }
            else{
                ESP_LOGE(TAG, "Error updating PWM Value");
            }
    }
    else{
        ESP_LOGE(TAG, "Error setting PWM Value");
    }
    if(ledc_set_duty(ledc_channel[LEDC_B].speed_mode, ledc_channel[LEDC_B].channel, b_duty) == ESP_OK){
            if (ledc_update_duty(ledc_channel[LEDC_B].speed_mode, ledc_channel[LEDC_B].channel) == ESP_OK){
                global_led_state.b.duty = b_duty;
                global_led_state.b.hex_val = b;
            }
            else{
                ESP_LOGE(TAG, "Error updating PWM Value");
            }
    }
    else{
        ESP_LOGE(TAG, "Error setting PWM Value");
    }
}

/*routine to set a color change from mqtt or http*/
void led_task_set_static(void *pvParameter){
    sLed_state *ptr_led_config;
    ptr_led_config = (sLed_state *)pvParameter;
    while(1){
        set_color(ptr_led_config->r.hex_val,ptr_led_config->g.hex_val,ptr_led_config->b.hex_val);
        vTaskDelete(NULL);
    }

}

void led_task_set_fade(void *pvParameter){
    sLed_state *ptr_led_config;
    ptr_led_config = (sLed_state *)pvParameter;
    /*calculate steps required for fade time*/
    /*steps for r*/
    uint8_t start_red = global_led_state.r.hex_val;
    uint8_t end_red = ptr_led_config->r.hex_val;
    uint8_t steps_red = abs(start_red - end_red);
    uint8_t direction_red = (start_red > end_red) ? 0 : 1;
    uint8_t changetime_red = ptr_led_config->fadetime/steps_red;

    uint16_t fade_time_ms = 1000;
    portTickType ticks_to_count = fade_time_ms / portTICK_PERIOD_MS;
    portTickType start = xTaskGetTickCount();
    ESP_LOGI("Main App","ticks to count = %d start = %d",ticks_to_count,start);
    while(1){
        if((xTaskGetTickCount() - start) >= ticks_to_count){
            ESP_LOGI("Main App","contado 1 segundo");
            start = xTaskGetTickCount();
            vTaskDelete(NULL);
            }
    }
}

/*This function starts tasks with routines fot the leds*/
void change_mode(sLed_state *rgb_config){
    switch(rgb_config->mode){
        case LED_MODE_CONNECTING_TO_AP:
            break;
        case LED_MODE_READY_FOR_CONFIG:
            break;
        case LED_MODE_STATIC:
            ESP_LOGI("led_task", "LED_MODE_STATIC");
            xTaskCreate(&led_task_set_static, "led_task", 2048, rgb_config, 5, NULL);
            break;
        case LED_MODE_FADE:
            break;
        case LED_MODE_RANDOM_FADE:
            break;
        case LED_MODE_MUSIC:
            break;
        case LED_MODE_BEAT:
            break;
        case LED_MODE_STROBE:
            break;
        case LED_MODE_STOPPED:
            break;
    }
}



void led_control_init()
{
    int ch;
    
    global_led_state.mode = LED_MODE_STOPPED;
    // Set configuration of timer0 for high speed channels
    ledc_timer_config(&ledc_timer);

    // Set LED Controller with previously prepared configuration
    for (ch = 0; ch < LEDC_TEST_CH_NUM; ch++) {
        ledc_channel_config(&ledc_channel[ch]);
    }
}