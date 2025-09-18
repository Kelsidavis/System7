/*
 * HelpManager.h - Mac OS Help Manager API
 *
 * This file provides the complete Mac OS Help Manager API for context-sensitive
 * help balloons and user assistance features. The Help Manager provides balloon
 * help that appears when the user hovers over interface elements.
 *
 * Key features:
 * - Help balloon display and positioning
 * - Context-sensitive help detection
 * - Help resource loading and management
 * - Help navigation and cross-references
 * - Multi-format help content (text, pictures, styled text)
 * - User preference integration
 * - Modern help system abstraction
 */

#ifndef HELPMANAGER_H
#define HELPMANAGER_H

#include "MacTypes.h"
#include "Quickdraw.h"
#include "Menus.h"
#include "ResourceManager.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Help Manager version */
#define kHMBalloonHelpVersion 0x0002

/* Help Manager error codes (-850 to -874) */
enum {
    hmHelpDisabled = -850,           /* Show Balloons mode was off, call ignored */
    hmBalloonAborted = -853,         /* Mouse was moving or not in window port rect */
    hmSameAsLastBalloon = -854,      /* Menu & item same as last time */
    hmHelpManagerNotInited = -855,   /* Help menu not setup */
    hmSkippedBalloon = -857,         /* Help message specified skip balloon */
    hmWrongVersion = -858,           /* Help manager resource wrong version */
    hmUnknownHelpType = -859,        /* Bad type in help message record */
    hmOperationUnsupported = -861,   /* Bad method passed to routine */
    hmNoBalloonUp = -862,            /* No balloon visible when call made */
    hmCloseViewActive = -863         /* CloseView was active */
};

/* Help Manager resource and menu IDs */
enum {
    kHMHelpMenuID = -16490,          /* Resource ID and menu ID of help menu */
    kHMAboutHelpItem = 1,            /* About Balloon Help menu item */
    kHMShowBalloonsItem = 3,         /* Show/Hide Balloons menu item */
    kHMHelpID = -5696,               /* Help Manager package resources */
    kBalloonWDEFID = 126             /* WDEF proc for standard balloons */
};

/* Dialog item template constants */
enum {
    helpItem = 1                     /* Key value in DITL for help item */
};

/* Options for Help Manager resources */
enum {
    hmDefaultOptions = 0,            /* Default options */
    hmUseSubID = 1,                  /* Treat resIDs as subIDs of driver base */
    hmAbsoluteCoords = 2,            /* Ignore window port origin */
    hmSaveBitsNoWindow = 4,          /* Don't create window, just blast bits */
    hmSaveBitsWindow = 8,            /* Create window, restore bits behind */
    hmMatchInTitle = 16              /* Match string anywhere in window title */
};

/* Help content types */
enum {
    kHMStringItem = 1,               /* Pascal string */
    kHMPictItem = 2,                 /* PICT resource ID */
    kHMStringResItem = 3,            /* STR# resource ID & index */
    kHMTEResItem = 6,                /* Styled Text Edit resource */
    kHMSTRResItem = 7,               /* STR resource ID */
    kHMSkipItem = 256,               /* Don't display balloon */
    kHMCompareItem = 512,            /* Compare strings (menus only) */
    kHMNamedResourceItem = 1024,     /* Use named resource (menus only) */
    kHMTrackCntlItem = 2048          /* Track control item (reserved) */
};

/* Help message types for HMMessageRecord */
enum {
    khmmString = 1,                  /* Help message contains Pascal string */
    khmmPict = 2,                    /* Resource ID to PICT resource */
    khmmStringRes = 3,               /* Resource ID & index to STR# */
    khmmTEHandle = 4,                /* Text Edit handle */
    khmmPictHandle = 5,              /* Picture handle */
    khmmTERes = 6,                   /* Resource ID to TEXT & styl resources */
    khmmSTRRes = 7                   /* Resource ID to STR resource */
};

/* Resource types for styled TextEdit */
#define kHMTETextResType    'TEXT'   /* Text data without style info */
#define kHMTEStyleResType   'styl'   /* Style information */

/* Item states for extracting help messages */
enum {
    kHMEnabledItem = 0,              /* Item enabled, not checked */
    kHMDisabledItem = 1,             /* Item disabled/grayed */
    kHMCheckedItem = 2,              /* Item enabled and checked */
    kHMOtherItem = 3                 /* Item enabled, control value > 1 */
};

/* Help resource types */
#define kHMMenuResType      'hmnu'   /* Help resource for menus */
#define kHMDialogResType    'hdlg'   /* Help resource for dialogs */
#define kHMWindListResType  'hwin'   /* Help resource for windows */
#define kHMRectListResType  'hrct'   /* Help resource for rectangles */
#define kHMOverrideResType  'hovr'   /* Help resource for overrides */
#define kHMFinderApplResType 'hfdr'  /* Help resource for Finder */

/* Balloon display methods */
enum {
    kHMRegularWindow = 0,            /* Regular floating window */
    kHMSaveBitsNoWindow = 1,         /* Save bits, no window (MDEF) */
    kHMSaveBitsWindow = 2            /* Regular window, save bits behind */
};

/* Forward declarations */
typedef struct HMMessageRecord HMMessageRecord;
typedef HMMessageRecord *HMMessageRecPtr;

/* String resource type for help messages */
typedef struct HMStringResType {
    short hmmResID;                  /* Resource ID */
    short hmmIndex;                  /* String index */
} HMStringResType;

/* Help message record - holds various help content types */
typedef struct HMMessageRecord {
    short hmmHelpType;               /* Type of help message */
    union {
        char hmmString[256];         /* Pascal string */
        short hmmPict;               /* PICT resource ID */
        Handle hmmTEHandle;          /* TextEdit handle */
        HMStringResType hmmStringRes; /* STR# resource and index */
        short hmmPictRes;            /* PICT resource ID */
        Handle hmmPictHandle;        /* Picture handle */
        short hmmTERes;              /* TEXT/styl resource ID */
        short hmmSTRRes;             /* STR resource ID */
    } u;
} HMMessageRecord;

/* Help Manager function prototypes */

/* Core Help Manager functions */
OSErr HMGetHelpMenuHandle(MenuHandle *mh);
OSErr HMShowBalloon(const HMMessageRecord *aHelpMsg, Point tip,
                   RectPtr alternateRect, Ptr tipProc, short theProc,
                   short variant, short method);
OSErr HMRemoveBalloon(void);
Boolean HMGetBalloons(void);
OSErr HMSetBalloons(Boolean flag);
Boolean HMIsBalloon(void);

/* Menu help functions */
OSErr HMShowMenuBalloon(short itemNum, short itemMenuID, long itemFlags,
                       long itemReserved, Point tip, RectPtr alternateRect,
                       Ptr tipProc, short theProc, short variant);

/* Help message extraction */
OSErr HMGetIndHelpMsg(ResType whichType, short whichResID, short whichMsg,
                     short whichState, long *options, Point *tip, Rect *altRect,
                     short *theProc, short *variant, HMMessageRecord *aHelpMsg,
                     short *count);
OSErr HMExtractHelpMsg(ResType whichType, short whichResID, short whichMsg,
                      short whichState, HMMessageRecord *aHelpMsg);

/* Font and appearance */
OSErr HMSetFont(short font);
OSErr HMSetFontSize(short fontSize);
OSErr HMGetFont(short *font);
OSErr HMGetFontSize(short *fontSize);

/* Resource management */
OSErr HMSetDialogResID(short resID);
OSErr HMSetMenuResID(short menuID, short resID);
OSErr HMGetDialogResID(short *resID);
OSErr HMGetMenuResID(short menuID, short *resID);

/* Balloon utilities */
OSErr HMBalloonRect(const HMMessageRecord *aHelpMsg, Rect *coolRect);
OSErr HMBalloonPict(const HMMessageRecord *aHelpMsg, PicHandle *coolPict);
OSErr HMGetBalloonWindow(WindowPtr *window);

/* Template scanning */
OSErr HMScanTemplateItems(short whichID, short whichResFile, ResType whichType);

/* Modern help system abstraction */
typedef enum {
    kHMHelpSystemClassic = 0,        /* Classic Mac OS balloon help */
    kHMHelpSystemHTML = 1,           /* HTML Help system */
    kHMHelpSystemWebKit = 2,         /* WebKit-based help */
    kHMHelpSystemAccessible = 3      /* Screen reader accessible help */
} HMHelpSystemType;

/* Modern help configuration */
typedef struct HMModernHelpConfig {
    HMHelpSystemType systemType;     /* Type of help system to use */
    Boolean useAccessibility;        /* Enable accessibility features */
    Boolean useMultiLanguage;        /* Support multiple languages */
    Boolean useSearch;               /* Enable help search */
    Boolean useNavigation;           /* Enable help navigation */
    char helpBaseURL[256];           /* Base URL for web-based help */
    char fallbackLanguage[8];        /* Fallback language code */
} HMModernHelpConfig;

/* Modern help system functions */
OSErr HMConfigureModernHelp(const HMModernHelpConfig *config);
OSErr HMShowModernHelp(const char *helpTopic, const char *anchor);
OSErr HMSearchHelp(const char *searchTerm, Handle *results);
OSErr HMNavigateHelp(const char *linkTarget);

/* Help Manager initialization and shutdown */
OSErr HMInitialize(void);
void HMShutdown(void);

/* Private Help Manager functions (for internal use) */
#ifdef HELP_MANAGER_PRIVATE
OSErr HMCountDITLHelpItems(short ditlID, short *count);
OSErr HMModalDialogMenuSetup(short menuID);
OSErr HMInvalidateSavedBits(WindowPtr theWindow, RgnHandle invalRgn);
OSErr HMTrackModalHelpItems(void);
OSErr HMBalloonBulk(void);
OSErr HMInitHelpMenu(void);
OSErr HMDrawBalloonFrame(const Rect *balloonRect, short variant);
OSErr HMSetupBalloonRgns(const Rect *balloonRect, Point tip,
                        RgnHandle balloonRgn, RgnHandle structRgn,
                        short variant);
#endif

#ifdef __cplusplus
}
#endif

#endif /* HELPMANAGER_H */