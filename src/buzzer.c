#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"

static int duty_cycle = 0;
static int dir = 0;
static int color = 0;

#define BUZZER_PIN 36

//Functions
void buzzer_init(){
    gpio_set_function(BUZZER_PIN, GPIO_FUNC_PWM);
    uint slice_num = pwm_gpio_to_slice_num(BUZZER_PIN);
    uint channel = pwm_gpio_to_channel(BUZZER_PIN);
    pwm_config config = pwm_get_default_config();
    pwm_init(slice_num, &config, true);
}

void buzzer_play(uint freq, uint time){
    uint slice_num = pwm_gpio_to_slice_num(BUZZER_PIN);

    //play sound
    uint clk_freq = clock_get_hz(clk_sys);
    uint divider = 150; //adjust if needed
    //uint wrap = clk_freq / (divider * freq); -> this is chat
    uint wrap = (1 / freq) - 1; //is this how you calculate wrap..??
    pwm_set_clkdiv(slice_num, divider);
    pwm_set_wrap(slice_num, wrap);
    pwm_set_chan_level(slice_num, pwm_gpio_to_channel(BUZZER_PIN), wrap / 2); //50% duty cycle = wrap/2
    pwm_set_enabled(slice_num, true);
    sleep_ms(time); //time in ms

    //stop sound
    pwm_set_chan_level(slice_num, pwm_gpio_to_channel(BUZZER_PIN), 0);
    //do i have to put false for pwm_set_enabled now?
}

void happy_sound(){
    buzzer_play(1000, 50);
    buzzer_play(1300, 50);
    buzzer_play(1600, 50);
}

void sad_sound(){
    buzzer_play(700, 50);
    buzzer_play(500, 50);
    buzzer_play(300, 50);
}