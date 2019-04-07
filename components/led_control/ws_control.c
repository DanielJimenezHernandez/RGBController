#include "ws_control.h"


esp_err_t ws_control_init(pixel_strip_t *strip){
  return ESP_OK;
}

esp_err_t ws_control_create(rmt_pixel_strip_t *rps, pixel_strip_t *strip){
  return ESP_OK;
}

esp_err_t ws_control_main_config(dstrip_type_t strip_type, uint16_t width, uint16_t height, uint8_t max_cc_val){
  return ESP_OK;
}

void ws_control_config_music(sWsLedConfig_Music *ledConfig){

}

void ws_control_config_display(sWsLedConfig_Music *ledConfig){

}

void ws_control_set_led_mode(eWs_led_mode mode){

}

void ws_control_set_music_mode(eWs_music_mode mode){

}

esp_err_t ws_control_set_band_values(sWs_bands val){
  return ESP_OK;
}
