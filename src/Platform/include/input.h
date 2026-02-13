#ifndef HAL_INPUT_H
#define HAL_INPUT_H

#include <stdint.h>
#include "MacTypes.h"

/* Mouse input source selection */
typedef enum {
    kMouseSourcePS2,
    kMouseSourceUSBTablet
} MouseSource;

void hal_input_set_mouse_source(MouseSource src);
Boolean hal_input_ps2_mouse_active(void);

void hal_input_poll(void);
void hal_input_get_mouse(Point* mouse_loc);
Boolean hal_input_get_keyboard_state(KeyMap key_map);

#endif /* HAL_INPUT_H */
