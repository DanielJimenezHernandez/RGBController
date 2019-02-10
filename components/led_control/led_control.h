#ifndef LED_CONTROL_H
#define LED_CONTROL_H

#define LEDC_CH_NUM       (3)
#define LEDC_R             0
#define LEDC_G             1
#define LEDC_B             2

/* must match with the eLed_mode enum below*/ 
#define LED_TASKS_NUMBER 9

typedef void (*led_task_done_cb_t)(void);

typedef enum led_mode_ {
    LED_MODE_CONNECTING_TO_AP = 0,
    LED_MODE_READY_FOR_CONFIG,
    LED_MODE_STATIC,
    LED_MODE_FADE,
    LED_MODE_RANDOM_FADE,
    LED_MODE_MUSIC,
    LED_MODE_BEAT,
    LED_MODE_STROBE,
    LED_MODE_STOPPED
}eLed_mode;

typedef struct {
    uint32_t duty;
    uint8_t hex_val;
}led_channel_config_t;

typedef struct{
    eLed_mode mode;
    uint32_t bightness;
    uint16_t fadetime_s;
    led_channel_config_t channel[LEDC_CH_NUM];
}led_strip_config_t;

typedef struct {
    uint8_t direction;
    double step_rate;
}led_fade_params_t;


void led_control_init();
void change_mode(led_strip_config_t *rgb_config);
char *jsonify_colors(void);

led_strip_config_t get_led_state();

void led_register_done_cb(led_task_done_cb_t cb, eLed_mode mode);
char* hexify_colors();

#endif /* LED_CONTROL_H */