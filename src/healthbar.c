#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/gpio.h"
#include "chardisp.h"
#include "healthbar.h"

// External pin definitions
extern const int SPI_OLED_SCK;
extern const int SPI_OLED_MOSI;
extern const int SPI_OLED_CSn;
extern const int OLED_DC;

#define OLED_SPI spi1

// OLED display dimensions
#define OLED_WIDTH 128
#define OLED_HEIGHT 128

//global vairbale or health
// int health = 100;

// Draw a filled rectangle (health bar)
void oled_draw_rect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint16_t color) {
    oled_set_window(x, y, x + w - 1, y + h - 1);
    oled_write_cmd(0x5C); // RAM write

    gpio_put(OLED_DC, 1);
    gpio_put(SPI_OLED_CSn, 0);

    uint8_t buf[2] = { color >> 8, color & 0xFF };

    for (int i = 0; i < w * h; i++) {
        spi_write_blocking(OLED_SPI, buf, 2);
    }

    gpio_put(SPI_OLED_CSn, 1);
}

// Draw a health bar based on percentage (0–100)
void oled_draw_healthbar(uint8_t x, uint8_t y, uint8_t width, uint8_t height, int health) {
    if (health < 0) health = 0;
    if (health > 100) health = 100;

    int fill_width = (width * health) / 100;

    uint16_t green  = 0x07E0;  // 565 green
    uint16_t red    = 0xF800;  // 565 red
    uint16_t black  = 0x0000;  // background

    // Draw background (empty bar)
    oled_draw_rect(x, y, width, height, black);

    // Draw filled portion
    uint16_t bar_color = health > 30 ? green : red;  // turn red if low
    oled_draw_rect(x, y, fill_width, height, bar_color);
}

void decrease_health_bar()
{
    hw_clear_bits(&timer1_hw->intr, 1u<<0);

    health-= 20;
    if(health<0)
    {
        health = 0;
    }

    oled_draw_healthbar(10,10,100,12,health);
    uint64_t next = timer1_hw->timerawl + (30000 * 1000);
    timer1_hw->alarm[0] = (uint32_t) next;

}
void healthbar_init_timer()
{
    hw_set_bits(&timer1_hw->inte, (1u << 0));
    irq_set_exclusive_handler(TIMER1_IRQ_0,decrease_health_bar);
    nvic_hw->iser[0] |= 1 << TIMER1_IRQ_0;
    uint64_t temp = 20000 + timer1_hw->timerawl;
    timer1_hw->alarm[0] = (uint32_t)temp;
}