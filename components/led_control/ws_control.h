#ifndef WS_CONTROL_H
#define WS_CONTROL_H

#include "dled_pixel.h"
#include "dled_strip.h"
#include "esp32_rmt_dled.h"
#include "esp_log.h"
#include "freertos/task.h"

//modes
//  music
//    Linear
//    Mixed
//  image
typedef enum {
    WS_MODE_CONNECTING_TO_AP,
    WS_MODE_READY_FOR_CONFIG,
    WS_MODE_STATIC,
    WS_MODE_FADE,
    WS_MODE_RANDOM_FADE,
    WS_MODE_MUSIC,
    WS_MODE_BEAT,
    WS_MODE_STROBE,
    WS_MODE_STOPPED,
    WS_MODE_IMAGE
} eWs_led_mode;

typedef enum{
  NONE,
  MUSIC_MODE_LINEAR,
  MUSIC_MODE_MIXED
}eWs_music_mode;

//Led types
//  strip
//  display
typedef enum{
  LED_TYPE_STRIP,
  LED_TYPE_DISPLAY
}eWs_type;

//Number of bands
typedef struct{
  int8_t bass;
  int8_t trebble;
  int8_t agudo;
}sWs_bands;

typedef struct{
  eWs_led_mode mode;
  eWs_type type;
  eWs_music_mode musicMode;
  sWs_bands bands;
}sWsLedConfig_Music;

esp_err_t ws_control_init(rmt_pixel_strip_t *rps, pixel_strip_t *strip);

esp_err_t ws_control_main_config(dstrip_type_t strip_type, uint16_t width, uint16_t height, uint8_t max_cc_val);

void ws_control_config_music(sWsLedConfig_Music *ledConfig);

void ws_control_config_display(sWsLedConfig_Music *ledConfig);

void ws_control_set_led_mode(eWs_led_mode mode);

void ws_control_set_music_mode(eWs_music_mode mode);

esp_err_t ws_control_set_band_values(sWs_bands val);

#endif /* WS_CONTROL_H */
