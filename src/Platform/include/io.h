#ifndef HAL_IO_H
#define HAL_IO_H

#include <stdint.h>

void hal_outb(uint16_t port, uint8_t value);
uint8_t hal_inb(uint16_t port);
void hal_outw(uint16_t port, uint16_t value);
uint16_t hal_inw(uint16_t port);
void hal_outl(uint16_t port, uint32_t value);
uint32_t hal_inl(uint16_t port);

/* I/O delay for timing */
void hal_io_wait(void);

#endif /* HAL_IO_H */
