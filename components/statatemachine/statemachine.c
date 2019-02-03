#include "statemachine.h"
#include "esp_log.h"

static sm_event_cb_t callback = ((void *)0);

void init_sm(sm_event_cb_t cb){
    callback = cb;
}

State get_system_state(){
    return systemState;
}


char *state2str(State state){
    char *str = "";
    switch (state)
    {
        case STATE_INIT:
            str = "STATE_INIT";
            break;
        case STATE_WIFI_CONNECTING:
            str = "STATE_WIFI_CONNECTING";
            break;
        case STATE_WIFI_CONNECTED:
            str = "STATE_WIFI_CONNECTED";
            break;
        case STATE_AP_STARTED:
            str = "STATE_AP_STARTED";
            break;
        case STATE_AP_GOT_CONFIG:
            str = "STATE_AP_GOT_CONFIG";
            break;
        case STATE_MQTT_CONNECTED:
            str = "STATE_MQTT_CONNECTED";
            break;
        case STATE_RGB_STARTING:
            str = "STATE_RGB_STARTING";
            break;
        case STATE_RGB_STARTED:
            str = "STATE_RGB_STARTED";
            break;
        case STATE_UNDEFINED:
            str = "STATE_UNDEFINED";
            break;
    }
    return str;
}

void set_system_state(State state){
    ESP_LOGI("State Machine","setting state %s",state2str(state));
    systemState = state;
    callback(state);
}