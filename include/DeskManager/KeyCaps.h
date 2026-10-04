#ifndef KEYCAPS_H
#define KEYCAPS_H

#include "SystemTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Built-in US keyboard display and a Mac Roman text strip. Window ownership
 * and desk accessory registration belong to BuiltinDAs.c. */
typedef struct KeyCaps {
    UInt16 modifiers;          /* Event Manager modifier bits */
    UInt32 clickState;         /* Caller-owned dead-key state for mouse input */
    char typed[64];
    SInt16 typedLen;
    SInt16 litKey;             /* Display index of the pressed key, or -1 */
    UInt32 litTick;
} KeyCaps;

#define KEYCAPS_ERR_NONE            0
#define KEYCAPS_ERR_INVALID_KEY     -2
#define KEYCAPS_ERR_INVALID_PARAM   -4

int KeyCaps_Initialize(KeyCaps *keyCaps);
void KeyCaps_Reset(KeyCaps *keyCaps);

/* Draw the whole keyboard in the current port. */
void KeyCaps_DrawKeyboard(KeyCaps *keyCaps);

/* Points are in window-local coordinates; modifiers use Event Manager bits. */
int KeyCaps_HandleClick(KeyCaps *keyCaps, Point point, UInt16 modifiers);

/* keyCode contains the virtual key in bits 8-15 and its Mac Roman byte in
 * bits 0-7, as in a keyDown or autoKey event message. */
int KeyCaps_HandleKeyPress(KeyCaps *keyCaps, UInt16 keyCode, UInt16 modifiers);

/* Release the highlighted key after twelve ticks and refresh modifier labels. */
void KeyCaps_Idle(KeyCaps *keyCaps, UInt16 modifiers);

#ifdef __cplusplus
}
#endif

#endif /* KEYCAPS_H */
