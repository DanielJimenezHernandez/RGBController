#include <string.h>
#include "sntp_component.h"
#include "esp_log.h"
#include "lwip/err.h"
#include "lwip/apps/sntp.h"

#ifdef CONFIG_EXTERNAL_RTC
    #include "i2cdev.h"
    #include "ds3231.h"

    i2c_dev_t dev;
#endif


static const char *TAG = "SNTP Component";



char strftime_buf[64];

void initialize_external_rtc(){

#ifdef CONFIG_EXTERNAL_RTC
    while (ds3231_init_desc(&dev, 0, SDA_GPIO, SCL_GPIO) != ESP_OK)
    {
        printf("Could not init device descriptor\n");
        vTaskDelay(250 / portTICK_PERIOD_MS);
    }
#endif

}

/*Compare returned times*/

void compare_time(struct tm *t1, struct tm *t2){
    printf("************************************************\n");
    printf("tm_sec[%d] - tm_sec[%d]\n",t1->tm_sec,t2->tm_sec);
    printf("tm_min[%d] - tm_min[%d]\n",t1->tm_min,t2->tm_min);
    printf("tm_hour[%d] - tm_hour[%d]\n",t1->tm_hour,t2->tm_hour);
    printf("tm_mday[%d] - tm_mday[%d]\n",t1->tm_mday,t2->tm_mday);
    printf("tm_mon[%d] - tm_mon[%d]\n",t1->tm_mon,t2->tm_mon);
    printf("tm_year[%d] - tm_year[%d]\n",t1->tm_year,t2->tm_year);
    printf("tm_wday[%d] - tm_wday[%d]\n",t1->tm_sec,t2->tm_sec);
    printf("tm_yday[%d] - tm_yday[%d]\n",t1->tm_sec,t2->tm_sec);
    printf("tm_isdst[%d] - tm_isdst[%d]\n",t1->tm_sec,t2->tm_sec);
}

void initialize_sntp(const char *tz)
{
    time_t now = 0;
    struct tm timeinfo = { 0 };
    struct tm rtc_timeinfo = { 0 };
    int retry = 0;
    const int retry_count = 10;

    ESP_LOGI(TAG, "Initializing SNTP with tz[%s]",tz);
    set_time_zone(tz);
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "pool.ntp.org");
    sntp_init();

    while(timeinfo.tm_year < (2016 - 1900) && ++retry < retry_count) {
        ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        time(&now);
        localtime_r(&now, &timeinfo);
    }
    if (retry == retry_count){

#ifdef CONFIG_EXTERNAL_RTC
        struct timeval t;
        time_t externalRTC;
        struct timezone tz  = {0};
        /* Set the local time according to what is configured in the RTC */
        ESP_LOGW(TAG, "Unable to set time time functions getting time from external RTC");

        ds3231_get_time(&dev, &timeinfo);
        timeinfo.tm_year -= 1900;
        externalRTC = mktime(&timeinfo);
        
        t.tv_sec = externalRTC;
        t.tv_usec = 0;
        settimeofday(&t,&tz);

#else
        ESP_LOGE(TAG, "Unable to set time time functions will be unavailble");
#endif

    }
    else{
       
        time(&now);
        localtime_r(&now, &timeinfo);
#ifdef CONFIG_EXTERNAL_RTC
        /* 
        Set the time in the rtc but remember that is UTC for any use of this
        Local time with the correct timezone configured by the user should be used
        */
        
        /* correct the expected year for the rtc*/
        timeinfo.tm_year += 1900;
        ds3231_set_time(&dev, &timeinfo);
        ds3231_get_time(&dev, &rtc_timeinfo);
        compare_time(&timeinfo,&rtc_timeinfo);

#endif 
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

void get_external_rtc_time(struct tm *timeinfo){

#ifdef CONFIG_EXTERNAL_RTC 
    ds3231_get_time(&dev, timeinfo);
#endif

}

void set_time_zone(char *TZ){
    setenv("TZ", TZ , 1);
}

