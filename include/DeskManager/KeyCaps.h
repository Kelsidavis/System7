#ifndef KEYCAPS_H
#define KEYCAPS_H

#include "SystemTypes.h"

/* Forward declarations */


/*
 * KeyCaps.h - Key Caps Desk Accessory
 *
 * Provides a visual keyboard layout display showing all available characters
 * for the current keyboard layout. Users can see what characters are produced
 * by different key combinations and can click to insert characters.
 *
 * Derived from ROM analysis (System 7)
 */

#include "DeskAccessory.h"

/* Key Caps Constants */
#define KEYCAPS_VERSION         0x0100      /* Key Caps version 1.0 */
#define KEYCAPS_MAX_CHARS       65536       /* Maximum character codes (Unicode) */
#define KEYCAPS_MAX_KEYS        128         /* Maximum physical keys */
#define KEYCAPS_FONT_SIZE       9           /* Default font size */

/* Keyboard Layout Constants */
#define KBD_LAYOUT_US           0           /* US layout */
#define KBD_LAYOUT_INTERNATIONAL 1          /* International layout */
#define KBD_LAYOUT_DVORAK       2           /* Dvorak layout */
#define KBD_LAYOUT_CUSTOM       255         /* Custom layout */

/* Modifier Key Masks */
typedef enum {
    MOD_NONE = 0x0000,
    MOD_SHIFT = 0x0001,
    MOD_CONTROL = 0x0002,
    MOD_OPTION = 0x0004,
    MOD_COMMAND = 0x0008,
    MOD_CAPS_LOCK = 0x0100
} ModifierMask;

/* Key Types */
typedef enum {
    KEY_TYPE_NORMAL = 0,
    KEY_TYPE_MODIFIER = 1,
    KEY_TYPE_FUNCTION = 2,
    KEY_TYPE_DEADKEY = 3
} KeyType;

/* Key Information */
typedef struct KeyInfo {
    UInt8 scanCode;
    KeyType type;
    char label[32];
    UInt16 baseChar;
    UInt16 shiftChar;
    UInt16 optionChar;
    UInt16 shiftOptionChar;
    Boolean isDeadKey;
    Rect bounds;
} KeyInfo;

/* Keyboard Layout */
typedef struct KeyboardLayout {
    char name[32];
    UInt16 layoutID;
    UInt8 scriptCode;
    UInt8 languageCode;
    SInt16 numKeys;
    char fontName[32];
    SInt16 fontSize;
    KeyInfo keys[128];
} KeyboardLayout;

/* Character Information */
typedef struct CharInfo {
    UInt16 charCode;
    char displayChar;
    Boolean isDeadKey;
    Boolean isPrintable;
    UInt16 deadKeyCombinations[16];
    SInt16 combinationCount;
} CharInfo;

/* Key Caps State */
typedef struct KeyCaps {
    Rect windowBounds;
    KeyboardLayout *currentLayout;
    ModifierMask modifiers;
    ModifierMask stickyMods;
    Boolean capsLockOn;
    Boolean deadKeyActive;
    UInt16 selectedChar;
    Boolean showModifiers;
    Boolean showCharInfo;
    Boolean windowVisible;
    Boolean insertMode;
    Rect keyboardDisplayRect;
    Rect charDisplayRect;
    char typed[64];             /* the characters typed or clicked, shown on top */
    SInt16 typedLen;
    SInt16 litKey;              /* index of the key shown pressed, or -1 */
    UInt32 litTick;             /* when it was pressed */
} KeyCaps;

/* Key Caps Functions */

/**
 * Initialize Key Caps
 * @param keyCaps Pointer to Key Caps structure
 * @return 0 on success, negative on error
 */
int KeyCaps_Initialize(KeyCaps *keyCaps);

/**
 * Shutdown Key Caps
 * @param keyCaps Pointer to Key Caps structure
 */
void KeyCaps_Shutdown(KeyCaps *keyCaps);

/**
 * Reset Key Caps to default state
 * @param keyCaps Pointer to Key Caps structure
 */
void KeyCaps_Reset(KeyCaps *keyCaps);

/* Keyboard Layout Functions */


/**
 * Create custom keyboard layout
 * @param name Layout name
 * @param baseLayout Base layout to copy from
 * @return Pointer to new layout or NULL on error
 */
KeyboardLayout *KeyCaps_CreateCustomLayout(const char *name,
                                           KeyboardLayout *baseLayout);

/* Key Mapping Functions */

/**
 * Get character for key with modifiers
 * @param keyCaps Pointer to Key Caps structure
 * @param scanCode Key scan code
 * @param modifiers Modifier keys
 * @return Character code or 0 if none
 */
UInt16 KeyCaps_GetCharForKey(KeyCaps *keyCaps, UInt8 scanCode,
                               ModifierMask modifiers);

/**
 * Get key information by scan code
 * @param keyCaps Pointer to Key Caps structure
 * @param scanCode Key scan code
 * @return Pointer to key info or NULL if not found
 */
const KeyInfo *KeyCaps_GetKeyInfo(KeyCaps *keyCaps, UInt8 scanCode);

/**
 * Find key by character
 * @param keyCaps Pointer to Key Caps structure
 * @param charCode Character code
 * @param modifiers Pointer to required modifiers (output)
 * @return Scan code or 0 if not found
 */
UInt8 KeyCaps_FindKeyForChar(KeyCaps *keyCaps, UInt16 charCode,
                               ModifierMask *modifiers);

/* Modifier Key Functions */

/**
 * Set modifier key state
 * @param keyCaps Pointer to Key Caps structure
 * @param modifiers Modifier mask
 */
void KeyCaps_SetModifiers(KeyCaps *keyCaps, ModifierMask modifiers);

/**
 * Toggle modifier key
 * @param keyCaps Pointer to Key Caps structure
 * @param modifier Modifier to toggle
 */
void KeyCaps_ToggleModifier(KeyCaps *keyCaps, ModifierMask modifier);

/**
 * Check if modifier is active
 * @param keyCaps Pointer to Key Caps structure
 * @param modifier Modifier to check
 * @return true if modifier is active
 */
Boolean KeyCaps_IsModifierActive(KeyCaps *keyCaps, ModifierMask modifier);

/* Dead Key Functions */

/**
 * Process dead key input
 * @param keyCaps Pointer to Key Caps structure
 * @param deadKeyChar Dead key character
 * @param nextChar Next character typed
 * @return Combined character or 0 if no combination
 */
UInt16 KeyCaps_ProcessDeadKey(KeyCaps *keyCaps, UInt16 deadKeyChar,
                                UInt16 nextChar);


/**
 * Get dead key combinations
 * @param keyCaps Pointer to Key Caps structure
 * @param deadKeyChar Dead key character
 * @param combinations Array to fill with combinations
 * @param maxCombinations Maximum number of combinations
 * @return Number of combinations returned
 */
int KeyCaps_GetDeadKeyCombinations(KeyCaps *keyCaps, UInt16 deadKeyChar,
                                   UInt16 *combinations, int maxCombinations);

/* Display Functions */

/**
 * Draw keyboard layout
 * @param keyCaps Pointer to Key Caps structure
 * @param updateRect Rectangle to update or NULL for all
 */
void KeyCaps_DrawKeyboard(KeyCaps *keyCaps, const Rect *updateRect);


/* Event Handling */

/**
 * Handle mouse click in Key Caps window
 * @param keyCaps Pointer to Key Caps structure
 * @param point Click location
 * @param modifiers Modifier keys held
 * @return 0 on success, negative on error
 */
int KeyCaps_HandleClick(KeyCaps *keyCaps, Point point, ModifierMask modifiers);

/**
 * Handle a key typed while Key Caps is in front: its key is shown pressed and
 * the character added to the text strip. Draws in the current port.
 * @param keyCaps Pointer to Key Caps structure
 * @param charCode The character typed
 * @param modifiers Modifier keys
 * @return 0 on success, negative on error
 */
int KeyCaps_HandleKeyPress(KeyCaps *keyCaps, UInt16 charCode,
                           ModifierMask modifiers);

/**
 * Periodic work: let a pressed key back up, and relabel the keys when Shift or
 * Caps Lock changes. Draws in the current port.
 */
void KeyCaps_Idle(KeyCaps *keyCaps, ModifierMask modifiers);


/* Character Functions */


/**
 * Insert character into target window
 * @param keyCaps Pointer to Key Caps structure
 * @param charCode Character to insert
 * @return 0 on success, negative on error
 */
int KeyCaps_InsertChar(KeyCaps *keyCaps, UInt16 charCode);


/* Utility Functions */


/* Desk Accessory Integration */

/**
 * Register Key Caps as a desk accessory
 * @return 0 on success, negative on error
 */
int KeyCaps_RegisterDA(void);

/**
 * Create Key Caps DA instance
 * @return Pointer to DA instance or NULL on error
 */
DeskAccessory *KeyCaps_CreateDA(void);

/* Key Caps Error Codes */
#define KEYCAPS_ERR_NONE            0       /* No error */
#define KEYCAPS_ERR_INVALID_LAYOUT  -1      /* Invalid keyboard layout */
#define KEYCAPS_ERR_INVALID_KEY     -2      /* Invalid key code */
#define KEYCAPS_ERR_INVALID_CHAR    -3      /* Invalid character code */
#define KEYCAPS_ERR_NO_LAYOUT       -4      /* No keyboard layout loaded */
#define KEYCAPS_ERR_FONT_ERROR      -5      /* Font loading error */
#define KEYCAPS_ERR_DRAW_ERROR      -6      /* Drawing error */

#endif /* KEYCAPS_H */