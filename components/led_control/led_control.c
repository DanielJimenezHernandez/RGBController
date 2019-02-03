
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "driver/ledc.h"
#include "esp_err.h"
#include "esp_log.h"
#include "led_control.h"
#include "gamma.h"

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
#define TIMER_DUTY_RES         LEDC_TIMER_13_BIT




static EventGroupHandle_t s_ledc_event_group;

bool cancel_fade = false;

static led_strip_config_t global_led_state;

static const char *TAG = "Led_Control";

/*
    * Prepare and set configuration of timers
    * that will be used by LED Controller
    */
ledc_timer_config_t ledc_timer = {
    .duty_resolution = TIMER_DUTY_RES, // resolution of PWM duty
    .freq_hz = 5000,                      // frequency of PWM signal
    .speed_mode = LEDC_HS_MODE,           // timer mode
    .timer_num = LEDC_HS_TIMER            // timer index
};

uint32_t max_duty = (0x01 << TIMER_DUTY_RES) - 1;

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
ledc_channel_config_t ledc_channel[LEDC_CH_NUM] = {
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

led_strip_config_t get_led_state(){
    return global_led_state;
}

uint8_t gamma_correction(uint8_t color){
    return gamma8[color];
}


uint32_t hex2duty(uint8_t hex){
    return hex * max_duty / 255;
}

uint8_t duty2hex(uint32_t duty){
    return duty * 255 / max_duty;
} 

void set_duty_chan(uint32_t duty, uint8_t ch){
    if(ledc_set_duty(ledc_channel[ch].speed_mode, ledc_channel[ch].channel, duty) == ESP_OK){
            if (ledc_update_duty(ledc_channel[ch].speed_mode, ledc_channel[ch].channel) == ESP_OK){
                global_led_state.channel[ch].duty = duty;
                global_led_state.channel[ch].hex_val = duty2hex(duty);
            }
            else{
                ESP_LOGE(TAG, "Error updating PWM Value");
            }
    }
    else{
        ESP_LOGE(TAG, "Error setting PWM Value");
    }
}

void set_color(uint8_t r,uint8_t g,uint8_t b, bool gamma){

    uint32_t r_duty;
    uint32_t g_duty;
    uint32_t b_duty;
    /* color sanity check*/
    ESP_LOGI(TAG, "Setting - r[%d] g[%d] b[%d]",r,g,b);

    if (gamma){
        /* convert color to duty with gamma correction*/
        r_duty = hex2duty(gamma_correction(r));
        g_duty = hex2duty(gamma_correction(g));
        b_duty = hex2duty(gamma_correction(b));   
    }
    else{
        /* convert color to duty*/
        r_duty = hex2duty(r);
        g_duty = hex2duty(g);
        b_duty = hex2duty(b); 
    }
    set_duty_chan(r_duty,LEDC_R);
    set_duty_chan(g_duty,LEDC_G);
    set_duty_chan(b_duty,LEDC_B);
}

/*Function using hardware fade, only to give a smooth transition between change of color in mode static*/
void smooth_color_transition(uint8_t r,uint8_t g,uint8_t b){

    /* convert color to duty*/
    uint32_t r_duty = hex2duty(gamma_correction(r));
    uint32_t g_duty = hex2duty(gamma_correction(g));
    uint32_t b_duty = hex2duty(gamma_correction(b));

    ledc_set_fade_with_time(ledc_channel[LEDC_R].speed_mode, ledc_channel[LEDC_R].channel,r_duty,500);
    ledc_set_fade_with_time(ledc_channel[LEDC_G].speed_mode, ledc_channel[LEDC_G].channel,g_duty,500);
    ledc_set_fade_with_time(ledc_channel[LEDC_B].speed_mode, ledc_channel[LEDC_B].channel,b_duty,500);

    ledc_fade_start(ledc_channel[LEDC_R].speed_mode,ledc_channel[LEDC_R].channel, LEDC_FADE_NO_WAIT);
    ledc_fade_start(ledc_channel[LEDC_G].speed_mode,ledc_channel[LEDC_G].channel, LEDC_FADE_NO_WAIT);
    ledc_fade_start(ledc_channel[LEDC_B].speed_mode,ledc_channel[LEDC_B].channel, LEDC_FADE_NO_WAIT);

    global_led_state.channel[LEDC_R].duty = r_duty;
    global_led_state.channel[LEDC_R].hex_val = r;

    global_led_state.channel[LEDC_G].duty = g_duty;
    global_led_state.channel[LEDC_G].hex_val = g;

    global_led_state.channel[LEDC_B].duty = b_duty;
    global_led_state.channel[LEDC_B].hex_val = b;

}

/*routine to set a color change from mqtt or http*/
void led_task_set_static(void *pvParameter){
    led_strip_config_t *ptr_led_config;
    ptr_led_config = (led_strip_config_t *)pvParameter;
    while(1){
        smooth_color_transition(
            ptr_led_config->channel[LEDC_R].hex_val,
            ptr_led_config->channel[LEDC_G].hex_val,
            ptr_led_config->channel[LEDC_B].hex_val);
        vTaskDelete(NULL);
    }

}

void led_task_set_fade(void *pvParameter){
    bool r_flag = false;
    bool g_flag = false;
    bool b_flag = false;

    led_strip_config_t *ptr_led_config;
    ptr_led_config = (led_strip_config_t *)pvParameter;

    uint32_t fadetime_ms = ptr_led_config->fadetime_s * 1000;
    /*calculate steps (in duty) required for fade time*/
    /*steps for r*/
    uint32_t start_red = hex2duty(global_led_state.channel[LEDC_R].hex_val);
    uint32_t end_red = hex2duty(ptr_led_config->channel[LEDC_R].hex_val);
    uint32_t steps_red = abs(start_red-end_red);
    uint8_t direction_red = (start_red > end_red) ? 0 : 1;
    /*steps per ms*/
    double step_per_tick_red = (steps_red != 0) ? (double)steps_red/(double)fadetime_ms : 0;
    /*amount of steps per tick*/
    double step_rate_red = 1/step_per_tick_red;

    uint32_t start_green = hex2duty(global_led_state.channel[LEDC_G].hex_val);
    uint32_t end_green = hex2duty(ptr_led_config->channel[LEDC_G].hex_val);
    uint32_t steps_green = abs(start_green-end_green);
    uint8_t direction_green = (start_green > end_green) ? 0 : 1;
    /*steps per ms*/
    double step_per_tick_green = (steps_green != 0) ? (double)steps_green/(double)fadetime_ms : 0;
    /*amount of steps per tick*/
    double step_rate_green = 1/step_per_tick_green;

    uint32_t start_blue = hex2duty(global_led_state.channel[LEDC_B].hex_val);
    uint32_t end_blue = hex2duty(ptr_led_config->channel[LEDC_B].hex_val);
    uint32_t steps_blue = abs(start_blue-end_blue);
    uint8_t direction_blue = (start_blue > end_blue) ? 0 : 1;
    /*steps per ms*/
    double step_per_tick_blue = (steps_blue != 0) ? (double)steps_blue/(double)fadetime_ms : 0;
    /*amount of steps per tick*/
    double step_rate_blue = 1/step_per_tick_blue;

    ESP_LOGI(TAG,"fadetime_ms = %d",fadetime_ms);


    ESP_LOGI(TAG,"increase 1 step every %.10f",step_rate_red);

    ESP_LOGI(TAG,"\nstart_red[%d]\nend_red[%d]\nsteps_to_red[%d]\ndirection_red[%d]\nsteps_per_tick[%.10f]",
        start_red,end_red,steps_red,direction_red,step_per_tick_red);

    portTickType countflag_red = 0, countflag_green = 0, countflag_blue = 0;
    portTickType xTimeBefore, xTotalTimeSuspended;
    xTimeBefore = xTaskGetTickCount();
    while( (!r_flag || !g_flag || !b_flag) && !cancel_fade){
        /*Red channel*/
        if((countflag_red >= step_rate_red) && !r_flag){
            (direction_red) ? start_red++ : start_red--;
            set_duty_chan(gamma[start_red],LEDC_R);
            countflag_red = 0;
            if(start_red == end_red){
                r_flag = true;
            }
        }
        /* Green channel*/
        if((countflag_green >= step_rate_green) && !g_flag){
            (direction_green) ? start_green++ : start_green--;
            set_duty_chan(gamma[start_green],LEDC_G);
            countflag_green = 0;
            if(start_green == end_green){
                g_flag = true;
            }
        }
        /*Blue channel*/
        if((countflag_blue >= step_rate_blue) && !b_flag){
            (direction_blue) ? start_blue++ : start_blue--;
            set_duty_chan(gamma[start_blue],LEDC_B);
            countflag_blue = 0;
            if(start_blue == end_blue){
                b_flag = true;
            }
        }
        countflag_red++;
        countflag_green++;
        countflag_blue++;
        vTaskDelay(1);
    }
    xTotalTimeSuspended = xTaskGetTickCount() - xTimeBefore;
    ESP_LOGI(TAG,"Fade Task Done in %d ticks",xTotalTimeSuspended);
    cancel_fade = false;
    vTaskDelete(NULL);
}

/*This function starts tasks with routines fot the leds*/
void change_mode(led_strip_config_t *rgb_config){
    switch(rgb_config->mode){
        case LED_MODE_CONNECTING_TO_AP:
            break;
        case LED_MODE_READY_FOR_CONFIG:
            break;
        case LED_MODE_STATIC:
            cancel_fade = true;
            ESP_LOGI(TAG, "LED_MODE_STATIC");
            xTaskCreate(&led_task_set_static, "led_task_set_static", 2048, rgb_config, 5, NULL);
            break;
        case LED_MODE_FADE:
            ESP_LOGI(TAG, "LED_MODE_FADE");
            xTaskCreate(&led_task_set_fade, "led_task_set_fade", 2048, rgb_config, 5, NULL);
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
    for (ch = 0; ch < LEDC_CH_NUM; ch++) {
        ledc_channel_config(&ledc_channel[ch]);
    }

    // Initialize fade service for smooth transition.
    ledc_fade_func_install(0);
}