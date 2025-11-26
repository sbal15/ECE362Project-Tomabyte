#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include <stdio.h>

#define PHOTORESISTOR_PIN 12

// External pet state access
extern int pet_state;
#define STATE_NORMAL 0
#define STATE_SLEEPING 7
#define STATE_DEAD 9

int curr_state = 0;

void photoresistor_init() {
    gpio_init(PHOTORESISTOR_PIN);
    gpio_set_dir(PHOTORESISTOR_PIN, GPIO_IN);
}

void check_photoresistor() {
    if (gpio_get(PHOTORESISTOR_PIN)) {
        // printf("Light detected - waking up\n");
        if (pet_state == STATE_SLEEPING) {
            pet_state = curr_state;
        }
    } else {
        // printf("Dark - going to sleep\n");
        if (pet_state != STATE_DEAD) {
            if (pet_state != STATE_SLEEPING) curr_state = pet_state;
            pet_state = STATE_SLEEPING;
        }
    }
}