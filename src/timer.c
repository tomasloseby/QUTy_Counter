#include "timer.h"
#include "spi.h"
#include <avr/interrupt.h>
#include <stdbool.h>

extern uint8_t disp[];

extern volatile bool error_flash_active;  // from main.c

extern volatile uint8_t pb_filtered;
extern volatile uint8_t pb_state;
extern volatile uint8_t pb_falling;
extern volatile uint8_t pb_history[4];

volatile uint8_t digit = 0;
volatile uint32_t sys_ticks = 0;  // global tick counter (ms)

// Debounce buttons (called every ~10 ms)
void pb_debounce(void) {
    uint8_t raw = PORTA.IN;

    for (uint8_t i = 0; i < 4; i++) {
        uint8_t mask = (1 << (4 + i));
        uint8_t sample = (raw & mask) ? 1 : 0;

        pb_history[i] = (pb_history[i] << 1) | sample;

        if (pb_history[i] == 0x00)
            pb_filtered &= ~mask;   // stable pressed
        else if (pb_history[i] == 0xFF)
            pb_filtered |= mask;    // stable released
    }

    pb_falling = pb_state & ~pb_filtered;
    pb_state = pb_filtered;
}

// Timer init
void timer_init(void) {
    cli();

    TCB0.CTRLB = TCB_CNTMODE_INT_gc;   // periodic interrupt mode
    TCB0.CCMP  = 33333;                // ~10 ms at 3 MHz clock
    TCB0.INTCTRL = TCB_CAPT_bm;        // enable interrupt
    TCB0.CTRLA = TCB_ENABLE_bm;        // start timer

    // Configure DP pin (PB5) as output, default off (active-low)
    PORTB.DIRSET = PIN5_bm;
    PORTB.OUTSET = PIN5_bm;

    sei();
}

// ======================================================
// Display multiplex + debounce ISR
// ======================================================
ISR(TCB0_INT_vect) {
    uint8_t out = disp[digit];

    if (digit == 0) {
        out |= (1 << 7);  // enable digit 0
        // Normally DP on for left digit, but not during error flash
        if (!error_flash_active) {
            PORTB.OUTCLR = PIN5_bm;  // DP ON (active-low)
        } else {
            PORTB.OUTSET = PIN5_bm;  // DP OFF during flash
        }
    } else {
        out &= ~(1 << 7); // enable digit 1
        PORTB.OUTSET = PIN5_bm;  // DP always off for right digit
    }

    spi_write(out);

    digit ^= 1;       // toggle between digits
    pb_debounce();    // sample buttons

    sys_ticks += 10;  // 10 ms tick counter
    TCB0.INTFLAGS = TCB_CAPT_bm; // clear interrupt flag
}
