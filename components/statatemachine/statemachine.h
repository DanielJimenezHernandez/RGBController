#ifndef STATEMACHINE_H
#define STATEMACHINE_H

typedef enum state {
        STATE_INIT,
        STATE_WIFI_CONNECTING,
        STATE_WIFI_CONNECTED,
        STATE_WIFI_DISCONNECTED,
        STATE_AP_STARTED,
        STATE_AP_GOT_CONFIG,
        STATE_MQTT_CONNECTED,
        STATE_MQTT_DISCONNECTED,
        STATE_FULLY_OPERATIONAL,
        STATE_UNDEFINED
    } State;

State systemState;

void set_state(State state);
State get_state();

#endif /* STATEMACHINE_H */