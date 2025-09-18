/*
 * DialogTypes.h - Dialog Manager Type Definitions
 *
 * This header defines all the internal structures and types used by the
 * Dialog Manager, maintaining exact compatibility with Mac System 7.1.
 */

#ifndef DIALOG_TYPES_H
#define DIALOG_TYPES_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Include base types */
#ifndef DIALOG_MANAGER_H
typedef struct Rect {
    int16_t top, left, bottom, right;
} Rect;

typedef struct Point {
    int16_t v, h;
} Point;

typedef unsigned char Str255[256];
typedef void* Handle;
typedef void** RgnHandle;
typedef int16_t OSErr;
typedef struct WindowRecord* WindowPtr;
typedef struct TERecord** TEHandle;
#endif

/* Dialog Manager globals structure */
typedef struct DialogGlobals {
    void (*resumeProc)(void);           /* Resume procedure after system error */
    void (*soundProc)(int16_t);         /* Sound procedure for errors */
    int16_t         alertStage;         /* Current alert stage (0-3) */
    int16_t         dialogFont;         /* Font for dialogs */
    Str255          paramText[4];       /* Parameter text for alerts */
    uint8_t         spareFlags;         /* Spare flags for extended features */
    WindowPtr       frontModal;         /* Front-most modal dialog */
    int16_t         defaultItem;        /* Global default item */
    int16_t         cancelItem;         /* Global cancel item */
    bool            tracksCursor;       /* Dialog tracks cursor */
} DialogGlobals;

/* Stage list structure for alert dialogs */
typedef union StageListUnion {
    int16_t value;
    struct {
        unsigned sound1:3;      /* Sound for stage 1 */
        unsigned boxDrwn1:1;    /* Draw box for stage 1 */
        unsigned boldItm1:1;    /* Bold item for stage 1 */
        unsigned sound2:3;      /* Sound for stage 2 */
        unsigned boxDrwn2:1;    /* Draw box for stage 2 */
        unsigned boldItm2:1;    /* Bold item for stage 2 */
        unsigned sound3:3;      /* Sound for stage 3 */
        unsigned boxDrwn3:1;    /* Draw box for stage 3 */
        unsigned boldItm3:1;    /* Bold item for stage 3 */
        unsigned sound4:3;      /* Sound for stage 4 */
        unsigned boxDrwn4:1;    /* Draw box for stage 4 */
        unsigned boldItm4:1;    /* Bold item for stage 4 */
    } stages;
} StageListUnion;

/* Dialog item internal structure */
typedef struct DialogItemInternal {
    uint8_t         type;               /* Item type and flags */
    uint8_t         length;             /* Length of item data */
    Handle          handle;             /* Item handle or procedure pointer */
    Rect            bounds;             /* Item bounds rectangle */
    /* Variable length item data follows */
} DialogItemInternal;

/* Dialog item list header */
typedef struct DialogItemListHeader {
    int16_t         count;              /* Number of items minus 1 */
    /* Dialog items follow */
} DialogItemListHeader;

/* Extended dialog item structure for internal use */
typedef struct DialogItemEx {
    DialogItemInternal  base;           /* Base item structure */
    bool                visible;        /* Item visibility */
    bool                enabled;        /* Item enabled state */
    int16_t             refCon;         /* Reference constant */
    void*               userData;       /* User data pointer */
    void*               platformData;   /* Platform-specific data */
} DialogItemEx;

/* Dialog resource structures */
typedef struct DialogResourceHeader {
    int16_t         version;            /* Resource version */
    int16_t         flags;              /* Resource flags */
    int32_t         reserved;           /* Reserved for future use */
} DialogResourceHeader;

/* DLOG resource structure */
typedef struct DLOGResource {
    DialogResourceHeader header;        /* Resource header */
    Rect                boundsRect;     /* Dialog bounds */
    int16_t             procID;         /* Window definition proc ID */
    uint8_t             visible;        /* Visibility flag */
    uint8_t             filler1;        /* Padding */
    uint8_t             goAwayFlag;     /* Has close box */
    uint8_t             filler2;        /* Padding */
    int32_t             refCon;         /* Reference constant */
    int16_t             itemsID;        /* Resource ID of item list */
    Str255              title;          /* Dialog title */
} DLOGResource;

/* DITL resource structure */
typedef struct DITLResource {
    DialogResourceHeader header;        /* Resource header */
    int16_t             count;          /* Number of items minus 1 */
    /* Variable length array of DialogItemInternal follows */
} DITLResource;

/* ALRT resource structure */
typedef struct ALRTResource {
    DialogResourceHeader header;        /* Resource header */
    Rect                boundsRect;     /* Alert bounds */
    int16_t             itemsID;        /* Resource ID of item list */
    StageListUnion      stages;         /* Alert stages */
} ALRTResource;

/* Dialog manager internal state */
typedef struct DialogManagerState {
    DialogGlobals       globals;        /* Global state */
    bool                initialized;    /* Initialization flag */
    int16_t             modalLevel;     /* Modal nesting level */
    WindowPtr           modalStack[16]; /* Stack of modal dialogs */
    bool                systemModal;    /* System modal flag */

    /* Platform integration */
    bool                useNativeDialogs;   /* Use native platform dialogs */
    bool                useAccessibility;   /* Enable accessibility features */
    float               scaleFactor;        /* High-DPI scale factor */
    void*               platformContext;    /* Platform-specific context */
} DialogManagerState;

/* Dialog event handling state */
typedef struct DialogEventState {
    bool                inModalLoop;        /* Currently in modal loop */
    DialogPtr           activeDialog;       /* Active dialog */
    int16_t             lastItemHit;        /* Last item hit */
    uint32_t            lastEventTime;      /* Time of last event */
    bool                defaultHandled;     /* Default button handled */
    bool                cancelHandled;      /* Cancel button handled */
    bool                textChanged;        /* Text field changed */
} DialogEventState;

/* Dialog drawing state */
typedef struct DialogDrawState {
    bool                needsRedraw;        /* Dialog needs redraw */
    bool                itemsNeedRedraw;    /* Items need redraw */
    RgnHandle           updateRegion;       /* Update region */
    int16_t             activeEditField;    /* Active edit field */
    bool                textEditActive;     /* TextEdit is active */
    void*               drawingContext;     /* Platform drawing context */
} DialogDrawState;

/* Platform abstraction structure */
typedef struct DialogPlatform {
    /* Platform-specific function pointers */
    bool (*init)(void);
    void (*cleanup)(void);
    void (*beep)(int16_t sound);
    bool (*showNativeAlert)(const char* message, const char* title, int iconType);
    bool (*showNativeFileDialog)(bool save, const char* title,
                                 const char* defaultPath, char* result, size_t resultSize);
    void (*setTheme)(const void* theme);
    void (*invalidateRect)(DialogPtr dialog, const Rect* rect);
    void (*drawDialogFrame)(DialogPtr dialog);
    void (*drawDialogItem)(DialogPtr dialog, int16_t itemNo);
    bool (*handlePlatformEvent)(DialogPtr dialog, void* platformEvent);
} DialogPlatform;

/* Error codes specific to Dialog Manager */
enum {
    dialogErr_NoError               = 0,
    dialogErr_OutOfMemory          = -108,
    dialogErr_InvalidDialog        = -1700,
    dialogErr_InvalidItem          = -1701,
    dialogErr_ResourceNotFound     = -1702,
    dialogErr_BadResourceFormat    = -1703,
    dialogErr_PlatformError        = -1704,
    dialogErr_NotInitialized       = -1705,
    dialogErr_AlreadyModal         = -1706,
    dialogErr_NotModal             = -1707
};

/* Dialog Manager feature flags */
enum {
    dialogFeature_NativeDialogs    = 0x0001,
    dialogFeature_Accessibility   = 0x0002,
    dialogFeature_HighDPI          = 0x0004,
    dialogFeature_ColorSupport     = 0x0008,
    dialogFeature_Unicode          = 0x0010,
    dialogFeature_Animation        = 0x0020,
    dialogFeature_Transparency     = 0x0040,
    dialogFeature_TouchSupport     = 0x0080
};

/* Dialog item type masks and flags */
enum {
    itemTypeMask        = 0x7F,         /* Mask for item type */
    itemDisableMask     = 0x80,         /* Mask for disabled flag */

    /* Extended item type flags */
    itemVisibleFlag     = 0x0100,       /* Item is visible */
    itemEnabledFlag     = 0x0200,       /* Item is enabled */
    itemSelectedFlag    = 0x0400,       /* Item is selected */
    itemHighlightFlag   = 0x0800,       /* Item is highlighted */
    itemCustomDrawFlag  = 0x1000,       /* Item uses custom drawing */
    itemAccessibleFlag  = 0x2000,       /* Item supports accessibility */
    itemAnimatedFlag    = 0x4000,       /* Item supports animation */
    itemPlatformFlag    = 0x8000        /* Item uses platform widget */
};

/* Dialog window classes and styles */
enum {
    /* Standard dialog window classes */
    kDialogClass_Modal          = 1,
    kDialogClass_Modeless       = 2,
    kDialogClass_SystemModal    = 3,
    kDialogClass_Floating       = 4,

    /* Dialog style flags */
    kDialogStyle_Default        = 0x0000,
    kDialogStyle_NoTitle        = 0x0001,
    kDialogStyle_NoFrame        = 0x0002,
    kDialogStyle_Resizable      = 0x0004,
    kDialogStyle_Movable        = 0x0008,
    kDialogStyle_CloseBox       = 0x0010,
    kDialogStyle_MinimizeBox    = 0x0020,
    kDialogStyle_MaximizeBox    = 0x0040,
    kDialogStyle_ToolWindow     = 0x0080
};

/* Color theme structure for modern platforms */
typedef struct DialogColorTheme {
    uint32_t    backgroundColor;        /* Dialog background color */
    uint32_t    foregroundColor;        /* Text and border color */
    uint32_t    buttonColor;            /* Button background color */
    uint32_t    buttonTextColor;        /* Button text color */
    uint32_t    editFieldColor;         /* Edit field background */
    uint32_t    editTextColor;          /* Edit field text color */
    uint32_t    selectionColor;         /* Selection highlight color */
    uint32_t    disabledColor;          /* Disabled item color */
    uint32_t    shadowColor;            /* Shadow color for 3D effects */
    uint32_t    highlightColor;         /* Highlight color for 3D effects */
    bool        isDarkTheme;            /* Dark theme flag */
    float       opacity;                /* Dialog opacity (0.0-1.0) */
} DialogColorTheme;

/* Accessibility information */
typedef struct DialogAccessibility {
    const char*     title;              /* Dialog title for screen readers */
    const char*     description;        /* Dialog description */
    int16_t         focusedItem;        /* Currently focused item */
    bool            canTabNavigate;     /* Supports tab navigation */
    bool            announceChanges;    /* Announce changes to screen reader */
    void*           accessibilityData;  /* Platform accessibility data */
} DialogAccessibility;

#ifdef __cplusplus
}
#endif

#endif /* DIALOG_TYPES_H */