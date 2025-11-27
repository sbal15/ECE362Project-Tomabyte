#include "pico/stdlib.h"
#include "hardware/adc.h"
#include <stdio.h>

#define PHOTORESISTOR_ADC_CHANNEL 1  

// External pet state access - must match chardisp.c enum
typedef enum {
    STATE_NORMAL,
    STATE_HUNGRY,
    STATE_EATING,
    STATE_HAPPY,
    STATE_SAD,
    STATE_CLEAN,
    STATE_DIRTY,
    STATE_SLEEPING,
    STATE_PET,
    STATE_DEAD
} PetState;

extern PetState pet_state;
PetState curr_state = STATE_NORMAL;

void photoresistor_init() {
    adc_init();
    adc_gpio_init(41);
    adc_select_input(PHOTORESISTOR_ADC_CHANNEL);
}

void check_photoresistor() {
    if (pet_state == STATE_DEAD) {
        return;
    }
    
    uint16_t adc_value = adc_read();
    if (adc_value > 1000) {
        if (pet_state == STATE_SLEEPING) {
            pet_state = curr_state;
        }
    } else {
        if (pet_state != STATE_SLEEPING) {
            curr_state = pet_state;
            pet_state = STATE_SLEEPING;
        }
    }
}