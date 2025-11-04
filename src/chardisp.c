#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/gpio.h"
#include "chardisp.h"

// External pin definitions
extern const int SPI_OLED_SCK;
extern const int SPI_OLED_MOSI;
extern const int SPI_OLED_CSn;
extern const int OLED_DC;

#define OLED_SPI spi1

// OLED display dimensions
#define OLED_WIDTH 128
#define OLED_HEIGHT 128

// -----------------------------
// SPI + GPIO setup
// -----------------------------
void init_oled_pins() {
    spi_init(OLED_SPI, 10 * 1000 * 1000); // 10 MHz SPI
    gpio_set_function(SPI_OLED_SCK, GPIO_FUNC_SPI);
    gpio_set_function(SPI_OLED_MOSI, GPIO_FUNC_SPI);

    gpio_init(SPI_OLED_CSn);
    gpio_set_dir(SPI_OLED_CSn, GPIO_OUT);
    gpio_put(SPI_OLED_CSn, 1);

    gpio_init(OLED_DC);
    gpio_set_dir(OLED_DC, GPIO_OUT);
}

// -----------------------------
// Low-level SPI helpers
// -----------------------------
void oled_write_cmd(uint8_t cmd) {
    gpio_put(OLED_DC, 0); // Command mode
    gpio_put(SPI_OLED_CSn, 0);
    spi_write_blocking(OLED_SPI, &cmd, 1);
    gpio_put(SPI_OLED_CSn, 1);
}

void oled_write_data(uint8_t data) {
    gpio_put(OLED_DC, 1); // Data mode
    gpio_put(SPI_OLED_CSn, 0);
    spi_write_blocking(OLED_SPI, &data, 1);
    gpio_put(SPI_OLED_CSn, 1);
}

// -----------------------------
// SSD1351 initialization sequence
// -----------------------------
void oled_init() {
    sleep_ms(10);

    oled_write_cmd(0xFD); // Command lock
    oled_write_data(0x12);
    oled_write_cmd(0xFD);
    oled_write_data(0xB1);

    oled_write_cmd(0xAE); // Display off
    oled_write_cmd(0xB3);
    oled_write_data(0xF1); // Clock div

    oled_write_cmd(0xCA);
    oled_write_data(0x7F); // MUX ratio (128)

    oled_write_cmd(0xA0);
    oled_write_data(0x74); // RGB color remap

    oled_write_cmd(0xA1);
    oled_write_data(0x00); // Column start

    oled_write_cmd(0xA2);
    oled_write_data(0x00); // Row start

    oled_write_cmd(0xB5);
    oled_write_data(0x00); // GPIO off

    oled_write_cmd(0xAB);
    oled_write_data(0x01); // Enable internal VDD regulator

    oled_write_cmd(0xB1);
    oled_write_data(0x32); // Set precharge

    oled_write_cmd(0xBE);
    oled_write_data(0x05); // VCOMH

    oled_write_cmd(0xA6); // Normal display
    oled_write_cmd(0xC1);
    oled_write_data(0xC8);
    oled_write_data(0x80);
    oled_write_data(0xC8);

    oled_write_cmd(0xC7);
    oled_write_data(0x0F); // Contrast

    oled_write_cmd(0xAF); // Display ON
    sleep_ms(100);
}

// -----------------------------
// Simple drawing helpers
// -----------------------------
void oled_set_window(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1) {
    oled_write_cmd(0x15); // Column
    oled_write_data(x0);
    oled_write_data(x1);

    oled_write_cmd(0x75); // Row
    oled_write_data(y0);
    oled_write_data(y1);
}

void oled_fill(uint16_t color) {
    oled_set_window(0, 0, OLED_WIDTH - 1, OLED_HEIGHT - 1);
    oled_write_cmd(0x5C);

    gpio_put(OLED_DC, 1);
    gpio_put(SPI_OLED_CSn, 0);
    uint8_t buffer[2] = { color >> 8, color & 0xFF };
    for (int i = 0; i < OLED_WIDTH * OLED_HEIGHT; i++)
        spi_write_blocking(OLED_SPI, buffer, 2);
    gpio_put(SPI_OLED_CSn, 1);
}

// Placeholder simple text
void oled_draw_text(uint8_t x, uint8_t y, const char *text, uint16_t color, uint16_t bg) {
    // In a real project you’d use a font table — for now just fill a block per letter
    while (*text) {
        for (int dy = 0; dy < 8; dy++) {
            for (int dx = 0; dx < 6; dx++) {
                uint16_t drawColor = (*text != ' ') ? color : bg;
                oled_set_window(x + dx, y + dy, x + dx, y + dy);
                oled_write_cmd(0x5C);
                oled_write_data(drawColor >> 8);
                oled_write_data(drawColor & 0xFF);
            }
        }
        x += 6;
        text++;
    }
}
