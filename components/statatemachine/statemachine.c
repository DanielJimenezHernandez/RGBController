#include "statemachine.h"

State get_state(){
    return systemState;
}

void set_state(State state){
    systemState = state;
}