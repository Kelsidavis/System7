#ifndef X86_PIT_H
#define X86_PIT_H

#include <stdint.h>

void pit_init_hz(uint32_t hz);
uint16_t pit_get_divisor(void);

#endif /* X86_PIT_H */
