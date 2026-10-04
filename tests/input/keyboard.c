#include "EventManager/KeyboardEvents.h"
#include "EventManager/EventManager.h"
#include "EventManager/AppSwitcher.h"
#include "check.h"
#include <string.h>

static EventRecord posted[8];
static unsigned postedCount;

UInt32 TickCount(void) { return 100; }
void GetMouse(Point* point) { *point = (Point){.h = 123, .v = 234}; }
void AppSwitcher_CycleForward(void) {}
void AppSwitcher_CycleBackward(void) {}
void AppSwitcher_HandleKeyUp(void) {}
Boolean AppSwitcher_IsActive(void) { return false; }

OSErr PostEventWithModifiers(EventMask what, UInt32 message, UInt16 modifiers)
{
    if (postedCount < sizeof(posted) / sizeof(posted[0])) {
        posted[postedCount] = (EventRecord){.what = what, .message = message, .modifiers = modifiers};
    }
    ++postedCount;
    return noErr;
}

OSErr PostEvent(EventMask what, UInt32 message)
{
    return PostEventWithModifiers(what, message, GetModifierState());
}

static int TestTranslation(void)
{
    UInt32 state = 0;
    CHECK(KeyTranslate(NULL, shiftKey, &state) == 'A', 1);
    CHECK(KeyTranslate(NULL, alphaLock, &state) == 'A', 2);
    CHECK(KeyTranslate(NULL, alphaLock | shiftKey, &state) == 'a', 3);
    KeyTransState stream = {0};
    CHECK(TranslateScanCode(0, shiftKey, &stream) == 'A', 4);
    CHECK(TranslateScanCode(128, shiftKey, &stream) == 0, 5);
    CHECK(GetKeyCharacter(kScanLeftArrow, shiftKey) == 0x1c, 6);
    for (UInt16 code = 64; code < 128; ++code) {
        CHECK(GetKeyCharacter(code, shiftKey) == GetKeyCharacter(code, 0), 7);
    }
    return 0;
}

static int TestDeadKeys(void)
{
    UInt32 first = 0;
    UInt32 second = 0;
    CHECK(KeyTranslate(NULL, 0x0e | optionKey, &first) == 0 && first == kDeadKeyAcute, 1);
    CHECK(KeyTranslate(NULL, 0, &second) == 'a' && second == 0, 2);
    UInt32 saved = first;
    CHECK(KeyTranslate(NULL, 0x8e, &first) == 'e' && first == saved, 3);
    CHECK(KeyTranslate(NULL, 0, &first) == 0x87 && first == 0, 4);
    CHECK(ProcessDeadKey(0x0e, 'e') == 0x8e, 5);
    CHECK(KeyTranslate(NULL, 0x0e | optionKey, &first) == 0, 6);
    CHECK(KeyTranslate(NULL, 0x2d, &first) == (0xab0000 | 'n') && first == 0, 7);
    CHECK(KeyTranslate(NULL, 0x0e | optionKey, &first) == 0, 8);
    CHECK(KeyTranslate(NULL, kScanSpace, &first) == 0xab && first == 0, 9);
    CHECK(GetDeadKeyType(0x18, optionKey) == kDeadKeyNone, 10); /* Option-equal is not Option-E. */
    KeyTransState stream = {0};
    CHECK(TranslateScanCode(0x22, optionKey, &stream) == 0 && stream.state == kDeadKeyCircumflex, 11);
    CHECK(TranslateScanCode(0x0e, 0, &stream) == 0x90 && stream.state == 0, 12);
    CHECK(TranslateScanCode(0x22, optionKey, &stream) == 0, 15);
    CHECK(TranslateScanCode(kScanSpace, 0, &stream) == 0xf6, 16);
    CHECK(TranslateScanCode(0x2d, optionKey, &stream) == 0, 17);
    CHECK(TranslateScanCode(kScanSpace, 0, &stream) == 0xf7, 18);
    CHECK(KeyTranslate(NULL, 0x0e | optionKey, &first) == 0, 13);
    ResetDeadKeyState();
    CHECK(first == kDeadKeyAcute, 14); /* Resetting live input does not reset caller-owned streams. */
    return 0;
}

static int TestComposition(void)
{
    static const UInt8 expected[4][10] = {
        {0x87, 0x8e, 0x92, 0x97, 0x9c, 0xe7, 0x83, 0xea, 0xee, 0xf2},
        {0x88, 0x8f, 0x93, 0x98, 0x9d, 0xcb, 0xe9, 0xed, 0xf1, 0xf4},
        {0x89, 0x90, 0x94, 0x99, 0x9e, 0xe5, 0xe6, 0xeb, 0xef, 0xf3},
        {0x8a, 0x91, 0x95, 0x9a, 0x9f, 0x80, 0xe8, 0xec, 0x85, 0x86}
    };
    static const char vowels[] = "aeiouAEIOU";
    for (unsigned accent = 0; accent < 4; ++accent) {
        for (unsigned letter = 0; letter < 10; ++letter) {
            CHECK(ComposeCharacter(vowels[letter], accent + 1) == expected[accent][letter], 1);
        }
    }
    CHECK(ComposeCharacter('n', kDeadKeyTilde) == 0x96 && ComposeCharacter('N', kDeadKeyTilde) == 0x84, 2);
    CHECK(ComposeCharacter('a', kDeadKeyTilde) == 0x8b && ComposeCharacter('A', kDeadKeyTilde) == 0xcc, 3);
    CHECK(ComposeCharacter('o', kDeadKeyTilde) == 0x9b && ComposeCharacter('O', kDeadKeyTilde) == 0xcd, 4);
    CHECK(ComposeCharacter('x', kDeadKeyAcute) == 'x', 5);
    CHECK(ComposeCharacter('y', kDeadKeyUmlaut) == 0xd8 && ComposeCharacter('Y', kDeadKeyUmlaut) == 0xd9, 6);
    return 0;
}

static int TestRawComposition(void)
{
    ResetKeyboardState();
    postedCount = 0;
    CHECK(ProcessRawKeyboardEvent(0x0e, true, optionKey, 100) == 0 && postedCount == 0, 1);
    UInt32 independent = 0;
    CHECK(KeyTranslate(NULL, 0, &independent) == 'a' && independent == 0, 6);
    ProcessRawKeyboardEvent(0x0e, false, optionKey, 100);
    postedCount = 0;
    CHECK(ProcessRawKeyboardEvent(0x0e, true, 0, 100) == 1, 2);
    CHECK(postedCount == 1 && posted[0].message == 0x0e8e, 3);
    ResetKeyboardState();
    postedCount = 0;
    ProcessRawKeyboardEvent(0x0e, true, optionKey, 100);
    CHECK(ProcessRawKeyboardEvent(0x2d, true, 0, 100) == 2, 4);
    CHECK(postedCount == 2 && posted[0].message == 0x2dab && posted[1].message == 0x2d6e, 5);
    return 0;
}

static int TestEventBuilders(void)
{
    EventRecord events[] = {
        GenerateKeyDownEvent(0x12ff, 0xabcd, shiftKey),
        GenerateKeyUpEvent(0x12ff, 0xabcd, shiftKey),
        GenerateAutoKeyEvent(0x12ff, 0xabcd, shiftKey)
    };
    for (unsigned i = 0; i < sizeof(events) / sizeof(events[0]); ++i) {
        CHECK(events[i].message == 0xffcd && events[i].when == 100, 1);
        CHECK(events[i].where.h == 123 && events[i].where.v == 234, 2);
        CHECK(events[i].modifiers == shiftKey, 3);
    }
    CHECK(events[0].what == keyDown && events[1].what == keyUp && events[2].what == autoKey, 4);
    return 0;
}

static int TestNamesAndPrintable(void)
{
    char guards[] = {'x', 'x', 'x'};
    CHECK(GetKeyName(0, 0, guards + 1, 1) == 0, 1);
    CHECK(guards[0] == 'x' && guards[1] == 0 && guards[2] == 'x', 2);
    CHECK(GetKeyName(kScanReturn, 0, guards + 1, 1) == 0, 3);
    CHECK(guards[2] == 'x', 4);
    CHECK(GetKeyName(0, 0, NULL, 1) == 0, 5);
    for (UInt16 code = 0; code < 128; ++code) {
        char onlyByte = 'x';
        CHECK(GetKeyName(code, shiftKey, &onlyByte, 1) == 0 && onlyByte == 0, 9);
    }
    char buffer[32];
    CHECK(GetKeyName(kScanRightCommand, 0, buffer, sizeof(buffer)) == 13, 6);
    CHECK(strcmp(buffer, "Right Command") == 0, 7);
    for (UInt32 code = 0; code < 258; ++code) {
        CHECK(IsCharacterPrintable(code) == (code >= 32 && code <= 255 && code != 127), 8);
    }
    return 0;
}

static int TestAbortChord(void)
{
    ResetKeyboardState();
    CHECK(!CheckAbort(), 1);
    ProcessRawKeyboardEvent(0x2f, true, 0, 100);
    CHECK(!CheckAbort(), 2);
    ProcessRawKeyboardEvent(kScanCommand, true, cmdKey, 100);
    CHECK(CheckAbort(), 3);
    ProcessRawKeyboardEvent(kScanRightCommand, true, cmdKey, 100);
    ProcessRawKeyboardEvent(kScanCommand, false, cmdKey, 100);
    CHECK(CheckAbort(), 4);
    ProcessRawKeyboardEvent(0x2f, false, cmdKey, 100);
    CHECK(!CheckAbort(), 5);
    ResetKeyboardState();
    CHECK(!CheckAbort(), 6);
    return 0;
}

int main(void)
{
    CHECK(InitKeyboardEvents() == noErr, 1);
    int result = TestTranslation();
    result |= TestDeadKeys();
    result |= TestComposition();
    result |= TestRawComposition();
    result |= TestEventBuilders();
    result |= TestNamesAndPrintable();
    result |= TestAbortChord();
    ShutdownKeyboardEvents();
    return result;
}
