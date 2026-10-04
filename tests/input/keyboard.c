#include "EventManager/KeyboardEvents.h"
#include "EventManager/EventManager.h"
#include "EventManager/AppSwitcher.h"
#include "DeskManager/KeyCaps.h"
#include "QuickDraw/QuickDraw.h"
#include "FontManager/FontManager.h"
#include "check.h"
#include <string.h>

static EventRecord posted[8];
static unsigned postedCount;
static UInt32 currentTick = 100;
static GrafPort drawingPort;
GrafPtr g_currentPort = &drawingPort;
static SInt16 penV;
static UInt8 labels[47];
static unsigned labelCount;
static unsigned invertedCount;
static const char keyCapsPlain[] = "`1234567890-=qwertyuiop[]\\asdfghjkl;'zxcvbnm,./";
static const char keyCapsShifted[] = "~!@#$%^&*()_+QWERTYUIOP{}|ASDFGHJKL:\"ZXCVBNM<>?";

UInt32 TickCount(void) { return currentTick; }
void TextFont(short font) { (void)font; }
void TextSize(short size) { (void)size; }
short TextWidth(const void* text, short first, short count)
{
    (void)text;
    (void)first;
    return (short)(count * 6);
}
void MoveTo(SInt16 h, SInt16 v) { (void)h; penV = v; }
void DrawText(const void* text, short first, short count)
{
    if (penV >= 55 && penV <= 127 && count == 1 && labelCount < sizeof(labels)) {
        labels[labelCount++] = ((const UInt8*)text)[first];
    }
}
void EraseRect(const Rect* rect) { (void)rect; }
void FrameRect(const Rect* rect) { (void)rect; }
void FrameRoundRect(const Rect* rect, SInt16 width, SInt16 height)
{
    (void)rect;
    (void)width;
    (void)height;
}
void InsetRect(Rect* rect, short dh, short dv)
{
    rect->left += dh;
    rect->right -= dh;
    rect->top += dv;
    rect->bottom -= dv;
}
void InvertRect(const Rect* rect) { (void)rect; ++invertedCount; }
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

static int TestKeyCapsLabels(void)
{
    KeyCaps caps;
    CHECK(KeyCaps_Initialize(&caps) == 0 && caps.litKey == -1, 1);
    labelCount = 0;
    KeyCaps_DrawKeyboard(&caps);
    CHECK(labelCount == sizeof(labels) && memcmp(labels, keyCapsPlain, sizeof(labels)) == 0, 2);
    labelCount = 0;
    KeyCaps_Idle(&caps, shiftKey);
    CHECK(labelCount == sizeof(labels) && memcmp(labels, keyCapsShifted, sizeof(labels)) == 0, 3);
    labelCount = 0;
    KeyCaps_Idle(&caps, shiftKey | alphaLock);
    for (unsigned i = 0; i < sizeof(labels); ++i) {
        UInt8 expected = keyCapsPlain[i] >= 'a' && keyCapsPlain[i] <= 'z' ?
                         (UInt8)keyCapsPlain[i] : (UInt8)keyCapsShifted[i];
        CHECK(labels[i] == expected, 4);
    }
    labelCount = 0;
    KeyCaps_Idle(&caps, optionKey);
    CHECK(labelCount == sizeof(labels), 5);
    CHECK(labels[0] == '`' && labels[15] == 0xab && labels[20] == 0xf6 &&
          labels[19] == 0xac && labels[42] == 0xf7, 6);
    CHECK(caps.typedLen == 0 && caps.clickState == 0, 7);
    ResetKeyboardState();
    postedCount = 0;
    CHECK(ProcessRawKeyboardEvent(0x0e, true, optionKey, 100) == 0, 8);
    KeyCaps_DrawKeyboard(&caps);
    CHECK(ProcessRawKeyboardEvent(0, true, 0, 100) == 1, 9);
    CHECK(postedCount == 1 && posted[0].message == 0x87, 10);
    ResetKeyboardState();
    return 0;
}

static int TestKeyCapsEveryKey(void)
{
    static const SInt16 starts[] = {10, 46, 52, 64};
    static const unsigned lengths[] = {13, 13, 11, 10};
    static const UInt16 modifiers[] = {0, shiftKey, alphaLock, shiftKey | alphaLock};
    KeyCaps caps;
    for (unsigned mode = 0; mode < sizeof(modifiers) / sizeof(modifiers[0]); ++mode) {
        unsigned index = 0;
        for (unsigned row = 0; row < sizeof(lengths) / sizeof(lengths[0]); ++row) {
            for (unsigned col = 0; col < lengths[row]; ++col, ++index) {
                KeyCaps_Initialize(&caps);
                Point point = {.h = (SInt16)(starts[row] + 26 * col), .v = (SInt16)(40 + 24 * row)};
                CHECK(KeyCaps_HandleClick(&caps, point, modifiers[mode]) == 0, 1);
                char expected = modifiers[mode] & shiftKey ? keyCapsShifted[index] : keyCapsPlain[index];
                if ((modifiers[mode] & alphaLock) && keyCapsPlain[index] >= 'a' && keyCapsPlain[index] <= 'z') {
                    expected = modifiers[mode] & shiftKey ? keyCapsPlain[index] : keyCapsShifted[index];
                }
                CHECK(caps.typedLen == 1 && caps.typed[0] == expected && caps.litKey == (SInt16)index, 2);
            }
        }
        CHECK(index == sizeof(labels), 3);
    }
    return 0;
}

static int TestKeyCapsInput(void)
{
    KeyCaps caps;
    KeyCaps_Initialize(&caps);
    Point a = {.h = 53, .v = 89};
    Point e = {.h = 99, .v = 65};
    Point n = {.h = 195, .v = 113};
    Point space = {.h = 150, .v = 140};
    CHECK(KeyCaps_HandleClick(&caps, a, shiftKey | alphaLock) == 0, 1);
    CHECK(caps.typedLen == 1 && caps.typed[0] == 'a' && caps.litKey == 26, 2);
    CHECK(KeyCaps_HandleClick(&caps, a, shiftKey) == 0 && caps.typed[1] == 'A', 3);
    CHECK(KeyCaps_HandleClick(&caps, e, optionKey) == 0, 4);
    CHECK(caps.clickState == kDeadKeyAcute && caps.typedLen == 2 && caps.litKey == 15, 5);
    KeyCaps_Idle(&caps, 0);
    CHECK(caps.clickState == kDeadKeyAcute, 6);
    CHECK(KeyCaps_HandleClick(&caps, a, 0) == 0 && (UInt8)caps.typed[2] == 0x87, 7);
    CHECK(caps.clickState == 0 && caps.typedLen == 3, 8);
    KeyCaps_HandleClick(&caps, e, optionKey);
    KeyCaps_HandleClick(&caps, n, 0);
    CHECK(caps.typedLen == 5 && (UInt8)caps.typed[3] == 0xab && caps.typed[4] == 'n', 9);
    KeyCaps_HandleClick(&caps, e, optionKey);
    KeyCaps_HandleClick(&caps, space, 0);
    CHECK(caps.typedLen == 6 && (UInt8)caps.typed[5] == 0xab && caps.litKey == -1, 10);
    SInt16 count = caps.typedLen;
    CHECK(KeyCaps_HandleClick(&caps, (Point){.h = 34, .v = 40}, 0) == KEYCAPS_ERR_INVALID_KEY, 11);
    CHECK(caps.typedLen == count, 12);
    KeyCaps_HandleClick(&caps, e, optionKey);
    invertedCount = 0;
    CHECK(KeyCaps_HandleKeyPress(&caps, 0x0e8e, 0) == 0, 13);
    CHECK(caps.clickState == 0 && caps.litKey == 15 && invertedCount > 0, 14);
    CHECK((UInt8)caps.typed[caps.typedLen - 1] == 0x8e, 15);
    KeyCaps_HandleKeyPress(&caps, 0x3308, 0);
    CHECK(caps.typedLen == count && caps.litKey == -1, 16);
    CHECK(KeyCaps_HandleKeyPress(&caps, 0x80ff, 0) == KEYCAPS_ERR_INVALID_KEY, 17);
    KeyCaps_Reset(&caps);
    CHECK(caps.typedLen == 0 && caps.litKey == -1 && caps.modifiers == 0 && caps.clickState == 0, 18);
    for (unsigned i = 0; i < 100; ++i) KeyCaps_HandleKeyPress(&caps, (UInt16)(0x1200 | (32 + i)), 0);
    CHECK(caps.typedLen == 64, 19);
    /* DEL is ignored; the retained bytes are the latest 64 printable inputs. */
    CHECK((UInt8)caps.typed[0] == 67 && (UInt8)caps.typed[63] == 131, 20);
    currentTick = 0xfffffff8;
    KeyCaps_HandleKeyPress(&caps, 0x0061, 0);
    currentTick = 4;
    KeyCaps_Idle(&caps, 0);
    CHECK(caps.litKey == 26, 21);
    currentTick = 5;
    KeyCaps_Idle(&caps, 0);
    CHECK(caps.litKey == -1, 22);
    currentTick = 100;
    g_currentPort = NULL;
    KeyCaps_HandleKeyPress(&caps, 0x0061, shiftKey);
    KeyCaps_Reset(&caps);
    g_currentPort = &drawingPort;
    CHECK(KeyCaps_Initialize(NULL) == KEYCAPS_ERR_INVALID_PARAM, 23);
    CHECK(KeyCaps_HandleClick(NULL, a, 0) == KEYCAPS_ERR_INVALID_PARAM, 24);
    CHECK(KeyCaps_HandleKeyPress(NULL, 0, 0) == KEYCAPS_ERR_INVALID_PARAM, 25);
    KeyCaps_Reset(NULL);
    KeyCaps_Idle(NULL, 0);
    KeyCaps_DrawKeyboard(NULL);
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
    result |= TestKeyCapsLabels();
    result |= TestKeyCapsEveryKey();
    result |= TestKeyCapsInput();
    ShutdownKeyboardEvents();
    return result;
}
