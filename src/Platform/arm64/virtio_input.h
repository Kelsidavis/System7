/*
 * VirtIO Input Driver Header
 * For QEMU virtio-input device support
 */

#ifndef VIRTIO_INPUT_H
#define VIRTIO_INPUT_H

#include <stdint.h>
#include <stdbool.h>

/* Initialize virtio-input device */
bool virtio_input_init(void);

/* Poll for input events - call this regularly */
void virtio_input_poll(void);

/* Get current modifier key state */
uint16_t virtio_input_get_modifiers(void);

/* Check if a key event is available */
bool virtio_input_key_available(void);

/* Get next key event from queue
 * Returns true if event was available */
bool virtio_input_get_key(uint8_t *keycode, uint8_t *modifiers, bool *pressed);

/* Check if input device is initialized */
bool virtio_input_is_initialized(void);

#endif /* VIRTIO_INPUT_H */
