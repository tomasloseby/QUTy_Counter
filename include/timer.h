/*
timer.h - PEriodic timer configuration and ISR.
Provides 10ms interrupt for button debouncing and display multiplexing
*/
#include <avr/interrupt.h>
#include <stdint.h>

// Configure TCB0 for periodic interrupts (10ms).
void timer_init(void);