#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"
#include "chardisp.h"
#include "healthbar.h"
#include "bitmap.h"
#include "buzzer.h"

// External pin definitions
extern const int SPI_OLED_SCK;
extern const int SPI_OLED_MOSI;
extern const int SPI_OLED_CSn;
extern const int OLED_DC;
extern const int FEED_BUTTON;
extern const int PET_BUTTON;
extern const int CLEAN_BUTTON;
extern const int CLEAN_BUTTON;

#define OLED_SPI spi1

// OLED display dimensions
#define OLED_WIDTH 128
#define OLED_HEIGHT 128

extern int health; // for health bar

//pet states
typedef enum {
    STATE_START_SCREEN,
    STATE_NORMAL,
    STATE_HUNGRY,
    STATE_EATING,
    STATE_HAPPY,
    STATE_SAD,
    STATE_CLEAN,
    STATE_DIRTY,
    STATE_SLEEPING,
    STATE_SLEEPY,
    STATE_PET,
    STATE_DEAD
} PetState;

PetState pet_state = STATE_START_SCREEN;
static alarm_id_t random_timer_id;
static alarm_id_t death_alarm_id;
static alarm_id_t hungry_sound_alarm_id;
static alarm_id_t dirty_sound_alarm_id;
static alarm_id_t sadness_sound_alarm_id;
static alarm_id_t sleepy_sound_alarm_id;
alarm_id_t animation_timer_id;
static bool play_death_sound = false;
static bool play_hungry_sound = false;
static bool play_dirty_sound = false;
static bool play_sadness_sound = false;
static bool play_sleepy_sound = false;
static bool walk_toggle = false; //switch between the default walking bitmaps
static bool hungry_toggle = false; //switch between the hungry animation bitmaps
static bool happy_toggle = false; //happy skip type animation hopefully
static bool pet_toggle = false;
static bool clean_toggle = false;
static bool dirty_toggle = false;
static int eating_frame = 0; //cycle through 4 eating frames
static int startscreen_frame = 0; //cycle through 4 start screen frames
static uint32_t random_seed = 12345; // Simple PRNG seed


// -----------------------------
// SPI + GPIO setup
// -----------------------------
void init_oled_pins() { //initializing SPI peripheral
    spi_init(OLED_SPI, 10 * 1000 * 1000); // 10 MHz SPI
    gpio_set_function(SPI_OLED_SCK, GPIO_FUNC_SPI);
    gpio_set_function(SPI_OLED_MOSI, GPIO_FUNC_SPI);

    gpio_init(SPI_OLED_CSn);
    gpio_set_dir(SPI_OLED_CSn, GPIO_OUT); //CSn = out
    gpio_put(SPI_OLED_CSn, 1); //set Csn

    gpio_init(OLED_DC);
    gpio_set_dir(OLED_DC, GPIO_OUT); //DC = out
}

// -----------------------------
// Low-level SPI helpers
// -----------------------------
void oled_write_cmd(uint8_t cmd) {
    gpio_put(OLED_DC, 0); // DC: Command mode
    gpio_put(SPI_OLED_CSn, 0);
    spi_write_blocking(OLED_SPI, &cmd, 1);
    gpio_put(SPI_OLED_CSn, 1);
}

void oled_write_data(uint8_t data) {
    gpio_put(OLED_DC, 1); // DC: Data mode
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
void oled_set_window(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1) { //defines rect region where pixel data is written
    oled_write_cmd(0x15); // Column
    oled_write_data(x0);
    oled_write_data(x1);

    oled_write_cmd(0x75); // Row
    oled_write_data(y0);
    oled_write_data(y1);
}

void oled_fill(uint16_t color) { //fills entire screen with one color
    oled_set_window(0, 0, OLED_WIDTH - 1, OLED_HEIGHT - 1);
    oled_write_cmd(0x5C);

    gpio_put(OLED_DC, 1);
    gpio_put(SPI_OLED_CSn, 0);
    uint8_t buffer[2] = { color >> 8, color & 0xFF };
    for (int i = 0; i < OLED_WIDTH * OLED_HEIGHT; i++)
        spi_write_blocking(OLED_SPI, buffer, 2);
    gpio_put(SPI_OLED_CSn, 1);
}


//draws the defined letters (above) for us
//y --> can go in forms of 1, 16, 32, 64, 128
//implemented lowercase letters now
void oled_draw_text_scaled(uint8_t x, uint8_t y, const char *text, uint16_t color, uint16_t bg, uint8_t scale) {
    while (*text) {
        char c = *text++;
        if (c < 32 || c > 127)
            c = '?';

        const uint8_t *glyph = font6x8[c - 32];

        for (int row = 0; row < 8; row++) {
            for (int srow = 0; srow < scale; srow++) {
                for (int col = 0; col < 6; col++) {
                    uint8_t line = glyph[col];
                    uint16_t drawColor = (line & (1 << row)) ? color : bg;
                    for (int scol = 0; scol < scale; scol++) {
                        oled_set_window(x + col * scale + scol, y + row * scale + srow, 
                                        x + col * scale + scol, y + row * scale + srow);
                        oled_write_cmd(0x5C);
                        oled_write_data(drawColor >> 8);
                        oled_write_data(drawColor & 0xFF);
                    }
                }
            }
        }

        x += 6 * scale;
        if (x + 6 * scale >= OLED_WIDTH) {
            x = 0;
            y += 8 * scale;
        }
    }
}

void oled_draw_sprite_scaled(uint8_t x, uint8_t y, const uint16_t *sprite, uint8_t w, uint8_t h, uint8_t scale) {
    for (int row = 0; row < h; row++) {
        for (int col = 0; col < w; col++) {
            uint16_t color = sprite[row * w + col];

            // Draw a block of pixels for scaling
            for (int dy = 0; dy < scale; dy++) {
                for (int dx = 0; dx < scale; dx++) {
                    uint8_t px = x + col * scale + dx;
                    uint8_t py = y + row * scale + dy;

                    oled_set_window(px, py, px, py);
                    oled_write_cmd(0x5C);
                    oled_write_data(color >> 8);
                    oled_write_data(color & 0xFF);
                }
            }
        }
    }
}

int64_t hungry_sound_callback(alarm_id_t id, void *user_data) {
    if (pet_state == STATE_HUNGRY) {
        play_hungry_sound = true;
        hungry_sound_alarm_id = add_alarm_in_ms(3000, hungry_sound_callback, NULL, false);
    }
    return 0;
}

int64_t dirty_sound_callback(alarm_id_t id, void *user_data) {
    if (pet_state == STATE_DIRTY) {
        play_dirty_sound = true;
        dirty_sound_alarm_id = add_alarm_in_ms(3000, dirty_sound_callback, NULL, false);
    }
    return 0;
}

int64_t sad_sound_callback(alarm_id_t id, void *user_data) {
    if (pet_state == STATE_SAD) {
        play_sadness_sound = true;
        sadness_sound_alarm_id = add_alarm_in_ms(3000, sad_sound_callback, NULL, false);
    }
    return 0;
}

int64_t sleepy_sound_callback(alarm_id_t id, void *user_data) {
    if (pet_state == STATE_SLEEPY) {
        play_sleepy_sound = true;
        sleepy_sound_alarm_id = add_alarm_in_ms(3000, sleepy_sound_callback, NULL, false);
    }
    return 0;
}

int64_t death_callback(alarm_id_t id, void *user_data) {
    pet_state = STATE_DEAD;
    play_death_sound = true;
    update_screen();
    return 0;
}

int64_t random_timer_callback(alarm_id_t id, void *user_data) {
    if (pet_state == STATE_NORMAL) {
        // Better randomization using simple PRNG
        random_seed = random_seed * 1103515245 + 12345;
        random_seed ^= to_ms_since_boot(get_absolute_time());
        uint32_t random_val = (random_seed >> 16) % 4;
        
        switch (random_val) {
            case 0:
                pet_state = STATE_HUNGRY;
                health -= 20;
                hungry_sound_alarm_id = add_alarm_in_ms(1000, hungry_sound_callback, NULL, false);
                break;
            case 1:
                pet_state = STATE_SAD;
                health -= 10;
                sadness_sound_alarm_id = add_alarm_in_ms(1000, sad_sound_callback, NULL, false);
                break;
            case 2:
                pet_state = STATE_DIRTY;
                health -= 15;
                dirty_sound_alarm_id = add_alarm_in_ms(1000, dirty_sound_callback, NULL, false);
                break;
            case 3:
                pet_state = STATE_SLEEPY;
                health -= 5;
                sleepy_sound_alarm_id = add_alarm_in_ms(1000, sleepy_sound_callback, NULL, false);
                break;
        }
        
        if (health <= 0) {
            death_alarm_id = add_alarm_in_ms(1000, death_callback, NULL, false);
        }
        update_screen();
        
        // Set timeout to return to normal after 10 seconds if no button pressed
        random_timer_id = add_alarm_in_ms(1000, auto_reset_callback, NULL, false);
    } else {
        // Continue timer cycle
        random_timer_id = add_alarm_in_ms(1000, auto_reset_callback, NULL, false);
    }
    return 0;
}

int64_t auto_reset_callback(alarm_id_t id, void *user_data) {
    if (pet_state != STATE_NORMAL && pet_state != STATE_DEAD && pet_state != STATE_START_SCREEN) {
        // Cancel sound timers
        if (pet_state == STATE_HUNGRY) health -= 20;
        if (pet_state == STATE_SAD) health -= 10;
        if (pet_state == STATE_DIRTY) health -= 15;
        if (pet_state == STATE_SLEEPY) health -= 5;
        update_screen();
        
        // Restart the random timer
        random_timer_id = add_alarm_in_ms(1000, random_timer_callback, NULL, false);
    }
    return 0;
}

int64_t animation_callback(alarm_id_t id, void *user_data) {
    if (pet_state == STATE_START_SCREEN) {
        startscreen_frame = (startscreen_frame + 1) % 4;
        draw_start_screen_sprite();
    } else {
        walk_toggle = !walk_toggle;
        hungry_toggle = !hungry_toggle;
        happy_toggle = !happy_toggle;
        pet_toggle = !pet_toggle;
        clean_toggle = !clean_toggle;
        dirty_toggle = !dirty_toggle;
        eating_frame = (eating_frame + 1) % 4;
        draw_pet();
    }
    return 10000;
}

void check_death_sound() {
    if (play_death_sound) {
        sad_sound();
        play_death_sound = false;
    }
}

void check_hungry_sound() {
    if (play_hungry_sound) {
        bounce_sound();
        play_hungry_sound = false;
    }
}

void check_dirty_sound() {
    if(play_dirty_sound){
        fart_sound();
        play_dirty_sound = false;
    }
}

void check_sad_sound(){
    if (play_sadness_sound) {
        really_sad_sound();
        play_sadness_sound = false;
    }
}

void check_sleepy_sound(){
    if(play_sleepy_sound){
        sleepy_sound();
        play_sleepy_sound = false;
    }
}

void start_random_timer() {
    cancel_alarm(random_timer_id);
    random_timer_id = add_alarm_in_ms(1000, random_timer_callback, NULL, false);
}

void check_clean_button() {
    if (!gpio_get(CLEAN_BUTTON) && pet_state != STATE_DEAD && pet_state == STATE_DIRTY) { // active low, can't clean if dead
        cancel_alarm(death_alarm_id); // Cancel death timer
        cancel_alarm(dirty_sound_alarm_id); // Cancel dirty sounds
        pet_state = STATE_CLEAN;
        giggle_sound();
        health = (health < 80) ? health + 30 : 100; // Add health, max 100
        update_screen();
        sleep_ms(100);
        pet_state = STATE_NORMAL;
        update_screen();
        start_random_timer();
        sleep_ms(200); // debounce
    }
}

void init_clean_button() {
    gpio_init(CLEAN_BUTTON);
    gpio_set_dir(CLEAN_BUTTON, GPIO_IN);
    gpio_pull_up(CLEAN_BUTTON); // stable high when not pressed
}

void check_pet_button() {
    if (!gpio_get(PET_BUTTON) && pet_state != STATE_DEAD && pet_state == STATE_SAD) { // active low, can't play if dead
        cancel_alarm(death_alarm_id); // Cancel death timer
        cancel_alarm(sadness_sound_alarm_id); // Cancel sad sounds
        pet_state = STATE_PET;
        happy_sound();
        health = (health < 80) ? health + 20 : 100; // Add health, max 100
        update_screen();
        sleep_ms(100);
        pet_state = STATE_HAPPY;
        update_screen();
        sleep_ms(100);
        pet_state = STATE_NORMAL;
        update_screen();
        start_random_timer();
        sleep_ms(200); // debounce
    }
}

void init_pet_button() {
    gpio_init(PET_BUTTON);
    gpio_set_dir(PET_BUTTON, GPIO_IN);
    gpio_pull_up(PET_BUTTON); // stable high when not pressed
}

void check_feed_button() {
    if (!gpio_get(FEED_BUTTON) && pet_state != STATE_DEAD && pet_state == STATE_HUNGRY) { // active low, can't feed if dead
        cancel_alarm(death_alarm_id); // Cancel death timer
        cancel_alarm(hungry_sound_alarm_id); // Cancel hungry sound timer
        pet_state = STATE_EATING;
        chirp_sound();
        health = (health < 60) ? health + 40 : 100; // Add health, max 100
        update_screen();
        sleep_ms(100);
        pet_state = STATE_HAPPY;
        update_screen();
        sleep_ms(100);
        pet_state = STATE_NORMAL;
        update_screen();
        start_random_timer();
        sleep_ms(200); // debounce
    }
}

void init_feed_button() {
    gpio_init(FEED_BUTTON);
    gpio_set_dir(FEED_BUTTON, GPIO_IN);
    gpio_pull_up(FEED_BUTTON); // stable high when not pressed
}

void check_reset_button() {
    // Disabled reset button to prevent accidental resets
    if ((pet_state == STATE_DEAD) && (!gpio_get(FEED_BUTTON) || !gpio_get(PET_BUTTON) || !gpio_get(CLEAN_BUTTON))) {
        oled_fill(0xFFFF);
        pet_state = STATE_START_SCREEN;
        health = 100;
        startscreen_frame = 0;
        oled_fill(0xFFFF); // Clear death screen (get rid of death text)
        update_screen();
        sleep_ms(200);
    }
}

void check_sleep_photoresistor(){
    uint16_t adc_value = adc_read();
    if (adc_value < 1000 && pet_state != STATE_DEAD && pet_state == STATE_SLEEPY) {
        cancel_alarm(death_alarm_id);
        cancel_alarm(sleepy_sound_alarm_id);
        pet_state = STATE_SLEEPING;
        energized_sound();
        health = (health < 90) ? health + 10 : 100;
        update_screen();
        sleep_ms(200);
        pet_state = STATE_HAPPY;
        update_screen();
        sleep_ms(100);
        pet_state = STATE_NORMAL;
        update_screen();
        start_random_timer();
        sleep_ms(200);
    }
}

void init_sleepy_photoresistor(){
    adc_init();
    adc_gpio_init(41);
    adc_select_input(1); // Use ADC0 for photoresistor
}

void check_health(){
    if (health <= 0){
        pet_state = STATE_DEAD;
        play_death_sound = true;
        update_screen();
    }
}

void check_start_game() {
    if (pet_state == STATE_START_SCREEN) {
        if (!gpio_get(FEED_BUTTON) || !gpio_get(PET_BUTTON) || !gpio_get(CLEAN_BUTTON)) {
            cancel_alarm(animation_timer_id);
            pet_state = STATE_NORMAL;
            animation_timer_id = add_alarm_in_ms(500, animation_callback, NULL, true);
            update_screen();
            start_random_timer();
            sleep_ms(200); // debounce
        }
    }
}

void draw_start_screen() {
    oled_fill(0xFFFF); // white background
    
    // Write "TOMABYTE" text higher up
    oled_draw_text_scaled(16, 10, "TOMABYTE", 0x0000, 0xFFFF, 2);
    // Write "Press any button to start" below it
    oled_draw_text_scaled(4, 30, "Press any button to start", 0x0000, 0xFFFF, 1);
    
    // Draw initial sprite
    draw_start_screen_sprite();
}

void draw_start_screen_sprite() {
    // Animate pet sprite in bottom half with smaller size
    if (startscreen_frame == 0) {
        oled_draw_sprite_scaled(64, 70, pet_sprite_startscreen_default, 16, 16, 3);
    } else if (startscreen_frame == 1) {
        oled_draw_sprite_scaled(64, 70, pet_sprite_startscreen_jump, 16, 16, 3);
    } else if (startscreen_frame == 2) {
        oled_draw_sprite_scaled(64, 70, pet_sprite_startscreen_default, 16, 16, 3);
    } else {
        oled_draw_sprite_scaled(64, 70, pet_sprite_startscreen_squash, 16, 16, 3);
    }
}

void draw_pet() //renamed update screen to draw pet
{
    // Draw pet sprite in center(if hungry, draw hungry bitmap)
    if (pet_state == STATE_NORMAL) {
        if (walk_toggle) {
            oled_draw_sprite_scaled(32, 50, pet_sprite, 16, 16, 4);
        } else {
            oled_draw_sprite_scaled(32, 50, pet_sprite_walk, 16, 16, 4);
        }
    } else if (pet_state == STATE_HUNGRY) {
        if (hungry_toggle) {
            oled_draw_sprite_scaled(32, 50, pet_sprite_hungry, 16, 16, 4);
        } else {
            oled_draw_sprite_scaled(32, 50, pet_sprite_hungry2, 16, 16, 4);
        }
    } else if (pet_state == STATE_EATING) {
        if (eating_frame == 0) {
            oled_draw_sprite_scaled(32, 50, pet_sprite_eating1, 16, 16, 4);
        } else if (eating_frame == 1) {
            oled_draw_sprite_scaled(32, 50, pet_sprite_eating2, 16, 16, 4);
        } else if (eating_frame == 2) {
            oled_draw_sprite_scaled(32, 50, pet_sprite_eating3, 16, 16, 4);
        } else {
            oled_draw_sprite_scaled(32, 50, pet_sprite_eating4, 16, 16, 4);
        }
    } else if (pet_state == STATE_SLEEPING){
        oled_draw_sprite_scaled(32, 50, pet_sprite_sleeping, 16, 16, 4);
    } else if (pet_state == STATE_HAPPY){
        if (happy_toggle) {
            oled_draw_sprite_scaled(32, 50, pet_sprite_happy1, 16, 16, 4);
        } else {
            oled_draw_sprite_scaled(32, 50, pet_sprite_happy2, 16, 16, 4);
        }
    } else if (pet_state == STATE_SAD){
        oled_draw_sprite_scaled(32, 50, pet_sprite_sad, 16, 16, 4);
    } else if (pet_state == STATE_PET){
        if (pet_toggle) {
            oled_draw_sprite_scaled(32, 50, pet_sprite_pet1, 16, 16, 4);
        } else {
            oled_draw_sprite_scaled(32, 50, pet_sprite_pet2, 16, 16, 4);
        }
    } else if (pet_state == STATE_DIRTY){
        if (dirty_toggle) {
            oled_draw_sprite_scaled(32, 50, pet_sprite_dirty1, 16, 16, 4);
        } else {
            oled_draw_sprite_scaled(32, 50, pet_sprite_dirty2, 16, 16, 4);
        }
    } else if (pet_state == STATE_CLEAN){
        if (clean_toggle) {
            oled_draw_sprite_scaled(32, 50, pet_sprite_clean1, 16, 16, 4);
        } else {
            oled_draw_sprite_scaled(32, 50, pet_sprite_clean2, 16, 16, 4);
        }
    } else if (pet_state == STATE_SLEEPY){
        oled_draw_sprite_scaled(32, 50, pet_sprite_sleepy, 16, 16, 4);
    }
}

void update_screen(){
    if (pet_state == STATE_START_SCREEN) {
        draw_start_screen();
    } else if (pet_state == STATE_DEAD) {
        oled_fill(0xFFFF); // white background
        
        // Write "GAME OVER" text in top half
        oled_draw_text_scaled(16, 10, "GAME OVER", 0x0000, 0xFFFF, 2);
        // Write "Press RESET to start over"
        oled_draw_text_scaled(8, 30, "Press RESET to start over", 0x0000, 0xFFFF, 1);
        
        // Draw dead pet sprite in bottom half
        oled_draw_sprite_scaled(32, 50, pet_sprite_dead, 16, 16, 4);
    } else {
        oled_fill(0xFFFF); // clears the screen (white background)

        // Draw pet once (initial frame)
        draw_pet();

        //draw health bar only if not dead
        if (pet_state != STATE_DEAD) {
            oled_draw_healthbar(10,10,100,12,health);
        }
    }
}

