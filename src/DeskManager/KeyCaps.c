#include "MemoryMgr/MemoryManager.h"
#include <stdlib.h>
#include <string.h>
/*
 * KeyCaps.c - Key Caps Desk Accessory Implementation
 *
 * Provides a visual keyboard layout display showing all available characters
 * for the current keyboard layout. Users can see what characters are produced
 * by different key combinations and can click to insert characters.
 *
 * Derived from ROM analysis (System 7)
 */

#include "SystemTypes.h"
#include "System71StdLib.h"

#include "DeskManager/KeyCaps.h"
#include "QuickDraw/QuickDraw.h"
#include "FontManager/FontManager.h"
#include "DeskManager/DeskManager.h"
#include "TimeManager/TimeBase.h"


/* Default US keyboard layout */
static const char *g_defaultKeyLabels[KEYCAPS_MAX_KEYS] = {
    "`", "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "-", "=",
    "q", "w", "e", "r", "t", "y", "u", "i", "o", "p", "[", "]", "\\",
    "a", "s", "d", "f", "g", "h", "j", "k", "l", ";", "'",
    "z", "x", "c", "v", "b", "n", "m", ",", ".", "/"
};

/* What each of those types with Shift, in the same order */
static const char g_shiftedChars[KEYCAPS_MAX_KEYS + 1] =
    "~!@#$%^&*()_+" "QWERTYUIOP{}|" "ASDFGHJKL:\"" "ZXCVBNM<>?";

/*
 * The keyboard as drawn: four rows of character keys, the first key of each
 * row starting further right as on the real thing, with the wide keys either
 * side and the space bar below. Key rectangles are in the window's local
 * coordinates; everything is measured from these.
 */
enum {
    kKeyW = 24, kKeyH = 22, kKeyPitch = 26, kRowPitch = 24,
    kStripTop = 8, kStripBottom = 30, kKeysTop = 40, kLeft = 10,
    kRowCount = 4
};
static const SInt16 kRowStart[kRowCount] = { kLeft, kLeft + 36, kLeft + 42, kLeft + 54 };
static const SInt16 kRowKeys[kRowCount]  = { 13, 13, 11, 10 };

/* The keys that type nothing, drawn so the keyboard looks like one */
typedef struct { SInt16 row, left, right; } WideKey;
static const WideKey kWideKeys[] = {
    { 0, kLeft + 13 * kKeyPitch, kLeft + 13 * kKeyPitch + 36 },   /* delete */
    { 1, kLeft, kLeft + 34 },                                      /* tab */
    { 2, kLeft, kLeft + 40 },                                      /* caps lock */
    { 2, kLeft + 42 + 11 * kKeyPitch, kLeft + 42 + 11 * kKeyPitch + 46 }, /* return */
    { 3, kLeft, kLeft + 52 },                                      /* shift */
    { 3, kLeft + 54 + 10 * kKeyPitch, kLeft + 54 + 10 * kKeyPitch + 60 }, /* shift */
};

/*
 * Initialize Key Caps
 */
int KeyCaps_Initialize(KeyCaps *keyCaps)
{
    if (!keyCaps) {
        return KEYCAPS_ERR_INVALID_LAYOUT;
    }

    memset(keyCaps, 0, sizeof(KeyCaps));

    /* Create default keyboard layout */
    keyCaps->currentLayout = NewPtr(sizeof(KeyboardLayout));
    if (!keyCaps->currentLayout) {
        return KEYCAPS_ERR_NO_LAYOUT;
    }

    KeyboardLayout *layout = keyCaps->currentLayout;
    strncpy(layout->name, "US", sizeof(layout->name) - 1);
    layout->name[sizeof(layout->name) - 1] = '\0';
    layout->layoutID = KBD_LAYOUT_US;
    layout->scriptCode = 0;
    layout->languageCode = 0;
    layout->numKeys = 47;  /* Basic QWERTY keys */
    strncpy(layout->fontName, "Monaco", sizeof(layout->fontName) - 1);
    layout->fontName[sizeof(layout->fontName) - 1] = '\0';
    layout->fontSize = KEYCAPS_FONT_SIZE;

    /* Initialize key mappings */
    for (int i = 0; i < layout->numKeys && i < KEYCAPS_MAX_KEYS; i++) {
        KeyInfo *key = &layout->keys[i];
        key->scanCode = i;
        key->type = KEY_TYPE_NORMAL;
        strncpy(key->label, g_defaultKeyLabels[i], sizeof(key->label) - 1);
        key->baseChar = g_defaultKeyLabels[i][0];
        key->shiftChar = (key->baseChar >= 'a' && key->baseChar <= 'z') ?
                         (key->baseChar - 'a' + 'A') : key->baseChar;
        key->optionChar = key->baseChar;
        key->shiftOptionChar = key->shiftChar;
        key->isDeadKey = false;

        key->shiftChar = (UInt8)g_shiftedChars[i];
        key->shiftOptionChar = key->shiftChar;

        int row = 0, col = i;
        while (row < kRowCount - 1 && col >= kRowKeys[row]) {
            col -= kRowKeys[row];
            row++;
        }
        key->bounds.left = (SInt16)(kRowStart[row] + col * kKeyPitch);
        key->bounds.top = (SInt16)(kKeysTop + row * kRowPitch);
        key->bounds.right = (SInt16)(key->bounds.left + kKeyW);
        key->bounds.bottom = (SInt16)(key->bounds.top + kKeyH);
    }
    keyCaps->litKey = -1;

    /* Set window bounds */
    (keyCaps)->windowBounds.left = 100;
    (keyCaps)->windowBounds.top = 100;
    (keyCaps)->windowBounds.right = 500;
    (keyCaps)->windowBounds.bottom = 300;

    /* Set keyboard display area */
    (keyCaps)->windowBounds.left = 10;
    (keyCaps)->windowBounds.top = 30;
    (keyCaps)->windowBounds.right = 390;
    (keyCaps)->windowBounds.bottom = 150;

    /* Set character display area */
    (keyCaps)->windowBounds.left = 10;
    (keyCaps)->windowBounds.top = 160;
    (keyCaps)->windowBounds.right = 390;
    (keyCaps)->windowBounds.bottom = 190;

    keyCaps->showModifiers = true;
    keyCaps->showCharInfo = true;
    keyCaps->windowVisible = false;

    return KEYCAPS_ERR_NONE;
}

/*
 * Shutdown Key Caps
 */
void KeyCaps_Shutdown(KeyCaps *keyCaps)
{
    if (keyCaps) {
        DisposePtr((Ptr)keyCaps->currentLayout);
        keyCaps->currentLayout = NULL;
    }
}

/*
 * Reset Key Caps to default state
 */
void KeyCaps_Reset(KeyCaps *keyCaps)
{
    if (keyCaps) {
        keyCaps->modifiers = MOD_NONE;
        keyCaps->stickyMods = MOD_NONE;
        keyCaps->capsLockOn = false;
        keyCaps->deadKeyActive = false;
        keyCaps->selectedChar = 0;
    }
}

/*
 * Get character for key with modifiers
 */
UInt16 KeyCaps_GetCharForKey(KeyCaps *keyCaps, UInt8 scanCode,
                               ModifierMask modifiers)
{
    if (!keyCaps || !keyCaps->currentLayout || scanCode >= keyCaps->currentLayout->numKeys) {
        return 0;
    }

    const KeyInfo *key = &keyCaps->currentLayout->keys[scanCode];

    if (modifiers & MOD_SHIFT) {
        if (modifiers & MOD_OPTION) {
            return key->shiftOptionChar;
        } else {
            return key->shiftChar;
        }
    } else if (modifiers & MOD_OPTION) {
        return key->optionChar;
    } else {
        return key->baseChar;
    }
}

/*
 * Get key information by scan code
 */
const KeyInfo *KeyCaps_GetKeyInfo(KeyCaps *keyCaps, UInt8 scanCode)
{
    if (!keyCaps || !keyCaps->currentLayout || scanCode >= keyCaps->currentLayout->numKeys) {
        return NULL;
    }

    return &keyCaps->currentLayout->keys[scanCode];
}

/*
 * Set modifier key state
 */
void KeyCaps_SetModifiers(KeyCaps *keyCaps, ModifierMask modifiers)
{
    if (keyCaps) {
        keyCaps->modifiers = modifiers;
    }
}

/*
 * Toggle modifier key
 */
void KeyCaps_ToggleModifier(KeyCaps *keyCaps, ModifierMask modifier)
{
    if (keyCaps) {
        keyCaps->modifiers ^= modifier;
    }
}

/*
 * Check if modifier is active
 */
Boolean KeyCaps_IsModifierActive(KeyCaps *keyCaps, ModifierMask modifier)
{
    return keyCaps ? (keyCaps->modifiers & modifier) != 0 : false;
}

/* The character a key shows: Shift gives the shifted set, Caps Lock the
 * capital letters. */
static char KeyCaps_Label(const KeyCaps *keyCaps, const KeyInfo *key)
{
    char c = (char)key->baseChar;
    if (keyCaps->modifiers & MOD_SHIFT) {
        c = (char)key->shiftChar;
    } else if ((keyCaps->modifiers & MOD_CAPS_LOCK) && c >= 'a' && c <= 'z') {
        c = (char)(c - 'a' + 'A');
    }
    return c;
}

static void KeyCaps_DrawKey(const KeyCaps *keyCaps, int index)
{
    extern void FrameRoundRect(const Rect* r, short ovalWidth, short ovalHeight);
    const KeyInfo *key = &keyCaps->currentLayout->keys[index];
    Rect r = key->bounds;
    EraseRect(&r);
    FrameRoundRect(&r, 6, 6);
    char c = KeyCaps_Label(keyCaps, key);
    short w = TextWidth(&c, 0, 1);
    MoveTo((short)((r.left + r.right - w) / 2), (short)(r.top + 15));
    DrawText(&c, 0, 1);
    if (index == keyCaps->litKey) {
        InsetRect(&r, 1, 1);
        InvertRect(&r);
    }
}

static void KeyCaps_DrawStrip(const KeyCaps *keyCaps)
{
    Rect strip = { kStripTop, kLeft, kStripBottom, kLeft + 13 * kKeyPitch + 36 };
    EraseRect(&strip);
    FrameRect(&strip);
    /* The most recent characters that fit */
    SInt16 start = 0;
    while (start < keyCaps->typedLen &&
           TextWidth(keyCaps->typed, start, (short)(keyCaps->typedLen - start)) >
               strip.right - strip.left - 8) {
        start++;
    }
    MoveTo((short)(strip.left + 4), (short)(strip.top + 15));
    DrawText(keyCaps->typed, start, (short)(keyCaps->typedLen - start));
}

/*
 * Draw keyboard layout - the whole window, in the current port.
 *
 * This was an empty placeholder, so Key Caps opened to a blank window.
 */
void KeyCaps_DrawKeyboard(KeyCaps *keyCaps, const Rect *updateRect)
{
    extern void FrameRoundRect(const Rect* r, short ovalWidth, short ovalHeight);
    extern GrafPtr g_currentPort;
    (void)updateRect;
    if (!keyCaps || !keyCaps->currentLayout || !g_currentPort) {
        return;
    }

    Rect all = g_currentPort->portRect;
    EraseRect(&all);
    TextFont(0);
    TextSize(12);

    KeyCaps_DrawStrip(keyCaps);
    for (int i = 0; i < keyCaps->currentLayout->numKeys; i++) {
        KeyCaps_DrawKey(keyCaps, i);
    }
    for (size_t k = 0; k < sizeof(kWideKeys) / sizeof(kWideKeys[0]); k++) {
        Rect r = { (SInt16)(kKeysTop + kWideKeys[k].row * kRowPitch), kWideKeys[k].left,
                   (SInt16)(kKeysTop + kWideKeys[k].row * kRowPitch + kKeyH), kWideKeys[k].right };
        FrameRoundRect(&r, 6, 6);
    }
    /* The bottom row: option and command either side of the space bar */
    SInt16 top = (SInt16)(kKeysTop + kRowCount * kRowPitch);
    Rect space = { top, kLeft + 90, (SInt16)(top + kKeyH), kLeft + 290 };
    FrameRoundRect(&space, 6, 6);
    Rect mods[4] = {
        { top, kLeft, (SInt16)(top + kKeyH), kLeft + 36 },
        { top, kLeft + 40, (SInt16)(top + kKeyH), kLeft + 86 },
        { top, kLeft + 294, (SInt16)(top + kKeyH), kLeft + 340 },
        { top, kLeft + 344, (SInt16)(top + kKeyH), kLeft + 13 * kKeyPitch + 36 },
    };
    for (int m = 0; m < 4; m++) FrameRoundRect(&mods[m], 6, 6);
}

/* Show key `index` pressed and add its character to the strip. */
static void KeyCaps_Press(KeyCaps *keyCaps, int index, char c)
{
    if (c >= 32 && c < 127) {
        if (keyCaps->typedLen >= (SInt16)sizeof(keyCaps->typed)) {
            memmove(keyCaps->typed, keyCaps->typed + 1, sizeof(keyCaps->typed) - 1);
            keyCaps->typedLen--;
        }
        keyCaps->typed[keyCaps->typedLen++] = c;
    } else if (c == 8 && keyCaps->typedLen > 0) {
        keyCaps->typedLen--;
    }
    keyCaps->selectedChar = (UInt8)c;
    KeyCaps_DrawStrip(keyCaps);

    int old = keyCaps->litKey;
    keyCaps->litKey = (SInt16)index;
    keyCaps->litTick = TickCount();
    if (old >= 0 && old != index) KeyCaps_DrawKey(keyCaps, old);
    if (index >= 0) KeyCaps_DrawKey(keyCaps, index);
}

/*
 * Handle mouse click in Key Caps window
 */
int KeyCaps_HandleClick(KeyCaps *keyCaps, Point point, ModifierMask modifiers)
{
    (void)modifiers;
    if (!keyCaps || !keyCaps->currentLayout) {
        return KEYCAPS_ERR_NO_LAYOUT;
    }

    TextFont(0);
    TextSize(12);
    for (int i = 0; i < keyCaps->currentLayout->numKeys; i++) {
        const KeyInfo *key = &keyCaps->currentLayout->keys[i];
        if (point.h >= key->bounds.left && point.h < key->bounds.right &&
            point.v >= key->bounds.top && point.v < key->bounds.bottom) {
            KeyCaps_Press(keyCaps, i, KeyCaps_Label(keyCaps, key));
            return KEYCAPS_ERR_NONE;
        }
    }

    /* The space bar */
    SInt16 top = (SInt16)(kKeysTop + kRowCount * kRowPitch);
    if (point.v >= top && point.v < top + kKeyH &&
        point.h >= kLeft + 90 && point.h < kLeft + 290) {
        KeyCaps_Press(keyCaps, -1, ' ');
        return KEYCAPS_ERR_NONE;
    }
    return KEYCAPS_ERR_INVALID_KEY;
}

/*
 * Handle key press - found by the character it types, which is what the
 * keyboard reports; the scan code this took was compared with the key's
 * index, so no typed key was ever shown.
 */
int KeyCaps_HandleKeyPress(KeyCaps *keyCaps, UInt16 charCode,
                           ModifierMask modifiers)
{
    if (!keyCaps || !keyCaps->currentLayout) {
        return KEYCAPS_ERR_NO_LAYOUT;
    }

    keyCaps->modifiers = modifiers;
    char c = (char)charCode;
    int found = -1;
    for (int i = 0; i < keyCaps->currentLayout->numKeys && found < 0; i++) {
        const KeyInfo *key = &keyCaps->currentLayout->keys[i];
        char lower = (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
        if ((char)key->baseChar == lower || (char)key->shiftChar == c) {
            found = i;
        }
    }
    TextFont(0);
    TextSize(12);
    KeyCaps_Press(keyCaps, found, c);
    return KEYCAPS_ERR_NONE;
}

void KeyCaps_Idle(KeyCaps *keyCaps, ModifierMask modifiers)
{
    if (!keyCaps || !keyCaps->currentLayout) {
        return;
    }

    TextFont(0);
    TextSize(12);
    ModifierMask shown = (ModifierMask)(modifiers & (MOD_SHIFT | MOD_CAPS_LOCK));
    if (shown != (keyCaps->modifiers & (MOD_SHIFT | MOD_CAPS_LOCK))) {
        keyCaps->modifiers = shown;
        for (int i = 0; i < keyCaps->currentLayout->numKeys; i++) {
            KeyCaps_DrawKey(keyCaps, i);
        }
    }
    if (keyCaps->litKey >= 0 && TickCount() - keyCaps->litTick > 12) {
        int lit = keyCaps->litKey;
        keyCaps->litKey = -1;
        KeyCaps_DrawKey(keyCaps, lit);
    }
}

/*
 * Insert character into target window
 */
int KeyCaps_InsertChar(KeyCaps *keyCaps, UInt16 charCode)
{
    (void)charCode;
    if (!keyCaps) {
        return KEYCAPS_ERR_INVALID_CHAR;
    }

    /* In a real implementation, this would insert the character
     * into the active text field or document */

    return KEYCAPS_ERR_NONE;
}

/*
 * Register Key Caps as a desk accessory
 */
int KeyCaps_RegisterDA(void)
{
    /* This is handled in BuiltinDAs.c */
    return KEYCAPS_ERR_NONE;
}

/*
 * Create Key Caps DA instance
 */
DeskAccessory *KeyCaps_CreateDA(void)
{
    /* This is handled in BuiltinDAs.c */
    return NULL;
}
