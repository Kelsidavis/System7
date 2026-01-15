/*
 * CursorManager.c - QuickDraw Cursor Management with System 7 Resources
 *
 * Manages standard System 7 cursors using embedded resource data.
 *
 * Copyright (c) 2024 System7.1-Portable Project
 * MIT License
 */

#include <limits.h>
#include <string.h>

#include "QuickDraw/QuickDrawInternal.h"
#include "SystemTypes.h"
#include "System71StdLib.h"

#include "EventManager/EventManager.h"
#include "MemoryMgr/MemoryManager.h"
#include "QuickDraw/QuickDraw.h"
#include "SystemInternal.h"

/* Standard cursor definitions */
static const Cursor* gArrowCursor = NULL;
static const Cursor* gIBeamCursor = NULL;
static const Cursor* gCrosshairCursor = NULL;
static const Cursor* gWatchCursor = NULL;

#define CURSOR_DIM 16
#define CURSOR_CENTER 7.5f
#define WATCH_SPIN_STEPS 12

static const float kWatchCosLUT[WATCH_SPIN_STEPS] = {
    1.000000f, 0.866025f, 0.500000f, 0.000000f, -0.500000f, -0.866025f,
   -1.000000f, -0.866025f, -0.500000f, -0.000000f, 0.500000f, 0.866025f
};

static const float kWatchSinLUT[WATCH_SPIN_STEPS] = {
    0.000000f, 0.500000f, 0.866025f, 1.000000f, 0.866025f, 0.500000f,
    0.000000f, -0.500000f, -0.866025f, -1.000000f, -0.866025f, -0.500000f
};

static const Cursor kArrowCursorResource = {
    .data = {
        0x0000, 0x07C0, 0x0460, 0x0460,
        0x0460, 0x7C7C, 0x4386, 0x4286,
        0x4386, 0x7C7E, 0x3C7E, 0x0460,
        0x0460, 0x07E0, 0x03E0, 0x0000
    },
    .mask = {
        0x0FC0, 0x0FE0, 0x0FF0, 0x0FF0,
        0xFFFF, 0xFFFE, 0xFC7F, 0xFC7F,
        0xFC7F, 0xFFFF, 0x7FFF, 0x7FFF,
        0x0FF0, 0x0FF0, 0x07F0, 0x03E0
    },
    .hotSpot = {8, 8}
};

static const Cursor kIBeamCursorResource = {
    .data = {
        0x0C60, 0x0280, 0x0100, 0x0100,
        0x0100, 0x0100, 0x0100, 0x0100,
        0x0100, 0x0100, 0x0100, 0x0100,
        0x0100, 0x0100, 0x0280, 0x0C60
    },
    .mask = {
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000
    },
    .hotSpot = {4, 7}
};

static const Cursor kCrosshairCursorResource = {
    .data = {
        0x0400, 0x0400, 0x0400, 0x0400,
        0x0400, 0xFFE0, 0x0400, 0x0400,
        0x0400, 0x0400, 0x0400, 0x0400,
        0x0000, 0x0000, 0x0000, 0x0000
    },
    .mask = {
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000
    },
    .hotSpot = {5, 5}
};

static const Cursor kWatchCursorResource = {
    .data = {
        0x3F00, 0x3F00, 0x3F00, 0x3F00,
        0x4080, 0x8440, 0x8440, 0x8460,
        0x9C60, 0x8040, 0x8040, 0x4080,
        0x3F00, 0x3F00, 0x3F00, 0x3F00
    },
    .mask = {
        0x3F00, 0x3F00, 0x3F00, 0x3F00,
        0x7F80, 0xFFC0, 0xFFC0, 0xFFC0,
        0xFFC0, 0xFFC0, 0xFFC0, 0x7F80,
        0x3F00, 0x3F00, 0x3F00, 0x3F00
    },
    .hotSpot = {8, 8}
};

typedef struct {
    Cursor currentImage;
    Boolean hasCursor;
    Point hotSpot;
    SInt16 hideLevel;
    Boolean obscured;
    Boolean revealOnMove;
    Boolean watchActive;
    short watchPhase;
    Boolean watchFramesReady;
    Cursor watchFrames[WATCH_SPIN_STEPS];
    Point lastMouse;
    Boolean lastMouseValid;
    Point obscurePoint;
} CursorState;

static CursorState gCursorState = {
    .hasCursor = false,
    .hotSpot = {0, 0},
    .hideLevel = 0,
    .obscured = false,
    .revealOnMove = false,
    .watchActive = false,
    .watchPhase = 0,
    .watchFramesReady = false,
    .lastMouse = {0, 0},
    .lastMouseValid = false,
    .obscurePoint = {0, 0}
};

extern void InvalidateCursor(void);

/* Check if menu tracking is active - don't switch cursor during menu operations */
extern Boolean IsMenuTrackingNew(void);

static inline UInt16 cursor_get_bit(UInt16 row, int col) {
    return (UInt16)((row >> (15 - col)) & 0x1);
}

static inline void cursor_set_bit(UInt16* row, int col, UInt16 value) {
    UInt16 mask = (UInt16)(0x8000 >> col);
    if (value) {
        *row |= mask;
    } else {
        *row &= (UInt16)~mask;
    }
}

static inline Boolean CursorManager_ShouldBeVisible(void) {
    return gCursorState.hasCursor &&
           (gCursorState.hideLevel == 0) &&
           !gCursorState.obscured;
}

static void CursorManager_CopyCursor(const Cursor* src, Cursor* dst) {
    memcpy(dst, src, sizeof(Cursor));
}

static void CursorManager_SetCursorInternal(const Cursor* crsr, Boolean watchActive) {
    if (!crsr) {
        return;
    }

    /* Don't allow cursor changes while menu is being tracked */
    if (IsMenuTrackingNew()) {
        return;
    }

    CursorManager_CopyCursor(crsr, &gCursorState.currentImage);
    gCursorState.hasCursor = true;
    /* Explicit field copy to avoid ARM64 struct assignment hang */
    gCursorState.hotSpot.h = crsr->hotSpot.h;
    gCursorState.hotSpot.v = crsr->hotSpot.v;
    gCursorState.watchActive = watchActive;
    if (!watchActive) {
        gCursorState.watchPhase = 0;
    }

    InvalidateCursor();
}

static void CursorManager_BuildWatchFrames(void) {
    if (!gWatchCursor) {
        return;
    }

    const Cursor* base = gWatchCursor;
    CursorManager_CopyCursor(base, &gCursorState.watchFrames[0]);

    for (int frame = 1; frame < WATCH_SPIN_STEPS; ++frame) {
        Cursor* dest = &gCursorState.watchFrames[frame];
        memset(dest, 0, sizeof(Cursor));
        /* Explicit field copy to avoid ARM64 struct assignment hang */
        dest->hotSpot.h = base->hotSpot.h;
        dest->hotSpot.v = base->hotSpot.v;

        float cosTheta = kWatchCosLUT[frame];
        float sinTheta = kWatchSinLUT[frame];

        for (int y = 0; y < CURSOR_DIM; ++y) {
            for (int x = 0; x < CURSOR_DIM; ++x) {
                float dx = (float)x - CURSOR_CENTER;
                float dy = (float)y - CURSOR_CENTER;

                float srcXf = cosTheta * dx + sinTheta * dy + CURSOR_CENTER;
                float srcYf = -sinTheta * dx + cosTheta * dy + CURSOR_CENTER;

                int srcX = (int)((srcXf >= 0.0f) ? (srcXf + 0.5f) : (srcXf - 0.5f));
                int srcY = (int)((srcYf >= 0.0f) ? (srcYf + 0.5f) : (srcYf - 0.5f));

                if (srcX < 0 || srcX >= CURSOR_DIM || srcY < 0 || srcY >= CURSOR_DIM) {
                    continue;
                }

                UInt16 maskBit = cursor_get_bit(base->mask[srcY], srcX);
                if (!maskBit) {
                    continue;
                }

                cursor_set_bit(&dest->mask[y], x, 1);
                UInt16 dataBit = cursor_get_bit(base->data[srcY], srcX);
                cursor_set_bit(&dest->data[y], x, dataBit);
            }
        }
    }

    gCursorState.watchFramesReady = true;
}

/* Initialize standard cursors from resources */
static OSErr InitStandardCursors(void) {
    gArrowCursor = &kArrowCursorResource;
    gIBeamCursor = &kIBeamCursorResource;
    gCrosshairCursor = &kCrosshairCursorResource;
    gWatchCursor = &kWatchCursorResource;

    /* Explicit field copy to avoid ARM64 struct assignment hang */
    gCursorState.hotSpot.h = kArrowCursorResource.hotSpot.h;
    gCursorState.hotSpot.v = kArrowCursorResource.hotSpot.v;
    gCursorState.watchFramesReady = false;
    gCursorState.watchPhase = 0;

    return noErr;
}

/* Set cursor to arrow */
void InitCursor(void) {
    /* Don't switch cursor while a menu is being tracked */
    if (IsMenuTrackingNew()) {
        return;
    }

    if (!gArrowCursor) {
        if (InitStandardCursors() != noErr) {
            return;
        }
    }

    if (gArrowCursor) {
        CursorManager_SetCursorInternal(gArrowCursor, false);
    }
}

/* Set cursor from resource ID */
void SetCursor(const Cursor* crsr) {
    /* Don't switch cursor while a menu is being tracked */
    if (IsMenuTrackingNew()) {
        return;
    }

    CursorManager_SetCursorInternal(crsr, false);
}

/* Get cursor from resource */
CursHandle GetCursor(short cursorID) {
    /* Load CURS resource from Resource Manager */
    extern Handle GetResource(ResType theType, ResID theID);

    /* CURS resource type is 'CURS' = 0x43555253 */
    return (CursHandle)GetResource(0x43555253, cursorID);
}

const Cursor* CursorManager_GetCurrentCursorImage(void) {
    return gCursorState.hasCursor ? &gCursorState.currentImage : NULL;
}

Point CursorManager_GetCursorHotspot(void) {
    return gCursorState.hotSpot;
}

/* Show/hide cursor */
void ShowCursor(void) {
    /* Don't show cursor while menu is being tracked */
    if (IsMenuTrackingNew()) {
        return;
    }

    Boolean wasVisible = CursorManager_ShouldBeVisible();
    if (gCursorState.hideLevel > 0) {
        gCursorState.hideLevel--;
    }
    Boolean isVisible = CursorManager_ShouldBeVisible();

    if (!wasVisible && isVisible) {
        InvalidateCursor();
    }
}

void HideCursor(void) {
    Boolean wasVisible = CursorManager_ShouldBeVisible();
    if (gCursorState.hideLevel < SHRT_MAX) {
        gCursorState.hideLevel++;
    }
    Boolean isVisible = CursorManager_ShouldBeVisible();

    if (wasVisible && !isVisible) {
        InvalidateCursor();
    }
}

/* Obscure cursor until mouse moves */
void ObscureCursor(void) {
    if (gCursorState.hideLevel != 0 || gCursorState.obscured) {
        return;
    }

    Point mousePoint;
    if (gCursorState.lastMouseValid) {
        mousePoint = gCursorState.lastMouse;
    } else {
        GetMouse(&mousePoint);
        gCursorState.lastMouse = mousePoint;
        gCursorState.lastMouseValid = true;
    }

    gCursorState.obscured = true;
    gCursorState.revealOnMove = true;
    gCursorState.obscurePoint = mousePoint;

    InvalidateCursor();
}

void CursorManager_HandleMouseMotion(Point newPos) {
    Boolean moved = !gCursorState.lastMouseValid ||
                    newPos.h != gCursorState.lastMouse.h ||
                    newPos.v != gCursorState.lastMouse.v;

    gCursorState.lastMouse = newPos;
    gCursorState.lastMouseValid = true;

    if (!moved) {
        return;
    }

    if (gCursorState.obscured && gCursorState.revealOnMove) {
        gCursorState.obscured = false;
        gCursorState.revealOnMove = false;
        if (gCursorState.hideLevel == 0) {
            InvalidateCursor();
        }
    }
}

int IsCursorVisible(void) {
    return CursorManager_ShouldBeVisible();
}

/* Spin the watch cursor (animated wait) */
void SpinCursor(short increment) {
    /* Don't spin cursor while menu is being tracked */
    if (IsMenuTrackingNew()) {
        return;
    }

    if (!gCursorState.watchActive || !gWatchCursor) {
        return;
    }

    if (increment <= 0) {
        increment = 1;
    }

    if (!gCursorState.watchFramesReady) {
        CursorManager_BuildWatchFrames();
    }

    gCursorState.watchPhase =
        (short)((gCursorState.watchPhase + increment) % WATCH_SPIN_STEPS);
    CursorManager_SetCursorInternal(
        &gCursorState.watchFrames[gCursorState.watchPhase], true);
}
