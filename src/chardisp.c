#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/dma.h"
#include "hardware/gpio.h"
#include "chardisp.h"


//NEED TO CHANGE CODE

// Make sure to set these in main.c
extern const int SPI_DISP_SCK; extern const int SPI_DISP_CSn; extern const int SPI_DISP_TX;

/***************************************************************** */

// "chardisp" stands for character display, which can be an LCD or OLED
void init_chardisp_pins() {
    // fill in

    // part1
    gpio_set_function(SPI_DISP_SCK, GPIO_FUNC_SPI);
    gpio_set_function(SPI_DISP_CSn, GPIO_FUNC_SPI);
    gpio_set_function(SPI_DISP_TX, GPIO_FUNC_SPI);

    //part2
    // spi_set_baudrate(spi1, 10000);
    spi_init(spi1,10000);
    spi_set_format(spi1, 9, 0, 0, SPI_MSB_FIRST);
}

void send_spi_cmd(spi_inst_t* spi, uint16_t value) {
    // fill in

    while(spi_is_busy(spi));

    uint16_t temp = value;
    spi_write16_blocking(spi, &temp, 1);
    
}

void send_spi_data(spi_inst_t* spi, uint16_t value) {
    // fill in
    uint16_t tempVal = value | 0x100;
    send_spi_cmd(spi, tempVal);
}

void cd_init() {
    // fill in

    //I NEED HELP FINDING THE VALUES, I DONT UNDERSTAND PLEASE HELP
    //sleeping 1 ms
    sleep_ms(1);
    // send_spi_cmd
    //8 bit interface, 2 line display mode, 11 dots per char
    send_spi_cmd(spi1, 0b0000101100);

    sleep_us(40);

    //turn on display, turrn off cursor, turrn off cursor blink
    send_spi_cmd(spi1, 0b0000001100); 

    sleep_us(40);

    send_spi_cmd(spi1,0b0000000001); 

    sleep_ms(2);

    //move cursor to the right and increment ddram addy by 1, do NOT shift display
    send_spi_cmd(spi1, 0b0000000110);

    sleep_us(40);

    send_spi_cmd(spi1, 0b0000000010);

}

void cd_display1(const char *str) {
    // fill in
    send_spi_cmd(spi1, 0b0000000010); //i dont know the value first line of the display

    // int len = (strlen(str) < 16) ? strlen(str) : 16;
    spi_inst_t* spi = spi1;

    for(int i = 0; i < 16; i++)
    {
        send_spi_data(spi, str[i]);
    }



}
void cd_display2(const char *str) {
    // fill in

    send_spi_cmd(spi1, 0b11000000); //i dont know the value the secod line of the display

    // int len = (strlen(str) < 16) ? strlen(str) : 16;
    spi_inst_t* spi = spi1;
    for(int i = 0; i < 16; i++)
    {
        send_spi_data(spi, str[i]);
    }
}

/***************************************************************** */