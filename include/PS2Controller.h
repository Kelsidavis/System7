/* PS/2 Controller Interface */
#ifndef PS2_CONTROLLER_H
#define PS2_CONTROLLER_H

#include <stdbool.h>
#include <stdint.h>
#include "SystemTypes.h"

/* PS/2 Controller Functions */
Boolean InitPS2Controller(void);
Boolean PS2_IsInitialized(void);
void PollPS2Input(void);
void GetMouse(Point* pt);
void UpdateMouseStateDelta(SInt16 dx, SInt16 dy, UInt8 buttons);
void UpdateMouseStateAbsolute(SInt16 x, SInt16 y, UInt8 buttons);
UInt16 GetPS2Modifiers(void);
Boolean GetPS2KeyboardState(KeyMap keyMap);
void PS2_SetIRQDriven(Boolean enabled);
Boolean PS2_IsIRQDriven(void);

#endif /* PS2_CONTROLLER_H */
