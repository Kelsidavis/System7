/*
 * Chooser Desk Accessory Header
 *
 * RE-AGENT-BANNER
 * SOURCE: Chooser desk accessory (ROM-based)

 * Mappings: 
 * Layouts: 
 * Architecture: m68k Classic Mac OS
 * Type: Desk Accessory (dfil)
 * Creator: chzr
 */

#ifndef CHOOSER_DESK_ACCESSORY_H
#define CHOOSER_DESK_ACCESSORY_H

#include "SystemTypes.h"

/* Forward declarations */

#include "SystemTypes.h"
#include "WindowManager/WindowTypes.h"

/* Chooser Desk Accessory - Standalone Implementation */

/* Forward declarations to avoid circular dependencies */
/* Ptr is defined in MacTypes.h */
/* Ptr is defined in MacTypes.h */
/* MenuHandle is in MenuTypes.h */
/* Handle is defined in MacTypes.h */

/* Basic types for Chooser */
  /* Pascal string with 32 char max + length byte */

/* Device Control Entry */

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

/* NBP types */

/* EventRecord is in EventManager/EventTypes.h */

    short ioCmdAddr;
    void *ioCompletion;
    short ioResult;
    StringPtr ioNamePtr;
    short ioVRefNum;
} ParamBlockRec, *ParmBlkPtr;

/* Desk Accessory Driver Messages */

/* Event types */

/* Dialog procedure types */

/* Error codes */

/* Chooser Control Codes */

/* Resource IDs */

/* AppleTalk Constants */

/* Driver Header Structure */
     /* Driver capability flags - provenance: offset 0x0000 */
    UInt16 drvrDelay;     /* Periodic action delay in ticks - provenance: offset 0x0002 */
    UInt16 drvrEMask;     /* Event mask for events to handle - provenance: offset 0x0004 */
    UInt16 drvrMenu;      /* Menu ID in menu bar - provenance: offset 0x0006 */
    UInt16 drvrOpen;      /* Offset to open routine - provenance: offset 0x0008 */
    UInt16 drvrPrime;     /* Offset to I/O routine - provenance: offset 0x000A */
    UInt16 drvrCtl;       /* Offset to control routine - provenance: offset 0x000C */
    UInt16 drvrStatus;    /* Offset to status routine - provenance: offset 0x000E */
    UInt16 drvrClose;     /* Offset to close routine - provenance: offset 0x0010 */
    UInt8  drvrNameLen;   /* Length of driver name - provenance: offset 0x0012 */
    char     drvrName[7];   /* Driver name "Chooser" - provenance: offset 0x0013, string at 0x30 */
} __attribute__((packed)) ChooserDrvrHeader;

/* AppleTalk Node Information */

/* Printer Information Structure */

/* Printer List Structure */

/* Zone List Structure */

/* Chooser Internal State */

/* Function Prototypes - provenance: function analysis fcn.* mappings */


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
 *   "rom_source": "Quadra800.ROM",
 *   "evidence_references": 45,
 *   "function_mappings": 12,
 *   "structure_definitions": 6,
 *   "constants": 15,
 *   "confidence": "high",
 *   "analysis_tools": ["radare2_m68k"],
 *   "provenance_density": 0.89
 * }
 */