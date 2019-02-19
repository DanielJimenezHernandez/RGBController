#include <string.h>
#include <math.h>
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


#define LEDC_HS_TIMER          LEDC_TIMER_0
#define LEDC_HS_MODE           LEDC_HIGH_SPEED_MODE
#define LEDC_HS_R_GPIO         CONFIG_RGB_CONTROLLER_R_Channel
#define LEDC_HS_R_CHANNEL      LEDC_CHANNEL_0
#define LEDC_HS_G_GPIO         CONFIG_RGB_CONTROLLER_G_Channel
#define LEDC_HS_G_CHANNEL      LEDC_CHANNEL_1
#define LEDC_HS_B_GPIO         CONFIG_RGB_CONTROLLER_B_Channel
#define LEDC_HS_B_CHANNEL      LEDC_CHANNEL_2
#define TIMER_DUTY_RES         LEDC_TIMER_13_BIT

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
sLedStripConfig_t rgb_config_fade,rgb_config_static,rgb_config_connecting;

/*Bits for the event groups*/

#define EG_FADE_START_BIT           BIT0
#define EG_FADE_CANCEL_BIT          BIT1

#define EG_CONNECTING_START_BIT     BIT0
#define EG_AP_STARTED_BIT

#define EG_BRIGHTNESS_SET_BIT       BIT0

#define DEFAULT_FADE_TRANSITION 200

/*Event Groups*/
EventGroupHandle_t eGLed_task_set_connecting;
EventGroupHandle_t eGLed_task_set_fade;
EventGroupHandle_t eGLedNotifyGroup;

/*Queues to notify tasks*/
QueueHandle_t xqTaskStatic;

sLedStripConfig_t global_led_state;
uint8_t global_brightness = 100;

sLedStripConfig_t off_colors;

uint32_t max_duty = (0x01 << TIMER_DUTY_RES) - 1;


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

uint32_t gamma_correction(uint32_t duty){

    double base = (double) duty / max_duty;
    double correction_factor = 2.8;
    double corrected_val = pow( base, correction_factor ) * (max_duty + 0.5);
    corrected_val = (corrected_val > (floor(corrected_val)+0.5f)) ? ceil(corrected_val) : floor(corrected_val);

    return (uint32_t)corrected_val;
}

void set_global_brightness(uint8_t brightness){
    global_brightness = brightness;
}

uint8_t get_global_brightness(){
    return global_brightness;
}

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

/*Home assistant compliant response*/
char* hass_colors(){
    sprintf(hexyfy_buffer,"%d,%d,%d",
            global_led_state.channel[LEDC_R].hex_val,
            global_led_state.channel[LEDC_G].hex_val,
            global_led_state.channel[LEDC_B].hex_val);
    return &hexyfy_buffer[0];
} 

sLedStripConfig_t get_led_state(){
    return global_led_state;
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

/*Helper function to adjust brightess*/
void adjust_brightness(asChannels_t color, asChannels_t colorAdj, uint8_t brightness){
    
    if(brightness > 100) brightness=100;

    colorAdj[LEDC_R].hex_val = brightness * color[LEDC_R].hex_val / 100;
    colorAdj[LEDC_G].hex_val = brightness * color[LEDC_G].hex_val / 100;
    colorAdj[LEDC_B].hex_val = brightness * color[LEDC_B].hex_val / 100;
}

void set_color(asChannels_t cfg, bool gamma){

    asChannels_t color,colorBrightnessAdj;
    memcpy(color,cfg,sizeof(asChannels_t));

    adjust_brightness(color,colorBrightnessAdj,global_brightness);
    colorBrightnessAdj[LEDC_R].duty = gamma_correction(hex2duty(colorBrightnessAdj[LEDC_R].hex_val));
    colorBrightnessAdj[LEDC_G].duty = gamma_correction(hex2duty(colorBrightnessAdj[LEDC_G].hex_val));
    colorBrightnessAdj[LEDC_B].duty = gamma_correction(hex2duty(colorBrightnessAdj[LEDC_B].hex_val));

    set_duty_chan(colorBrightnessAdj[LEDC_R].duty,LEDC_R);
    set_duty_chan(colorBrightnessAdj[LEDC_G].duty,LEDC_G);
    set_duty_chan(colorBrightnessAdj[LEDC_B].duty,LEDC_B);
}

void smooth_color_transition(asChannels_t cfg, uint16_t time_ms, bool blocking){
    /* convert color to duty*/
    ESP_LOGD(TAG,"smooth_color_transition");
    asChannels_t colorBrightnessAdj,color;
    esp_err_t err;
    memcpy(color,cfg,sizeof(asChannels_t));

    adjust_brightness(color,colorBrightnessAdj,global_brightness);

    colorBrightnessAdj[LEDC_R].duty = gamma_correction(hex2duty(colorBrightnessAdj[LEDC_R].hex_val));
    colorBrightnessAdj[LEDC_G].duty = gamma_correction(hex2duty(colorBrightnessAdj[LEDC_G].hex_val));
    colorBrightnessAdj[LEDC_B].duty = gamma_correction(hex2duty(colorBrightnessAdj[LEDC_B].hex_val));

    err = ledc_set_fade_with_time(ledc_channel[LEDC_R].speed_mode, 
                            ledc_channel[LEDC_R].channel,
                            colorBrightnessAdj[LEDC_R].duty,
                            time_ms);
    if(err != ESP_OK){
        ESP_LOGE(TAG,"Error fading LEDC_R channel err=%d",err);
    }
    err = ledc_set_fade_with_time(ledc_channel[LEDC_G].speed_mode, 
                            ledc_channel[LEDC_G].channel,
                            colorBrightnessAdj[LEDC_G].duty,
                            time_ms);
    if(err != ESP_OK){
        ESP_LOGE(TAG,"Error fading LEDC_G channel err=%d",err);
    }
    err = ledc_set_fade_with_time(ledc_channel[LEDC_B].speed_mode, 
                            ledc_channel[LEDC_B].channel,  
                            colorBrightnessAdj[LEDC_B].duty,
                            time_ms);
    if(err != ESP_OK){
        ESP_LOGE(TAG,"Error ledc_fade_start LEDC_R channel err=%d",err);
    }

    err = ledc_fade_start(ledc_channel[LEDC_R].speed_mode,
                    ledc_channel[LEDC_R].channel, 
                    LEDC_FADE_NO_WAIT);
    if(err != ESP_OK){
        ESP_LOGE(TAG,"Error ledc_fade_start LEDC_G channel err=%d",err);
    }
    err = ledc_fade_start(ledc_channel[LEDC_G].speed_mode,
                    ledc_channel[LEDC_G].channel, 
                    LEDC_FADE_NO_WAIT);
    if(err != ESP_OK){
        ESP_LOGE(TAG,"Error ledc_fade_start LEDC_B channel err=%d",err);
    }
    err = ledc_fade_start(ledc_channel[LEDC_B].speed_mode,
                    ledc_channel[LEDC_B].channel, 
                    (blocking) ? LEDC_FADE_WAIT_DONE : LEDC_FADE_NO_WAIT);
    if(err != ESP_OK){
        ESP_LOGE(TAG,"Error fading LEDC_B channel err=%d",err);
    }
    
    global_led_state.channel[LEDC_R].duty = colorBrightnessAdj[LEDC_R].duty;
    global_led_state.channel[LEDC_R].hex_val = color[LEDC_R].hex_val;

    global_led_state.channel[LEDC_G].duty = colorBrightnessAdj[LEDC_G].duty;
    global_led_state.channel[LEDC_G].hex_val = color[LEDC_G].hex_val;

    global_led_state.channel[LEDC_B].duty = colorBrightnessAdj[LEDC_B].duty;
    global_led_state.channel[LEDC_B].hex_val = color[LEDC_B].hex_val; 
}


void led_task_set_connecting(void *pvParameter){
    sLedStripConfig_t *ptr_led_config;
    ptr_led_config = (sLedStripConfig_t *)pvParameter;
    sLedStripConfig_t off_colors;
    for( ;; ){
        ESP_LOGW(TAG,"Waiting for EG_CONNECTING_START_BIT ");
        xEventGroupWaitBits(eGLed_task_set_connecting,
                            EG_CONNECTING_START_BIT,
                            pdTRUE,
                            pdTRUE,
                            portMAX_DELAY);
        /*Routine for sta connecting */
        while( (get_system_state() == STATE_WIFI_CONNECTING) ){
            xSemaphoreTake(xLedMutex,portMAX_DELAY);
            smooth_color_transition(ptr_led_config->channel,1000, true);
            xSemaphoreGive(xLedMutex);
            vTaskDelay(500);
            off_colors.channel[LEDC_R].hex_val = 0;
            off_colors.channel[LEDC_G].hex_val = 0;
            off_colors.channel[LEDC_B].hex_val = 0;
            xSemaphoreTake(xLedMutex,portMAX_DELAY);
            smooth_color_transition(off_colors.channel,200, true);
            xSemaphoreGive(xLedMutex);
        }
        
    }
}



/*routine to set a color change from mqtt or http*/
void led_task_set_static(void *pvParameter){
    EventBits_t brightness_set;
    while(1){
        asChannels_t channels;
        xQueueReceive( xqTaskStatic, &( channels ), portMAX_DELAY );
        xSemaphoreTake(xLedMutex,portMAX_DELAY);
        smooth_color_transition(channels,DEFAULT_FADE_TRANSITION,false);
        xSemaphoreGive(xLedMutex);
        if (led_callbacks[LED_MODE_STATIC] != NULL){
            led_callbacks[LED_MODE_STATIC]();
        }
        brightness_set = xEventGroupGetBits(eGLedNotifyGroup) & EG_BRIGHTNESS_SET_BIT;
        if (brightness_set){
            if(led_callbacks[LED_MODE_BRIGHTNESS] != NULL){
                led_callbacks[LED_MODE_BRIGHTNESS]();
                xEventGroupClearBits(eGLedNotifyGroup,EG_BRIGHTNESS_SET_BIT);
            }
        }
    }
}

/*Helper function to calculate step rate*/

inline sLedFadeParams_t get_step_rate(uint32_t start, uint32_t end, uint32_t fade_time){
    sLedFadeParams_t ret;
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
    sLedStripConfig_t *ptr_led_config = (sLedStripConfig_t *)pvParameter;
    /*Flags to indicate end of color fade*/
    bool r_flag;
    bool g_flag;
    bool b_flag;
    EventBits_t cancel_flag = 0;
    sLedFadeParams_t r_params, g_params, b_params;
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
                set_duty_chan(gamma_correction(start_red),LEDC_R);
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
                set_duty_chan(gamma_correction(start_green),LEDC_G);
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
                set_duty_chan(gamma_correction(start_blue),LEDC_B);
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
void change_mode(sLedStripConfig_t *rgb_config){
    
    if(xEventGroupGetBits(eGLed_task_set_fade) && EG_FADE_START_BIT){
        xEventGroupSetBits(eGLed_task_set_fade,EG_FADE_CANCEL_BIT);
    }
    switch(rgb_config->mode){
        case LED_MODE_CONNECTING_TO_AP:
            /*Notify the task to start*/
            memcpy(&rgb_config_connecting, rgb_config, sizeof(sLedStripConfig_t));
            xEventGroupSetBits(eGLed_task_set_connecting,
                                EG_CONNECTING_START_BIT);
            break;
        case LED_MODE_READY_FOR_CONFIG:
            break;
        case LED_MODE_STATIC:
            /*Send the colors to set to the queue*/
            xQueueSend( xqTaskStatic, ( void * ) &rgb_config->channel, ( TickType_t ) 0 );
            ESP_LOGI(TAG, "LED_MODE_STATIC");
            break;
        case LED_MODE_FADE:
            /*Notify the task to start*/
            memcpy(&rgb_config_fade, rgb_config, sizeof(sLedStripConfig_t));
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
            break;
        case LED_MODE_BRIGHTNESS:
            xQueueSend( xqTaskStatic, ( void * ) global_led_state.channel, ( TickType_t ) 0 );
            xEventGroupSetBits(eGLedNotifyGroup,EG_BRIGHTNESS_SET_BIT);
            ESP_LOGI(TAG, "LED_MODE_STATIC");

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
    eGLedNotifyGroup            = xEventGroupCreate();

    /*queue creation*/
    xqTaskStatic = xQueueCreate( 10, sizeof( asChannels_t ) );
    
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
                            NULL,
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