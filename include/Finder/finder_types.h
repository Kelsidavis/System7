/* Data structures used by Finder components. */

#ifndef __FINDER_TYPES_H__
#define __FINDER_TYPES_H__

#include "SystemTypes.h"

#include "WindowManager/WindowTypes.h"
#include "FileManager.h"
#include "QuickDraw/QuickDraw.h"
#include "FS/hfs_types.h"  /* For VRefNum */

#pragma pack(push, 2)  /* 68k alignment - even word boundaries */

/* Desktop database record. */
typedef struct DesktopRecord {
    SInt16  recordType;     /* 0=file, 1=folder */
    OSType  fileType;       /* File type code */
    OSType  creator;        /* Creator code */
    SInt16  iconLocalID;    /* Local icon ID */
    SInt16  iconType;       /* Icon type */
} DesktopRecord;

/* Icon position record. */
typedef struct IconPosition {
    UInt32  iconID;      /* Unique identifier for the icon */
    Point   position;    /* Position on desktop or in window */
} IconPosition;

/* Desktop item types. */
typedef enum {
    kDesktopItemVolume = 0,  /* Mounted volume/disk */
    kDesktopItemTrash = 1,   /* Trash can */
    kDesktopItemFile = 2,    /* File */
    kDesktopItemFolder = 3,  /* Folder */
    kDesktopItemAlias = 4,   /* Alias/shortcut */
    kDesktopItemApplication = 5  /* Application */
} DesktopItemType;

/* Desktop item details. */
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

/* Trash management record. */
typedef struct TrashRecord {
    UInt16  flags;          /* Trash flags */
    UInt16  itemCount;      /* Number of items in trash */
    UInt32  totalSize;      /* Total size of trash contents */
    UInt16  warningLevel;   /* Warning threshold */
    UInt32  lastEmptied;    /* Last time trash was emptied */
} TrashRecord;

#pragma pack(pop)

#endif /* __FINDER_TYPES_H__ */
