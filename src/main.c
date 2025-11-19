#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/gpio.h"
#include "chardisp.h"
#include "buzzer.h"
#include "healthbar.h"

// Pin definitions for SSD1351
const int SPI_OLED_SCK = 14;
const int SPI_OLED_MOSI = 11;
const int SPI_OLED_CSn = 13;
const int OLED_DC = 15;
const int FEED_BUTTON = 16;

// Using SPI1
#define OLED_SPI spi1
int health = 100;

int main() {
    stdio_init_all();
    init_oled_pins();
    oled_init();
    init_feed_button();

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
    reset_hunger_timer();

    // Draw initial screen
    update_screen();
    reset_hunger_timer();



    //
    while (true){
        check_feed_button();
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
