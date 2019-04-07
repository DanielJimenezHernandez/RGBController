#ifndef WS_CONTROL_H
#define WS_CONTROL_H

#include "dled_pixel.h"
#include "dled_strip.h"
#include "esp32_rmt_dled.h"

//modes
//  music
//    Linear
//    Mixed
//  image
typedef enum ws_led_mode {
    LED_MODE_CONNECTING_TO_AP = 0,
    LED_MODE_READY_FOR_CONFIG,
    LED_MODE_STATIC,
    LED_MODE_FADE,
    LED_MODE_RANDOM_FADE,
    LED_MODE_MUSIC,
    LED_MODE_BEAT,
    LED_MODE_STROBE,
    LED_MODE_STOPPED,
    LED_MODE_IMAGE
}eWs_led_mode;

typedef enum ws_music_mode{
  NONE = 0,
  MUSIC_MODE_LINEAR,
  MUSIC_MODE_MIXED
}eWs_music_mode;

//Led types
//  strip
//  display
typedef enum ws_type{
  LED_TYPE_STRIP = 0,
  LED_TYPE_DISPLAY
}eWs_type;

//Number of bands
typedef struct{
  int8_t bass;
  int8_t trebble;
  int8_t agudo;
}sWs_bands;

typedef struct {
  eWs_led_mode mode;
  eWs_type type;
  eWs_music_mode musicMode;
  sWs_bands bands;
}sWsLedConfig_Music;

esp_err_t ws_control_init(pixel_strip_t *strip);

esp_err_t ws_control_create(rmt_pixel_strip_t *rps, pixel_strip_t *strip);

esp_err_t ws_control_main_config(dstrip_type_t strip_type, uint16_t width, uint16_t height, uint8_t max_cc_val);

void ws_control_config_music(sWsLedConfig_Music *ledConfig);

void ws_control_config_display(sWsLedConfig_Music *ledConfig);

void ws_control_set_led_mode(eWs_led_mode mode);

void ws_control_set_music_mode(eWs_music_mode mode);

esp_err_t ws_control_set_band_values(sWs_bands val);

#endif /* WS_CONTROL_H */
