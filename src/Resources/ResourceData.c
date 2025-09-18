/*
 * ResourceData.c - System 7 Resource Data Management Implementation
 *
 * Copyright (c) 2024 System7.1-Portable Project
 * MIT License
 */

#include "Resources/ResourceData.h"
#include "Resources/system7_resources.h"
#include "QuickDraw/QuickDraw.h"
#include "MemoryManager/MemoryManager.h"
#include <stdio.h>
#include <string.h>

/* Resource data table */
static ResourceData gResourceTable[] = {
    /* Cursors */
    {kResourceCursor, kArrowCursorID, arrow_cursor, sizeof(arrow_cursor), 16, 16},
    {kResourceCursor, kIBeamCursorID, ibeam_cursor, sizeof(ibeam_cursor), 16, 16},
    {kResourceCursor, kCrosshairCursorID, crosshair_cursor, sizeof(crosshair_cursor), 16, 16},
    {kResourceCursor, kWatchCursorID, watch_cursor, sizeof(watch_cursor), 16, 16},

    /* Icons - 32x32 */
    {kResourceIcon, kCalculatorIconID, calculator_icon, sizeof(calculator_icon), 32, 32},
    {kResourceIcon, kStopIconID, stop_icon, sizeof(stop_icon), 32, 32},
    {kResourceIcon, kNoteIconID, note_icon, sizeof(note_icon), 32, 32},
    {kResourceIcon, kCautionIconID, caution_icon, sizeof(caution_icon), 32, 32},

    /* Icons - 16x16 */
    {kResourceIcon, kFolderIcon16ID, folder_icon_16, sizeof(folder_icon_16), 16, 16},
    {kResourceIcon, kDocumentIcon16ID, document_icon_16, sizeof(document_icon_16), 16, 16},
    {kResourceIcon, kApplicationIcon16ID, application_icon_16, sizeof(application_icon_16), 16, 16},
    {kResourceIcon, kTrashEmptyIcon16ID, trash_empty_16, sizeof(trash_empty_16), 16, 16},
    {kResourceIcon, kTrashFullIcon16ID, trash_full_16, sizeof(trash_full_16), 16, 16},

    /* Patterns */
    {kResourcePattern, kDesktopPatternID, desktop_pattern, sizeof(desktop_pattern), 8, 8},
    {kResourcePattern, kGray25PatternID, gray_pattern_25, sizeof(gray_pattern_25), 8, 8},
    {kResourcePattern, kGray50PatternID, gray_pattern_50, sizeof(gray_pattern_50), 8, 8},
    {kResourcePattern, kGray75PatternID, gray_pattern_75, sizeof(gray_pattern_75), 8, 8},
    {kResourcePattern, kScrollPatternID, scroll_pattern, sizeof(scroll_pattern), 8, 8},

    /* UI Elements */
    {kResourceIcon, kCheckboxEmptyID, checkbox_empty, sizeof(checkbox_empty), 8, 8},
    {kResourceIcon, kCheckboxCheckedID, checkbox_checked, sizeof(checkbox_checked), 8, 8},
    {kResourceIcon, kRadioEmptyID, radio_empty, sizeof(radio_empty), 8, 8},
    {kResourceIcon, kRadioSelectedID, radio_selected, sizeof(radio_selected), 8, 8},

    /* Window controls */
    {kResourceIcon, kCloseBoxID, close_box, sizeof(close_box), 7, 7},
    {kResourceIcon, kZoomBoxID, zoom_box, sizeof(zoom_box), 7, 7},
    {kResourceIcon, kCollapseBoxID, collapse_box, sizeof(collapse_box), 7, 7},

    /* Menu items */
    {kResourceIcon, kAppleLogoID, apple_logo, sizeof(apple_logo), 16, 16},
    {kResourceIcon, kCmdKeySymbolID, cmd_key_symbol, sizeof(cmd_key_symbol), 16, 16},
    {kResourceIcon, kCheckMarkID, check_mark, sizeof(check_mark), 16, 16},

    /* Sounds */
    {kResourceSound, kSystemBeepID, system_beep_data, sizeof(system_beep_data),
     SYSTEM_BEEP_SAMPLE_RATE, SYSTEM_BEEP_LENGTH}
};

#define NUM_RESOURCES (sizeof(gResourceTable) / sizeof(ResourceData))

/* Cursor mask table */
static const unsigned char* gCursorMasks[] = {
    arrow_cursor_mask,      /* kArrowCursorID */
    NULL,                   /* kIBeamCursorID - no mask */
    NULL,                   /* kCrosshairCursorID - no mask */
    NULL                    /* kWatchCursorID - no mask */
};

/* Icon mask table */
static const unsigned char* gIconMasks[] = {
    calculator_icon_mask,   /* kCalculatorIconID */
    NULL,                   /* Others have no separate masks */
};

/* Private state */
static struct {
    Boolean initialized;
    Pattern* patterns[5];   /* Cached pattern handles */
} gResourceState = {false, {NULL}};

/* Initialize resource data system */
OSErr InitResourceData(void) {
    if (gResourceState.initialized) {
        return noErr;
    }

    /* Cache commonly used patterns */
    for (int i = 0; i < 5; i++) {
        gResourceState.patterns[i] = NewPtr(sizeof(Pattern));
        if (!gResourceState.patterns[i]) {
            return memFullErr;
        }
    }

    /* Load patterns into cache */
    memcpy(gResourceState.patterns[0], desktop_pattern, sizeof(Pattern));
    memcpy(gResourceState.patterns[1], gray_pattern_25, sizeof(Pattern));
    memcpy(gResourceState.patterns[2], gray_pattern_50, sizeof(Pattern));
    memcpy(gResourceState.patterns[3], gray_pattern_75, sizeof(Pattern));
    memcpy(gResourceState.patterns[4], scroll_pattern, sizeof(Pattern));

    gResourceState.initialized = true;
    return noErr;
}

/* Get resource data by ID */
const ResourceData* GetResourceData(ResourceDataType type, UInt16 id) {
    for (UInt16 i = 0; i < NUM_RESOURCES; i++) {
        if (gResourceTable[i].type == type && gResourceTable[i].id == id) {
            return &gResourceTable[i];
        }
    }
    return NULL;
}

/* Load cursor from resource data */
CursHandle LoadResourceCursor(UInt16 cursorID) {
    const ResourceData* resData = GetResourceData(kResourceCursor, cursorID);
    if (!resData) {
        return NULL;
    }

    /* Allocate cursor handle */
    CursHandle cursor = (CursHandle)NewHandle(sizeof(Cursor));
    if (!cursor) {
        return NULL;
    }

    /* Copy cursor data */
    Cursor* cursPtr = *cursor;
    memcpy(cursPtr->data, resData->data, 32);  /* 16x16 bits = 32 bytes */

    /* Set mask if available */
    int maskIndex = cursorID - kArrowCursorID;
    if (maskIndex >= 0 && maskIndex < 4 && gCursorMasks[maskIndex]) {
        memcpy(cursPtr->mask, gCursorMasks[maskIndex], 32);
    } else {
        /* Use solid mask */
        memset(cursPtr->mask, 0xFF, 32);
    }

    /* Set hotspot */
    cursPtr->hotSpot = GetCursorHotspot(cursorID);

    return cursor;
}

/* Load icon from resource data */
Handle LoadResourceIcon(UInt16 iconID, Boolean is16x16) {
    const ResourceData* resData = GetResourceData(kResourceIcon, iconID);
    if (!resData) {
        return NULL;
    }

    /* Calculate icon size */
    Size iconSize = (resData->width * resData->height) / 8;

    /* Allocate handle */
    Handle iconHandle = NewHandle(iconSize);
    if (!iconHandle) {
        return NULL;
    }

    /* Copy icon data */
    memcpy(*iconHandle, resData->data, iconSize);

    return iconHandle;
}

/* Load pattern from resource data */
Pattern* LoadResourcePattern(UInt16 patternID) {
    /* Check cache first */
    int cacheIndex = -1;
    switch (patternID) {
        case kDesktopPatternID: cacheIndex = 0; break;
        case kGray25PatternID: cacheIndex = 1; break;
        case kGray50PatternID: cacheIndex = 2; break;
        case kGray75PatternID: cacheIndex = 3; break;
        case kScrollPatternID: cacheIndex = 4; break;
    }

    if (cacheIndex >= 0 && cacheIndex < 5) {
        return gResourceState.patterns[cacheIndex];
    }

    /* Load from resource table */
    const ResourceData* resData = GetResourceData(kResourcePattern, patternID);
    if (!resData) {
        return NULL;
    }

    Pattern* pattern = (Pattern*)NewPtr(sizeof(Pattern));
    if (!pattern) {
        return NULL;
    }

    memcpy(pattern, resData->data, sizeof(Pattern));
    return pattern;
}

/* Load sound from resource data */
Handle LoadResourceSound(UInt16 soundID) {
    const ResourceData* resData = GetResourceData(kResourceSound, soundID);
    if (!resData) {
        return NULL;
    }

    Handle soundHandle = NewHandle(resData->size);
    if (!soundHandle) {
        return NULL;
    }

    memcpy(*soundHandle, resData->data, resData->size);
    return soundHandle;
}

/* Get cursor hotspot */
Point GetCursorHotspot(UInt16 cursorID) {
    Point hotspot = {0, 0};

    switch (cursorID) {
        case kArrowCursorID:
            hotspot.h = ARROW_HOTSPOT_X;
            hotspot.v = ARROW_HOTSPOT_Y;
            break;
        case kIBeamCursorID:
            hotspot.h = IBEAM_HOTSPOT_X;
            hotspot.v = IBEAM_HOTSPOT_Y;
            break;
        case kCrosshairCursorID:
            hotspot.h = CROSSHAIR_HOTSPOT_X;
            hotspot.v = CROSSHAIR_HOTSPOT_Y;
            break;
        case kWatchCursorID:
            hotspot.h = WATCH_HOTSPOT_X;
            hotspot.v = WATCH_HOTSPOT_Y;
            break;
    }

    return hotspot;
}

/* Create bitmap from resource data */
BitMap* CreateBitmapFromResource(const ResourceData* resData) {
    if (!resData || resData->type != kResourceIcon) {
        return NULL;
    }

    BitMap* bitmap = (BitMap*)NewPtr(sizeof(BitMap));
    if (!bitmap) {
        return NULL;
    }

    /* Calculate row bytes (must be even) */
    short rowBytes = (resData->width + 7) / 8;
    if (rowBytes & 1) rowBytes++;

    /* Allocate bitmap data */
    Size dataSize = rowBytes * resData->height;
    bitmap->baseAddr = NewPtr(dataSize);
    if (!bitmap->baseAddr) {
        DisposePtr((Ptr)bitmap);
        return NULL;
    }

    /* Set bitmap fields */
    bitmap->rowBytes = rowBytes;
    SetRect(&bitmap->bounds, 0, 0, resData->width, resData->height);

    /* Copy bitmap data */
    memcpy(bitmap->baseAddr, resData->data, resData->size);

    return bitmap;
}

/* Draw icon at location */
void DrawResourceIcon(UInt16 iconID, short x, short y) {
    const ResourceData* resData = GetResourceData(kResourceIcon, iconID);
    if (!resData) {
        return;
    }

    /* Create temporary bitmap */
    BitMap* iconBitmap = CreateBitmapFromResource(resData);
    if (!iconBitmap) {
        return;
    }

    /* Set destination rectangle */
    Rect destRect;
    SetRect(&destRect, x, y, x + resData->width, y + resData->height);

    /* Copy bits to current port */
    GrafPtr currentPort;
    GetPort(&currentPort);
    CopyBits(iconBitmap, &currentPort->portBits,
             &iconBitmap->bounds, &destRect, srcCopy, NULL);

    /* Clean up */
    DisposePtr(iconBitmap->baseAddr);
    DisposePtr((Ptr)iconBitmap);
}

/* Play sound resource */
OSErr PlayResourceSound(UInt16 soundID) {
    Handle soundHandle = LoadResourceSound(soundID);
    if (!soundHandle) {
        return resNotFound;
    }

    /* TODO: Integrate with Sound Manager when available */
    /* For now, just beep */
    SysBeep(1);

    DisposeHandle(soundHandle);
    return noErr;
}

/* Set resource pattern */
void SetResourcePattern(UInt16 patternID) {
    Pattern* pattern = LoadResourcePattern(patternID);
    if (pattern) {
        /* TODO: Set as current pattern in QuickDraw */
    }
}

/* Fill with resource pattern */
void FillWithResourcePattern(const Rect* rect, UInt16 patternID) {
    Pattern* pattern = LoadResourcePattern(patternID);
    if (pattern) {
        FillRect(rect, pattern);
    }
}

/* Draw menu icon */
void DrawMenuIcon(UInt16 iconID, const Rect* destRect) {
    DrawResourceIcon(iconID, destRect->left, destRect->top);
}

/* Draw checkbox */
void DrawCheckbox(short x, short y, Boolean checked) {
    UInt16 iconID = checked ? kCheckboxCheckedID : kCheckboxEmptyID;
    DrawResourceIcon(iconID, x, y);
}

/* Draw radio button */
void DrawRadioButton(short x, short y, Boolean selected) {
    UInt16 iconID = selected ? kRadioSelectedID : kRadioEmptyID;
    DrawResourceIcon(iconID, x, y);
}

/* Draw window control */
void DrawWindowControl(short x, short y, UInt16 controlID) {
    DrawResourceIcon(controlID, x, y);
}

/* Count resources of type */
UInt16 CountResourcesOfType(ResourceDataType type) {
    UInt16 count = 0;
    for (UInt16 i = 0; i < NUM_RESOURCES; i++) {
        if (gResourceTable[i].type == type) {
            count++;
        }
    }
    return count;
}

/* Get indexed resource */
const ResourceData* GetIndexedResource(ResourceDataType type, UInt16 index) {
    UInt16 count = 0;
    for (UInt16 i = 0; i < NUM_RESOURCES; i++) {
        if (gResourceTable[i].type == type) {
            if (count == index) {
                return &gResourceTable[i];
            }
            count++;
        }
    }
    return NULL;
}

/* Dump resource info */
void DumpResourceInfo(ResourceDataType type, UInt16 id) {
    const ResourceData* resData = GetResourceData(type, id);
    if (resData) {
        printf("Resource ID %d: Type=%d, Size=%ld, Dimensions=%dx%d\n",
               id, type, (long)resData->size, resData->width, resData->height);
    } else {
        printf("Resource ID %d not found\n", id);
    }
}

/* List all resources */
void ListAllResources(void) {
    printf("System 7 Resource Data:\n");
    printf("-----------------------\n");

    const char* typeNames[] = {"Icon", "Cursor", "Pattern", "Sound", "Picture"};

    for (UInt16 i = 0; i < NUM_RESOURCES; i++) {
        printf("  %s #%d: %dx%d, %ld bytes\n",
               typeNames[gResourceTable[i].type],
               gResourceTable[i].id,
               gResourceTable[i].width,
               gResourceTable[i].height,
               (long)gResourceTable[i].size);
    }

    printf("Total: %d resources\n", (int)NUM_RESOURCES);
}