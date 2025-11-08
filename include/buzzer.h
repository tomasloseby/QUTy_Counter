#ifndef BUZZER_H
#define BUZZER_H

#include <stdint.h>
#include <stdbool.h>

/* tone period values for buzzer frequencies (smaller period = higher pitch) */
#define TONE1_PER   20754  // invalid (error)
#define TONE2_PER   10376  // valid decrement
#define TONE3_PER   7782   // increment success

void buzzer_init(void);
void buzzer_play(uint16_t period, uint16_t duration_ms);
void buzzer_task(void);

void beep_increment(void);
void beep_decrement(void);
void beep_error(void);

#endif
