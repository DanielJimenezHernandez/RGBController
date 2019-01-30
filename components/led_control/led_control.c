
/* LEDC (LED Controller) fade example
   This example code is in the Public Domain (or CC0 licensed, at your option.)
   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "driver/ledc.h"
#include "esp_err.h"
#include "esp_log.h"
#include "led_control.h"

/*
 * About this example
d5 = R
d18 = G
d19 = B
 * 1. Start with initializing LEDC module:
 *    a. Set the timer of LEDC first, this determines the frequency
 *       and resolution of PWM.
 *    b. Then set the LEDC channel you want to use,
 *       and bind with one of the timers.
 *
 * 2. You need first to install a default fade function,
 *    then you can use fade APIs.
 *
 * 3. You can also set a target duty directly without fading.
 *
 * 4. This example uses GPIO18/19/4/5 as LEDC output,
 *    and it will change the duty repeatedly.
 *
 * 5. GPIO18/19 are from high speed channel group.
 *    GPIO4/5 are from low speed channel group.
 *
 */
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

static eLed_mode get_led_mode(){
    return global_led_state.mode;
}

sLed_state get_led_state(){
    return global_led_state;
}

void set_color(uint8_t r,uint8_t g,uint8_t b){
    /* color sanity check*/
    ESP_LOGI(TAG, "Setting - r[%d] g[%d] b[%d]",r,g,b);

    /* convert color to duty*/
    uint32_t r_duty = r * brightness_limit / 255;
    uint32_t g_duty = g * brightness_limit / 255;
    uint32_t b_duty = b * brightness_limit / 255;
        
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
 
void fade_to_color(uint8_t r,uint8_t g,uint8_t b){


    /* convert color to duty*/
    uint32_t r_duty = r * brightness_limit / 255;
    uint32_t g_duty = g * brightness_limit / 255;
    uint32_t b_duty = b * brightness_limit / 255;

    ledc_set_fade_with_time(ledc_channel[LEDC_R].speed_mode, ledc_channel[LEDC_R].channel,r_duty,500);
    ledc_set_fade_with_time(ledc_channel[LEDC_G].speed_mode, ledc_channel[LEDC_G].channel,g_duty,500);
    ledc_set_fade_with_time(ledc_channel[LEDC_B].speed_mode, ledc_channel[LEDC_B].channel,b_duty,500);

    ledc_fade_start(ledc_channel[LEDC_R].speed_mode,ledc_channel[LEDC_R].channel, LEDC_FADE_NO_WAIT);
    ledc_fade_start(ledc_channel[LEDC_G].speed_mode,ledc_channel[LEDC_G].channel, LEDC_FADE_NO_WAIT);
    ledc_fade_start(ledc_channel[LEDC_B].speed_mode,ledc_channel[LEDC_B].channel, LEDC_FADE_NO_WAIT);

}

void change_mode(sLed_state *rgb_config){
    global_led_state.mode = rgb_config->mode;
    if(rgb_config->mode == LED_MODE_STATIC || rgb_config->mode == LED_MODE_FADE){
        new_r = rgb_config->r.hex_val;
        new_g = rgb_config->g.hex_val;
        new_b = rgb_config->b.hex_val;
    }
    xEventGroupSetBits(s_ledc_event_group, MODE_CHANGE_BIT);
}

void led_task(void *pvParameter){
    while(1){
        switch(get_led_mode()){
            case LED_MODE_CONNECTING_TO_AP:
                
                break;
            case LED_MODE_READY_FOR_CONFIG:
                break;
            case LED_MODE_STATIC:
                set_color(new_r,new_g,new_b);
                ESP_LOGI("led_task", "LED_MODE_STATIC");
                xEventGroupWaitBits(s_ledc_event_group, MODE_CHANGE_BIT, true, true, portMAX_DELAY);
                break;
            case LED_MODE_FADE:
                fade_to_color(new_r,new_g,new_b);
                ESP_LOGI("led_task", "LED_MODE_FADE");
                xEventGroupWaitBits(s_ledc_event_group, MODE_CHANGE_BIT, true, true, portMAX_DELAY);
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
                set_color(0,0,0);
                ESP_LOGI(TAG, "LED_MODE_STOPPED");
                xEventGroupWaitBits(s_ledc_event_group, MODE_CHANGE_BIT, true, true, portMAX_DELAY);
                break;
        }
    }

}

void led_control_init()
{
    int ch;

    s_ledc_event_group = xEventGroupCreate();
    
    global_led_state.mode = LED_MODE_STOPPED;
    // Set configuration of timer0 for high speed channels
    ledc_timer_config(&ledc_timer);

    // Set LED Controller with previously prepared configuration
    for (ch = 0; ch < LEDC_TEST_CH_NUM; ch++) {
        ledc_channel_config(&ledc_channel[ch]);
    }

    // Initialize fade service.
    ledc_fade_func_install(0);

    xTaskCreate(&led_task, "led_task", 2048, NULL, 5, NULL);
}