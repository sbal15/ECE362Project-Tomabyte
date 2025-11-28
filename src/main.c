#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/gpio.h"
#include "chardisp.h"
#include "buzzer.h"
#include "healthbar.h"
#include "photoresistor.h"

// Pin definitions for SSD1351
const int SPI_OLED_SCK = 14;
const int SPI_OLED_MOSI = 11;
const int SPI_OLED_CSn = 13;
const int OLED_DC = 15;
const int FEED_BUTTON = 16;
const int PET_BUTTON = 10;
const int CLEAN_BUTTON = 12;

// Using SPI1
#define OLED_SPI spi1
int health = 100;

int main() {
    stdio_init_all();
    init_oled_pins();
    oled_init();
    init_feed_button();
    init_pet_button();
    buzzer_init();
    init_sleepy_photoresistor();
    init_clean_button();

    //Hello Tomabyte Test
    //oled_fill(0x0000); // Clear screen (black)
    // // oled_draw_start_screen(); //draws the start creen with the tomagatchi
    // // reset_hunger_timer();
    // // //animate_bounce();
    // // oled_draw_healthbar(10,10,100,12,health);
    // healthbar_init_timer();

    // healthbar_init_timer();

    healthbar_init_timer();

    // Draw initial screen
    add_alarm_in_ms(500, animation_callback, NULL, true);
    update_screen();
    start_random_timer();

    
    while (true){
        check_health();
        check_feed_button();
        check_pet_button();
        check_clean_button();
        check_death_sound();
        check_hungry_sound();
        check_dirty_sound();
        check_sleepy_sound();
        check_sad_sound();
        check_sleep_photoresistor();
        tight_loop_contents();
    }


    // Buzzer test
    // buzzer_init();
    // while (true){
    //     happy_sound();
    //     sleep_ms(1000);
    //     sad_sound();
    //     sleep_ms(1000);
    // }


}
