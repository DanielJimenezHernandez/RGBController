#ifndef LED_CONTROL_H
#define LED_CONTROL_H

#include "driver/ledc.h"

#define LEDC_CH_NUM       (3)
#define LEDC_R             0
#define LEDC_G             1
#define LEDC_B             2

#define TIMER_DUTY_RES         LEDC_TIMER_13_BIT
#define MAX_DUTY ((0x01 << TIMER_DUTY_RES) - 1)

/* must match with the eLed_mode enum below*/ 
#define LED_TASKS_NUMBER 10

typedef void (*led_task_done_cb_t)(void);
void gamma_correction_math(uint32_t duty);
typedef enum led_mode_ {
    LED_MODE_CONNECTING_TO_AP = 0,
    LED_MODE_READY_FOR_CONFIG,
    LED_MODE_STATIC,
    LED_MODE_FADE,
    LED_MODE_RANDOM_FADE,
    LED_MODE_MUSIC,
    LED_MODE_BEAT,
    LED_MODE_STROBE,
    LED_MODE_STOPPED,
    LED_MODE_BRIGHTNESS
}eLed_mode;

typedef struct {
    uint32_t duty;
    uint8_t hex_val;
}sLedChannelConfig_t;

typedef sLedChannelConfig_t asChannels_t[LEDC_CH_NUM];

typedef struct{
    eLed_mode mode;
    uint8_t brightness;
    uint16_t fadetime_s;
    asChannels_t channel;
}sLedStripConfig_t;

typedef struct {
    uint8_t direction;
    double step_rate;
}sLedFadeParams_t;

uint8_t global_brightness;


void led_control_init();
void change_mode(sLedStripConfig_t *rgb_config);
char *jsonify_colors(void);

sLedStripConfig_t get_led_state();

void led_register_done_cb(led_task_done_cb_t cb, eLed_mode mode);
char* hexify_colors();
void hass_colors(char * buffer);
void set_global_brightness(uint8_t brightness);
uint8_t get_global_brightness();
void set_duty_chan(uint32_t duty, uint8_t ch);

#endif /* LED_CONTROL_H */