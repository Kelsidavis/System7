#ifndef ARM_TIMER_H
#define ARM_TIMER_H

#include <stdint.h>

int arm_platform_timer_init(void);
uint64_t arm_get_timer_ticks(void);
uint32_t arm_get_timer_frequency(void);
void arm_set_timer_frequency(uint32_t freq_hz);
uint32_t arm_calibrate_timer(void);
uint64_t arm_get_microseconds(void);
uint32_t arm_get_milliseconds(void);

#endif /* ARM_TIMER_H */
