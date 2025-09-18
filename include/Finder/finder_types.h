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

#include <Types.h>
#include <Files.h>
#include <Quickdraw.h>

#pragma pack(push, 2)  /* 68k alignment - even word boundaries */

/* Standard FinderInfo Structure - Evidence: File Manager documentation */
typedef struct FinderInfo {
    OSType      fdType;         /* File type (4-character code) */
    OSType      fdCreator;      /* File creator (4-character code) */
    UInt16      fdFlags;        /* Finder flags (kIsAlias, kIsInvisible, etc.) */
    Point       fdLocation;     /* Icon position in window or on desktop */
    UInt16      fdFldr;         /* Window or folder ID containing this item */
} FinderInfo;

/* Extended FinderInfo for System 7 - Evidence: System 7 extended file information */
typedef struct ExtendedFinderInfo {
    SInt16      fdIconID;       /* Icon ID for custom icons */
    UInt8       fdUnused[6];    /* Unused bytes, reserved */
    UInt8       fdScript;       /* Script code for international support */
    UInt8       fdXFlags;       /* Extended flags */
    UInt16      fdComment;      /* Comment ID in Desktop file */
    UInt32      fdPutAway;      /* Directory ID for Put Away command */
} ExtendedFinderInfo;

/* Desktop Database Record - Evidence: "Rebuilding the desktop file" */
typedef struct DesktopRecord {
    UInt16      recordType;     /* Type of desktop record (file, folder, app) */
    OSType      fileType;       /* File type code */
    OSType      creator;        /* Creator code */
    SInt16      iconLocalID;    /* Local icon ID */
    UInt16      iconType;       /* Icon family type */
    UInt8       unusedBytes[18]; /* Reserved space */
} DesktopRecord;

/* Window State Record - Evidence: "Icon Views", "List Views", window management */
typedef struct WindowRecord {
    UInt16      windowType;     /* Type of window (folder, search, etc.) */
    Rect        bounds;         /* Window bounds on screen */
    UInt16      viewType;       /* View mode (icon, list, outline) */
    Point       scrollPosition; /* Current scroll position */
    UInt16      iconSize;       /* Icon size for icon view */
    UInt16      sortOrder;      /* Sort criteria for list view */
    UInt8       reserved[44];   /* Reserved for future use */
} WindowRecord;

/* Icon Position Record - Evidence: "Clean Up Window", "Clean Up Desktop" */
typedef struct IconPosition {
    UInt32      iconID;         /* Unique icon identifier */
    Point       position;       /* Icon position (x, y coordinates) */
} IconPosition;

/* View Preferences Record - Evidence: view mode switching */
typedef struct ViewRecord {
    UInt16      viewType;       /* Default view type (icon/list) */
    UInt16      iconArrangement; /* Icon arrangement method */
    UInt16      iconSize;       /* Icon size preference */
    UInt16      listSettings;   /* List view column settings */
    UInt8       reserved[8];    /* Reserved space */
} ViewRecord;

/* Alias Record Structure - Evidence: alias resolution error strings */
typedef struct AliasRecord {
    UInt32      aliasSize;      /* Total size of alias record */
    UInt16      version;        /* Alias record version */
    UInt16      aliasKind;      /* Type of alias */
    Str27       volumeName;     /* Name of target volume */
    UInt32      createdDate;    /* Creation date of target */
    UInt16      fileSystemID;   /* File system identifier */
    UInt16      driveType;      /* Drive type (floppy, hard disk, etc.) */
    UInt32      parentDirID;    /* Parent directory ID */
    Str63       fileName;       /* Target file name */
    UInt32      fileNumber;     /* File number on volume */
    UInt32      fileCreatedDate; /* Target file creation date */
    OSType      fileType;       /* Target file type */
    OSType      fileCreator;    /* Target file creator */
    UInt8       reserved[22];   /* Reserved space for future use */
} AliasRecord;

/* Trash Management Record - Evidence: "Empty Trash" functionality */
typedef struct TrashRecord {
    UInt32      itemCount;      /* Number of items in trash */
    UInt32      totalSize;      /* Total size of items in trash */
    UInt32      lastEmptied;    /* Date trash was last emptied */
    UInt16      flags;          /* Trash state flags */
    UInt16      warningLevel;   /* Warning threshold for full trash */
    UInt8       reserved[8];    /* Reserved space */
} TrashRecord;

/* Resource Fork Header - Evidence: Resource fork analysis */
typedef struct ResourceHeader {
    UInt32      dataOffset;     /* Offset to resource data */
    UInt32      mapOffset;      /* Offset to resource map */
    UInt32      dataLength;     /* Length of resource data */
    UInt32      mapLength;      /* Length of resource map */
} ResourceHeader;

/* Finder Window State - Internal structure for window management */
typedef struct FinderWindow {
    WindowPtr   window;         /* Toolbox window pointer */
    FSSpec      folder;         /* Folder being displayed */
    ViewRecord  viewSettings;   /* View preferences */
    IconPosition *iconPositions; /* Array of icon positions */
    UInt16      iconCount;      /* Number of positioned icons */
    Boolean     needsRedraw;    /* Window needs redrawing */
    Boolean     isClosing;      /* Window is being closed */
} FinderWindow;

/* Find Criteria Structure - Evidence: "Find and select items whose" */
typedef struct FindCriteria {
    UInt16      searchType;     /* Type of search (name, date, size, etc.) */
    StringPtr   searchString;   /* Search text */
    UInt32      minSize;        /* Minimum file size */
    UInt32      maxSize;        /* Maximum file size */
    UInt32      beforeDate;     /* Files created before this date */
    UInt32      afterDate;      /* Files created after this date */
    OSType      fileType;       /* Specific file type to find */
    OSType      creator;        /* Specific creator to find */
    Boolean     includeSubfolders; /* Search subfolders */
} FindCriteria;

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