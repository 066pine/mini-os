#pragma once

#include <stdint.h>

#define TIMER_HZ 100

void timer_init(uint32_t frequency);
uint64_t timer_ticks(void);
uint64_t timer_tsc_hz(void);
void timer_irq_handler(void);
