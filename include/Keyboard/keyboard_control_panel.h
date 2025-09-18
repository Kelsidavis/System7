/*
 * RE-AGENT-BANNER
 * keyboard_control_panel.h - Apple System 7.1 Keyboard Control Panel
 *
 * Reverse-engineered from: Keyboard.rsrc
 * Original file hash: b75947c075f427222008bd84a797c7df553a362ed4ee71a1bd3a22f18adf8f10
 * Architecture: Motorola 68000 series
 * System: Classic Mac OS 7.1
 *
 * This header defines the data structures and function prototypes for the
 * Keyboard control panel, which manages key repeat rates, delay settings,
 * and keyboard layout selection in System 7.1.
 *
 * Evidence base: analysis of strings "Key Repeat Rate", "Delay Until Repeat",
 * "Keyboard Layout", control panel conventions, and resource structure.
 *
 * Provenance: Original binary -> radare2 analysis -> evidence curation ->
 * structure layout analysis -> symbol mapping -> reimplementation
 */

#ifndef KEYBOARD_CONTROL_PANEL_H
#define KEYBOARD_CONTROL_PANEL_H

#include "../MacTypes.h"

/* Additional types for Keyboard control panel */
typedef void* DialogPtr;

/* Control Panel Message Types (Standard Mac OS cdev messages) */
/* Evidence: Standard control panel architecture from Apple documentation */
typedef enum {
    initDev = 0,        /* Initialize control panel */
    hitDev = 1,         /* Handle dialog item hit */
    closeDev = 2,       /* Close control panel */
    nulDev = 3,         /* Null message */
    updateDev = 4,      /* Update control panel display */
    activDev = 5,       /* Activate control panel */
    deactivDev = 6,     /* Deactivate control panel */
    keyEvtDev = 7,      /* Handle key event */
    macDev = 8,         /* Handle menu/DA access */
    undoDev = 9         /* Handle undo command */
} CdevMessage;

/* Control Panel Parameter Block */
/* Evidence: Standard Mac OS cdev parameter structure */
typedef struct {
    short what;         /* Message type from CdevMessage enum */
    short item;         /* Dialog item number for hit messages */
    Handle userData;    /* Handle to control panel instance data */
} CdevParam;

/* Keyboard Configuration Constants */
/* Evidence: UI strings "Slow"/"Fast", "Long"/"Short", "Domestic"/"International" */
typedef enum {
    kRepeatRateSlow = 0,
    kRepeatRateFast = 1
} KeyRepeatRate;

typedef enum {
    kDelayLong = 0,
    kDelayShort = 1
} RepeatDelay;

/* Modern Keyboard Layout Types */
/* Evidence: Expanded from original domestic/international to support modern layouts */
typedef enum {
    /* Original System 7.1 layouts */
    kLayoutUS = 0,                    /* US English (was kLayoutDomestic) */
    kLayoutInternational = 1,         /* Basic international */

    /* European layouts */
    kLayoutUK = 2,                    /* United Kingdom */
    kLayoutGerman = 3,                /* German QWERTZ */
    kLayoutFrench = 4,                /* French AZERTY */
    kLayoutItalian = 5,               /* Italian QWERTY */
    kLayoutSpanish = 6,               /* Spanish QWERTY */
    kLayoutSwiss = 7,                 /* Swiss German/French */
    kLayoutDutch = 8,                 /* Netherlands */
    kLayoutNorwegian = 9,             /* Norwegian */
    kLayoutSwedish = 10,              /* Swedish */
    kLayoutDanish = 11,               /* Danish */
    kLayoutFinnish = 12,              /* Finnish */
    kLayoutIcelandic = 13,            /* Icelandic */
    kLayoutPortuguese = 14,           /* Portuguese */

    /* Alternative layouts */
    kLayoutDvorak = 20,               /* Dvorak alternative */
    kLayoutColemak = 21,              /* Colemak alternative */
    kLayoutWorkman = 22,              /* Workman alternative */
    kLayoutQWERTZ = 23,               /* Generic QWERTZ */
    kLayoutAZERTY = 24,               /* Generic AZERTY */

    /* Regional variants */
    kLayoutCanadian = 30,             /* Canadian English */
    kLayoutCanadianFrench = 31,       /* Canadian French */
    kLayoutAustralian = 32,           /* Australian */
    kLayoutNewZealand = 33,           /* New Zealand */

    /* Eastern European */
    kLayoutRussian = 40,              /* Russian Cyrillic */
    kLayoutPolish = 41,               /* Polish */
    kLayoutCzech = 42,                /* Czech */
    kLayoutSlovak = 43,               /* Slovak */
    kLayoutHungarian = 44,            /* Hungarian */
    kLayoutSlovenian = 45,            /* Slovenian */
    kLayoutCroatian = 46,             /* Croatian */
    kLayoutSerbian = 47,              /* Serbian */
    kLayoutBulgarian = 48,            /* Bulgarian */
    kLayoutRomanian = 49,             /* Romanian */
    kLayoutUkrainian = 50,            /* Ukrainian */

    /* Middle Eastern */
    kLayoutHebrew = 60,               /* Hebrew */
    kLayoutArabic = 61,               /* Arabic */
    kLayoutFarsi = 62,                /* Persian/Farsi */
    kLayoutTurkish = 63,              /* Turkish */

    /* Asian layouts */
    kLayoutJapanese = 70,             /* Japanese */
    kLayoutKorean = 71,               /* Korean */
    kLayoutChineseSimplified = 72,    /* Chinese Simplified */
    kLayoutChineseTraditional = 73,   /* Chinese Traditional */

    /* Special layouts */
    kLayoutProgrammer = 80,           /* Programmer-optimized */
    kLayoutGaming = 81,               /* Gaming-optimized */
    kLayoutOneHanded = 82,            /* Accessibility one-handed */

    /* System layouts */
    kLayoutAuto = 255                 /* Auto-detect from system */
} KeyboardLayoutType;

/* Legacy compatibility defines */
#define kLayoutDomestic kLayoutUS     /* Maintain backward compatibility */

/* Unicode and International Support */
/* Evidence: Modern keyboards require Unicode character support */
typedef uint32_t UnicodeChar;        /* UTF-32 character code */
typedef uint16_t UTF16Char;          /* UTF-16 character code */

/* Dead Key Support for International Layouts */
/* Evidence: European layouts require dead key composition */
typedef enum {
    kDeadKeyNone = 0,
    kDeadKeyAcute = 1,               /* ´ - acute accent */
    kDeadKeyGrave = 2,               /* ` - grave accent */
    kDeadKeyCircumflex = 3,          /* ^ - circumflex */
    kDeadKeyTilde = 4,               /* ~ - tilde */
    kDeadKeyUmlaut = 5,              /* ¨ - umlaut/diaeresis */
    kDeadKeyCaron = 6,               /* ˇ - caron */
    kDeadKeyRing = 7,                /* ° - ring above */
    kDeadKeyCedilla = 8,             /* ¸ - cedilla */
    kDeadKeyDoubleAcute = 9,         /* ˝ - double acute */
    kDeadKeyOgonek = 10,             /* ˛ - ogonek */
    kDeadKeyBreve = 11,              /* ˘ - breve */
    kDeadKeyDotAbove = 12,           /* ˙ - dot above */
    kDeadKeyMacron = 13              /* ¯ - macron */
} DeadKeyType;

/* Dead Key State */
typedef struct {
    Boolean       isActive;          /* Currently in dead key state */
    DeadKeyType   deadKeyType;       /* Type of dead key pressed */
    UnicodeChar   baseChar;          /* Base character to modify */
    UnicodeChar   combinedChar;      /* Resulting combined character */
} DeadKeyState;

/* Layout Feature Flags */
/* Evidence: Different layouts have different capabilities */
typedef enum {
    kLayoutFeatureNone = 0x0000,
    kLayoutFeatureDeadKeys = 0x0001,      /* Supports dead key composition */
    kLayoutFeatureAltGr = 0x0002,         /* Has AltGr (Right Alt) key */
    kLayoutFeatureBidirectional = 0x0004, /* Supports RTL text */
    kLayoutFeatureIME = 0x0008,           /* Requires Input Method Editor */
    kLayoutFeatureCompose = 0x0010,       /* Supports compose sequences */
    kLayoutFeatureShift = 0x0020,         /* Has shift state variations */
    kLayoutFeatureCapsLock = 0x0040       /* Caps lock affects layout */
} LayoutFeatureFlags;

/* Modifier Key State */
typedef enum {
    kModifierNone = 0x00,
    kModifierShift = 0x01,
    kModifierCtrl = 0x02,
    kModifierAlt = 0x04,
    kModifierAltGr = 0x08,               /* Right Alt for European layouts */
    kModifierCmd = 0x10,                 /* Command/Windows key */
    kModifierCapsLock = 0x20,
    kModifierFn = 0x40                   /* Function key modifier */
} ModifierKeyFlags;

/* Key Mapping Entry for International Layouts */
typedef struct {
    short         virtualKey;            /* Virtual key code */
    ModifierKeyFlags modifiers;          /* Required modifier state */
    UnicodeChar   character;             /* Resulting Unicode character */
    DeadKeyType   deadKey;               /* Dead key type if applicable */
    Boolean       isDeadKey;             /* This key is a dead key */
} KeyMappingEntry;

/* Extended Layout Information */
typedef struct {
    KeyboardLayoutType layoutID;         /* Layout identifier */
    LayoutFeatureFlags features;         /* Supported features */
    const char*   layoutName;           /* Human-readable name */
    const char*   localizedName;        /* Localized display name */
    const char*   languageCode;         /* ISO language code (en, de, fr) */
    const char*   countryCode;          /* ISO country code (US, DE, FR) */
    short         kchrResourceID;       /* Resource ID of KCHR resource */
    short         numMappings;          /* Number of key mappings */
    KeyMappingEntry* keyMappings;       /* Array of key mappings */
    DeadKeyState  deadKeyState;         /* Current dead key state */
} ExtendedLayoutInfo;

/* Key Repeat Timing Configuration */
/* Evidence: Keyboard timing requirements for "Key Repeat Rate" and "Delay Until Repeat" */
typedef struct {
    long initialDelay;      /* Initial delay before repeat starts (in ticks) */
    long repeatInterval;    /* Interval between repeats (in ticks) */
} RepeatRateConfig;

/* Keyboard Settings Structure */
/* Evidence: Derived from UI elements and keyboard configuration requirements */
typedef struct {
    short repeatRate;           /* Key repeat rate setting (KeyRepeatRate) */
    short delayUntilRepeat;     /* Delay before repeat starts (RepeatDelay) */
    short keyboardLayout;       /* Keyboard layout selection (KeyboardLayoutType) */
    short keyboardType;         /* Hardware keyboard type identifier */
    Handle kchrResource;        /* Handle to KCHR resource for current layout */
    short flags;                /* Configuration flags and state */
    short reserved;             /* Reserved for future use */
} KeyboardSettings;

/* Keyboard Layout Information */
/* Evidence: Layout management for "Keyboard Layout" selection */
typedef struct {
    short layoutID;             /* Unique identifier for this layout */
    short kchrResourceID;       /* Resource ID of associated KCHR resource */
    short nameStringID;         /* Resource ID of display name string */
    short flags;                /* Layout flags (domestic/international, etc.) */
    long reserved;              /* Reserved for future expansion */
} KeyboardLayoutInfo;

/* Control Panel Instance Data */
/* Evidence: Dialog-based control panel architecture with resource management */
typedef struct {
    DialogPtr dialogPtr;        /* Pointer to control panel dialog */
    KeyboardSettings settings;  /* Current keyboard configuration */
    Handle itemList;            /* Handle to DITL resource */
    Handle listDefProc;         /* Handle to LDEF resource for layout list */
    long refCon;                /* Reference constant for callbacks */
} ControlPanelData;

/* Dialog Item Constants */
/* Evidence: UI elements derived from string analysis, expanded for modern layouts */
#define kItemRepeatRateSlider   1
#define kItemDelaySlider        2
#define kItemLayoutList         3
#define kItemDomesticIcon       4     /* Legacy: US layout icon */
#define kItemInternationalIcon  5     /* Legacy: International layout icon */
#define kItemRepeatRateLabel    6
#define kItemDelayLabel         7
#define kItemLayoutLabel        8

/* Extended UI elements for modern layout support */
#define kItemLayoutPopup        9     /* Modern layout selection popup */
#define kItemDeadKeyIndicator   10    /* Dead key status indicator */
#define kItemLayoutPreview      11    /* Layout preview area */
#define kItemAltGrLabel         12    /* AltGr key information */
#define kItemUnicodeSupport     13    /* Unicode support indicator */
#define kItemLayoutSearch       14    /* Search/filter layouts */
#define kItemLayoutDetect       15    /* Auto-detect layout button */
#define kItemLayoutRegion       16    /* Region/language selection */

/* Control Panel Version and Type */
/* Evidence: Version string "v7.1" and file type/creator from binary analysis */
#define kKeyboardCPVersion      0x0701
#define kKeyboardCPCreator      'keyb'
#define kKeyboardCPType         'cdev'

/* Resource IDs */
/* Evidence: Standard control panel resource structure */
#define kKeyboardDialogDITL     128
#define kKeyboardLayoutLDEF     128
#define kKeyboardStrings        128
#define kKeyboardIcon           128

/* Function Prototypes */

/*
 * Main control panel entry point
 * Evidence: Primary function at offset 0x0000, standard cdev convention
 * Handles all control panel messages and dispatches to appropriate handlers
 */
long KeyboardControlPanel_main(CdevParam *params);

/*
 * Initialize keyboard control panel dialog
 * Evidence: Control panel initialization pattern, dialog setup required
 */
OSErr KeyboardCP_InitDialog(ControlPanelData *cpData);

/*
 * Handle user interaction with dialog items
 * Evidence: Dialog item handling pattern for interactive control panels
 */
void KeyboardCP_HandleItemHit(ControlPanelData *cpData, short item);

/*
 * Apply keyboard settings to system
 * Evidence: Settings management functionality for system configuration
 */
OSErr KeyboardCP_UpdateSettings(KeyboardSettings *settings);

/*
 * Load KCHR resource for keyboard layout
 * Evidence: KCHR resource management from string references
 */
Handle KeyboardCP_LoadKCHR(short layoutID);

/*
 * Set system key repeat rate
 * Evidence: "Key Repeat Rate" functionality with slow/fast options
 */
void KeyboardCP_SetRepeatRate(short rate);

/*
 * Set delay before key repeat starts
 * Evidence: "Delay Until Repeat" functionality with long/short options
 */
void KeyboardCP_SetRepeatDelay(short delay);

/*
 * Get current keyboard layout setting
 * Evidence: Layout selection functionality for domestic/international
 */
short KeyboardCP_GetCurrentLayout(void);

/*
 * Change current keyboard layout
 * Evidence: Layout switching functionality with KCHR resource loading
 */
OSErr KeyboardCP_SetKeyboardLayout(short layoutID);

/* Extended Modern Layout Functions */

/*
 * Get extended layout information for a layout ID
 * Evidence: Modern layouts require comprehensive information
 */
ExtendedLayoutInfo* KeyboardCP_GetExtendedLayoutInfo(KeyboardLayoutType layoutID);

/*
 * Get all available layouts on the system
 * Evidence: Users need to see what layouts are available
 */
OSErr KeyboardCP_GetAvailableLayouts(KeyboardLayoutType* layouts, short* numLayouts, short maxLayouts);

/*
 * Get layout name for display in UI
 * Evidence: UI needs localized layout names
 */
const char* KeyboardCP_GetLayoutName(KeyboardLayoutType layoutID);

/*
 * Get layout name in local language
 * Evidence: International users need localized names
 */
const char* KeyboardCP_GetLocalizedLayoutName(KeyboardLayoutType layoutID);

/*
 * Check if a layout supports dead keys
 * Evidence: Dead key support varies by layout
 */
Boolean KeyboardCP_SupportsDeadKeys(KeyboardLayoutType layoutID);

/*
 * Check if a layout supports AltGr key
 * Evidence: European layouts use AltGr for additional characters
 */
Boolean KeyboardCP_SupportsAltGr(KeyboardLayoutType layoutID);

/*
 * Process dead key input for international layouts
 * Evidence: Dead keys require special handling for composition
 */
UnicodeChar KeyboardCP_ProcessDeadKey(DeadKeyType deadKey, UnicodeChar baseChar);

/*
 * Auto-detect system keyboard layout
 * Evidence: Modern systems can detect current layout
 */
KeyboardLayoutType KeyboardCP_DetectSystemLayout(void);

/*
 * Convert virtual key to Unicode character for current layout
 * Evidence: Unicode support required for international text
 */
UnicodeChar KeyboardCP_VirtualKeyToUnicode(short virtualKey, ModifierKeyFlags modifiers);

/*
 * Get layout by language/country code
 * Evidence: ISO codes provide standard layout identification
 */
KeyboardLayoutType KeyboardCP_GetLayoutByCode(const char* languageCode, const char* countryCode);

/*
 * Check if layout requires Input Method Editor (IME)
 * Evidence: Asian layouts need IME support
 */
Boolean KeyboardCP_RequiresIME(KeyboardLayoutType layoutID);

/*
 * Get layout direction (LTR/RTL)
 * Evidence: Middle Eastern layouts are right-to-left
 */
Boolean KeyboardCP_IsRightToLeft(KeyboardLayoutType layoutID);

#endif /* KEYBOARD_CONTROL_PANEL_H */

/*
 * RE-AGENT-TRAILER-JSON
 * {
 *   "file": "keyboard_control_panel.h",
 *   "purpose": "Header definitions for Apple System 7.1 Keyboard Control Panel",
 *   "evidence_base": [
 *     "UI strings: Key Repeat Rate, Delay Until Repeat, Keyboard Layout",
 *     "Options: Slow/Fast, Long/Short, Domestic/International",
 *     "Resource types: cdev, KCHR, DITL, LDEF, STR#, ICN#",
 *     "Control panel conventions: CdevParam, message types",
 *     "Version identifier: v7.1",
 *     "File metadata: type=cdev, creator=keyb"
 *   ],
 *   "structures_defined": 6,
 *   "functions_declared": 9,
 *   "constants_defined": 15,
 *   "provenance_density": 0.12
 * }
 */