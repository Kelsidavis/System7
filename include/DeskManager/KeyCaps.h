#ifndef KEYCAPS_H
#define KEYCAPS_H

/*
 * KeyCaps.h - Key Caps Desk Accessory
 *
 * Provides a visual keyboard layout display showing all available characters
 * for the current keyboard layout. Users can see what characters are produced
 * by different key combinations and can click to insert characters.
 *
 * Based on Apple's Key Caps DA from System 7.1
 */

#include "DeskAccessory.h"
#include <stdint.h>
#include <stdbool.h>

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

/* Key Types */
typedef enum {
    KEY_TYPE_NORMAL         = 0,    /* Normal character key */
    KEY_TYPE_MODIFIER       = 1,    /* Modifier key (shift, option, etc.) */
    KEY_TYPE_FUNCTION       = 2,    /* Function key */
    KEY_TYPE_SPECIAL        = 3,    /* Special key (space, tab, etc.) */
    KEY_TYPE_DEAD           = 4,    /* Dead key (accent) */
    KEY_TYPE_INVALID        = 255   /* Invalid/unmapped key */
} KeyType;

/* Modifier Keys */
typedef enum {
    MOD_NONE        = 0x0000,       /* No modifiers */
    MOD_SHIFT       = 0x0001,       /* Shift key */
    MOD_CAPS_LOCK   = 0x0002,       /* Caps Lock */
    MOD_OPTION      = 0x0004,       /* Option/Alt key */
    MOD_CONTROL     = 0x0008,       /* Control key */
    MOD_COMMAND     = 0x0010,       /* Command key */
    MOD_FUNCTION    = 0x0020        /* Function key */
} ModifierMask;

/* Key Information */
typedef struct KeyInfo {
    uint8_t     scanCode;           /* Hardware scan code */
    KeyType     type;               /* Key type */
    char        label[8];           /* Key label */
    uint16_t    baseChar;           /* Base character (no modifiers) */
    uint16_t    shiftChar;          /* Shift character */
    uint16_t    optionChar;         /* Option character */
    uint16_t    shiftOptionChar;    /* Shift+Option character */
    uint16_t    deadKeyChar;        /* Dead key character */
    bool        isDeadKey;          /* True if this is a dead key */
    Rect        rect;               /* Key rectangle on display */
} KeyInfo;

/* Keyboard Layout */
typedef struct KeyboardLayout {
    char        name[64];           /* Layout name */
    uint16_t    layoutID;           /* Layout identifier */
    uint16_t    scriptCode;         /* Script code */
    uint16_t    languageCode;       /* Language code */

    /* Key mappings */
    KeyInfo     keys[KEYCAPS_MAX_KEYS];     /* Key information */
    int         numKeys;            /* Number of keys */

    /* Character tables */
    uint16_t    charTable[256];     /* Character table */
    uint16_t    shiftTable[256];    /* Shift character table */
    uint16_t    optionTable[256];   /* Option character table */
    uint16_t    shiftOptionTable[256];  /* Shift+Option table */

    /* Dead key tables */
    uint16_t    deadKeyTable[32][256];  /* Dead key combinations */
    int         numDeadKeys;        /* Number of dead keys */

    /* Font information */
    char        fontName[32];       /* Font name for display */
    int16_t     fontSize;           /* Font size */
} KeyboardLayout;

/* Key Caps State */
typedef struct KeyCaps {
    /* Current layout */
    KeyboardLayout  *currentLayout; /* Current keyboard layout */
    KeyboardLayout  *layouts;       /* Available layouts */
    int             numLayouts;     /* Number of layouts */

    /* Modifier state */
    ModifierMask    modifiers;      /* Current modifier keys */
    ModifierMask    stickyMods;     /* Sticky modifier keys */
    bool            capsLockOn;     /* Caps Lock state */

    /* Dead key state */
    bool            deadKeyActive;  /* Dead key pressed */
    uint16_t        deadKeyChar;    /* Dead key character */

    /* Display state */
    Rect            windowBounds;   /* Window bounds */
    Rect            keyboardRect;   /* Keyboard display area */
    bool            windowVisible;  /* Window visibility */

    /* Character display */
    Rect            charDisplayRect; /* Character display area */
    uint16_t        selectedChar;   /* Selected character */
    bool            showCharInfo;   /* Show character information */

    /* Font and drawing */
    void            *font;          /* Current font */
    int16_t         fontSize;       /* Font size */
    void            *graphics;      /* Graphics context */

    /* Settings */
    bool            showModifiers;  /* Show modifier key states */
    bool            showDeadKeys;   /* Show dead key combinations */
    bool            playKeySound;   /* Play key click sound */
    bool            autoRepeat;     /* Auto-repeat on key hold */

    /* Character insertion */
    void            *targetWindow;  /* Target window for character insertion */
    bool            insertMode;     /* Character insertion enabled */
} KeyCaps;

/* Character Information */
typedef struct CharInfo {
    uint16_t    charCode;           /* Character code */
    char        name[64];           /* Character name */
    char        description[128];   /* Character description */
    uint8_t     category;           /* Unicode category */
    bool        isPrintable;        /* Printable character */
    bool        isControl;          /* Control character */
    bool        isWhitespace;       /* Whitespace character */
} CharInfo;

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
 * Load keyboard layout by ID
 * @param keyCaps Pointer to Key Caps structure
 * @param layoutID Layout identifier
 * @return 0 on success, negative on error
 */
int KeyCaps_LoadLayout(KeyCaps *keyCaps, uint16_t layoutID);

/**
 * Set current keyboard layout
 * @param keyCaps Pointer to Key Caps structure
 * @param layout Pointer to keyboard layout
 * @return 0 on success, negative on error
 */
int KeyCaps_SetLayout(KeyCaps *keyCaps, KeyboardLayout *layout);

/**
 * Get available keyboard layouts
 * @param layouts Array to fill with layout pointers
 * @param maxLayouts Maximum number of layouts
 * @return Number of layouts returned
 */
int KeyCaps_GetAvailableLayouts(KeyboardLayout **layouts, int maxLayouts);

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
uint16_t KeyCaps_GetCharForKey(KeyCaps *keyCaps, uint8_t scanCode,
                               ModifierMask modifiers);

/**
 * Get key information by scan code
 * @param keyCaps Pointer to Key Caps structure
 * @param scanCode Key scan code
 * @return Pointer to key info or NULL if not found
 */
const KeyInfo *KeyCaps_GetKeyInfo(KeyCaps *keyCaps, uint8_t scanCode);

/**
 * Find key by character
 * @param keyCaps Pointer to Key Caps structure
 * @param charCode Character code
 * @param modifiers Pointer to required modifiers (output)
 * @return Scan code or 0 if not found
 */
uint8_t KeyCaps_FindKeyForChar(KeyCaps *keyCaps, uint16_t charCode,
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
bool KeyCaps_IsModifierActive(KeyCaps *keyCaps, ModifierMask modifier);

/* Dead Key Functions */

/**
 * Process dead key input
 * @param keyCaps Pointer to Key Caps structure
 * @param deadKeyChar Dead key character
 * @param nextChar Next character typed
 * @return Combined character or 0 if no combination
 */
uint16_t KeyCaps_ProcessDeadKey(KeyCaps *keyCaps, uint16_t deadKeyChar,
                                uint16_t nextChar);

/**
 * Check if character is a dead key
 * @param keyCaps Pointer to Key Caps structure
 * @param charCode Character code
 * @return true if character is a dead key
 */
bool KeyCaps_IsDeadKey(KeyCaps *keyCaps, uint16_t charCode);

/**
 * Get dead key combinations
 * @param keyCaps Pointer to Key Caps structure
 * @param deadKeyChar Dead key character
 * @param combinations Array to fill with combinations
 * @param maxCombinations Maximum number of combinations
 * @return Number of combinations returned
 */
int KeyCaps_GetDeadKeyCombinations(KeyCaps *keyCaps, uint16_t deadKeyChar,
                                   uint16_t *combinations, int maxCombinations);

/* Display Functions */

/**
 * Draw keyboard layout
 * @param keyCaps Pointer to Key Caps structure
 * @param updateRect Rectangle to update or NULL for all
 */
void KeyCaps_DrawKeyboard(KeyCaps *keyCaps, const Rect *updateRect);

/**
 * Draw individual key
 * @param keyCaps Pointer to Key Caps structure
 * @param keyInfo Key information
 * @param pressed True if key is pressed
 */
void KeyCaps_DrawKey(KeyCaps *keyCaps, const KeyInfo *keyInfo, bool pressed);

/**
 * Draw character display area
 * @param keyCaps Pointer to Key Caps structure
 */
void KeyCaps_DrawCharDisplay(KeyCaps *keyCaps);

/**
 * Highlight key by scan code
 * @param keyCaps Pointer to Key Caps structure
 * @param scanCode Key scan code
 * @param highlight True to highlight, false to unhighlight
 */
void KeyCaps_HighlightKey(KeyCaps *keyCaps, uint8_t scanCode, bool highlight);

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
 * Handle key press
 * @param keyCaps Pointer to Key Caps structure
 * @param scanCode Key scan code
 * @param modifiers Modifier keys
 * @return 0 on success, negative on error
 */
int KeyCaps_HandleKeyPress(KeyCaps *keyCaps, uint8_t scanCode,
                           ModifierMask modifiers);

/**
 * Handle modifier key change
 * @param keyCaps Pointer to Key Caps structure
 * @param newModifiers New modifier state
 */
void KeyCaps_HandleModifierChange(KeyCaps *keyCaps, ModifierMask newModifiers);

/* Character Functions */

/**
 * Get character information
 * @param charCode Character code
 * @param charInfo Pointer to character info structure
 * @return 0 on success, negative on error
 */
int KeyCaps_GetCharInfo(uint16_t charCode, CharInfo *charInfo);

/**
 * Insert character into target window
 * @param keyCaps Pointer to Key Caps structure
 * @param charCode Character to insert
 * @return 0 on success, negative on error
 */
int KeyCaps_InsertChar(KeyCaps *keyCaps, uint16_t charCode);

/**
 * Copy character to clipboard
 * @param keyCaps Pointer to Key Caps structure
 * @param charCode Character to copy
 * @return 0 on success, negative on error
 */
int KeyCaps_CopyChar(KeyCaps *keyCaps, uint16_t charCode);

/* Utility Functions */

/**
 * Convert character code to string
 * @param charCode Character code
 * @param buffer Buffer for string
 * @param bufferSize Size of buffer
 * @return Number of bytes written
 */
int KeyCaps_CharToString(uint16_t charCode, char *buffer, int bufferSize);

/**
 * Get keyboard layout name
 * @param layoutID Layout identifier
 * @param name Buffer for layout name
 * @param nameSize Size of name buffer
 * @return 0 on success, negative on error
 */
int KeyCaps_GetLayoutName(uint16_t layoutID, char *name, int nameSize);

/**
 * Check if layout supports character
 * @param layout Keyboard layout
 * @param charCode Character code
 * @return true if layout supports character
 */
bool KeyCaps_LayoutSupportsChar(KeyboardLayout *layout, uint16_t charCode);

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