#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/gpio.h"
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
    STATE_NORMAL,
    STATE_HUNGRY,
    STATE_EATING,
    STATE_HAPPY,
    STATE_SAD,
    STATE_CLEAN,
    STATE_DIRTY,
    STATE_SLEEPING,
    STATE_PET,
    STATE_DEAD
} PetState;

PetState pet_state = STATE_NORMAL;
static alarm_id_t hunger_alarm_id;
static alarm_id_t death_alarm_id;
static alarm_id_t hungry_sound_alarm_id;
static alarm_id_t sadness_alarm_id;
static alarm_id_t dirty_alarm_id;
static alarm_id_t dirty_sound_alarm_id;
static alarm_id_t sadness_sound_alarm_id;
static bool play_death_sound = false;
static bool play_hungry_sound = false;
static bool play_dirty_sound = false;
static bool play_sadness_sound = false;
static bool walk_toggle = false; //switch between the default walking bitmaps
static bool hungry_toggle = false; //switch between the hungry animation bitmaps
static bool happy_toggle = false; //happy skip type animation hopefully
static bool pet_toggle = false;
static bool clean_toggle = false;
static bool dirty_toggle = false;
static int eating_frame = 0; //cycle through 4 eating frames


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
        // Schedule next hungry sound in 3 seconds
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

int64_t death_callback(alarm_id_t id, void *user_data) {
    pet_state = STATE_DEAD;
    play_death_sound = true;
    update_screen();
    return 0;
}

int64_t dirty_callback(alarm_id_t id, void *user_data) {
    pet_state = STATE_DIRTY;
    health -= 15;
    if (health <= 0) {
        death_alarm_id = add_alarm_in_ms(10000, death_callback, NULL, false);
    }
    update_screen(); // Update health bar
    // Start dirty sound timer immediately
    dirty_sound_alarm_id = add_alarm_in_ms(1000, dirty_sound_callback, NULL, false);
    // if (pet_state != STATE_DEAD) {
    //     pet_state = STATE_DIRTY;
    //     health -= 15;
    //     if (health <= 0) {
    //         death_alarm_id = add_alarm_in_ms(10000, death_callback, NULL, false);
    //     }
    //     update_screen(); // Update health bar
    //     // Start dirty sound timer
    //     dirty_sound_alarm_id = add_alarm_in_ms(30000, dirty_sound_callback, NULL, false);
    // }
    return 0;
}

int64_t sadness_callback(alarm_id_t id, void *user_data) {
    pet_state = STATE_SAD;
    health -= 10;
    if (health <= 0) {
        death_alarm_id = add_alarm_in_ms(10000, death_callback, NULL, false);
    }
    update_screen(); // Update health bar
    // Start sadness sound timer immediately
    sadness_sound_alarm_id = add_alarm_in_ms(1000, sad_sound_callback, NULL, false);
    // if (pet_state != STATE_DEAD){
    //     pet_state = STATE_SAD;
    //     health -= 10;
    //     if (health <= 0) {
    //         death_alarm_id = add_alarm_in_ms(10000, death_callback, NULL, false);
    //     }
    //     update_screen(); // Update health bar
    //     // Start sadness sound timer
    //     sadness_sound_alarm_id = add_alarm_in_ms(30000, sad_sound_callback, NULL, false);
    // }
    
    return 0;
}

int64_t hunger_callback(alarm_id_t id, void *user_data) {
    pet_state = STATE_HUNGRY;
    health -= 20;
    if (health <= 0) {
        death_alarm_id = add_alarm_in_ms(10000, death_callback, NULL, false);
    }
    update_screen(); // Update health bar
    // Start hungry sound timer immediately
    hungry_sound_alarm_id = add_alarm_in_ms(1000, hungry_sound_callback, NULL, false);
    return 0;
}

int64_t animation_callback(alarm_id_t id, void *user_data) {
    walk_toggle = !walk_toggle;
    hungry_toggle = !hungry_toggle;
    happy_toggle = !happy_toggle;
    pet_toggle = !pet_toggle;
    clean_toggle = !clean_toggle;
    dirty_toggle = !dirty_toggle;
    eating_frame = (eating_frame + 1) % 4;
    draw_pet();
    return 10000; // repeat every 10000 ms
}

void reset_hunger_timer() {
    // Cancel old alarm if active
    cancel_alarm(hunger_alarm_id);
    // Schedule new alarm in 60,000 ms (1 s)
    hunger_alarm_id = add_alarm_in_ms(10000, hunger_callback, NULL, false);
    // health = 100;
    // oled_draw_healthbar(10,10,100,12,health); //makes the health bar 100 again TEMPORARY
}

void reset_sadness_timer() {
    // Cancel old sadness alarm if active
    cancel_alarm(sadness_alarm_id);
    // Schedule sadness timer - pet gets sad after 30 seconds
    sadness_alarm_id = add_alarm_in_ms(30000, sadness_callback, NULL, false);
}

void reset_dirty_timer() {
    // Cancel old dirty alarm if active
    cancel_alarm(dirty_alarm_id);
    // Schedule dirty timer - pet gets dirty after 45 seconds
    dirty_alarm_id = add_alarm_in_ms(45000, dirty_callback, NULL, false);
}

void check_clean_button() {
    if (!gpio_get(CLEAN_BUTTON) && pet_state != STATE_DEAD) { // active low, can't clean if dead
        cancel_alarm(death_alarm_id); // Cancel death timer
        pet_state = STATE_CLEAN;
        giggle_sound();
        health = (health < 70) ? health + 30 : 100; // Add health, max 100
        update_screen(); // Update health bar
        sleep_ms(500); // Show cleaning for a moment
        pet_state = STATE_NORMAL;
        update_screen(); // Update health bar
        reset_dirty_timer();
        sleep_ms(200); // debounce
    }
}

void init_clean_button() {
    gpio_init(CLEAN_BUTTON);
    gpio_set_dir(CLEAN_BUTTON, GPIO_IN);
    gpio_pull_up(CLEAN_BUTTON); // stable high when not pressed
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

void check_pet_button() {
    if (!gpio_get(PET_BUTTON) && pet_state != STATE_DEAD) { // active low, can't play if dead
        cancel_alarm(death_alarm_id); // Cancel death timer
        pet_state = STATE_PET;
        happy_sound();
        health = (health < 80) ? health + 20 : 100; // Add health, max 100
        update_screen();
        pet_state = STATE_HAPPY;
        update_screen();
        sleep_ms(500);
        pet_state = STATE_NORMAL;
        update_screen();
        reset_sadness_timer();
        sleep_ms(200); // debounce
    }
}

void init_pet_button() {
    gpio_init(PET_BUTTON);
    gpio_set_dir(PET_BUTTON, GPIO_IN);
    gpio_pull_up(PET_BUTTON); // stable high when not pressed
}

void check_feed_button() {
    if (!gpio_get(FEED_BUTTON) && pet_state != STATE_DEAD) { // active low, can't feed if dead
        cancel_alarm(death_alarm_id); // Cancel death timer
        cancel_alarm(hungry_sound_alarm_id); // Cancel hungry sound timer
        pet_state = STATE_EATING;
        chirp_sound();
        health = (health < 60) ? health + 40 : 100; // Add health, max 100
        update_screen();
        pet_state = STATE_HAPPY;
        update_screen();
        pet_state = STATE_NORMAL;
        walk_toggle = false;
        hungry_toggle = false;
        update_screen();
        reset_hunger_timer();
        sleep_ms(200); // debounce
    }
}

void init_feed_button() {
    gpio_init(FEED_BUTTON);
    gpio_set_dir(FEED_BUTTON, GPIO_IN);
    gpio_pull_up(FEED_BUTTON); // stable high when not pressed
}

void check_health(){
    if (health <= 0){
        pet_state = STATE_DEAD;
        play_death_sound = true;
        update_screen();
    }
}

void draw_pet() //renamed update screen to draw pet
{
    // Draw pet sprite in center(if hungry, draw hungry bitmap)
    if (pet_state == STATE_NORMAL) {
        if (walk_toggle) {
            oled_draw_sprite_scaled(56, 50, pet_sprite, 16, 16, 4);
        } else {
            oled_draw_sprite_scaled(56, 50, pet_sprite_walk, 16, 16, 4);
        }
    } else if (pet_state == STATE_HUNGRY) {
        if (hungry_toggle) {
            oled_draw_sprite_scaled(56, 50, pet_sprite_hungry, 16, 16, 4);
        } else {
            oled_draw_sprite_scaled(56, 50, pet_sprite_hungry2, 16, 16, 4);
        }
    } else if (pet_state == STATE_EATING) {
        if (eating_frame == 0) {
            oled_draw_sprite_scaled(56, 50, pet_sprite_eating1, 16, 16, 4);
        } else if (eating_frame == 1) {
            oled_draw_sprite_scaled(56, 50, pet_sprite_eating2, 16, 16, 4);
        } else if (eating_frame == 2) {
            oled_draw_sprite_scaled(56, 50, pet_sprite_eating3, 16, 16, 4);
        } else {
            oled_draw_sprite_scaled(56, 50, pet_sprite_eating4, 16, 16, 4);
        }
    } else if (pet_state == STATE_DEAD){
        oled_draw_sprite_scaled(56, 50, pet_sprite_dead, 16, 16, 4);
    } else if (pet_state == STATE_SLEEPING){
        oled_draw_sprite_scaled(56, 50, pet_sprite_sleeping, 16, 16, 4);
    } else if (pet_state == STATE_HAPPY){
        if (happy_toggle) {
            oled_draw_sprite_scaled(56, 50, pet_sprite_happy1, 16, 16, 4);
        } else {
            oled_draw_sprite_scaled(56, 50, pet_sprite_happy2, 16, 16, 4);
        }
    } else if (pet_state == STATE_SAD){
        oled_draw_sprite_scaled(56, 50, pet_sprite_sad, 16, 16, 4);
    } else if (pet_state == STATE_PET){
        if (pet_toggle) {
            oled_draw_sprite_scaled(56, 50, pet_sprite_pet1, 16, 16, 4);
        } else {
            oled_draw_sprite_scaled(56, 50, pet_sprite_pet2, 16, 16, 4);
        }
    } else if (pet_state == STATE_DIRTY){
        if (dirty_toggle) {
            oled_draw_sprite_scaled(56, 50, pet_sprite_dirty1, 16, 16, 4);
        } else {
            oled_draw_sprite_scaled(56, 50, pet_sprite_dirty2, 16, 16, 4);
        }
    } else if (pet_state == STATE_CLEAN){
        if (clean_toggle) {
            oled_draw_sprite_scaled(56, 50, pet_sprite_clean1, 16, 16, 4);
        } else {
            oled_draw_sprite_scaled(56, 50, pet_sprite_clean2, 16, 16, 4);
        }
    }
}

void update_screen(){
    oled_fill(0xFFFF); // clears the screen (white background)

    // Draw pet once (initial frame)
    draw_pet();

    //draw health bar only if not dead
    if (pet_state != STATE_DEAD) {
        oled_draw_healthbar(10,10,100,12,health);
    }
}

