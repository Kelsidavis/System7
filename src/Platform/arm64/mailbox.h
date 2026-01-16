/*
 * ARM64 Mailbox Interface
 * VideoCore mailbox for Raspberry Pi 3/4/5
 */

#ifndef ARM64_MAILBOX_H
#define ARM64_MAILBOX_H

#include <stdint.h>
#include <stdbool.h>

/* Mailbox buffer - shared with framebuffer driver */
extern uint32_t mailbox_buffer[256];

/* Initialize mailbox */
bool mailbox_init(void);

/* Call mailbox with property tags */
bool mailbox_call(uint32_t channel);

/* Get hardware information */
bool mailbox_get_board_model(uint32_t *model);
bool mailbox_get_board_revision(uint32_t *revision);
bool mailbox_get_arm_memory(uint32_t *base, uint32_t *size);

/* Power management */
bool mailbox_power_off(void);

/* Check if mailbox is available */
bool mailbox_is_available(void);

#endif /* ARM64_MAILBOX_H */
