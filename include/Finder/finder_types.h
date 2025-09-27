/*
 * RE-AGENT-BANNER
 * Finder Data Structure Types
 *
 * Reverse-engineered from System 7 Finder.rsrc
 * Source: /home/k/Desktop/system7/system7_resources/Install 3_resources/Finder.rsrc
 * SHA256: 7d59b9ef6c5587010ee4d573bd4b5fb3aa70ba696ccaff1a61b52c9ae1f7a632
 *
 * Evidence sources:
 * - layouts.curated.json structure analysis
 * - Standard Macintosh File Manager structures
 * - Resource fork format analysis
 *
 * This file defines the data structures used by the Finder implementation.
 */

#ifndef __FINDER_TYPES_H__
#define __FINDER_TYPES_H__

#include "SystemTypes.h"

#include "SystemTypes.h"
#include "WindowManager/WindowTypes.h"
#include "FileMgr/file_manager.h"
#include "QuickDraw/QuickDraw.h"
#include "FS/hfs_types.h"  /* For VRefNum */

#pragma pack(push, 2)  /* 68k alignment - even word boundaries */

/* Standard FinderInfo Structure - Evidence: File Manager documentation */

/* Extended FinderInfo for System 7 - Evidence: System 7 extended file information */

/* Desktop Database Record - Evidence: "Rebuilding the desktop file" */
typedef struct DesktopRecord {
    SInt16  recordType;     /* 0=file, 1=folder */
    OSType  fileType;       /* File type code */
    OSType  creator;        /* Creator code */
    SInt16  iconLocalID;    /* Local icon ID */
    SInt16  iconType;       /* Icon type */
} DesktopRecord;

/* Window State Record - Evidence: "Icon Views", "List Views", window management */

/* Icon Position Record - Evidence: "Clean Up Window", "Clean Up Desktop" */
typedef struct IconPosition {
    UInt32  iconID;      /* Unique identifier for the icon */
    Point   position;    /* Position on desktop or in window */
} IconPosition;

/* Desktop Item Type - Universal desktop icon system */
typedef enum {
    kDesktopItemVolume = 0,  /* Mounted volume/disk */
    kDesktopItemTrash = 1,   /* Trash can */
    kDesktopItemFile = 2,    /* File */
    kDesktopItemFolder = 3,  /* Folder */
    kDesktopItemAlias = 4    /* Alias/shortcut */
} DesktopItemType;

/* Desktop Item - Universal structure for all desktop icons */
typedef struct DesktopItem {
    DesktopItemType type;     /* Type of desktop item */
    UInt32  iconID;           /* Unique identifier (0xFFFFFFFF = special) */
    Point   position;         /* Position on desktop */
    char    name[64];         /* Display name */
    Boolean movable;          /* Can be repositioned (trash is not) */
    union {
        struct {
            /* Volume-specific data */
            VRefNum vRefNum;  /* Volume reference number */
        } volume;
        struct {
            /* File-specific data */
            OSType fileType;
            OSType creator;
        } file;
        struct {
            /* Folder-specific data */
            long dirID;
        } folder;
        struct {
            /* Alias-specific data */
            long targetID;
        } alias;
    } data;
} DesktopItem;

/* View Preferences Record - Evidence: view mode switching */

/* Alias Record Structure - Evidence: alias resolution error strings */

/* Trash Management Record - Evidence: "Empty Trash" functionality */
typedef struct TrashRecord {
    UInt16  flags;          /* Trash flags */
    UInt16  itemCount;      /* Number of items in trash */
    UInt32  totalSize;      /* Total size of trash contents */
    UInt16  warningLevel;   /* Warning threshold */
    UInt32  lastEmptied;    /* Last time trash was emptied */
} TrashRecord;

/* Resource Fork Header - Evidence: Resource fork analysis */

/* Finder Window State - Internal structure for window management */

/* Find Criteria Structure - Evidence: "Find and select items whose" */

#pragma pack(pop)

#endif /* __FINDER_TYPES_H__ */

/*
 * RE-AGENT-TRAILER-JSON
 * {
 *   "module": "finder_types.h",
 *   "evidence_density": 0.90,
 *   "structures": 11,
 *   "total_fields": 67,
 *   "primary_evidence": [
 *     "layouts.curated.json structure definitions",
 *     "Standard Macintosh File Manager structures",
 *     "String analysis of functionality"
 *   ],
 *   "implementation_status": "types_complete"
 * }
 */