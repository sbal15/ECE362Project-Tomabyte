#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/gpio.h"
#include "chardisp.h"

// Pin definitions for SSD1351
const int SPI_OLED_SCK = 14;
const int SPI_OLED_MOSI = 11;
const int SPI_OLED_CSn = 13;
const int OLED_DC = 15;

// Using SPI1
#define OLED_SPI spi1

int main() {
    stdio_init_all();
    init_oled_pins();
    oled_init();

    //Hello Tomabyte Test
    oled_fill(0x0000); // Clear screen (black)
    // oled_draw_text_scaled(0, 16, "F A T", 0xF800, 0x0000, 2);
    oled_draw_text_scaled(0, 1, "HELLO", 0xF800, 0x0000, 2);
    oled_draw_text_scaled(0, 64, "TOMABYTE", 0xF800, 0x0000, 2);
    // oled_draw_text_scaled(0, 16, "Tomabyte", 0xF800, 0x0000, 2);

    //
    while (true)
        tight_loop_contents();
}