#include <avr/io.h>
#include <avr/interrupt.h>
#include "buzzer.h"
#include "timer.h"

/* Shared system tick counter from timer.c */
extern volatile uint32_t sys_ticks;

static volatile bool buzzer_active = false;
static volatile uint32_t buzzer_end_time = 0;

/* Initialise buzzer PWM (same as your original version) */
void buzzer_init(void) {
    cli();
    PORTB.OUTSET = PIN0_bm;        // ensure buzzer off
    PORTB.DIRSET = PIN0_bm;        // PB0 as output

    // Configure TCA0 - Clock source: CLK_PER / 1, single-slope PWM
    TCA0.SINGLE.CTRLA = TCA_SINGLE_CLKSEL_DIV1_gc;
    TCA0.SINGLE.CTRLB = TCA_SINGLE_WGMODE_SINGLESLOPE_gc | TCA_SINGLE_CMP0EN_bm;

    TCA0.SINGLE.CTRLA |= TCA_SINGLE_ENABLE_bm; // enable timer
    sei();
}

/* Non-blocking buzzer play*/
void buzzer_play(uint16_t period, uint16_t duration_ms) {
    cli();
    TCA0.SINGLE.PER = period;
    TCA0.SINGLE.CMP0 = period / 2;   // 50% duty cycle
    PORTB.OUTCLR = PIN0_bm;          // buzzer on (active-low)
    buzzer_active = true;
    buzzer_end_time = sys_ticks + duration_ms;
    sei();
}

/* Called every loop iteration */
void buzzer_task(void) {
    if (buzzer_active && sys_ticks >= buzzer_end_time) {
        TCA0.SINGLE.CMP0 = 0;
        PORTB.OUTSET = PIN0_bm;    // buzzer off
        buzzer_active = false;
    }
}

/* Helper tones */
void beep_increment(void) { buzzer_play(TONE3_PER, 50); }
void beep_decrement(void) { buzzer_play(TONE2_PER, 70); }
void beep_error(void)     { buzzer_play(TONE1_PER, 80); }
