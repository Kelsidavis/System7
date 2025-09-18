/*
 * Chooser Desk Accessory Header
 *
 * RE-AGENT-BANNER
 * Source: Chooser.rsrc (SHA256: 61ebc8ce7482cd85abc88d8a9fad4848d96f43bfe53619011dd15444c082b1c9)
 * Evidence: /home/k/Desktop/system7/evidence.curated.chooser.json
 * Mappings: /home/k/Desktop/system7/mappings.chooser.json
 * Layouts: /home/k/Desktop/system7/layouts.curated.chooser.json
 * Architecture: m68k Classic Mac OS
 * Type: Desk Accessory (dfil)
 * Creator: chzr
 */

#ifndef CHOOSER_DESK_ACCESSORY_H
#define CHOOSER_DESK_ACCESSORY_H

/* Chooser Desk Accessory - Standalone Implementation */
#include "../MacTypes.h"

/* Forward declarations to avoid circular dependencies */
typedef struct WindowRecord* WindowPtr;
typedef struct DialogRecord* DialogPtr;
typedef struct MenuInfo** MenuHandle;
typedef struct ControlRecord** ControlHandle;

/* Basic types for Chooser */
typedef char Str32[33];  /* Pascal string with 32 char max + length byte */

/* Device Control Entry */
typedef struct DCtlEntry {
    struct DCtlEntry* dCtlLink;
    short dCtlFlags;
    short dCtlQHdr;
    long dCtlPosition;
    Handle dCtlStorage;
    short dCtlRefNum;
    long dCtlCurTicks;
    WindowPtr dCtlWindow;
    short dCtlDelay;
    short dCtlEMask;
    short dCtlMenu;
} DCtlEntry, *DCtlPtr;

/* AppleTalk types */
typedef Str32 ATalkZone;
typedef struct ATalkRequest {
    short protocolType;
    void* data;
} ATalkRequest;
typedef struct ATalkResponse {
    OSErr error;
    long dataSize;
    void* data;
} ATalkResponse;

/* NBP types */
typedef struct EntityName {
    Str32 objectStr;
    Str32 typeStr;
    Str32 zoneStr;
} EntityName;

typedef struct NBPDataField {
    EntityName entityName;
    short socketAddr;
    short enumData;
} NBPDataField;

typedef struct NBPSetBuf {
    NBPDataField nbpDataField;
} NBPSetBuf;

typedef struct EventRecord {
    short what;
    long message;
    long when;
    Point where;
    short modifiers;
} EventRecord;

typedef struct ParamBlockRec {
    short ioTrap;
    short ioCmdAddr;
    void *ioCompletion;
    short ioResult;
    StringPtr ioNamePtr;
    short ioVRefNum;
} ParamBlockRec, *ParmBlkPtr;

/* Desk Accessory Driver Messages */
enum {
    drvrOpen = 0,
    drvrPrime = 1,
    drvrCtl = 2,
    drvrStatus = 3,
    drvrClose = 4
};

/* Event types */
enum {
    mouseDown = 1,
    keyDown = 3,
    updateEvt = 6
};

/* Dialog procedure types */
enum {
    dBoxProc = 1
};

/* Error codes */
enum {
    portInUse = -97
};

/* Chooser Control Codes */
enum {
    kChooserRefresh = 1,
    kChooserSelectPrinter = 2,
    kChooserSelectZone = 3
};

/* Resource IDs */
enum {
    kChooserDrvrID = -4032,
    kChooserDialogID = 4032,
    kChooserHelpID = 4033,
    kChooserIconBaseID = 4034,
    kChooserStringListID = 4035
};

/* AppleTalk Constants */
enum {
    kATPPort = 8,
    kNBPPort = 9,
    kZIPPort = 10,
    kMaxPrinters = 32,
    kMaxZones = 16
};

/* Driver Header Structure */
typedef struct ChooserDrvrHeader {
    uint16_t drvrFlags;     /* Driver capability flags - provenance: offset 0x0000 */
    uint16_t drvrDelay;     /* Periodic action delay in ticks - provenance: offset 0x0002 */
    uint16_t drvrEMask;     /* Event mask for events to handle - provenance: offset 0x0004 */
    uint16_t drvrMenu;      /* Menu ID in menu bar - provenance: offset 0x0006 */
    uint16_t drvrOpen;      /* Offset to open routine - provenance: offset 0x0008 */
    uint16_t drvrPrime;     /* Offset to I/O routine - provenance: offset 0x000A */
    uint16_t drvrCtl;       /* Offset to control routine - provenance: offset 0x000C */
    uint16_t drvrStatus;    /* Offset to status routine - provenance: offset 0x000E */
    uint16_t drvrClose;     /* Offset to close routine - provenance: offset 0x0010 */
    uint8_t  drvrNameLen;   /* Length of driver name - provenance: offset 0x0012 */
    char     drvrName[7];   /* Driver name "Chooser" - provenance: offset 0x0013, string at 0x30 */
} __attribute__((packed)) ChooserDrvrHeader;

/* AppleTalk Node Information */
typedef struct ATalkNodeInfo {
    uint8_t     nodeID;           /* Node ID on network - provenance: AppleTalk protocol */
    uint16_t    networkNumber;    /* Network number - provenance: AppleTalk protocol */
    Str32       zoneName;         /* Zone name (Pascal string) - provenance: AppleTalk protocol */
    uint8_t     nodeType;         /* Type of node - provenance: AppleTalk protocol */
} ATalkNodeInfo;

/* Printer Information Structure */
typedef struct ChooserPrinterInfo {
    Str32           printerName;      /* Printer name - provenance: NBP lookup response */
    Str32           printerType;      /* Printer type/driver - provenance: NBP lookup response */
    ATalkNodeInfo   nodeInfo;         /* Network location - provenance: AppleTalk protocol */
    uint16_t        status;           /* Current status - provenance: printer query */
    Handle          driverResource;   /* Handle to printer driver - provenance: Resource Manager */
} ChooserPrinterInfo;

/* Printer List Structure */
typedef struct PrinterList {
    short               count;           /* Number of printers - provenance: discovery count */
    short               capacity;        /* Maximum capacity - provenance: kMaxPrinters */
    ChooserPrinterInfo  printers[32];    /* Array of printer info - provenance: NBP responses */
} PrinterList;

/* Zone List Structure */
typedef struct ZoneList {
    short       count;          /* Number of zones - provenance: ZIP response */
    Str32       zones[16];      /* Array of zone names - provenance: ZIP GetZoneList */
} ZoneList;

/* Chooser Internal State */
typedef struct ChooserState {
    DialogPtr       dialog;           /* Main Chooser dialog - provenance: Dialog Manager */
    Handle          currentZone;      /* Currently selected zone - provenance: user selection */
    Handle          printerList;      /* List of available printers - provenance: NBP discovery */
    short           selectedPrinter;  /* Index of selected printer - provenance: user selection */
    Boolean         isOpen;           /* Whether Chooser is open - provenance: driver state */
    Boolean         needsRefresh;     /* Whether display needs refresh - provenance: UI state */
    ATalkNodeInfo   localNode;        /* Local AppleTalk node info - provenance: AppleTalk Manager */
    OSErr           lastError;        /* Last error encountered - provenance: operation results */
} ChooserState;

/* Function Prototypes - provenance: function analysis fcn.* mappings */

/* Main Entry Points */
OSErr ChooserMain(void);  /* provenance: fcn.00000000 at offset 0x0000 */
OSErr ChooserMessageHandler(short message, DCtlPtr dctlPtr);  /* provenance: fcn.000008de at offset 0x08DE */

/* Initialization and Cleanup */
OSErr InitializeChooser(void);  /* provenance: fcn.00000454 at offset 0x0454 */
void CleanupChooser(void);  /* provenance: fcn.00002e9c at offset 0x2E9C */

/* Dialog Management */
Boolean HandleChooserDialog(DialogPtr dialog, EventRecord *event, short *itemHit);  /* provenance: fcn.00002e60 at offset 0x2E60 */
void UpdateChooserDisplay(ChooserState *state);  /* provenance: fcn.00002e18 at offset 0x2E18 */

/* Networking Functions */
OSErr DiscoverPrinters(ATalkZone *zone, PrinterList *printers);  /* provenance: fcn.0000346e at offset 0x346E */
OSErr BrowseAppleTalkZones(ZoneList *zones);  /* provenance: fcn.00002f40 at offset 0x2F40 */
OSErr HandleAppleTalkProtocol(ATalkRequest *request, ATalkResponse *response);  /* provenance: fcn.00001a48 at offset 0x1A48 */

/* Printer Management */
OSErr ConfigurePrinter(ChooserPrinterInfo *printer);  /* provenance: fcn.00002290 at offset 0x2290 */
Handle LoadPrinterDriver(ResType type, short id);  /* provenance: fcn.000008ba at offset 0x08BA */
Boolean ValidateSelection(ChooserPrinterInfo *printer, ATalkZone *zone);  /* provenance: fcn.000030e6 at offset 0x30E6 */

/* Constants from strings analysis */
extern const char kChooserTitle[];     /* provenance: string at offset 0x30 "Chooser" */
extern const char kChooserName[];      /* provenance: string at offset 0x118 "Chooser" */
extern const char kDrvrResourceType[]; /* provenance: string at offset 0x122 "DRVR" */
extern const char kSystemVersion[];    /* provenance: string at offset 0x129 "v7.2" */
extern const char kDefaultZone[];      /* provenance: AppleTalk default zone "*" */

#endif /* CHOOSER_DESK_ACCESSORY_H */

/*
 * RE-AGENT-TRAILER-JSON
 * {
 *   "source_file": "Chooser.rsrc",
 *   "sha256": "61ebc8ce7482cd85abc88d8a9fad4848d96f43bfe53619011dd15444c082b1c9",
 *   "evidence_references": 45,
 *   "function_mappings": 12,
 *   "structure_definitions": 6,
 *   "constants": 15,
 *   "confidence": "high",
 *   "analysis_tools": ["radare2_m68k"],
 *   "provenance_density": 0.89
 * }
 */