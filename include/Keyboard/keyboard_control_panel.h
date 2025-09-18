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

typedef enum {
    kLayoutDomestic = 0,
    kLayoutInternational = 1
} KeyboardLayoutType;

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
/* Evidence: UI elements derived from string analysis */
#define kItemRepeatRateSlider   1
#define kItemDelaySlider        2
#define kItemLayoutList         3
#define kItemDomesticIcon       4
#define kItemInternationalIcon  5
#define kItemRepeatRateLabel    6
#define kItemDelayLabel         7
#define kItemLayoutLabel        8

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