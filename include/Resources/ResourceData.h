/*
 * ResourceData.h - System 7 Resource Data Management
 *
 * Provides access to embedded System 7 resources including icons, cursors,
 * patterns, and sounds extracted from original System 7 resource files.
 *
 * Copyright (c) 2024 System7.1-Portable Project
 * MIT License
 */

#ifndef RESOURCE_DATA_H
#define RESOURCE_DATA_H

#include <Types.h>
#include <QuickDraw.h>

/* Include the extracted System 7 resources */
#include "system7_resources.h"

/* Resource types */
typedef enum {
    kResourceIcon,
    kResourceCursor,
    kResourcePattern,
    kResourceSound,
    kResourcePicture
} ResourceDataType;

/* Resource IDs for standard System 7 resources */
enum {
    /* Cursors */
    kArrowCursorID = 1,
    kIBeamCursorID = 2,
    kCrosshairCursorID = 3,
    kWatchCursorID = 4,

    /* Icons - 32x32 */
    kCalculatorIconID = 128,
    kStopIconID = 129,
    kNoteIconID = 130,
    kCautionIconID = 131,

    /* Icons - 16x16 */
    kFolderIcon16ID = 256,
    kDocumentIcon16ID = 257,
    kApplicationIcon16ID = 258,
    kTrashEmptyIcon16ID = 259,
    kTrashFullIcon16ID = 260,

    /* Patterns */
    kDesktopPatternID = 512,
    kGray25PatternID = 513,
    kGray50PatternID = 514,
    kGray75PatternID = 515,
    kScrollPatternID = 516,

    /* UI Elements */
    kCheckboxEmptyID = 768,
    kCheckboxCheckedID = 769,
    kRadioEmptyID = 770,
    kRadioSelectedID = 771,

    /* Window controls */
    kCloseBoxID = 1024,
    kZoomBoxID = 1025,
    kCollapseBoxID = 1026,

    /* Menu items */
    kAppleLogoID = 1280,
    kCmdKeySymbolID = 1281,
    kCheckMarkID = 1282,

    /* Sounds */
    kSystemBeepID = 2048
};

/* Resource data structure */
typedef struct ResourceData {
    ResourceDataType    type;
    UInt16             id;
    const void*        data;
    Size               size;
    UInt16             width;
    UInt16             height;
} ResourceData;

/* Public API */

/* Initialize resource data system */
OSErr InitResourceData(void);

/* Get resource data by ID */
const ResourceData* GetResourceData(ResourceDataType type, UInt16 id);

/* Load cursor from resource data */
CursHandle LoadResourceCursor(UInt16 cursorID);

/* Load icon from resource data */
Handle LoadResourceIcon(UInt16 iconID, Boolean is16x16);

/* Load pattern from resource data */
Pattern* LoadResourcePattern(UInt16 patternID);

/* Load sound from resource data */
Handle LoadResourceSound(UInt16 soundID);

/* Get cursor hotspot */
Point GetCursorHotspot(UInt16 cursorID);

/* Create bitmap from resource data */
BitMap* CreateBitmapFromResource(const ResourceData* resData);

/* Draw icon at location */
void DrawResourceIcon(UInt16 iconID, short x, short y);

/* Play sound resource */
OSErr PlayResourceSound(UInt16 soundID);

/* Pattern utilities */
void SetResourcePattern(UInt16 patternID);
void FillWithResourcePattern(const Rect* rect, UInt16 patternID);

/* Menu icon utilities */
void DrawMenuIcon(UInt16 iconID, const Rect* destRect);

/* UI element drawing */
void DrawCheckbox(short x, short y, Boolean checked);
void DrawRadioButton(short x, short y, Boolean selected);
void DrawWindowControl(short x, short y, UInt16 controlID);

/* Resource table access */
UInt16 CountResourcesOfType(ResourceDataType type);
const ResourceData* GetIndexedResource(ResourceDataType type, UInt16 index);

/* Debug utilities */
void DumpResourceInfo(ResourceDataType type, UInt16 id);
void ListAllResources(void);

#endif /* RESOURCE_DATA_H */