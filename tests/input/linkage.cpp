#include "EventManager/EventManagerInternal.h"
#include "PS2Controller.h"
#include "DeskManager/KeyCaps.h"

extern "C" {
OSErr Proc_PostEventWithModifiers(EventMask, UInt32, UInt16);
OSErr PostEventWithModifiers(EventMask, UInt32, UInt16);
void Event_InitQueue(void);
void UpdateMouseStateDelta(SInt16, SInt16, UInt8);
void UpdateMouseStateAbsolute(SInt16, SInt16, UInt8);
void PS2_SetIRQDriven(Boolean);
extern volatile Boolean gInMouseTracking;
int KeyCaps_HandleClick(KeyCaps*, Point, UInt16);
int KeyCaps_HandleKeyPress(KeyCaps*, UInt16, UInt16);
void KeyCaps_DrawKeyboard(KeyCaps*);
}
