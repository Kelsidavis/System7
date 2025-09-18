/*
 * CursorManager.c - QuickDraw Cursor Management with System 7 Resources
 *
 * Manages standard System 7 cursors using embedded resource data.
 *
 * Copyright (c) 2024 System7.1-Portable Project
 * MIT License
 */

#include "QuickDraw/QuickDraw.h"
#include "Resources/ResourceData.h"
#include "MemoryManager/MemoryManager.h"
#include <string.h>

/* Standard cursor handles */
static CursHandle gArrowCursor = NULL;
static CursHandle gIBeamCursor = NULL;
static CursHandle gCrosshairCursor = NULL;
static CursHandle gWatchCursor = NULL;

/* Current cursor state */
static struct {
    CursHandle currentCursor;
    Boolean visible;
    Point hotSpot;
} gCursorState = {NULL, true, {0, 0}};

/* Initialize standard cursors from resources */
OSErr InitStandardCursors(void) {
    OSErr err;

    /* Initialize resource data system */
    err = InitResourceData();
    if (err != noErr) {
        return err;
    }

    /* Load standard cursors */
    gArrowCursor = LoadResourceCursor(kArrowCursorID);
    if (!gArrowCursor) {
        return memFullErr;
    }

    gIBeamCursor = LoadResourceCursor(kIBeamCursorID);
    if (!gIBeamCursor) {
        DisposeHandle((Handle)gArrowCursor);
        return memFullErr;
    }

    gCrosshairCursor = LoadResourceCursor(kCrosshairCursorID);
    if (!gCrosshairCursor) {
        DisposeHandle((Handle)gArrowCursor);
        DisposeHandle((Handle)gIBeamCursor);
        return memFullErr;
    }

    gWatchCursor = LoadResourceCursor(kWatchCursorID);
    if (!gWatchCursor) {
        DisposeHandle((Handle)gArrowCursor);
        DisposeHandle((Handle)gIBeamCursor);
        DisposeHandle((Handle)gCrosshairCursor);
        return memFullErr;
    }

    /* Set arrow as default cursor */
    gCursorState.currentCursor = gArrowCursor;
    gCursorState.hotSpot = GetCursorHotspot(kArrowCursorID);

    return noErr;
}

/* Set cursor to arrow */
void InitCursor(void) {
    if (!gArrowCursor) {
        InitStandardCursors();
    }

    if (gArrowCursor) {
        SetCursor(*gArrowCursor);
        gCursorState.currentCursor = gArrowCursor;
        gCursorState.hotSpot = GetCursorHotspot(kArrowCursorID);
    }
}

/* Set cursor from resource ID */
void SetCursorFromResource(UInt16 cursorID) {
    CursHandle cursor = NULL;

    /* Check for standard cursors */
    switch (cursorID) {
        case kArrowCursorID:
            cursor = gArrowCursor;
            break;
        case kIBeamCursorID:
            cursor = gIBeamCursor;
            break;
        case kCrosshairCursorID:
            cursor = gCrosshairCursor;
            break;
        case kWatchCursorID:
            cursor = gWatchCursor;
            break;
        default:
            /* Try to load custom cursor */
            cursor = LoadResourceCursor(cursorID);
            break;
    }

    if (cursor) {
        SetCursor(*cursor);
        gCursorState.currentCursor = cursor;
        gCursorState.hotSpot = GetCursorHotspot(cursorID);
    }
}

/* Set I-beam cursor for text editing */
void SetIBeamCursor(void) {
    SetCursorFromResource(kIBeamCursorID);
}

/* Set crosshair cursor for precision work */
void SetCrosshairCursor(void) {
    SetCursorFromResource(kCrosshairCursorID);
}

/* Set watch cursor for waiting */
void SetWatchCursor(void) {
    SetCursorFromResource(kWatchCursorID);
}

/* Get current cursor */
CursHandle GetCurrentCursor(void) {
    return gCursorState.currentCursor;
}

/* Get cursor hotspot */
Point GetCurrentHotspot(void) {
    return gCursorState.hotSpot;
}

/* Show/hide cursor */
void ShowCursor(void) {
    if (!gCursorState.visible) {
        gCursorState.visible = true;
        /* TODO: Call HAL to show cursor */
    }
}

void HideCursor(void) {
    if (gCursorState.visible) {
        gCursorState.visible = false;
        /* TODO: Call HAL to hide cursor */
    }
}

/* Obscure cursor until mouse moves */
void ObscureCursor(void) {
    /* Temporarily hide cursor until mouse movement */
    HideCursor();
    /* TODO: Set flag to show on next mouse move */
}

/* Spin the watch cursor (animated wait) */
void SpinCursor(short increment) {
    static short spinPhase = 0;

    if (gCursorState.currentCursor == gWatchCursor) {
        spinPhase = (spinPhase + increment) & 7;  /* 8 phases */

        /* TODO: Rotate watch cursor based on phase */
        /* For now, just ensure watch cursor is set */
        SetCursor(*gWatchCursor);
    }
}

/* Clean up cursor resources */
void CleanupCursors(void) {
    if (gArrowCursor) {
        DisposeHandle((Handle)gArrowCursor);
        gArrowCursor = NULL;
    }
    if (gIBeamCursor) {
        DisposeHandle((Handle)gIBeamCursor);
        gIBeamCursor = NULL;
    }
    if (gCrosshairCursor) {
        DisposeHandle((Handle)gCrosshairCursor);
        gCrosshairCursor = NULL;
    }
    if (gWatchCursor) {
        DisposeHandle((Handle)gWatchCursor);
        gWatchCursor = NULL;
    }

    gCursorState.currentCursor = NULL;
}

/* Create custom cursor from bitmap */
CursHandle CreateCustomCursor(const BitMap* bitmap, const BitMap* mask, Point hotSpot) {
    if (!bitmap || bitmap->bounds.right - bitmap->bounds.left != 16 ||
        bitmap->bounds.bottom - bitmap->bounds.top != 16) {
        return NULL;
    }

    CursHandle cursor = (CursHandle)NewHandle(sizeof(Cursor));
    if (!cursor) {
        return NULL;
    }

    Cursor* cursPtr = *cursor;

    /* Copy bitmap data (16x16 = 32 bytes) */
    short rowBytes = bitmap->rowBytes;
    Ptr src = bitmap->baseAddr;
    UInt16* dest = (UInt16*)cursPtr->data;

    for (int row = 0; row < 16; row++) {
        *dest++ = *(UInt16*)(src + row * rowBytes);
    }

    /* Copy mask data if provided */
    if (mask) {
        src = mask->baseAddr;
        dest = (UInt16*)cursPtr->mask;
        rowBytes = mask->rowBytes;

        for (int row = 0; row < 16; row++) {
            *dest++ = *(UInt16*)(src + row * rowBytes);
        }
    } else {
        /* Use solid mask */
        memset(cursPtr->mask, 0xFF, 32);
    }

    /* Set hotspot */
    cursPtr->hotSpot = hotSpot;

    return cursor;
}