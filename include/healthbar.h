#ifndef HEALTHBAR_H
#define HEALTHBAR_H

#include <stdint.h>

extern int health;
void oled_draw_rect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint16_t color);
void oled_draw_healthbar(uint8_t x, uint8_t y, uint8_t width, uint8_t height, int health);
void healthbar_init_timer();
void decrease_health_bar();

#endif
