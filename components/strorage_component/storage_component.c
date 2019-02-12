/* Non-Volatile Storage (NVS) Read and Write a Blob - Example
   For other examples please check:
   https://github.com/espressif/esp-idf/tree/master/examples
   This example code is in the Public Domain (or CC0 licensed, at your option.)
   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include <string.h>
#include "esp_system.h"
#include "nvs_flash.h"
#include "nvs.h"

#include "storage_component.h"

#define STORAGE_CONFIG_NAMESPACE "config"
#define STORAGE_ALARMS_NAMESPACE "alarms"

esp_err_t save_global_configs(){
    nvs_handle my_handle;
    esp_err_t err;
    err = nvs_open(STORAGE_CONFIG_NAMESPACE, NVS_READWRITE, &my_handle);
    if (err != ESP_OK) return err;

    size_t global_configs_size = sizeof(_global_configs);
    err = nvs_set_blob(my_handle, "global_config", &_global_configs, global_configs_size);
    if (err != ESP_OK) return err;
    err = nvs_commit(my_handle);
    if (err != ESP_OK) return err;
    nvs_close(my_handle);

    return ESP_OK;
}

esp_err_t get_global_configs(){
    nvs_handle my_handle;
    esp_err_t err = nvs_open(STORAGE_CONFIG_NAMESPACE, NVS_READWRITE, &my_handle);
    if (err != ESP_OK) return err;
    size_t required_size = 0;  
    err = nvs_get_blob(my_handle, "global_config", NULL, &required_size);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) return err;
    if (required_size == 0) {
        printf("global_config not saved yet!\n");
    } else {
        size_t global_configs_size = sizeof(_global_configs);
        err = nvs_get_blob(my_handle, "global_config", &_global_configs, &global_configs_size);
    if (err != ESP_OK) return err;
    }
    // *******  print out 'schedules' array here and find garbage in it  ********
    nvs_close(my_handle);

    return ESP_OK;
}

void set_global_deviceID(unsigned char *device_id){
    memcpy(&_global_configs.deviceID_global,device_id,sizeof(_global_configs.deviceID_global));
}

void set_global_deviceID_str(char *device_id_str){
    memcpy(&_global_configs.deviceID_str_global,device_id_str,sizeof(_global_configs.deviceID_str_global));
}

