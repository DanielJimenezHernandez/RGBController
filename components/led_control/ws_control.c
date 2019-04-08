#include "ws_control.h"

rmt_pixel_strip_t *my_rps;
pixel_strip_t *my_strip;

static const char *TAG  = "ws_control";

esp_err_t ws_control_create(rmt_pixel_strip_t *rps, pixel_strip_t *strip){
  esp_err_t err;
  my_strip = strip;
  my_rps = rps;

  err = dled_strip_init(my_strip);
  err = rmt_dled_create(my_rps, my_strip);

  return err;
}

esp_err_t ws_control_main_config(dstrip_type_t strip_type, uint16_t width, uint16_t height, uint8_t max_cc_val){
  if(width == 0 || height == 0){
    return ESP_ERR_INVALID_ARG;
  }

  dled_strip_create(my_strip, strip_type, (width * height), max_cc_val);

  esp_err_t err;
  err = rmt_dled_config(my_rps, 18, 0);
  if (err != ESP_OK) {
      ESP_LOGE(TAG, "[0x%x] rmt_dled_config failed", err);
      while(true) { }
  }
  uint16_t step;
  step = 0;
  while (true) {
      dled_pixel_rainbow_step(my_strip->pixels, my_strip->length, my_strip->max_cc_val, step);
      dled_strip_fill_buffer(my_strip);
      rmt_dled_send(my_rps);
      step++;
      vTaskDelay(100 / portTICK_PERIOD_MS);
      if(step > 6 * my_strip->length){
        step = 0;
      }
  }

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
