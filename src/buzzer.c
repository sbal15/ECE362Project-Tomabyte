#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"

#define BUZZER_PIN 21

//Functions
void buzzer_init(){
    gpio_set_function(BUZZER_PIN, GPIO_FUNC_PWM);
    uint slice_num = pwm_gpio_to_slice_num(BUZZER_PIN);
    pwm_config config = pwm_get_default_config();
    pwm_init(slice_num, &config, true);
}

void buzzer_play(uint freq, uint time){
    uint slice_num = pwm_gpio_to_slice_num(BUZZER_PIN);

    //play sound
    uint clk_freq = clock_get_hz(clk_sys);
    float divider = 150; // can change if doesn't work for other sounds, was using 100.0f at one point
    uint wrap = clk_freq / (divider * freq) - 1;
    
    pwm_set_clkdiv(slice_num, divider);
    pwm_set_wrap(slice_num, wrap);
    pwm_set_chan_level(slice_num, pwm_gpio_to_channel(BUZZER_PIN), wrap / 2);
    pwm_set_enabled(slice_num, true);
    sleep_ms(time);

    //stop sound
    pwm_set_enabled(slice_num, false);
}

// void happy_sound(){
//     buzzer_play(1000, 50);
//     buzzer_play(1300, 50);
//     buzzer_play(1600, 50);
// }

// void sad_sound(){
//     buzzer_play(700, 50);
//     buzzer_play(500, 50);
//     buzzer_play(300, 50);
// }


void chirp_sound(){
    buzzer_play(2000, 30);
    sleep_ms(10);
    buzzer_play(2500, 30);
}

void bounce_sound(){
    buzzer_play(800, 40);
    buzzer_play(1200, 40);
    buzzer_play(800, 40);
    buzzer_play(1200, 40);
}

// using as happy, former is twinkle
void happy_sound(){
    buzzer_play(1500, 80);
    sleep_ms(20);
    buzzer_play(1800, 80);
    sleep_ms(20);
    buzzer_play(2200, 120);
}

void giggle_sound(){
    for(int i = 0; i < 4; i++){
        buzzer_play(1000 + i*200, 60);
        sleep_ms(30);
    }
}

// use for sad (or incorrect or warning or something)--> former name purr
void sad_sound(){
    for(int i = 0; i < 8; i++){
        buzzer_play(400 + (i % 2) * 100, 80);
        sleep_ms(20);
    }
}