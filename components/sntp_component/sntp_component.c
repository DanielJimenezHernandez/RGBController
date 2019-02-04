#include <string.h>
#include "sntp_component.h"
#include "esp_log.h"
#include "lwip/err.h"
#include "lwip/apps/sntp.h"


static const char *TAG = "SNTP Component";

char strftime_buf[64];

void initialize_sntp(void)
{
    time_t now = 0;
    struct tm timeinfo = { 0 };
    int retry = 0;
    const int retry_count = 10;

    ESP_LOGI(TAG, "Initializing SNTP");
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "pool.ntp.org");
    sntp_init();


    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    ESP_LOGI(TAG, "Time after setting is: %s", strftime_buf);
    while(timeinfo.tm_year < (2016 - 1900) && ++retry < retry_count) {
        ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        time(&now);
        localtime_r(&now, &timeinfo);
    }
    if (retry_count == 0){
        ESP_LOGE(TAG, "Unable to set time");
    }
    else{
        set_time_zone("CST-6");
        time(&now);
        localtime_r(&now, &timeinfo);
        strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
        ESP_LOGI(TAG, "The current local time is: %s", strftime_buf);
        time_set_flag = true;
    }
}

void get_system_time(struct tm *timeinfo){
    time_t now = 0;
    time(&now);
    localtime_r(&now, timeinfo);
}

void set_time_zone(char *TZ){
    setenv("TZ", TZ , 1);
}

