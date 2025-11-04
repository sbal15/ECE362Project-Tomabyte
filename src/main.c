#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "pico/stdlib.h"
// #include "support.h"

// Base library headers ncluded for your convenience.
// ** You may have to add more depending on your practical. **
#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "hardware/timer.h"
#include "hardware/adc.h"
#include "hardware/dma.h"
#include "hardware/pwm.h"
#include "hardware/spi.h"
#include "hardware/uart.h"
#include "pico/rand.h"

void grader();

//////////////////////////////////////////////////////////////////////////////

// Pins for my oled display
const int SPI_OLED_SCK = 14;
const int SPI_OLED_CSn = 13; 
const int SPI_OLED_TX_MOSI = 15;

void init_chardisp_pins();
void cd_init();
void cd_display1(const char *str);
void cd_display2(const char *str);
void send_spi_cmd(spi_inst_t* spi, uint16_t value);
void send_spi_data(spi_inst_t* spi, uint16_t value);


int main()
{
    
    init_chardisp_pins();
    cd_init();
    cd_display1("hello");
    cd_display2("tomabyte");

    for(;;);
    return 0;
}