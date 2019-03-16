// Copyright 2018-2019 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "driver/adc.h"
#include "driver/timer.h"
#include "esp_adc_cal.h"
#include <math.h>
#include "fft.h"
#include "sound_component.h"
#include "led_control.h"

#include "sound_component.h"

#define REDUCTION_FACTOR 1000

timer_config_t timer_config = {
  .alarm_en = false,
  .counter_en = true,
  .counter_dir = TIMER_COUNT_UP,
  .divider = 80   /* 1 us per tick */
};

double start, end, start_all, end_all;

void clock_init()
{
  timer_init(TIMER_GROUP_0, TIMER_0, &timer_config);
  timer_set_counter_value(TIMER_GROUP_0, TIMER_0, 0);
  timer_start(TIMER_GROUP_0, TIMER_0);
}

static esp_adc_cal_characteristics_t *adc_chars;
static const adc_channel_t channel = ADC1_CHANNEL_0;     //GPIO34 if ADC1, GPIO14 if ADC2
static const adc_atten_t atten = ADC_ATTEN_DB_11;
static const adc_unit_t unit = ADC_UNIT_1;

static void check_efuse()
{
    //Check TP is burned into eFuse
    if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_TP) == ESP_OK) {
        printf("eFuse Two Point: Supported\n");
    } else {
        printf("eFuse Two Point: NOT supported\n");
    }

    //Check Vref is burned into eFuse
    if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_VREF) == ESP_OK) {
        printf("eFuse Vref: Supported\n");
    } else {
        printf("eFuse Vref: NOT supported\n");
    }
}

static void print_char_val_type(esp_adc_cal_value_t val_type)
{
    if (val_type == ESP_ADC_CAL_VAL_EFUSE_TP) {
        printf("Characterized using Two Point Value\n");
    } else if (val_type == ESP_ADC_CAL_VAL_EFUSE_VREF) {
        printf("Characterized using eFuse Vref\n");
    } else {
        printf("Characterized using Default Vref\n");
    }
}

static const char *TAG = "main";


void sampling_task(void * pvParameters){
    int k;
    float noise_filter = 1000;
    uint16_t band1_peak = 0,band2_peak = 0,band3_peak = 0;
    uint16_t band1_biggest = 1, band2_biggest = 1, band3_biggest = 1;
    asChannels_t colors;
    
    while(1){
        double sub_bass = 0 , bass = 0 , low_mid_range = 0;
        double mid_range = 0 , upper_mid_range = 0 , presence = 0;
        double brilliance = 0 , magnitude;
        fft_config_t *real_fft_plan = fft_init(N_SAMPLES, FFT_REAL, FFT_FORWARD, NULL, NULL);
        uint8_t count_sub_bass = 0;
        uint8_t count_bass = 0;
        uint8_t count_low_mid_range = 0;
        uint8_t count_upper_mid_range = 0;
        uint8_t count_presence = 0;
        uint8_t count_brilliance = 0;
        uint8_t count_mid_range = 0;
        timer_get_counter_time_sec(TIMER_GROUP_0, TIMER_0, &start);
        for (k = 0; k < real_fft_plan->size; k++) {
            /*(1.2 * 1023 / 3.9) remove 1.2v vias voltage*/
            real_fft_plan->input[k] = adc1_get_raw((adc1_channel_t)channel);
        }
        timer_get_counter_time_sec(TIMER_GROUP_0, TIMER_0, &end);
        // printf("[");
        // for( int i = 0; i < N_SAMPLES; i++ ){
        //     printf("%f,",real_fft_plan->input[i]);
        // }
        // printf("]\n");
        double sample_time_s = (end - start);
        double sample_rate = sample_time_s/N_SAMPLES;
        double sampling_frequency = 1/sample_rate;
        double max_FFT_freq = sampling_frequency/2;
        double frequency_per_bin = max_FFT_freq/(N_SAMPLES/2);

        int beat_detection_lower_limit = 70 / frequency_per_bin;
        int beat_detection_upper_limit = 250 / frequency_per_bin;

        double beat = 0;

        // int sub_bass_lower_limit = 20 / frequency_per_bin;
        // int sub_bass_upper_limit = 60 / frequency_per_bin;

        // int bass_lower_limit = 60 / frequency_per_bin;
        // int bass_upper_limit = 250 / frequency_per_bin;

        // int low_mid_range_lower_limit = 250 / frequency_per_bin;
        // int low_mid_range_upper_limit = 500 / frequency_per_bin;

        // int mid_range_lower_limit = 500 / frequency_per_bin;
        // int mid_range_upper_limit = 2000 / frequency_per_bin;

        // int upper_mid_range_lower_limit = 2000 / frequency_per_bin;
        // int upper_mid_range_upper_limit = 4000 / frequency_per_bin;

        // int presence_lower_limit = 4000 / frequency_per_bin;
        // int presence_upper_limit = 6000 / frequency_per_bin;

        // int brilliance_lower_limit = 6000 / frequency_per_bin;
        // int brilliance_upper_limit = max_FFT_freq / frequency_per_bin;

        // printf("sub_bass [%d - %d]\n",sub_bass_lower_limit,sub_bass_upper_limit);
        // printf("bass [%d - %d]\n",bass_lower_limit,bass_upper_limit);
        // printf("low_mid_range [%d - %d]\n",low_mid_range_lower_limit,low_mid_range_upper_limit);
        // printf("mid_range [%d - %d]\n",mid_range_lower_limit,mid_range_upper_limit);
        // printf("upper_mid_range [%d - %d]\n",upper_mid_range_lower_limit,upper_mid_range_upper_limit);
        // printf("presence [%d - %d]\n",presence_lower_limit,presence_upper_limit);
        // printf("brilliance [%d - %d]\n",brilliance_lower_limit,brilliance_upper_limit);
        

        // Execute transformation
        fft_execute(real_fft_plan);

        for (k = 1 ; k < real_fft_plan->size / 2 ; k++){
            magnitude = sqrt(pow(real_fft_plan->output[2*k],2) + pow(real_fft_plan->output[2*k+1],2));
            if(k >= beat_detection_lower_limit && k < beat_detection_upper_limit){
                beat += magnitude;
            }
        }

        beat = beat / (beat_detection_upper_limit - beat_detection_lower_limit);

        if (beat > 4000){
            printf("beat[%f]\n",beat);
        }

        // // printf("[");
        // for (k = 1 ; k < real_fft_plan->size / 2 ; k++){
        //     magnitude = sqrt(pow(real_fft_plan->output[2*k],2) + pow(real_fft_plan->output[2*k+1],2));

        //     if(k >= sub_bass_lower_limit && k < sub_bass_upper_limit){
        //         /*Select the bigger peak (Predominant frequency in that range) after noise filter*/
        //         if (magnitude > noise_filter){
        //             sub_bass += magnitude;
        //             count_sub_bass++;
        //         }
               
        //     }

        //     if(k >= bass_lower_limit && k < bass_upper_limit){
        //         if (magnitude > noise_filter){
        //             bass += magnitude;
        //             count_bass++;
        //         }
                
        //     }

        //     if(k >= low_mid_range_lower_limit && k < low_mid_range_upper_limit){
        //         if (magnitude > noise_filter){
        //             low_mid_range += magnitude;
        //             count_low_mid_range++;
        //         }
                
        //     }

        //     if(k >= mid_range_lower_limit && k < mid_range_upper_limit){
        //         if (magnitude > noise_filter){
        //             mid_range += magnitude;
        //             count_mid_range++;
        //         }
                
        //     }

        //     if(k >= upper_mid_range_lower_limit && k < upper_mid_range_upper_limit){
        //         if (magnitude > noise_filter){
        //             upper_mid_range += magnitude;
        //             count_upper_mid_range++;
        //         }
        //     }

        //     if(k >= presence_lower_limit && k < presence_upper_limit){
        //         if (magnitude > noise_filter){
        //             presence += magnitude;
        //             count_presence++;
        //         }
        //     }

        //     if(k >= brilliance_lower_limit && k <= brilliance_upper_limit){
        //         if (magnitude > noise_filter){
        //             brilliance += magnitude;
        //             count_brilliance++;
        //         }
        //     }
        //     // printf("%f,",magnitude);
        // }
        // sub_bass = sub_bass / ((count_sub_bass != 0) ? count_sub_bass : 1);
        // bass = bass / ((count_bass != 0) ? count_bass : 1);
        // low_mid_range = low_mid_range / ((count_low_mid_range) != 0 ? count_low_mid_range : 1);
        // mid_range = mid_range / ((count_mid_range != 0) ? count_mid_range : 1);
        // upper_mid_range = upper_mid_range / ((count_upper_mid_range != 0) ? count_upper_mid_range : 1);
        // presence = presence / ((count_presence !=0) ? count_presence : 1);
        // brilliance = brilliance / ((count_brilliance != 0) ? count_brilliance : 1);

        // printf("]\n");
        // printf("Synth spectrum\n");
        // printf("[%f,%f,%f,%f,%f,%f,%f]\n",
        //         sub_bass,
        //         bass,
        //         low_mid_range,
        //         mid_range,
        //         upper_mid_range,
        //         presence,
        //         brilliance);
        // double band1 = sub_bass * bass / 2;
        // double band2 = low_mid_range * mid_range * upper_mid_range / 3;
        // double band3 = presence * brilliance / 2;
        // if(band1_peak < band1){
        //     band1_peak = band1;
        // } 
        // else if(band1_peak >= REDUCTION_FACTOR){
        //     band1_peak -= REDUCTION_FACTOR;
        // }
        // else{
        //     band1_peak = 0;
        // }

        // if(band2_peak < band2){
        //     band2_peak = band2;
        // }
        // else if(band2_peak >= REDUCTION_FACTOR){
        //     band2_peak -= REDUCTION_FACTOR;
        // }
        // else{
        //     band2_peak = 0;
        // }
        
        // if(band3_peak < band3){
        //     band3_peak = band3;
        // }
        // else if(band3_peak >= REDUCTION_FACTOR){
        //     band3_peak -= REDUCTION_FACTOR;
        // }
        // else{
        //     band3_peak = 0;
        // }

        // if (band1_biggest < band1_peak) band1_biggest = band1_peak;
        // if (band2_biggest < band2_peak) band2_biggest = band2_peak;
        // if (band3_biggest < band3_peak) band3_biggest = band3_peak;

        // uint16_t r,g,b;

        // r = band1_peak * MAX_DUTY / band1_biggest;
        // g = band2_peak * MAX_DUTY/4 / band2_biggest;
        // b = band3_peak * MAX_DUTY/2 / band3_biggest;

        // //printf("[%d,%d,%d]\n",r,g,b);

        // set_duty_chan(r, LEDC_R);
        // set_duty_chan(g, LEDC_G);
        // set_duty_chan(b, LEDC_B);

        //printf("band1[%f - %d] band2[%f - %d] band3[%f - %d]\n",band1,band1_peak,band2,band2_peak,band3,band3_peak);

        fft_destroy(real_fft_plan);
        // to avoid watchdog trigger
        vTaskDelay(1);
    }

}

void sound_init()
{
   /*ADC Related Stuff*/
    clock_init();
    //Check if Two Point or Vref are burned into eFuse
    check_efuse();

    //Configure ADC
    adc1_config_width(ADC_WIDTH_BIT_10);
    adc1_config_channel_atten(channel, atten);

    //Characterize ADC
    adc_chars = calloc(1, sizeof(esp_adc_cal_characteristics_t));
    esp_adc_cal_value_t val_type = esp_adc_cal_characterize(unit, atten, ADC_WIDTH_BIT_10, DEFAULT_VREF, adc_chars);
    print_char_val_type(val_type);

    xTaskCreatePinnedToCore(sampling_task,"SamplingTask",2048,NULL,1,NULL,1);
}