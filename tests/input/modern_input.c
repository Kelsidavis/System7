#include "EventManager/EventManager.h"
#include "EventManager/EventManagerInternal.h"
#include "Platform/PS2Input.h"
#include "System71StdLib.h"
#include "check.h"

static Point backendPosition = {.h = 400, .v = 300};
static UInt8 buttons;
static UInt32 ticks = 100;
static unsigned polls;
static Boolean injectMotion;
static EventMask postedTypes[8];
static UInt32 postedMessages[8];
static Point postedPositions[8];
static unsigned postedCount;
static UInt16 keyModifiers[8];
static unsigned keyCount;
static unsigned transitionIndex;
static unsigned transitionCount;
static const UInt8 transitionCodes[] = {kScanCommand, 0, 0, kScanCommand};
static const Boolean transitionPressed[] = {true, true, false, false};

void serial_logf(SystemLogModule module, SystemLogLevel level, const char* fmt, ...)
{
    (void)module;
    (void)level;
    (void)fmt;
}

Boolean PS2_IsInitialized(void) { return true; }
Boolean InitPS2Controller(void) { return true; }
Boolean PS2_IsIRQDriven(void) { return false; }
void PollPS2Input(void) { ++polls; }
void xhci_poll_hid_x86(void) {}
SInt16 InitKeyboardEvents(void) { return noErr; }
void ShutdownKeyboardEvents(void) {}
UInt32 TickCount(void) { return ticks; }

void GetMouse(Point* position) { *position = backendPosition; }

UInt8 GetMouseButtonsLatched(void)
{
    if (injectMotion) {
        backendPosition.h = 901;
        backendPosition.v = 701;
        injectMotion = false;
    }
    return buttons;
}

Boolean GetPS2KeyboardState(KeyMap map)
{
    memset(map, 0, sizeof(KeyMap));
    return true;
}

Boolean PS2_DequeueKeyTransition(UInt8* code, Boolean* pressed)
{
    if (transitionIndex == transitionCount) return false;
    *code = transitionCodes[transitionIndex];
    *pressed = transitionPressed[transitionIndex++];
    return true;
}

SInt16 ProcessRawKeyboardEvent(UInt16 code, Boolean pressed, UInt16 modifiers, UInt32 timestamp)
{
    (void)code;
    (void)pressed;
    (void)timestamp;
    keyModifiers[keyCount++] = modifiers;
    return 1;
}

UInt32 GetKeyCharacter(UInt16 code, UInt16 modifiers)
{
    (void)code;
    (void)modifiers;
    return 0;
}

OSErr PostEvent(EventMask what, UInt32 message)
{
    postedTypes[postedCount] = what;
    postedMessages[postedCount] = message;
    GetMouse(&postedPositions[postedCount++]);
    return noErr;
}

OSErr PostEventWithModifiers(EventMask what, UInt32 message, UInt16 modifiers)
{
    (void)modifiers;
    return PostEvent(what, message);
}

int main(void)
{
    ProcessModernInput();
    CHECK(polls == 0, 1);
    CHECK(InitModernInput("PS2") == noErr, 2);
    ProcessModernInput();
    CHECK(polls == 1 && postedCount == 0, 3);

    buttons = 1;
    injectMotion = true;
    ProcessModernInput();
    CHECK(backendPosition.h == 901 && backendPosition.v == 701, 4);
    CHECK(postedCount == 1 && postedTypes[0] == mouseDown, 5);
    CHECK(postedMessages[0] == 0x10000 && gCurrentButtons == 1, 6);
    CHECK(postedPositions[0].h == 901 && postedPositions[0].v == 701, 7);

    buttons = 0;
    ++ticks;
    ProcessModernInput();
    CHECK(postedCount == 2 && postedTypes[1] == mouseUp && gCurrentButtons == 0, 8);

    buttons = 1;
    ticks += 5;
    backendPosition = (Point){.h = 400, .v = 300};
    ProcessModernInput();
    CHECK(postedCount == 3 && postedMessages[2] == 0x20000, 9);
    buttons = 0;
    ProcessModernInput();
    CHECK(postedCount == 4, 10);

    gInMouseTracking = true;
    buttons = 1;
    ticks += 5;
    ProcessModernInput();
    CHECK(postedCount == 4 && gCurrentButtons == 1, 11);
    buttons = 0;
    ProcessModernInput();
    CHECK(postedCount == 4 && gCurrentButtons == 0, 12);
    gInMouseTracking = false;
    ProcessModernInput();
    CHECK(postedCount == 4, 13);

    transitionCount = 4;
    ProcessModernInput();
    CHECK(keyCount == 4, 14);
    CHECK((keyModifiers[1] & cmdKey) && (keyModifiers[2] & cmdKey), 15);
    CHECK(!(keyModifiers[3] & cmdKey), 16);
    ShutdownModernInput();
    ProcessModernInput();
    CHECK(!IsModernInputInitialized(), 17);
    return 0;
}
