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
void GetMouse(Point* pt);          /* global (screen) coordinates */
void GetMouseLocal(Point* pt);     /* current port's coordinates */
void UpdateMouseStateDelta(SInt16 dx, SInt16 dy, UInt8 buttons);
void UpdateMouseStateAbsolute(SInt16 x, SInt16 y, UInt8 buttons);
UInt16 GetPS2Modifiers(void);
Boolean GetPS2KeyboardState(KeyMap keyMap);
void PS2_SetIRQDriven(Boolean enabled);
Boolean PS2_IsIRQDriven(void);

/* Drain one key press/release recorded by the scancode handler. Returns false
 * when the ring is empty. See the ring's comment in ps2.c for why key events
 * are queued rather than derived from KeyMap snapshots. */
Boolean PS2_DequeueKeyTransition(UInt8* macCode, Boolean* isPressed);

#endif /* PS2_CONTROLLER_H */
