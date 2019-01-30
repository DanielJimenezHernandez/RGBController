
/* LEDC (LED Controller) fade example
   This example code is in the Public Domain (or CC0 licensed, at your option.)
   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
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

uint32_t brightness_limit = 8191;
//uint32_t brightness_limit = 8191;
sLed_state global_led_state;

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

eLed_mode get_led_mode(){
    return global_led_state.mode;
}

sLed_state get_led_state(){
    return global_led_state;
}

void set_color(uint8_t r,uint8_t g,uint8_t b){
    /* color sanity check*/
    if (r > 254){r = 254;}else if(r < 0){r = 0;}
    if (g > 254){g = 254;}else if(g < 0){g = 0;}
    if (b > 254){b = 254;}else if(b < 0){b = 0;}

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
 


void led_task(void *pvParameter){
    while(1){
        switch(get_led_mode()){
            case LED_MODE_CONNECTING_TO_AP:
                break;
            case LED_MODE_READY_FOR_CONFIG:
                break;
            case LED_MODE_STATIC:
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

    // Initialize fade service.
    ledc_fade_func_install(0);

    xTaskCreate(&led_task, "led_task", 1024, NULL, 5, NULL);
}