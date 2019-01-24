#ifndef LED_CONTROL_H
#define LED_CONTROL_H

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

}led_mode_e;


void led_control_init();

#endif /* LED_CONTROL_H */