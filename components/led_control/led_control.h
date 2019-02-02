#ifndef LED_CONTROL_H
#define LED_CONTROL_H

#define LEDC_CH_NUM       (3)
#define LEDC_R             0
#define LEDC_G             1
#define LEDC_B             2

typedef enum led_mode_ {
    LED_MODE_CONNECTING_TO_AP,
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


void led_control_init();
void change_mode(led_strip_config_t *rgb_config);

#endif /* LED_CONTROL_H */