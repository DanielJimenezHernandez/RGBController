#ifndef STORAGE_COMPONENT_H
#define STORAGE_COMPONENT_H

#include "mqtt_client_component.h"

typedef struct  {
    uint8_t             mqtt_global;
    uint8_t             http_global;
    unsigned char       deviceID_global[16];
    char                deviceID_str_global[33];
    mqtt_subscribers_t  mqtt_subscribers_global;
    char                device_name_global[16];
}global_configs_t;

global_configs_t _global_configs;

esp_err_t save_global_configs();
esp_err_t get_global_configs();

void set_global_deviceID(unsigned char *device_id);
void set_global_deviceID_str(char * device_id_str);

#endif /* STORAGE_COMPONENT_H */