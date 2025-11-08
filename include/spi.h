/*
spi.h - Serial Peripheral Interface control.
Configures SPI0 for communication with 7-segment display.
*/
#include <avr/io.h>
#include <avr/interrupt.h>

//Initialise SPI0 as master for display control.
void spi_init(void);

// Write 1 byte over SPI bus.
void spi_write(uint8_t b);