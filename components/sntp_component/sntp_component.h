#ifndef SNTP_COMPONENT_H
#define SNTP_COMPONENT_H

#include <time.h>
#include <sys/time.h>
#include "esp_system.h"

#define NUMBER_OF_ALARMS 3

#define SUN (0x01 << 0)
#define MON (0x01 << 1)
#define TUE (0x01 << 2)
#define WED (0x01 << 3)
#define THR (0x01 << 4)
#define FRI (0x01 << 5)
#define SAT (0x01 << 6)
#define ALL (0x01 << 7)

typedef void (*alarm_triggered_callback_t)(void);


typedef struct {
    int enable;
    int triggered;
    int	a_min;
    int	a_hour;
    int a_days;
    alarm_triggered_callback_t cb;
}alarms_t;

alarms_t alarms[NUMBER_OF_ALARMS];

void initialize_sntp(char *tz);
void set_time_zone(char *TZ);
void get_system_time(struct tm *timeinfo);
void initialize_external_rtc(void);
void get_external_rtc_time(struct tm *timeinfo);
void set_alarm(uint8_t alarm_index);

bool time_set_flag;

#endif /* SNTP_COMPONENT_H */