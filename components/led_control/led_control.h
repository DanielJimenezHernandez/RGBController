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

}eLed_mode;

typedef struct {
    uint32_t duty;
    uint8_t hex_val;
}led_info;

typedef struct {
    eLed_mode mode;
    uint32_t bightness;      /*configured brignthness*/
    uint32_t fadetime;
    led_info r;
    led_info g;
    led_info b;
} sLed_state;


void led_control_init();
void change_mode(sLed_state *rgb_config);

#endif /* LED_CONTROL_H */