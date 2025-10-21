/*
 * PS2Input.h - PS/2 Keyboard and Mouse Input
 *
 * Provides PS/2 controller initialization and input polling
 * for keyboard and mouse on x86 platforms.
 *
 * Copyright (c) 2025 - System 7.1 Portable Project
 */

#ifndef __PS2INPUT_H__
#define __PS2INPUT_H__

#include "SystemTypes.h"
#include "EventManager/EventTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* PS/2 Controller Initialization */
Boolean InitPS2Controller(void);

/* PS/2 Input Polling */
void PollPS2Input(void);

/* Mouse Functions */
void GetMouse(Point* mouseLoc);
UInt8 GetMouseButtons(void);  /* Get current mouse button state */

/* Keyboard Functions */
UInt16 GetPS2Modifiers(void);
Boolean GetPS2KeyboardState(KeyMap keyMap);

#ifdef __cplusplus
}
#endif

#endif /* __PS2INPUT_H__ */
