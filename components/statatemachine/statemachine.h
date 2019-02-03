#ifndef STATEMACHINE_H
#define STATEMACHINE_H

typedef enum state {
        STATE_INIT,
        STATE_WIFI_CONNECTING,
        STATE_WIFI_CONNECTED,
        STATE_AP_STARTED,
        STATE_AP_GOT_CONFIG,
        STATE_MQTT_CONNECTED,
        STATE_RGB_STARTING,
        STATE_RGB_STARTED,
        STATE_UNDEFINED
    } State;
typedef void (*sm_event_cb_t)(State state);
State systemState;

char *state2str(State state);
void init_sm(sm_event_cb_t cb);
void set_system_state(State state);
State get_system_state();

#endif /* STATEMACHINE_H */