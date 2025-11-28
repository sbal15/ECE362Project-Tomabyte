#ifndef CHARDISP_H
#define CHARDISP_H

void init_oled_pins();
void oled_init();
void oled_write_cmd(uint8_t cmd);
void oled_write_data(uint8_t data);
void oled_set_window(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1);
void oled_fill(uint16_t color);
void oled_draw_text(uint8_t x, uint8_t y, const char *text, uint16_t color, uint16_t bg);
int64_t animation_callback(alarm_id_t id, void *user_data);
int64_t auto_reset_callback(alarm_id_t id, void *user_data);
void draw_pet();
void start_random_timer();

#endif
