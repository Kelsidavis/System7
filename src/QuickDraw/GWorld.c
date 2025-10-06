/*
 * GWorld.c - Offscreen Graphics World Implementation
 *
 * Implements offscreen bitmap rendering for double-buffering and compositing.
 * Provides NewGWorld, DisposeGWorld, SetGWorld, LockPixels, and related functions.
 *
 * Copyright (c) 2025 - System 7.1 Portable Project
 * Derived from System 7 ROM analysis (Ghidra) QuickDraw
 */

#include "SystemTypes.h"
#include "QuickDraw/QuickDraw.h"
#include "QuickDraw/ColorQuickDraw.h"
#include "QuickDrawConstants.h"
#include "MemoryMgr/MemoryManager.h"
#include "System71StdLib.h"
#include <string.h>

/* Current GWorld state */
static CGrafPtr g_currentGWorld = NULL;
static GDHandle g_currentGDevice = NULL;

/* External QuickDraw globals */
extern GrafPtr g_currentPort;

/*
 * NewGWorld - Create new offscreen graphics world
 *
 * Parameters:
 *   offscreenGWorld - receives new GWorld handle
 *   pixelDepth - bits per pixel (1, 2, 4, 8, 16, 32)
 *   boundsRect - dimensions of offscreen buffer
 *   cTable - color table (NULL for default)
 *   aGDevice - graphics device (NULL for main screen)
 *   flags - creation flags
 *
 * Returns: noErr on success, error code otherwise
 */
OSErr NewGWorld(GWorldPtr *offscreenGWorld, SInt16 pixelDepth,
                const Rect *boundsRect, CTabHandle cTable,
                GDHandle aGDevice, GWorldFlags flags) {
    extern void serial_logf(SystemLogModule module, SystemLogLevel level, const char* fmt, ...);
    serial_logf((SystemLogModule)3, (SystemLogLevel)2, "[GWORLD] NewGWorld called: depth=%d bounds=(%d,%d,%d,%d)\n",
               pixelDepth, boundsRect ? boundsRect->left : -1,
               boundsRect ? boundsRect->top : -1,
               boundsRect ? boundsRect->right : -1,
               boundsRect ? boundsRect->bottom : -1);

    if (!offscreenGWorld || !boundsRect) {
        serial_logf((SystemLogModule)3, (SystemLogLevel)2, "[GWORLD] NewGWorld: paramErr (null params)\n");
        return paramErr;
    }

    /* Calculate dimensions */
    SInt16 width = boundsRect->right - boundsRect->left;
    SInt16 height = boundsRect->bottom - boundsRect->top;

    if (width <= 0 || height <= 0) {
        return paramErr;
    }

    /* Validate pixel depth */
    if (pixelDepth != 1 && pixelDepth != 2 && pixelDepth != 4 &&
        pixelDepth != 8 && pixelDepth != 16 && pixelDepth != 32) {
        return paramErr;
    }

    /* Allocate CGrafPort structure */
    CGrafPtr gworld = (CGrafPtr)NewPtr(sizeof(CGrafPort));
    if (!gworld) {
        serial_logf((SystemLogModule)3, (SystemLogLevel)2, "[GWORLD] NewGWorld: Failed to allocate CGrafPort\n");
        return memFullErr;
    }

    /* Initialize port structure */
    memset(gworld, 0, sizeof(CGrafPort));
    gworld->portRect = *boundsRect;
    gworld->portVersion = 0xC000;  /* Color port version */

    /* Allocate PixMap */
    PixMapHandle pmHandle = NewPixMap();
    if (!pmHandle || !*pmHandle) {
        serial_logf((SystemLogModule)3, (SystemLogLevel)2, "[GWORLD] NewGWorld: Failed to allocate PixMap\n");
        DisposePtr((Ptr)gworld);
        return memFullErr;
    }

    PixMapPtr pm = *pmHandle;

    /* Calculate rowBytes (must be even, with high bit set for PixMap) */
    SInt16 rowBytes = ((width * pixelDepth + 15) / 16) * 2;
    pm->rowBytes = rowBytes | 0x8000;  /* Set high bit to indicate PixMap */
    pm->bounds = *boundsRect;
    pm->pmVersion = 0;
    pm->packType = 0;
    pm->packSize = 0;
    pm->hRes = 0x00480000;  /* 72 dpi */
    pm->vRes = 0x00480000;  /* 72 dpi */
    pm->pixelType = 0;      /* Chunky pixel format */
    pm->pixelSize = pixelDepth;
    pm->cmpCount = (pixelDepth == 32) ? 3 : 1;
    pm->cmpSize = (pixelDepth == 32) ? 8 : pixelDepth;
    pm->pmTable = NULL;
    pm->pmReserved = 0;

    /* Allocate pixel buffer */
    UInt32 bufferSize = (UInt32)height * (UInt32)rowBytes;
    Ptr pixelBuffer = NewPtr(bufferSize);
    if (!pixelBuffer) {
        serial_logf((SystemLogModule)3, (SystemLogLevel)2, "[GWORLD] NewGWorld: Failed to allocate pixel buffer (size=%u)\n", bufferSize);
        DisposePixMap(pmHandle);
        DisposePtr((Ptr)gworld);
        return memFullErr;
    }

    /* Clear pixel buffer to white (0xFFFFFFFF for 32-bit ARGB) */
    if (pixelDepth == 32) {
        /* Fill with white: 0xFFFFFFFF for 32-bit ARGB */
        UInt32 *pixels = (UInt32*)pixelBuffer;
        UInt32 pixelCount = bufferSize / 4;
        for (UInt32 i = 0; i < pixelCount; i++) {
            pixels[i] = 0xFFFFFFFF;
        }
    } else {
        /* For other depths, fill with zeros */
        memset(pixelBuffer, 0, bufferSize);
    }
    pm->baseAddr = pixelBuffer;

    /* Attach PixMap to port */
    gworld->portPixMap = pmHandle;

    serial_logf((SystemLogModule)3, (SystemLogLevel)2, "[GWORLD] NewGWorld: GWorld created successfully at %p, buffer=%p\n",
               gworld, pixelBuffer);

    /* Initialize regions */
    gworld->visRgn = NewRgn();
    gworld->clipRgn = NewRgn();
    if (!gworld->visRgn || !gworld->clipRgn) {
        serial_logf((SystemLogModule)3, (SystemLogLevel)2, "[GWORLD] NewGWorld: Failed to allocate regions\n");
        if (gworld->visRgn) DisposeRgn(gworld->visRgn);
        if (gworld->clipRgn) DisposeRgn(gworld->clipRgn);
        DisposePtr(pixelBuffer);
        DisposePixMap(pmHandle);
        DisposePtr((Ptr)gworld);
        return memFullErr;
    }

    /* Set regions to full bounds */
    RectRgn(gworld->visRgn, boundsRect);
    RectRgn(gworld->clipRgn, boundsRect);

    /* Initialize color fields */
    gworld->rgbFgColor.red = 0x0000;
    gworld->rgbFgColor.green = 0x0000;
    gworld->rgbFgColor.blue = 0x0000;
    gworld->rgbBkColor.red = 0xFFFF;
    gworld->rgbBkColor.green = 0xFFFF;
    gworld->rgbBkColor.blue = 0xFFFF;
    gworld->fgColor = 33;      /* Black */
    gworld->bkColor = 30;      /* White */

    /* Initialize pen state */
    gworld->pnLoc.h = 0;
    gworld->pnLoc.v = 0;
    gworld->pnSize.h = 1;
    gworld->pnSize.v = 1;
    gworld->pnMode = patCopy;
    gworld->pnVis = 0;

    /* Initialize text state */
    gworld->txFont = 0;
    gworld->txFace = 0;
    gworld->txMode = srcOr;
    gworld->txSize = 12;

    *offscreenGWorld = gworld;
    return noErr;
}

/*
 * DisposeGWorld - Dispose of offscreen graphics world
 */
void DisposeGWorld(GWorldPtr offscreenGWorld) {
    if (!offscreenGWorld) return;

    /* Free pixel buffer */
    if (offscreenGWorld->portPixMap && *offscreenGWorld->portPixMap) {
        PixMapPtr pm = *offscreenGWorld->portPixMap;
        if (pm->baseAddr) {
            DisposePtr(pm->baseAddr);
            pm->baseAddr = NULL;
        }
    }

    /* Free PixMap */
    if (offscreenGWorld->portPixMap) {
        DisposePixMap(offscreenGWorld->portPixMap);
    }

    /* Free regions */
    if (offscreenGWorld->visRgn) {
        DisposeRgn(offscreenGWorld->visRgn);
    }
    if (offscreenGWorld->clipRgn) {
        DisposeRgn(offscreenGWorld->clipRgn);
    }

    /* Clear current if this was current */
    if (g_currentGWorld == offscreenGWorld) {
        g_currentGWorld = NULL;
    }

    /* Free port structure */
    DisposePtr((Ptr)offscreenGWorld);
}

/*
 * UpdateGWorld - Update GWorld dimensions/depth
 *
 * Returns: noErr if no reallocation needed, error code otherwise
 */
OSErr UpdateGWorld(GWorldPtr *offscreenGWorld, SInt16 pixelDepth,
                   const Rect *boundsRect, CTabHandle cTable,
                   GDHandle aGDevice, GWorldFlags flags) {
    if (!offscreenGWorld || !*offscreenGWorld || !boundsRect) {
        return paramErr;
    }

    GWorldPtr gw = *offscreenGWorld;
    PixMapHandle pmHandle = gw->portPixMap;

    if (!pmHandle || !*pmHandle) {
        return paramErr;
    }

    PixMapPtr pm = *pmHandle;

    /* Check if dimensions or depth changed */
    Boolean needsRealloc = false;

    if (!EqualRect(&pm->bounds, boundsRect)) {
        needsRealloc = true;
    }
    if (pixelDepth != pm->pixelSize) {
        needsRealloc = true;
    }

    if (!needsRealloc) {
        return noErr;  /* No changes needed */
    }

    /* Reallocate by creating new GWorld and disposing old */
    GWorldPtr newGWorld;
    OSErr err = NewGWorld(&newGWorld, pixelDepth, boundsRect, cTable, aGDevice, flags);
    if (err != noErr) {
        return err;
    }

    /* Dispose old GWorld */
    DisposeGWorld(*offscreenGWorld);

    /* Return new GWorld */
    *offscreenGWorld = newGWorld;

    return noErr;
}

/*
 * SetGWorld - Set current graphics world for drawing
 */
void SetGWorld(CGrafPtr port, GDHandle gdh) {
    g_currentGWorld = port;
    g_currentGDevice = gdh;

    /* Also set as current port for QuickDraw */
    if (port) {
        extern CGrafPtr g_currentCPort;  /* from ColorQuickDraw.c */
        g_currentPort = (GrafPtr)port;
        g_currentCPort = port;  /* CRITICAL: Set color port for port type detection */
    }
}

/*
 * GetGWorld - Get current graphics world
 */
void GetGWorld(CGrafPtr *port, GDHandle *gdh) {
    if (port) {
        *port = g_currentGWorld;
    }
    if (gdh) {
        *gdh = g_currentGDevice;
    }
}

/*
 * LockPixels - Lock pixel buffer for direct access
 *
 * Returns: true if pixels locked successfully
 */
Boolean LockPixels(PixMapHandle pm) {
    if (!pm || !*pm) {
        return false;
    }

    /* In this implementation, pixels are always accessible */
    /* Real Mac OS would handle relocatable memory here */
    return ((*pm)->baseAddr != NULL);
}

/*
 * UnlockPixels - Unlock pixel buffer
 */
void UnlockPixels(PixMapHandle pm) {
    /* In this implementation, no-op since pixels are always locked */
    /* Real Mac OS would allow memory manager to relocate */
    (void)pm;
}

/*
 * GetGWorldPixMap - Get PixMap from GWorld
 */
PixMapHandle GetGWorldPixMap(GWorldPtr offscreenGWorld) {
    if (!offscreenGWorld) {
        return NULL;
    }
    return offscreenGWorld->portPixMap;
}

/*
 * GetPixBaseAddr - Get base address of pixel buffer
 */
Ptr GetPixBaseAddr(PixMapHandle pm) {
    if (!pm || !*pm) {
        return NULL;
    }
    return (*pm)->baseAddr;
}
