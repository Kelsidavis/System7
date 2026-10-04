/* Key Caps uses the Event Manager's built-in US translation. */
#include "DeskManager/KeyCaps.h"
#include "EventManager/KeyboardEvents.h"
#include "FontManager/FontManager.h"
#include "QuickDraw/QuickDraw.h"
#include "TimeManager/TimeBase.h"
#include <string.h>

/* Virtual key codes in display order, not character or translation tables. */
static const UInt8 kKeyCodes[] = {
    0x32, 0x12, 0x13, 0x14, 0x15, 0x17, 0x16, 0x1a, 0x1c, 0x19, 0x1d, 0x1b, 0x18,
    0x0c, 0x0d, 0x0e, 0x0f, 0x11, 0x10, 0x20, 0x22, 0x1f, 0x23, 0x21, 0x1e, 0x2a,
    0x00, 0x01, 0x02, 0x03, 0x05, 0x04, 0x26, 0x28, 0x25, 0x29, 0x27,
    0x06, 0x07, 0x08, 0x09, 0x0b, 0x2d, 0x2e, 0x2b, 0x2f, 0x2c
};

enum {
    kKeyCount = sizeof(kKeyCodes) / sizeof(kKeyCodes[0]),
    kKeyW = 24, kKeyH = 22, kKeyPitch = 26, kRowPitch = 24,
    kStripTop = 8, kStripBottom = 30, kKeysTop = 40, kLeft = 10,
    kRowCount = 4,
    kLabelModifiers = shiftKey | alphaLock | optionKey
};
static const SInt16 kRowStart[kRowCount] = { kLeft, kLeft + 36, kLeft + 42, kLeft + 54 };
static const SInt16 kRowKeys[kRowCount] = { 13, 13, 11, 10 };

/* Non-character keys are outlines; the space bar accepts clicks separately. */
typedef struct { SInt16 row, left, right; } WideKey;
static const WideKey kWideKeys[] = {
    { 0, kLeft + 13 * kKeyPitch, kLeft + 13 * kKeyPitch + 36 },       /* delete */
    { 1, kLeft, kLeft + 34 },                                      /* tab */
    { 2, kLeft, kLeft + 40 },                                      /* caps lock */
    { 2, kLeft + 42 + 11 * kKeyPitch, kLeft + 42 + 11 * kKeyPitch + 46 }, /* return */
    { 3, kLeft, kLeft + 52 },                                      /* shift */
    { 3, kLeft + 54 + 10 * kKeyPitch, kLeft + 54 + 10 * kKeyPitch + 60 }, /* shift */
};

static Rect KeyCaps_KeyBounds(int index)
{
    int row = 0, col = index;
    while (row < kRowCount - 1 && col >= kRowKeys[row]) {
        col -= kRowKeys[row++];
    }
    SInt16 left = (SInt16)(kRowStart[row] + col * kKeyPitch);
    SInt16 top = (SInt16)(kKeysTop + row * kRowPitch);
    return (Rect){ top, left, (SInt16)(top + kKeyH), (SInt16)(left + kKeyW) };
}

static int KeyCaps_KeyIndex(UInt8 scanCode)
{
    for (int i = 0; i < kKeyCount; ++i) {
        if (kKeyCodes[i] == scanCode) return i;
    }
    return -1;
}

/* A local translation stream shows a dead key's standalone accent without
 * consuming either live keyboard input or a pending mouse composition. */
static UInt8 KeyCaps_Label(const KeyCaps *keyCaps, int index)
{
    UInt32 state = 0;
    UInt32 character = (UInt32)KeyTranslate(NULL,
        kKeyCodes[index] | (keyCaps->modifiers & kLabelModifiers), &state);
    if (state) character = (UInt32)KeyTranslate(NULL, kScanSpace, &state);
    return (UInt8)character;
}

static void KeyCaps_DrawKey(const KeyCaps *keyCaps, int index)
{
    if (!g_currentPort) return;
    Rect r = KeyCaps_KeyBounds(index);
    EraseRect(&r);
    FrameRoundRect(&r, 6, 6);
    UInt8 c = KeyCaps_Label(keyCaps, index);
    short width = TextWidth(&c, 0, 1);
    MoveTo((short)((r.left + r.right - width) / 2), (short)(r.top + 15));
    DrawText(&c, 0, 1);
    if (index == keyCaps->litKey) {
        InsetRect(&r, 1, 1);
        InvertRect(&r);
    }
}

static void KeyCaps_DrawStrip(const KeyCaps *keyCaps)
{
    if (!g_currentPort) return;
    Rect strip = { kStripTop, kLeft, kStripBottom, kLeft + 13 * kKeyPitch + 36 };
    EraseRect(&strip);
    FrameRect(&strip);
    SInt16 start = 0;
    while (start < keyCaps->typedLen &&
           TextWidth(keyCaps->typed, start, (short)(keyCaps->typedLen - start)) >
               strip.right - strip.left - 8) {
        ++start;
    }
    MoveTo((short)(strip.left + 4), (short)(strip.top + 15));
    DrawText(keyCaps->typed, start, (short)(keyCaps->typedLen - start));
}

static void KeyCaps_SetModifiers(KeyCaps *keyCaps, UInt16 modifiers)
{
    Boolean changed = ((keyCaps->modifiers ^ modifiers) & kLabelModifiers) != 0;
    keyCaps->modifiers = modifiers;
    if (changed) {
        for (int i = 0; i < kKeyCount; ++i) KeyCaps_DrawKey(keyCaps, i);
    }
}

int KeyCaps_Initialize(KeyCaps *keyCaps)
{
    if (!keyCaps) return KEYCAPS_ERR_INVALID_PARAM;
    memset(keyCaps, 0, sizeof(*keyCaps));
    keyCaps->litKey = -1;
    return KEYCAPS_ERR_NONE;
}

void KeyCaps_Reset(KeyCaps *keyCaps)
{
    if (!keyCaps) return;
    KeyCaps_Initialize(keyCaps);
    KeyCaps_DrawKeyboard(keyCaps);
}

void KeyCaps_DrawKeyboard(KeyCaps *keyCaps)
{
    if (!keyCaps || !g_currentPort) return;
    Rect all = g_currentPort->portRect;
    EraseRect(&all);
    TextFont(0);
    TextSize(12);
    KeyCaps_DrawStrip(keyCaps);
    for (int i = 0; i < kKeyCount; ++i) KeyCaps_DrawKey(keyCaps, i);
    for (size_t k = 0; k < sizeof(kWideKeys) / sizeof(kWideKeys[0]); ++k) {
        Rect r = { (SInt16)(kKeysTop + kWideKeys[k].row * kRowPitch), kWideKeys[k].left,
                   (SInt16)(kKeysTop + kWideKeys[k].row * kRowPitch + kKeyH), kWideKeys[k].right };
        FrameRoundRect(&r, 6, 6);
    }
    SInt16 top = (SInt16)(kKeysTop + kRowCount * kRowPitch);
    Rect space = { top, kLeft + 90, (SInt16)(top + kKeyH), kLeft + 290 };
    FrameRoundRect(&space, 6, 6);
    Rect mods[4] = {
        { top, kLeft, (SInt16)(top + kKeyH), kLeft + 36 },
        { top, kLeft + 40, (SInt16)(top + kKeyH), kLeft + 86 },
        { top, kLeft + 294, (SInt16)(top + kKeyH), kLeft + 340 },
        { top, kLeft + 344, (SInt16)(top + kKeyH), kLeft + 13 * kKeyPitch + 36 },
    };
    for (int m = 0; m < 4; ++m) FrameRoundRect(&mods[m], 6, 6);
}

static void KeyCaps_Append(KeyCaps *keyCaps, UInt8 character)
{
    if (IsCharacterPrintable(character)) {
        if (keyCaps->typedLen == (SInt16)sizeof(keyCaps->typed)) {
            memmove(keyCaps->typed, keyCaps->typed + 1, sizeof(keyCaps->typed) - 1);
            --keyCaps->typedLen;
        }
        keyCaps->typed[keyCaps->typedLen++] = (char)character;
    } else if (character == 8 && keyCaps->typedLen > 0) {
        --keyCaps->typedLen;
    }
}

static void KeyCaps_Press(KeyCaps *keyCaps, int index, UInt32 character)
{
    if (character >> 16) KeyCaps_Append(keyCaps, (UInt8)(character >> 16));
    KeyCaps_Append(keyCaps, (UInt8)character);
    KeyCaps_DrawStrip(keyCaps);
    int old = keyCaps->litKey;
    keyCaps->litKey = (SInt16)index;
    keyCaps->litTick = TickCount();
    if (old >= 0 && old != index) KeyCaps_DrawKey(keyCaps, old);
    if (index >= 0) KeyCaps_DrawKey(keyCaps, index);
}

int KeyCaps_HandleClick(KeyCaps *keyCaps, Point point, UInt16 modifiers)
{
    if (!keyCaps) return KEYCAPS_ERR_INVALID_PARAM;
    TextFont(0);
    TextSize(12);
    KeyCaps_SetModifiers(keyCaps, modifiers);
    int index = -1;
    UInt8 scanCode = kScanSpace;
    for (int i = 0; i < kKeyCount; ++i) {
        Rect bounds = KeyCaps_KeyBounds(i);
        if (point.h >= bounds.left && point.h < bounds.right &&
            point.v >= bounds.top && point.v < bounds.bottom) {
            index = i;
            scanCode = kKeyCodes[i];
            break;
        }
    }
    if (index < 0) {
        SInt16 top = (SInt16)(kKeysTop + kRowCount * kRowPitch);
        if (point.v < top || point.v >= top + kKeyH ||
            point.h < kLeft + 90 || point.h >= kLeft + 290) {
            return KEYCAPS_ERR_INVALID_KEY;
        }
    }
    UInt32 character = (UInt32)KeyTranslate(NULL, scanCode | (modifiers & kLabelModifiers),
                                          &keyCaps->clickState);
    KeyCaps_Press(keyCaps, index, character);
    return KEYCAPS_ERR_NONE;
}

int KeyCaps_HandleKeyPress(KeyCaps *keyCaps, UInt16 keyCode, UInt16 modifiers)
{
    if (!keyCaps) return KEYCAPS_ERR_INVALID_PARAM;
    if ((keyCode >> 8) >= 128) return KEYCAPS_ERR_INVALID_KEY;
    TextFont(0);
    TextSize(12);
    KeyCaps_SetModifiers(keyCaps, modifiers);
    keyCaps->clickState = 0;
    KeyCaps_Press(keyCaps, KeyCaps_KeyIndex((UInt8)(keyCode >> 8)), (UInt8)keyCode);
    return KEYCAPS_ERR_NONE;
}

void KeyCaps_Idle(KeyCaps *keyCaps, UInt16 modifiers)
{
    if (!keyCaps) return;
    TextFont(0);
    TextSize(12);
    KeyCaps_SetModifiers(keyCaps, modifiers);
    if (keyCaps->litKey >= 0 && TickCount() - keyCaps->litTick > 12) {
        int lit = keyCaps->litKey;
        keyCaps->litKey = -1;
        KeyCaps_DrawKey(keyCaps, lit);
    }
}
