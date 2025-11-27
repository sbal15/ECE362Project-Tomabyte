#ifndef HEALTHBAR_H
#define HEALTHBAR_H

#include <stdint.h>
#include <stdbool.h>

extern int health;
void oled_draw_rect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint16_t color);
void oled_draw_healthbar(uint8_t x, uint8_t y, uint8_t width, uint8_t height, int health);
void healthbar_init_timer();
void decrease_health_bar();
void update_screen();

// Set to true by the health timer ISR when the health value changes.
// The main loop must clear this and call `update_screen()` to redraw.
extern volatile bool healthbar_update_needed;

#endif
