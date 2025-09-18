/*
 * KeyCaps.c - Key Caps Desk Accessory Implementation
 *
 * Provides a visual keyboard layout display showing all available characters
 * for the current keyboard layout. Users can see what characters are produced
 * by different key combinations and can click to insert characters.
 *
 * Based on Apple's Key Caps DA from System 7.1
 */

#include "DeskManager/KeyCaps.h"
#include "DeskManager/DeskManager.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Default US keyboard layout */
static const char *g_defaultKeyLabels[KEYCAPS_MAX_KEYS] = {
    "`", "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "-", "=",
    "q", "w", "e", "r", "t", "y", "u", "i", "o", "p", "[", "]", "\\",
    "a", "s", "d", "f", "g", "h", "j", "k", "l", ";", "'",
    "z", "x", "c", "v", "b", "n", "m", ",", ".", "/"
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
    keyCaps->currentLayout = malloc(sizeof(KeyboardLayout));
    if (!keyCaps->currentLayout) {
        return KEYCAPS_ERR_NO_LAYOUT;
    }

    KeyboardLayout *layout = keyCaps->currentLayout;
    strcpy(layout->name, "US");
    layout->layoutID = KBD_LAYOUT_US;
    layout->scriptCode = 0;
    layout->languageCode = 0;
    layout->numKeys = 47;  /* Basic QWERTY keys */
    strcpy(layout->fontName, "Monaco");
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

        /* Set key rectangle (simplified layout) */
        key->rect.left = (i % 13) * 30 + 10;
        key->rect.top = (i / 13) * 25 + 10;
        key->rect.right = key->rect.left + 25;
        key->rect.bottom = key->rect.top + 20;
    }

    /* Set window bounds */
    keyCaps->windowBounds.left = 100;
    keyCaps->windowBounds.top = 100;
    keyCaps->windowBounds.right = 500;
    keyCaps->windowBounds.bottom = 300;

    /* Set keyboard display area */
    keyCaps->keyboardRect.left = 10;
    keyCaps->keyboardRect.top = 30;
    keyCaps->keyboardRect.right = 390;
    keyCaps->keyboardRect.bottom = 150;

    /* Set character display area */
    keyCaps->charDisplayRect.left = 10;
    keyCaps->charDisplayRect.top = 160;
    keyCaps->charDisplayRect.right = 390;
    keyCaps->charDisplayRect.bottom = 190;

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
        free(keyCaps->currentLayout);
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
uint16_t KeyCaps_GetCharForKey(KeyCaps *keyCaps, uint8_t scanCode,
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
const KeyInfo *KeyCaps_GetKeyInfo(KeyCaps *keyCaps, uint8_t scanCode)
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
bool KeyCaps_IsModifierActive(KeyCaps *keyCaps, ModifierMask modifier)
{
    return keyCaps ? (keyCaps->modifiers & modifier) != 0 : false;
}

/*
 * Draw keyboard layout
 */
void KeyCaps_DrawKeyboard(KeyCaps *keyCaps, const Rect *updateRect)
{
    if (!keyCaps || !keyCaps->currentLayout) {
        return;
    }

    /* In a real implementation, this would draw the keyboard layout */
    /* For now, just a placeholder */
}

/*
 * Handle mouse click in Key Caps window
 */
int KeyCaps_HandleClick(KeyCaps *keyCaps, Point point, ModifierMask modifiers)
{
    if (!keyCaps || !keyCaps->currentLayout) {
        return KEYCAPS_ERR_NO_LAYOUT;
    }

    /* Find which key was clicked */
    for (int i = 0; i < keyCaps->currentLayout->numKeys; i++) {
        const KeyInfo *key = &keyCaps->currentLayout->keys[i];
        if (point.h >= key->rect.left && point.h < key->rect.right &&
            point.v >= key->rect.top && point.v < key->rect.bottom) {

            /* Get character for this key */
            uint16_t charCode = KeyCaps_GetCharForKey(keyCaps, key->scanCode,
                                                      keyCaps->modifiers);
            keyCaps->selectedChar = charCode;

            /* Insert character if in insert mode */
            if (keyCaps->insertMode) {
                KeyCaps_InsertChar(keyCaps, charCode);
            }

            return KEYCAPS_ERR_NONE;
        }
    }

    return KEYCAPS_ERR_INVALID_KEY;
}

/*
 * Handle key press
 */
int KeyCaps_HandleKeyPress(KeyCaps *keyCaps, uint8_t scanCode,
                           ModifierMask modifiers)
{
    if (!keyCaps) {
        return KEYCAPS_ERR_NO_LAYOUT;
    }

    /* Update modifier state */
    keyCaps->modifiers = modifiers;

    /* Get character for this key */
    uint16_t charCode = KeyCaps_GetCharForKey(keyCaps, scanCode, modifiers);
    keyCaps->selectedChar = charCode;

    return KEYCAPS_ERR_NONE;
}

/*
 * Insert character into target window
 */
int KeyCaps_InsertChar(KeyCaps *keyCaps, uint16_t charCode)
{
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