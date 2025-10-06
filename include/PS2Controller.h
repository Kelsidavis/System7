/* PS/2 Controller Interface */
#ifndef PS2_CONTROLLER_H
#define PS2_CONTROLLER_H

#include <stdbool.h>
#include <stdint.h>
#include "SystemTypes.h"

/* PS/2 Controller Functions */
Boolean InitPS2Controller(void);
void PollPS2Input(void);
void GetMouse(Point* pt);
UInt16 GetPS2Modifiers(void);
Boolean GetPS2KeyboardState(KeyMap keyMap);

#endif /* PS2_CONTROLLER_H */
