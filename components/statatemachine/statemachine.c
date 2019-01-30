#include "statemachine.h"

static sm_event_cb_t callback = ((void *)0);

void init_sm(sm_event_cb_t cb){
    callback = cb;
}

State get_system_state(){
    return systemState;
}

void set_system_state(State state){
    systemState = state;
    callback(state);
}