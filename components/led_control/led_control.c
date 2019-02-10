#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"

#include "driver/ledc.h"
#include "esp_err.h"
#include "esp_log.h"
#include "cJSON.h"

#include "statemachine.h"

#include "led_control.h"

#include "gamma.h"


#define LEDC_HS_TIMER          LEDC_TIMER_0
#define LEDC_HS_MODE           LEDC_HIGH_SPEED_MODE
#define LEDC_HS_R_GPIO         CONFIG_RGB_CONTROLLER_R_Channel
#define LEDC_HS_R_CHANNEL      LEDC_CHANNEL_0
#define LEDC_HS_G_GPIO         CONFIG_RGB_CONTROLLER_G_Channel
#define LEDC_HS_G_CHANNEL      LEDC_CHANNEL_1
#define LEDC_HS_B_GPIO         CONFIG_RGB_CONTROLLER_B_Channel
#define LEDC_HS_B_CHANNEL      LEDC_CHANNEL_2
#define TIMER_DUTY_RES         LEDC_TIMER_12_BIT

#define UP_D    1
#define DOWN_D  0
#define NOT_D   2

led_task_done_cb_t led_callbacks[LED_TASKS_NUMBER];

/*Debug String*/
static const char *TAG = "Led Control";
char hexyfy_buffer[8];

/*semaphore for accesing the led peripheral*/
SemaphoreHandle_t xLedMutex = NULL;

/*Tasks Handles*/
BaseType_t xReturned;
TaskHandle_t xHled_task_set_connecting;
TaskHandle_t xHled_task_set_fade;
TaskHandle_t xHled_task_set_static;

/*Config Structures for the tasks*/
led_strip_config_t rgb_config_fade;
led_strip_config_t rgb_config_static;
led_strip_config_t rgb_config_connecting;

/*Bits for the event groups*/

#define EG_FADE_START_BIT           BIT0
#define EG_FADE_CANCEL_BIT          BIT1

#define EG_CONNECTING_START_BIT     BIT0
#define EG_AP_STARTED_BIT

#define EG_STATIC_START_BIT         BIT0

/*Event Groups*/
EventGroupHandle_t eGLed_task_set_connecting;
EventGroupHandle_t eGLed_task_set_fade;
EventGroupHandle_t eGLed_task_set_static;

led_strip_config_t global_led_state;


/*Configuration structures*/
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

//create a monitor with a list of supported resolutions
//NOTE: Returns a heap allocated string, you are required to free it after use.
char* jsonify_colors(void)
{
    char *string = NULL;

    cJSON *root;
    cJSON *color;

    root=cJSON_CreateObject();	
	cJSON_AddItemToObject(root, "colors", color=cJSON_CreateObject());
    cJSON_AddNumberToObject(color,"red",	global_led_state.channel[LEDC_R].hex_val);
    cJSON_AddNumberToObject(color,"green",	global_led_state.channel[LEDC_G].hex_val);
    cJSON_AddNumberToObject(color,"blue",	global_led_state.channel[LEDC_B].hex_val);

    string = cJSON_PrintUnformatted(root);

    cJSON_Delete(root);
    return string;
}


char* hexify_colors(){
    sprintf(hexyfy_buffer,"#%02x%02x%02x",
            global_led_state.channel[LEDC_R].hex_val,
            global_led_state.channel[LEDC_G].hex_val,
            global_led_state.channel[LEDC_B].hex_val);
    return &hexyfy_buffer[0];
} 

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
    ptr_led_config = (led_strip_config_t *)pvParameter;


    for( ;; ){

        xEventGroupWaitBits(eGLed_task_set_connecting,
                            EG_CONNECTING_START_BIT,
                            pdTRUE,
                            pdTRUE,
                            portMAX_DELAY);
        /*Routine for sta connecting */
        while( (get_system_state() == STATE_WIFI_CONNECTING) ){
            xSemaphoreTake(xLedMutex,portMAX_DELAY);
            smooth_color_transition_blocking(
                ptr_led_config->channel[LEDC_R].hex_val,
                ptr_led_config->channel[LEDC_G].hex_val,
                ptr_led_config->channel[LEDC_B].hex_val,
                1000);
            xSemaphoreGive(xLedMutex);
            vTaskDelay(500);
            if( 0 ){
                /* If notification arrives cancel operation and delete task */
                xSemaphoreTake(xLedMutex,portMAX_DELAY);
                set_color(0,0,0,0);
                xSemaphoreGive(xLedMutex);
                vTaskDelete(NULL);
            }
            else{
                /*if not continue wit the program execution*/
                xSemaphoreTake(xLedMutex,portMAX_DELAY);
                smooth_color_transition_blocking(0,0,0,200);
                xSemaphoreGive(xLedMutex);
            }
        }
        
        /*Routine for ap started */
        while( (get_system_state() == STATE_AP_STARTED) ){
            xSemaphoreTake(xLedMutex,portMAX_DELAY);
            smooth_color_transition_blocking(
                ptr_led_config->channel[LEDC_R].hex_val,
                ptr_led_config->channel[LEDC_G].hex_val,
                ptr_led_config->channel[LEDC_B].hex_val,
                1000);
            xSemaphoreGive(xLedMutex);
            vTaskDelay(500);
            if( 0 ){
                /* If notification arrives cancel operation and delete task */
                xSemaphoreTake(xLedMutex,portMAX_DELAY);
                set_color(0,0,0,0);
                xSemaphoreGive(xLedMutex);
                vTaskDelete(NULL);
            }
            else{
                /*if not continue wit the program execution*/
                xSemaphoreTake(xLedMutex,portMAX_DELAY);
                smooth_color_transition_blocking(0,0,0,200);
                xSemaphoreGive(xLedMutex);
            }
        }
    }
}



/*routine to set a color change from mqtt or http*/
void led_task_set_static(void *pvParameter){
    led_strip_config_t *ptr_led_config;
    ptr_led_config = (led_strip_config_t *)pvParameter;
    while(1){
        xEventGroupWaitBits(
            eGLed_task_set_static,
            EG_STATIC_START_BIT,
            pdTRUE,
            pdTRUE,
            portMAX_DELAY);
        xSemaphoreTake(xLedMutex,portMAX_DELAY);
        smooth_color_transition_blocking(
            ptr_led_config->channel[LEDC_R].hex_val,
            ptr_led_config->channel[LEDC_G].hex_val,
            ptr_led_config->channel[LEDC_B].hex_val,
            500);
        xSemaphoreGive(xLedMutex);
        if (led_callbacks[LED_MODE_STATIC] != NULL){
            led_callbacks[LED_MODE_STATIC]();
        }
    }
}

/*Helper function to calculate step rate*/

inline led_fade_params_t get_step_rate(uint32_t start, uint32_t end, uint32_t fade_time){
    led_fade_params_t ret;
    uint32_t steps = abs(start-end);
    /* 1 = up 0 = down 2 = dont change*/
    uint8_t direction = (start > end) ? DOWN_D : UP_D;
    /*steps per ms*/
    double steps_per_tick = (fade_time != 0) ? (double)steps/fade_time : 0;
    /*amount of steps per tick*/
    double step_rate = (steps_per_tick != 0) ? 1/steps_per_tick : 0;
    if(step_rate == 0){
        direction = NOT_D;
    }
    // ESP_LOGW(TAG,"start=%d end=%d steps=%d direction=%d steps_per_tick=%f step_rate=%f",
    //     start,end,steps,direction,steps_per_tick,step_rate);
    ret.direction = direction;
    ret.step_rate = step_rate;
    return ret;
}

void led_task_set_fade(void *pvParameter){
    ESP_LOGI(TAG,"led_task_set_fade started");
    /*Pointer to the led config structure (Needs to be filled before unblock)*/
    led_strip_config_t *ptr_led_config = (led_strip_config_t *)pvParameter;
    /*Flags to indicate end of color fade*/
    bool r_flag;
    bool g_flag;
    bool b_flag;
    EventBits_t cancel_flag = 0;
    led_fade_params_t r_params, g_params, b_params;
    uint32_t start_red,end_red,start_green,end_green,start_blue,end_blue,fadetime_ms;
    portTickType countflag_red,countflag_green,countflag_blue;
    
    
    for ( ;; ){
        ESP_LOGI(TAG,"[led_task_set_fade] Waiting for signal...");
        xEventGroupWaitBits(eGLed_task_set_fade,
                            EG_FADE_START_BIT,
                            pdFALSE,
                            pdTRUE,
                            portMAX_DELAY);
        r_flag = false;
        g_flag = false;
        b_flag = false;
        
        cancel_flag = xEventGroupGetBits(eGLed_task_set_fade) & EG_FADE_CANCEL_BIT;

        fadetime_ms = ptr_led_config->fadetime_s * 1000;

        start_red = hex2duty(global_led_state.channel[LEDC_R].hex_val);
        end_red = hex2duty(ptr_led_config->channel[LEDC_R].hex_val);
        r_params = get_step_rate(start_red,end_red,fadetime_ms);

        start_green = hex2duty(global_led_state.channel[LEDC_G].hex_val);
        end_green = hex2duty(ptr_led_config->channel[LEDC_G].hex_val);
        g_params = get_step_rate(start_green,end_green,fadetime_ms);

        start_blue = hex2duty(global_led_state.channel[LEDC_B].hex_val);
        end_blue = hex2duty(ptr_led_config->channel[LEDC_B].hex_val);
        b_params = get_step_rate(start_blue,end_blue,fadetime_ms);

        if (r_params.direction == NOT_D){
            r_flag = true;
        }

        if (g_params.direction == NOT_D){
            g_flag = true;
        }

        if (b_params.direction == NOT_D){
            b_flag = true;
        }

        countflag_red = 0, countflag_green = 0, countflag_blue = 0;
        portTickType xTimeBefore, xTotalTime;

        xTimeBefore = xTaskGetTickCount();
        while( (!r_flag || !g_flag || !b_flag) && (!cancel_flag) ){
            /*Red channel*/
            if((countflag_red >= r_params.step_rate) && (!r_flag) && (r_flag == false)){
                (r_params.direction) ? start_red++ : start_red--;
                xSemaphoreTake(xLedMutex,portMAX_DELAY);
                set_duty_chan(gamma12[start_red],LEDC_R);
                xSemaphoreGive(xLedMutex);
                countflag_red = 0;
                if(start_red == end_red){
                    r_flag = true;
                }
            }
            /* Green channel*/
            if((countflag_green >= g_params.step_rate) && (!g_flag) && (g_flag == false)){
                (g_params.direction) ? start_green++ : start_green--;
                xSemaphoreTake(xLedMutex,portMAX_DELAY);
                set_duty_chan(gamma12[start_green],LEDC_G);
                xSemaphoreGive(xLedMutex);
                countflag_green = 0;
                if(start_green == end_green){
                    g_flag = true;
                }
            }
            /*Blue channel*/
            if((countflag_blue >= b_params.step_rate) && (!b_flag) && (b_flag == false)){
                (b_params.direction) ? start_blue++ : start_blue--;
                xSemaphoreTake(xLedMutex,portMAX_DELAY);
                set_duty_chan(gamma12[start_blue],LEDC_B);
                xSemaphoreGive(xLedMutex);
                countflag_blue = 0;
                if(start_blue == end_blue){
                    b_flag = true;
                }
            }

            countflag_red++;
            countflag_green++;
            countflag_blue++;
            cancel_flag = xEventGroupGetBits(eGLed_task_set_fade) & EG_FADE_CANCEL_BIT;
            vTaskDelay(1);
        }
        xTotalTime = xTaskGetTickCount() - xTimeBefore;
        xEventGroupClearBits(eGLed_task_set_fade,EG_FADE_CANCEL_BIT);
        xEventGroupClearBits(eGLed_task_set_fade,EG_FADE_START_BIT);
        ESP_LOGI(TAG,"Fade Task Done in %d ticks",xTotalTime);
    }

}

/*This function starts tasks with routines fot the leds*/
void change_mode(led_strip_config_t *rgb_config){
    
    if(xEventGroupGetBits(eGLed_task_set_fade) && EG_FADE_START_BIT){
        xEventGroupSetBits(eGLed_task_set_fade,EG_FADE_CANCEL_BIT);
    }
    switch(rgb_config->mode){
        case LED_MODE_CONNECTING_TO_AP:
            /*Notify the task to start*/
            memcpy(&rgb_config_connecting, rgb_config, sizeof(led_strip_config_t));
            xEventGroupSetBits(eGLed_task_set_connecting,
                                EG_CONNECTING_START_BIT);
            break;
        case LED_MODE_READY_FOR_CONFIG:
            break;
        case LED_MODE_STATIC:
            /*Notify the task to start*/
            memcpy(&rgb_config_static, rgb_config, sizeof(led_strip_config_t));
            xEventGroupSetBits(eGLed_task_set_static,
                                EG_STATIC_START_BIT);
            ESP_LOGI(TAG, "LED_MODE_STATIC");
            break;
        case LED_MODE_FADE:
            /*Notify the task to start*/
            memcpy(&rgb_config_fade, rgb_config, sizeof(led_strip_config_t));
            ESP_LOGI(TAG, "LED_MODE_FADE");
            xEventGroupSetBits(eGLed_task_set_fade,
                                EG_FADE_START_BIT);
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

void led_register_done_cb(led_task_done_cb_t cb, eLed_mode mode){
    led_callbacks[mode] = cb;
}


void led_control_init()
{   
    int ch;

    xLedMutex = xSemaphoreCreateMutex();
    set_system_state(STATE_RGB_STARTING);

    /* event groups creation*/
    eGLed_task_set_connecting   = xEventGroupCreate();
    eGLed_task_set_fade         = xEventGroupCreate();
    eGLed_task_set_static       = xEventGroupCreate();
    
    global_led_state.mode = LED_MODE_STOPPED;
    /* Set configuration of timer0 for high speed channels */
    ledc_timer_config(&ledc_timer);

    /* Set LED Controller with previously prepared configuration*/ 
    for (ch = 0; ch < LEDC_CH_NUM; ch++) {
        ledc_channel_config(&ledc_channel[ch]);
    }

    /* Initialize fade service for smooth transition.*/
    ledc_fade_func_install(0);

    /*Create Tasks*/

    xReturned = xTaskCreate(&led_task_set_fade,
                            "led_task_set_fade",
                            2048, &rgb_config_fade,
                            5,
                            xHled_task_set_fade);
    if(xReturned != pdPASS){
        ESP_LOGE(TAG,"Unable to create [led_task_set_fade] task");
    }
    xReturned = xTaskCreate(&led_task_set_static,
                            "led_task_set_static",
                            2048,
                            &rgb_config_static,
                            5,
                            xHled_task_set_static);
    if(xReturned != pdPASS){
        ESP_LOGE(TAG,"Unable to create [led_task_set_static] task");
    }
    xReturned = xTaskCreate(&led_task_set_connecting,
                            "led_task_set_connecting",
                            2048,
                            &rgb_config_connecting,
                            5,
                            xHled_task_set_connecting);
    if(xReturned != pdPASS){
        ESP_LOGE(TAG,"Unable to create [led_task_set_static] task");
    }

    set_system_state(STATE_RGB_STARTED);
}