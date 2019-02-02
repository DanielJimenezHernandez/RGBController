
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "driver/ledc.h"
#include "esp_err.h"
#include "esp_log.h"

#include "statemachine.h"

#include "led_control.h"
#include "gamma.h"


#define LEDC_HS_TIMER          LEDC_TIMER_0
#define LEDC_HS_MODE           LEDC_HIGH_SPEED_MODE
#define LEDC_HS_R_GPIO         (5)
#define LEDC_HS_R_CHANNEL      LEDC_CHANNEL_0
#define LEDC_HS_G_GPIO         (18)
#define LEDC_HS_G_CHANNEL      LEDC_CHANNEL_1
#define LEDC_HS_B_GPIO         (19)
#define LEDC_HS_B_CHANNEL      LEDC_CHANNEL_2
#define TIMER_DUTY_RES         LEDC_TIMER_12_BIT




static EventGroupHandle_t s_ledc_event_group;

bool cancel_fade = false;

static led_strip_config_t global_led_state;

static const char *TAG = "Led_Control";

const static int LED_CHANGE_MODE_BIT = BIT0;

/*Tasks Handles*/
BaseType_t xReturned;
TaskHandle_t xConnectingTaskHandle;

/*
    * Prepare and set configuration of timers
    * that will be used by LED Controller
    */
ledc_timer_config_t ledc_timer = {
    .duty_resolution = TIMER_DUTY_RES, // resolution of PWM duty
    .freq_hz = 240,                      // frequency of PWM signal
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

uint32_t gamma_correction(uint32_t duty){
    return gamma12[duty];
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
        r_duty = gamma_correction(hex2duty(r));
        g_duty = gamma_correction(hex2duty(g));
        b_duty = gamma_correction(hex2duty(b));   
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
void smooth_color_transition(uint8_t r,uint8_t g,uint8_t b, uint16_t time_ms){

    /* convert color to duty*/
    uint32_t r_duty = gamma_correction(hex2duty(r));
    uint32_t g_duty = gamma_correction(hex2duty(g));
    uint32_t b_duty = gamma_correction(hex2duty(b));

    ledc_set_fade_with_time(ledc_channel[LEDC_R].speed_mode, ledc_channel[LEDC_R].channel,r_duty,time_ms);
    ledc_set_fade_with_time(ledc_channel[LEDC_G].speed_mode, ledc_channel[LEDC_G].channel,g_duty,time_ms);
    ledc_set_fade_with_time(ledc_channel[LEDC_B].speed_mode, ledc_channel[LEDC_B].channel,b_duty,time_ms);

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

void smooth_color_transition_blocking(uint8_t r,uint8_t g,uint8_t b, uint16_t time_ms){
    /* convert color to duty*/
    uint32_t r_duty = gamma_correction(hex2duty(r));
    uint32_t g_duty = gamma_correction(hex2duty(g));
    uint32_t b_duty = gamma_correction(hex2duty(b));

    ledc_set_fade_with_time(ledc_channel[LEDC_R].speed_mode, ledc_channel[LEDC_R].channel,r_duty,time_ms);
    ledc_set_fade_with_time(ledc_channel[LEDC_G].speed_mode, ledc_channel[LEDC_G].channel,g_duty,time_ms);
    ledc_set_fade_with_time(ledc_channel[LEDC_B].speed_mode, ledc_channel[LEDC_B].channel,b_duty,time_ms);

    ledc_fade_start(ledc_channel[LEDC_R].speed_mode,ledc_channel[LEDC_R].channel, LEDC_FADE_NO_WAIT);
    ledc_fade_start(ledc_channel[LEDC_G].speed_mode,ledc_channel[LEDC_G].channel, LEDC_FADE_NO_WAIT);
    ledc_fade_start(ledc_channel[LEDC_B].speed_mode,ledc_channel[LEDC_B].channel, LEDC_FADE_WAIT_DONE);

    global_led_state.channel[LEDC_R].duty = r_duty;
    global_led_state.channel[LEDC_R].hex_val = r;

    global_led_state.channel[LEDC_G].duty = g_duty;
    global_led_state.channel[LEDC_G].hex_val = g;

    global_led_state.channel[LEDC_B].duty = b_duty;
    global_led_state.channel[LEDC_B].hex_val = b;  
}

/*routine to set a color change from mqtt or http*/
void led_task_set_connecting(void *pvParameter){
    led_strip_config_t *ptr_led_config;
    uint32_t xNotification;
    ptr_led_config = (led_strip_config_t *)pvParameter;
    while((get_system_state() == STATE_WIFI_CONNECTING) ||
           (get_system_state() == STATE_AP_STARTED) ){
        smooth_color_transition_blocking(
            ptr_led_config->channel[LEDC_R].hex_val,
            ptr_led_config->channel[LEDC_G].hex_val,
            ptr_led_config->channel[LEDC_B].hex_val,
            1000);
        // xNotification = ulTaskNotifyTake(pdTRUE, 1000 / portTICK_PERIOD_MS );
        vTaskDelay(500);
        if( 0 ){
            /* If notification arrives cance operation and delete task */
            set_color(0,0,0,0);
            vTaskDelete(NULL);
        }
        else{
            /*if not continue wit the program execution*/
            smooth_color_transition_blocking(0,0,0,200);
        }
        
    }
    

}


/*routine to set a color change from mqtt or http*/
void led_task_set_config(void *pvParameter){
    led_strip_config_t *ptr_led_config;
    ptr_led_config = (led_strip_config_t *)pvParameter;
    while(1){

    }
    vTaskDelete(NULL);
}


/*routine to set a color change from mqtt or http*/
void led_task_set_static(void *pvParameter){
    led_strip_config_t *ptr_led_config;
    ptr_led_config = (led_strip_config_t *)pvParameter;
    while(1){
        smooth_color_transition(
            ptr_led_config->channel[LEDC_R].hex_val,
            ptr_led_config->channel[LEDC_G].hex_val,
            ptr_led_config->channel[LEDC_B].hex_val,
            500);
        vTaskDelete(NULL);
    }
    xEventGroupClearBits(s_ledc_event_group,LED_CHANGE_MODE_BIT);
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
            set_duty_chan(gamma12[start_red],LEDC_R);
            countflag_red = 0;
            if(start_red == end_red){
                r_flag = true;
            }
        }
        /* Green channel*/
        if((countflag_green >= step_rate_green) && !g_flag){
            (direction_green) ? start_green++ : start_green--;
            set_duty_chan(gamma12[start_green],LEDC_G);
            countflag_green = 0;
            if(start_green == end_green){
                g_flag = true;
            }
        }
        /*Blue channel*/
        if((countflag_blue >= step_rate_blue) && !b_flag){
            (direction_blue) ? start_blue++ : start_blue--;
            set_duty_chan(gamma12[start_blue],LEDC_B);
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
    /*Adjust the exact end color in case is finished with some off color*/
    if(!cancel_fade){
        set_color(end_red,end_green,end_blue,1);
    }
    xTotalTimeSuspended = xTaskGetTickCount() - xTimeBefore;
    ESP_LOGI(TAG,"Fade Task Done in %d ticks",xTotalTimeSuspended);
    cancel_fade = false;
    vTaskDelete(NULL);
}

/*This function starts tasks with routines fot the leds*/
void change_mode(led_strip_config_t *rgb_config){

    // if(xConnectingTaskHandle != NULL){
    //     xTaskNotifyGive(xConnectingTaskHandle);
    // }
    
    switch(rgb_config->mode){
        case LED_MODE_CONNECTING_TO_AP:
            ESP_LOGI(TAG, "LED_MODE_CONNECTING_TO_AP");
            xReturned = xTaskCreate(&led_task_set_connecting, 
                "led_task_set_connecting", 2048, rgb_config, 5, xConnectingTaskHandle);
            if(xReturned == errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY){
                ESP_LOGE(TAG,"COuld not allocate memory for led_task_set_connecting task");
            }
            break;
        case LED_MODE_READY_FOR_CONFIG:
            break;
        case LED_MODE_STATIC:
            cancel_fade = true;
            ESP_LOGI(TAG, "LED_MODE_STATIC");
            xTaskCreate(&led_task_set_static, "led_task_set_static", 2048, rgb_config, 5, NULL);
            cancel_fade = false;
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
            smooth_color_transition(0,0,0,10);
            break;
    }
}



void led_control_init()
{
    s_ledc_event_group = xEventGroupCreate();
    set_system_state(STATE_RGB_STARTING);
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
    set_system_state(STATE_RGB_STARTED);
}