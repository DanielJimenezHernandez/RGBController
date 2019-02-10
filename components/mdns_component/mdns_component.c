/* MDNS-SD Query and advertise Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "mdns.h"
#include "driver/gpio.h"
#include <sys/socket.h>
#include <netdb.h>

#include "mdns_component.h"


static const char c_config_hostname[] = CONFIG_MDNS_HOSTNAME;


static const char *TAG = "mdns-test";


void initialise_mdns(const char * device_id){
    _Static_assert(sizeof(c_config_hostname) < CONFIG_MAIN_TASK_STACK_SIZE/2, "Configured mDNS name consumes more than half of the stack. Please select a shorter host name or extend the main stack size please.");
    const size_t config_hostname_len = sizeof(c_config_hostname) - 1; // without term char
    char hostname[config_hostname_len + 1 + 3*2 + 1]; // adding underscore + 3 digits + term char
    uint8_t mac[6];

    // adding 3 LSBs from mac addr to setup a board specific name
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    snprintf(hostname, sizeof(hostname), "%s_%s",c_config_hostname ,device_id);

    //initialize mDNS
    ESP_ERROR_CHECK( mdns_init() );
    //set mDNS hostname (required if you want to advertise services)
    ESP_ERROR_CHECK( mdns_hostname_set(hostname) );
    //set default mDNS instance name
    ESP_LOGI(TAG, "mdns hostname set to: [%s]", hostname);
    ESP_ERROR_CHECK( mdns_instance_name_set(CONFIG_MDNS_INSTANCE) );

    //structure with TXT records
    //TODO: grab CONFIG_DEVICE_DEF_NAME from storage if not set it as default
    mdns_txt_item_t http_api_serviceTxtData[6] = {
        {"path","/api"},
        {"id",device_id},
        {"version","1.0.0"},
        {"mqtt_en","true"},
        {"mqtt_dev_name",CONFIG_DEVICE_DEF_NAME},
        {"mqtt_fmt","json"}
    };

    //initialize service
    ESP_ERROR_CHECK( mdns_service_add("RGBLights_"CONFIG_DEVICE_DEF_NAME, "_http", "_tcp", CONFIG_HTTP_REST_PORT, http_api_serviceTxtData, 6) );

}
