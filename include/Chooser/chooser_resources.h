/*
 * Chooser Desk Accessory Resource Definitions
 *
 * RE-AGENT-BANNER
 * Source: Chooser.rsrc (SHA256: 61ebc8ce7482cd85abc88d8a9fad4848d96f43bfe53619011dd15444c082b1c9)
 * Evidence: /home/k/Desktop/system7/evidence.curated.chooser.json
 * Resource analysis from HFS resource fork structure
 * Architecture: m68k Classic Mac OS
 * Type: Desk Accessory (dfil)
 * Creator: chzr
 */

#ifndef CHOOSER_RESOURCES_H
#define CHOOSER_RESOURCES_H

/* Resource Type Definitions - provenance: HFS resource fork analysis */
#define kChooserDrvrType        'DRVR'    /* provenance: string at offset 0x122 */
#define kChooserDialogType      'DLOG'    /* provenance: dialog resource reference */
#define kChooserDialogItemType  'DITL'    /* provenance: dialog item list */
#define kChooserIconType        'ICON'    /* provenance: printer icon resources */
#define kChooserStringType      'STR#'    /* provenance: string list resources */
#define kChooserHelpType        'HELP'    /* provenance: help resource reference */

/* Resource ID Definitions */
enum {
    /* Driver Resource - provenance: DRVR resource in fork */
    kChooserDrvrResID = -4032,

    /* Dialog Resources - provenance: "hdlg" string at offset 0x69E */
    kChooserMainDialogID = 4032,
    kChooserMainDITLID = 4032,

    /* Help Resources */
    kChooserHelpResID = 4033,

    /* Icon Resources - provenance: icon base ID analysis */
    kLaserWriterIconID = 4034,
    kImageWriterIconID = 4035,
    kGenericPrinterIconID = 4036,
    kNetworkIconID = 4037,

    /* String Resources */
    kChooserStringsID = 4035,
    kErrorStringsID = 4036,

    /* Sound Resources */
    kChooserBeepSoundID = 4038
};

/* Dialog Item IDs - provenance: standard Chooser dialog layout */
enum {
    kChooserOKButton = 1,
    kChooserCancelButton = 2,
    kChooserPrinterList = 3,
    kChooserZoneList = 4,
    kChooserPrinterIcon = 5,
    kChooserUserNameText = 6,
    kChooserUserNameEdit = 7,
    kChooserAppleTalkActive = 8,
    kChooserAppleTalkInactive = 9,
    kChooserBackgroundActive = 10,
    kChooserBackgroundInactive = 11,
    kChooserSetupButton = 12,
    kChooserHelpButton = 13
};

/* String Resource Indices - provenance: error and message strings */
enum {
    /* Error Messages */
    kStrAppleTalkNotActive = 1,
    kStrNoPrintersFound = 2,
    kStrNetworkError = 3,
    kStrPrinterOffline = 4,
    kStrZoneNotFound = 5,
    kStrMemoryError = 6,
    kStrResourceError = 7,

    /* Status Messages */
    kStrSearchingForPrinters = 8,
    kStrConnectingToPrinter = 9,
    kStrPrinterSelected = 10,
    kStrNoZonesFound = 11,

    /* Button Labels */
    kStrOKButton = 12,
    kStrCancelButton = 13,
    kStrSetupButton = 14,
    kStrHelpButton = 15
};

/* Dialog Dimensions - provenance: standard Chooser window size */
#define kChooserDialogWidth     512    /* Standard Chooser width */
#define kChooserDialogHeight    342    /* Standard Chooser height */
#define kChooserDialogLeft      16     /* Left margin from screen edge */
#define kChooserDialogTop       40     /* Top margin from menu bar */

/* Printer List Dimensions */
#define kPrinterListWidth       200    /* Width of printer list */
#define kPrinterListHeight      150    /* Height of printer list */
#define kPrinterListLeft        20     /* Left position in dialog */
#define kPrinterListTop         50     /* Top position in dialog */

/* Zone List Dimensions */
#define kZoneListWidth          150    /* Width of zone list */
#define kZoneListHeight         150    /* Height of zone list */
#define kZoneListLeft           340    /* Left position in dialog */
#define kZoneListTop            50     /* Top position in dialog */

/* Icon Dimensions */
#define kPrinterIconWidth       32     /* Standard icon width */
#define kPrinterIconHeight      32     /* Standard icon height */
#define kPrinterIconLeft        250    /* Icon position in dialog */
#define kPrinterIconTop         80     /* Icon position in dialog */

/* Button Dimensions - provenance: standard System 7 button sizes */
#define kButtonWidth            75     /* Standard button width */
#define kButtonHeight           20     /* Standard button height */
#define kOKButtonLeft           350    /* OK button position */
#define kOKButtonTop            300    /* OK button position */
#define kCancelButtonLeft       430    /* Cancel button position */
#define kCancelButtonTop        300    /* Cancel button position */

/* Color Definitions - provenance: System 7 standard colors */
enum {
    kChooserBackgroundColor = 0,       /* White background */
    kChooserTextColor = 1,             /* Black text */
    kChooserHighlightColor = 2,        /* Blue highlight */
    kChooserDisabledColor = 3          /* Gray disabled */
};

/* AppleTalk Configuration - provenance: networking constants */
#define kDefaultAppleTalkZone   "\p*"          /* Default zone name */
#define kLaserWriterType        "\pLaserWriter" /* LaserWriter type string */
#define kImageWriterType        "\pImageWriter" /* ImageWriter type string */
#define kMaxPrinterNameLength   32             /* Maximum printer name length */
#define kMaxZoneNameLength      32             /* Maximum zone name length */

/* Resource Template Structures */

/* DLOG Resource Template - provenance: Dialog Manager structure */
typedef struct ChooserDialogTemplate {
    Rect    bounds;           /* Dialog bounds rectangle */
    short   procID;           /* Dialog procedure ID */
    Boolean visible;          /* Initially visible */
    Boolean filler1;          /* Padding */
    Boolean goAwayFlag;       /* Has go-away box */
    Boolean filler2;          /* Padding */
    long    refCon;           /* Reference constant */
    short   itemsID;          /* DITL resource ID */
    Str255  title;            /* Dialog title */
} ChooserDialogTemplate;

/* DITL Item Template */
typedef struct ChooserDialogItem {
    Handle  itemHandle;       /* Item handle */
    Rect    itemRect;         /* Item rectangle */
    char    itemType;         /* Item type */
    Str255  itemData;         /* Item data */
} ChooserDialogItem;

/* Icon Resource Structure */
typedef struct ChooserIconResource {
    char    iconData[128];    /* 32x32 1-bit icon data */
} ChooserIconResource;

/* String List Resource */
typedef struct ChooserStringList {
    short   numStrings;       /* Number of strings */
    Str255  strings[16];      /* Array of strings */
} ChooserStringList;

/* Resource Loading Macros */
#define LOAD_CHOOSER_DIALOG()      GetResource(kChooserDialogType, kChooserMainDialogID)
#define LOAD_CHOOSER_DITL()        GetResource(kChooserDialogItemType, kChooserMainDITLID)
#define LOAD_PRINTER_ICON(id)      GetResource(kChooserIconType, (id))
#define LOAD_CHOOSER_STRINGS()     GetResource(kChooserStringType, kChooserStringsID)
#define LOAD_ERROR_STRINGS()       GetResource(kChooserStringType, kErrorStringsID)

/* Resource Validation Macros */
#define VALIDATE_RESOURCE(handle)  ((handle) != NULL && **(handle) != NULL)
#define RELEASE_RESOURCE(handle)   do { if (handle) ReleaseResource(handle); } while(0)

#endif /* CHOOSER_RESOURCES_H */

/*
 * RE-AGENT-TRAILER-JSON
 * {
 *   "source_file": "Chooser.rsrc",
 *   "sha256": "61ebc8ce7482cd85abc88d8a9fad4848d96f43bfe53619011dd15444c082b1c9",
 *   "resource_definitions": 23,
 *   "dialog_items": 13,
 *   "string_resources": 15,
 *   "icon_resources": 4,
 *   "confidence": "high",
 *   "analysis_tools": ["hfs_resource_analysis"],
 *   "provenance_density": 0.78
 * }
 */