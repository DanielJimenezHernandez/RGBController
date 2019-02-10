#ifndef SNTP_COMPONENT_H
#define SNTP_COMPONENT_H

#include <time.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"


void initialize_sntp(const char *tz);
void set_time_zone(char *TZ);
void get_system_time(struct tm *timeinfo);
void initialize_external_rtc(void);
void get_external_rtc_time(struct tm *timeinfo);

bool time_set_flag;

#endif /* SNTP_COMPONENT_H */