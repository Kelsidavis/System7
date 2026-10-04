#include "EventManager/KeyMap.h"
#include "Finder/finder.h"
#include "check.h"

/* Feed the controller through port reads while keeping its private state in the fixture. */
#include "Platform/x86/ps2.c"

uint32_t fb_width = 800;
uint32_t fb_height = 600;
static UInt8 inputBytes[16];
static unsigned inputCount;
static unsigned inputIndex;

uint8_t hal_inb(uint16_t port)
{
    if (port == PS2_STATUS_PORT) return inputIndex < inputCount ? PS2_STATUS_OUTPUT_FULL : 0;
    if (port == PS2_DATA_PORT && inputIndex < inputCount) return inputBytes[inputIndex++];
    return 0;
}

void hal_outb(uint16_t port, uint8_t value) { (void)port; (void)value; }
Boolean hal_input_ps2_mouse_active(void) { return true; }
void FolderWindow_ScrollWheel(int8_t delta) { (void)delta; }
void serial_puts(const char* text) { (void)text; }

void serial_logf(SystemLogModule module, SystemLogLevel level, const char* format, ...)
{
    (void)module;
    (void)level;
    (void)format;
}

static void FeedScan(UInt8 scan, Boolean extended)
{
    inputIndex = 0;
    inputCount = 0;
    if (extended) inputBytes[inputCount++] = 0xe0;
    inputBytes[inputCount++] = scan;
    PollPS2Input();
}

static void ResetInput(void)
{
    ResetPS2KeyboardState();
    g_keyRingHead = 0;
    g_keyRingTail = 0;
    g_ps2Initialized = true;
    g_keyboardEnabled = true;
}

static int TestModifiers(void)
{
    static const struct {
        UInt8 scan;
        Boolean extended;
        UInt8 mac;
        UInt16 modifiers;
    } cases[] = {
        {0x2a, false, kScanShift, shiftKey},
        {0x36, false, kScanRightShift, shiftKey | rightShiftKey},
        {0x1d, false, kScanControl, controlKey},
        {0x1d, true, kScanRightControl, controlKey | rightControlKey},
        {0x38, false, kScanOption, optionKey},
        {0x38, true, kScanRightOption, optionKey | rightOptionKey},
        {0x5b, true, kScanCommand, cmdKey},
        {0x5c, true, kScanRightCommand, cmdKey}
    };
    for (unsigned i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        UInt8 code;
        Boolean pressed;
        KeyMap map;
        ResetInput();
        FeedScan(cases[i].scan, cases[i].extended);
        CHECK(GetPS2Modifiers() == cases[i].modifiers, 1);
        CHECK(GetPS2KeyboardState(map) && KeyMapHasKey(map, cases[i].mac), 2);
        CHECK(PS2_DequeueKeyTransition(&code, &pressed) && code == cases[i].mac && pressed, 3);
        FeedScan(cases[i].scan | 0x80, cases[i].extended);
        CHECK(GetPS2Modifiers() == 0, 4);
        CHECK(PS2_DequeueKeyTransition(&code, &pressed) && code == cases[i].mac && !pressed, 5);
        CHECK(GetPS2KeyboardState(map) && !KeyMapHasKey(map, cases[i].mac), 6);
    }
    ResetInput();
    FeedScan(0x2a, false);
    FeedScan(0x36, false);
    FeedScan(0xaa, false);
    CHECK(GetPS2Modifiers() == (shiftKey | rightShiftKey), 7);
    FeedScan(0xb6, false);
    CHECK(GetPS2Modifiers() == 0, 8);
    return 0;
}

static int TestCapsLockAndRepeat(void)
{
    UInt8 code;
    Boolean pressed;
    ResetInput();
    FeedScan(0x3a, false);
    FeedScan(0x3a, false);
    FeedScan(0xba, false);
    CHECK(GetPS2Modifiers() == alphaLock, 1);
    CHECK(PS2_DequeueKeyTransition(&code, &pressed) && code == kScanCapsLock && pressed, 2);
    CHECK(PS2_DequeueKeyTransition(&code, &pressed) && code == kScanCapsLock && !pressed, 3);
    CHECK(!PS2_DequeueKeyTransition(&code, &pressed), 4);
    FeedScan(0x3a, false);
    FeedScan(0xba, false);
    CHECK(GetPS2Modifiers() == 0, 5);

    ResetInput();
    FeedScan(0x1e, false);
    CHECK(!PS2_DequeueKeyTransition(NULL, &pressed), 6);
    CHECK(!PS2_DequeueKeyTransition(&code, NULL), 7);
    CHECK(PS2_DequeueKeyTransition(&code, &pressed) && code == 0 && pressed, 8);
    FeedScan(0x1e, false);
    FeedScan(0x9e, false);
    CHECK(PS2_DequeueKeyTransition(&code, &pressed) && code == 0 && !pressed, 9);
    CHECK(!PS2_DequeueKeyTransition(&code, &pressed), 10);
    return 0;
}

static int TestPrefixesAndWrap(void)
{
    UInt8 code;
    Boolean pressed;
    ResetInput();
    /* Ignore the complete Pause sequence without treating its bytes as modifiers. */
    static const UInt8 pause[] = {0xe1, 0x1d, 0x45, 0xe1, 0x9d, 0xc5};
    inputIndex = 0;
    inputCount = sizeof(pause);
    memcpy(inputBytes, pause, sizeof(pause));
    PollPS2Input();
    CHECK(GetPS2Modifiers() == 0 && !PS2_DequeueKeyTransition(&code, &pressed), 1);
    for (unsigned i = 0; i < sizeof(pause); ++i) FeedScan(pause[i], false);
    CHECK(GetPS2Modifiers() == 0 && !PS2_DequeueKeyTransition(&code, &pressed), 4);

    /* Prefixes may arrive in separate polls; Print Screen's fake Shift bytes are ignored. */
    FeedScan(0xe0, false);
    FeedScan(0x1d, false);
    CHECK(GetPS2Modifiers() == (controlKey | rightControlKey), 5);
    CHECK(PS2_DequeueKeyTransition(&code, &pressed) && code == kScanRightControl && pressed, 6);
    FeedScan(0xe0, false);
    FeedScan(0x9d, false);
    CHECK(GetPS2Modifiers() == 0, 7);
    CHECK(PS2_DequeueKeyTransition(&code, &pressed) && code == kScanRightControl && !pressed, 8);
    static const UInt8 printScreen[] = {0xe0, 0x2a, 0xe0, 0x37, 0xe0, 0xb7, 0xe0, 0xaa};
    for (unsigned i = 0; i < sizeof(printScreen); ++i) FeedScan(printScreen[i], false);
    CHECK(GetPS2Modifiers() == 0, 9);
    CHECK(PS2_DequeueKeyTransition(&code, &pressed) && code == 0x69 && pressed, 10);
    CHECK(PS2_DequeueKeyTransition(&code, &pressed) && code == 0x69 && !pressed, 11);
    CHECK(!PS2_DequeueKeyTransition(&code, &pressed), 12);
    for (unsigned i = 0; i < 100; ++i) {
        FeedScan(0x1e, false);
        FeedScan(0x9e, false);
        CHECK(PS2_DequeueKeyTransition(&code, &pressed) && code == 0 && pressed, 2);
        CHECK(PS2_DequeueKeyTransition(&code, &pressed) && code == 0 && !pressed, 3);
    }
    return 0;
}

int main(void)
{
    int result = TestModifiers();
    result |= TestCapsLockAndRepeat();
    result |= TestPrefixesAndWrap();
    return result;
}
