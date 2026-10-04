#include "EventManager/EventManagerInternal.h"
#include "PS2Controller.h"
#include "DeskManager/KeyCaps.h"
#include "DeskManager/DeskAccessory.h"
#include "DeskManager/Calculator.h"
#include "MenuManager/menu_private.h"

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
SInt16 OpenDeskAcc(const char*);
int DA_Register(const DARegistryEntry*);
int Calculator_Initialize(Calculator*);
const DARegistryEntry* DA_GetFirstRegisteredDA(void);
short Menu_InsertSortedName(MenuHandle, ConstStr255Param, short, short);
}
