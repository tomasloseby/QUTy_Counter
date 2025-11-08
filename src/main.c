/*
  main.c - FSM-based counter for QUTy
*/

#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdbool.h>
#include <stdint.h>

#include "timer.h"
#include "spi.h"
#include "buzzer.h"

// Error flash state

typedef enum {
    ERROR_NONE,
    ERROR_FLASH_ON,
    ERROR_FLASH_OFF
} ErrorFlashState;

static ErrorFlashState err_state = ERROR_NONE;
static uint32_t err_timer = 0;
static uint8_t err_cycles = 0;
volatile bool error_flash_active = false;

extern volatile uint32_t sys_ticks;

/* shared display buffer (defined in spi.c) */
extern uint8_t disp[];

/* Button globals (single definitions live here) */
volatile uint8_t pb_state = 0xFF;        // last stable state
volatile uint8_t pb_filtered = 0xFF;     // debounced state
volatile uint8_t pb_falling = 0x00;      // falling edges
volatile uint8_t pb_history[4] = {0xFF, 0xFF, 0xFF, 0xFF};

/* system tick (ms) provided by timer.c */
extern volatile uint32_t sys_ticks;

/* 7-seg lookup */
const uint8_t display_digit_values[10] = {
    0b00001000, 0b01101011, 0b01000100, 0b01000001, 0b00100011,
    0b00010001, 0b00010000, 0b01001011, 0b00000000, 0b00000011
};

/* digits */
uint8_t left_digit = 0;
uint8_t right_digit = 0;

/* simple display update helper */
void update_display(void) {
    disp[0] = display_digit_values[left_digit];
    disp[1] = display_digit_values[right_digit];
}

/* increment/decrement helpers */
void increment_integer(void)  { if (left_digit < 9) left_digit++; }
void decrement_integer(void)  { if (left_digit > 0) left_digit--; }

void increment_decimal(void) {
    right_digit++;
    if (right_digit > 9) {
        right_digit = 0;
        increment_integer();
    }
}

void decrement_decimal(void) {
    if (right_digit == 0) {
        right_digit = 9;
        decrement_integer();
    } else {
        right_digit--;
    }
}

/* button pull-ups */
void pb_init(void) {
    PORTA.PIN4CTRL = PORT_PULLUPEN_bm; // S1
    PORTA.PIN5CTRL = PORT_PULLUPEN_bm; // S2
    PORTA.PIN6CTRL = PORT_PULLUPEN_bm; // S3
    PORTA.PIN7CTRL = PORT_PULLUPEN_bm; // S4
}

/* FSM */
typedef enum {
    STATE_IDLE,
    STATE_INC_INT,
    STATE_DEC_INT,
    STATE_INC_DEC,
    STATE_DEC_DEC,
    STATE_ERROR_FLASH,
    STATE_UPDATE
} CounterState;

static CounterState state = STATE_IDLE;

/* Error flash control */

// Correct error flash pattern – both digits mid-bar only
void start_error_flash(void) {
    err_state = ERROR_FLASH_ON;
    err_timer = sys_ticks + 100;
    err_cycles = 0;
    error_flash_active = true;
    beep_error();

    PORTB.OUTSET = PIN5_bm;   // DP OFF (active-low)
}

// pattern: only middle segment (g) on; others off
#define MID_SEG_PATTERN 0b11110111  // segment g low

void error_flash_task(void) {
    switch (err_state) {
        case ERROR_FLASH_ON:
            // both digits: mid segment only, DP off
            disp[0] = MID_SEG_PATTERN;
            disp[1] = MID_SEG_PATTERN;
            if (sys_ticks >= err_timer) {
                err_state = ERROR_FLASH_OFF;
                err_timer = sys_ticks + 100;
            }
            break;

        case ERROR_FLASH_OFF:
            // both digits off, DP off
            disp[0] = 0xFF;
            disp[1] = 0xFF;
            if (sys_ticks >= err_timer) {
                err_cycles++;
                if (err_cycles >= 3) {
                    update_display();          // restore normal digits
                    err_state = ERROR_NONE;
                    error_flash_active = false;
                } else {
                    err_state = ERROR_FLASH_ON;
                    err_timer = sys_ticks + 100;
                    beep_error();
                }
            }
            break;

        case ERROR_NONE:
        default:
            break;
    }
}

/* State Machine */
void state_handler(void) {
    switch (state) {
        case STATE_IDLE:
            if (!error_flash_active) { // Lock input during error flash
                if (pb_falling & PIN4_bm) state = STATE_INC_INT;
                else if (pb_falling & PIN5_bm) state = STATE_DEC_INT;
                else if (pb_falling & PIN6_bm) state = STATE_INC_DEC;
                else if (pb_falling & PIN7_bm) state = STATE_DEC_DEC;
            }
            break;

        case STATE_INC_INT:
            if (left_digit < 9) {
                increment_integer();
                beep_increment();
                state = STATE_UPDATE;
            } else {
                start_error_flash();
                state = STATE_ERROR_FLASH;
            }
            break;

        case STATE_DEC_INT:
            if (left_digit > 0) {
                decrement_integer();
                beep_decrement();
                state = STATE_UPDATE;
            } else {
                start_error_flash();
                state = STATE_ERROR_FLASH;
            }
            break;

        case STATE_INC_DEC:
            if (left_digit == 9 && right_digit == 9) {
                start_error_flash();
                state = STATE_ERROR_FLASH;
            } else {
                increment_decimal();
                beep_increment();
                state = STATE_UPDATE;
            }
            break;

        case STATE_DEC_DEC:
            if (left_digit == 0 && right_digit == 0) {
                start_error_flash();
                state = STATE_ERROR_FLASH;
            } else {
                decrement_decimal();
                beep_decrement();
                state = STATE_UPDATE;
            }
            break;

        case STATE_ERROR_FLASH:
            // Wait until flash sequence completes
            if (!error_flash_active) {
                state = STATE_UPDATE;
            }
            break;

        case STATE_UPDATE:
            update_display();
            pb_falling = 0x00;  // clear handled edges
            state = STATE_IDLE;
            break;

        default:
            state = STATE_IDLE;
            break;
    }
}

/* Main */
int main(void) {
    pb_init();
    spi_init();
    timer_init();
    buzzer_init();

    /* DP pin as output; set to off (active-low on your board) */
    PORTB.DIRSET = PIN5_bm;
    PORTB.OUTSET = PIN5_bm;

    update_display();
    sei();

    while (1) {
        /* Periodic tasks (non-blocking) */
        buzzer_task();
        error_flash_task();

        /* FSM step */
        state_handler();
    }
    return 0;
}
