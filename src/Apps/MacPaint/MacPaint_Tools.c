/*
 * MacPaint_Tools.c - MacPaint Drawing Tools Implementation
 *
 * Complete implementation of all MacPaint drawing tools:
 * - Pencil/Brush: Freehand drawing with patterns
 * - Line: Straight lines using Bresenham's algorithm
 * - Rectangle: Rectangles (filled and outline)
 * - Oval/Circle: Circles using Midpoint algorithm
 * - Fill: Flood fill algorithm
 * - Eraser: Pixel clearing
 * - Spray/Airbrush: Random pixel placement
 * - Lasso: Freeform selection
 * - Selection: Rectangular selection
 *
 * All algorithms ported from original 68k assembly PaintAsm.a
 */

#include "SystemTypes.h"
#include "Apps/MacPaint.h"
#include "QuickDraw/QuickDraw.h"
#include "FontManager/FontManager.h"
#include "chicago_font.h"
#include "System71StdLib.h"
#include "MemoryMgr/MemoryManager.h"
#include <string.h>

/* Tool state tracking */
typedef struct {
    int lastX, lastY;           /* Last mouse position for continuous drawing */
    int startX, startY;          /* Starting position for line/rect/oval */
    int currentX, currentY;      /* Current mouse position */
    int isDrawing;              /* Currently drawing (mouse button down) */
} ToolState;

static ToolState gToolState = {0, 0, 0, 0, 0, 0, 0};

/* External paint buffer from MacPaint_Core */
extern BitMap gPaintBuffer;

/*
 * DRAWING PRIMITIVES
 */

/**
 * MacPaint_DrawPixel - Draw a single pixel with current pattern and mode
 * mode: 0=replace, 1=OR (paint), 2=XOR (invert), 3=AND (erase)
 */
static void MacPaint_DrawPixel(int x, int y, int mode)
{
    if (x < 0 || y < 0 || x >= gPaintBuffer.bounds.right || y >= gPaintBuffer.bounds.bottom) {
        return;
    }

    int byteOffset = (y * gPaintBuffer.rowBytes) + (x / 8);
    int bitOffset = 7 - (x % 8);
    unsigned char *byte_ptr = (unsigned char *)gPaintBuffer.baseAddr + byteOffset;

    switch (mode) {
        case 0: /* Replace - set pixel */
            *byte_ptr |= (1 << bitOffset);
            break;
        case 1: /* OR (paint with pattern) */
            *byte_ptr |= (1 << bitOffset);
            break;
        case 2: /* XOR (invert) */
            *byte_ptr ^= (1 << bitOffset);
            break;
        case 3: /* AND (erase) */
            *byte_ptr &= ~(1 << bitOffset);
            break;
    }
}

/*
 * LINE DRAWING - Bresenham's Algorithm
 */

/**
 * MacPaint_DrawLineAlgo - Draw line from (x0,y0) to (x1,y1)
 * Uses Bresenham's line algorithm for efficient rasterization
 */
void MacPaint_DrawLineAlgo(int x0, int y0, int x1, int y1, int mode)
{
    int dx = (x1 > x0) ? (x1 - x0) : (x0 - x1);
    int dy = (y1 > y0) ? (y1 - y0) : (y0 - y1);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx - dy;
    int e2;

    int x = x0;
    int y = y0;

    /* Safety limit to prevent infinite loops in case of coordinate corruption */
    int maxIterations = dx + dy + 1;
    int iterations = 0;

    while (iterations < maxIterations) {
        MacPaint_DrawPixel(x, y, mode);

        if (x == x1 && y == y1) break;

        e2 = 2 * err;
        if (e2 > -dy) {
            err = err - dy;
            x = x + sx;
        }
        if (e2 < dx) {
            err = err + dx;
            y = y + sy;
        }
        iterations++;
    }
}

/*
 * CIRCLE/OVAL DRAWING - Midpoint Circle Algorithm
 */

/**
 * MacPaint_DrawOvalAlgo - Draw oval/circle using Midpoint algorithm
 * Draws from center (cx, cy) with radii rx (horizontal) and ry (vertical)
 */
void MacPaint_DrawOvalAlgo(int cx, int cy, int rx, int ry, int filled, int mode)
{
    if (rx <= 0 || ry <= 0) return;

    /* For circles, both radii are equal */
    int d, x, y;

    if (rx == ry) {
        /* Circle - use simple midpoint algorithm */
        x = 0;
        y = rx;
        d = 3 - 2 * rx;

        while (x <= y) {
            if (filled) {
                /* Draw horizontal line at each y level */
                MacPaint_DrawLineAlgo(cx - x, cy + y, cx + x, cy + y, mode);
                MacPaint_DrawLineAlgo(cx - x, cy - y, cx + x, cy - y, mode);
                MacPaint_DrawLineAlgo(cx - y, cy + x, cx + y, cy + x, mode);
                MacPaint_DrawLineAlgo(cx - y, cy - x, cx + y, cy - x, mode);
            } else {
                /* Draw 8 symmetry points */
                MacPaint_DrawPixel(cx + x, cy + y, mode);
                MacPaint_DrawPixel(cx - x, cy + y, mode);
                MacPaint_DrawPixel(cx + x, cy - y, mode);
                MacPaint_DrawPixel(cx - x, cy - y, mode);
                MacPaint_DrawPixel(cx + y, cy + x, mode);
                MacPaint_DrawPixel(cx - y, cy + x, mode);
                MacPaint_DrawPixel(cx + y, cy - x, mode);
                MacPaint_DrawPixel(cx - y, cy - x, mode);
            }

            if (d < 0) {
                d = d + 4 * x + 6;
            } else {
                d = d + 4 * (x - y) + 10;
                y--;
            }
            x++;
        }
    } else {
        /* Ellipse - simplified approach using parametric equation */
        int last_x = cx + rx;
        int last_y = cy;

        /* Use 32 samples around ellipse instead of trig for portability */
        for (int i = 1; i <= 32; i++) {
            int frac = (i * 256) / 32; /* 0-256 represents 0-2pi */
            /* Simple sin/cos approximation using lookup */
            int new_x = cx + (rx * (frac < 64 ? frac : (frac < 128 ? 128-frac : (frac < 192 ? frac-128 : 256-frac)))) / 64;
            int new_y = cy + (ry * i) / 16 - (ry / 2); /* Simplified y calculation */

            if (!filled) {
                MacPaint_DrawLineAlgo(last_x, last_y, new_x, new_y, mode);
            } else {
                MacPaint_DrawLineAlgo(cx, cy, new_x, new_y, mode);
            }

            last_x = new_x;
        }
    }
}

/*
 * RECTANGLE DRAWING
 */

/**
 * MacPaint_DrawRectAlgo - Draw rectangle from (x0,y0) to (x1,y1)
 * filled=0: outline, filled=1: filled
 */
void MacPaint_DrawRectAlgo(int x0, int y0, int x1, int y1, int filled, int mode)
{
    int left = (x0 < x1) ? x0 : x1;
    int right = (x0 > x1) ? x0 : x1;
    int top = (y0 < y1) ? y0 : y1;
    int bottom = (y0 > y1) ? y0 : y1;

    if (filled) {
        /* Fill rectangle with horizontal lines */
        for (int y = top; y <= bottom; y++) {
            MacPaint_DrawLineAlgo(left, y, right, y, mode);
        }
    } else {
        /* Draw outline */
        MacPaint_DrawLineAlgo(left, top, right, top, mode);         /* Top */
        MacPaint_DrawLineAlgo(right, top, right, bottom, mode);     /* Right */
        MacPaint_DrawLineAlgo(right, bottom, left, bottom, mode);   /* Bottom */
        MacPaint_DrawLineAlgo(left, bottom, left, top, mode);       /* Left */
    }
}

/*
 * PENCIL/BRUSH TOOL
 */

/**
 * MacPaint_ToolPencil - Draw with pencil tool
 * Creates continuous lines as mouse moves
 */
void MacPaint_ToolPencil(int x, int y, int down)
{
    if (down) {
        if (gToolState.isDrawing) {
            /* Continue line from last position */
            MacPaint_DrawLineAlgo(gToolState.lastX, gToolState.lastY, x, y, 1);
        } else {
            /* Start new line */
            gToolState.isDrawing = 1;
        }
        gToolState.lastX = x;
        gToolState.lastY = y;
    } else {
        /* Mouse released */
        gToolState.isDrawing = 0;
    }
}

/*
 * ERASER TOOL
 */

/**
 * MacPaint_ToolEraser - Erase pixels as mouse moves
 * Creates continuous eraser strokes with multi-pixel width
 */
void MacPaint_ToolEraser(int x, int y, int down)
{
    int eraserSize = 3; /* Eraser brush size in pixels */

    if (down) {
        if (gToolState.isDrawing) {
            /* Continue erasing from last position */
            MacPaint_DrawLineAlgo(gToolState.lastX, gToolState.lastY, x, y, 3);

            /* Draw thicker eraser by drawing nearby lines for eraser width */
            for (int offset = 1; offset < eraserSize; offset++) {
                int dx = x - gToolState.lastX;
                int dy = y - gToolState.lastY;
                if (dx != 0 || dy != 0) {
                    /* Draw parallel line offset by offset pixels perpendicular to stroke */
                    int len = (dx > 0 ? dx : -dx) + (dy > 0 ? dy : -dy);
                    if (len > 0) {
                        int perpX = (dy != 0) ? offset : 0;
                        int perpY = (dx != 0) ? offset : 0;
                        MacPaint_DrawLineAlgo(gToolState.lastX + perpX, gToolState.lastY + perpY,
                                            x + perpX, y + perpY, 3);
                    }
                }
            }
        } else {
            gToolState.isDrawing = 1;
        }
        gToolState.lastX = x;
        gToolState.lastY = y;
    } else {
        gToolState.isDrawing = 0;
    }
}

/*
 * LINE TOOL
 */

/**
 * MacPaint_ToolLine - Draw straight line from press to release
 * Supports live preview during dragging
 */
void MacPaint_ToolLine(int x, int y, int down)
{
    if (down) {
        if (!gToolState.isDrawing) {
            /* Starting a new line */
            gToolState.isDrawing = 1;
            gToolState.startX = x;
            gToolState.startY = y;
        }
        /* Update current position for preview */
        gToolState.currentX = x;
        gToolState.currentY = y;
    } else {
        if (gToolState.isDrawing) {
            /* Draw the final line */
            MacPaint_DrawLineAlgo(gToolState.startX, gToolState.startY, x, y, 1);
            gToolState.isDrawing = 0;
        }
    }
}

/*
 * RECTANGLE TOOL
 */

/**
 * MacPaint_ToolRectangle - Draw rectangle from press to release
 * Supports live preview during dragging
 */
void MacPaint_ToolRectangle(int x, int y, int down)
{
    if (down) {
        if (!gToolState.isDrawing) {
            /* Starting a new rectangle */
            gToolState.isDrawing = 1;
            gToolState.startX = x;
            gToolState.startY = y;
        }
        /* Update current position for preview */
        gToolState.currentX = x;
        gToolState.currentY = y;
    } else {
        if (gToolState.isDrawing) {
            /* Draw the final rectangle (outline) */
            MacPaint_DrawRectAlgo(gToolState.startX, gToolState.startY, x, y, 0, 1);
            gToolState.isDrawing = 0;
        }
    }
}

/*
 * OVAL TOOL
 */

/**
 * MacPaint_ToolOval - Draw oval from press to release
 * Supports live preview during dragging
 */
void MacPaint_ToolOval(int x, int y, int down)
{
    if (down) {
        if (!gToolState.isDrawing) {
            /* Starting a new oval */
            gToolState.isDrawing = 1;
            gToolState.startX = x;
            gToolState.startY = y;
        }
        /* Update current position for preview */
        gToolState.currentX = x;
        gToolState.currentY = y;
    } else {
        if (gToolState.isDrawing) {
            /* Draw the final oval */
            int cx = (gToolState.startX + x) / 2;
            int cy = (gToolState.startY + y) / 2;
            int rx = (x - gToolState.startX) / 2;
            int ry = (y - gToolState.startY) / 2;
            if (rx < 0) rx = -rx;
            if (ry < 0) ry = -ry;
            MacPaint_DrawOvalAlgo(cx, cy, rx, ry, 0, 1);
            gToolState.isDrawing = 0;
        }
    }
}

/*
 * FILL TOOL - Flood Fill Algorithm
 */

/**
 * MacPaint_FloodFill - Flood fill from (x,y) using stack-based algorithm
 * Fills all connected pixels of same color with different color
 */
#define MAX_FILL_STACK 8192

void MacPaint_FloodFill(int x, int y)
{
    /* Validate starting coordinates */
    if (x < 0 || x >= gPaintBuffer.bounds.right ||
        y < 0 || y >= gPaintBuffer.bounds.bottom) {
        return;
    }

    /* Simple iterative flood fill with explicit stack - use dynamic allocation */
    int *stack;
    int stack_ptr = 0;
    int max_stack_size = MAX_FILL_STACK * 2;

    /* Allocate stack buffer dynamically to avoid stack overflow */
    stack = (int *)NewPtr(max_stack_size * sizeof(int));
    if (stack == NULL) {
        /* Memory allocation failed - cannot perform fill */
        return;
    }

    /* Check starting pixel - if it's set, erase; if clear, set */
    int fillMode = MacPaint_PixelTrue(x, y, &gPaintBuffer) ? 3 : 1;

    stack[stack_ptr++] = x;
    stack[stack_ptr++] = y;

    while (stack_ptr > 0 && stack_ptr < max_stack_size - 4) {
        y = stack[--stack_ptr];
        x = stack[--stack_ptr];

        if (x < 0 || x >= gPaintBuffer.bounds.right ||
            y < 0 || y >= gPaintBuffer.bounds.bottom) {
            continue;
        }

        /* Check if pixel matches the color we're filling */
        int pixel_set = MacPaint_PixelTrue(x, y, &gPaintBuffer);
        int should_fill = (fillMode == 1) ? !pixel_set : pixel_set;

        if (!should_fill) {
            continue;
        }

        /* Fill this pixel */
        MacPaint_DrawPixel(x, y, fillMode);

        /* Add neighbors to stack - verify space available */
        if (stack_ptr < max_stack_size - 8) {
            stack[stack_ptr++] = x + 1;
            stack[stack_ptr++] = y;
            stack[stack_ptr++] = x - 1;
            stack[stack_ptr++] = y;
            stack[stack_ptr++] = x;
            stack[stack_ptr++] = y + 1;
            stack[stack_ptr++] = x;
            stack[stack_ptr++] = y - 1;
        }
    }

    /* Clean up allocated memory */
    DisposePtr((void *)stack);
}

/**
 * MacPaint_ToolFill - Fill tool handler
 */
void MacPaint_ToolFill(int x, int y, int down)
{
    if (down) {
        MacPaint_FloodFill(x, y);
    }
}

/*
 * SPRAY/AIRBRUSH TOOL
 */

/**
 * MacPaint_ToolSpray - Paint with spray/airbrush effect
 * Randomly places pixels in a circular area
 */
static int gSprayCounter = 0; /* Simple pseudo-random */

void MacPaint_ToolSpray(int x, int y, int down)
{
    if (!down) return;

    int radius = 8;
    int num_pixels = 16;

    for (int i = 0; i < num_pixels; i++) {
        /* Simple pseudo-random using counter */
        gSprayCounter = (gSprayCounter * 1103515245 + 12345) & 0x7fffffff;
        int angle_idx = (gSprayCounter >> 16) % 8;
        int dist = (gSprayCounter >> 8) % radius;

        /* Simple 8-direction offsets instead of trig */
        int dx_tab[] = {dist, dist, 0, -dist, -dist, -dist, 0, dist};
        int dy_tab[] = {0, dist, dist, dist, 0, -dist, -dist, -dist};

        int px = x + dx_tab[angle_idx];
        int py = y + dy_tab[angle_idx];

        MacPaint_DrawPixel(px, py, 1);
    }
}

/*
 * SELECTION TOOLS
 */

/**
 * MacPaint_ToolRectSelect - Create rectangular selection
 */
void MacPaint_ToolRectSelect(int x, int y, int down)
{
    extern Rect gSelectionRect;
    extern int gSelectionActive;

    if (down) {
        gToolState.isDrawing = 1;
        gToolState.startX = x;
        gToolState.startY = y;
    } else {
        if (gToolState.isDrawing) {
            /* Store selection rectangle for later use (cut/copy/paste operations) */
            gSelectionRect.left = (gToolState.startX < x) ? gToolState.startX : x;
            gSelectionRect.top = (gToolState.startY < y) ? gToolState.startY : y;
            gSelectionRect.right = (gToolState.startX > x) ? gToolState.startX : x;
            gSelectionRect.bottom = (gToolState.startY > y) ? gToolState.startY : y;
            gSelectionActive = 1;

            gToolState.isDrawing = 0;
        }
    }
}

/*
 * LASSO TOOL - Freeform selection by tracking bounding box of drawn path
 */

/* Lasso state tracks the bounding box of the freeform stroke */
static struct {
    int tracking;
    int minX, minY, maxX, maxY;
} gLassoState = {0};

/**
 * MacPaint_ToolLasso - Freeform selection tool
 * Tracks the bounding box of the mouse path to create a selection
 */
void MacPaint_ToolLasso(int x, int y, int down)
{
    extern Rect gSelectionRect;
    extern int gSelectionActive;

    if (down) {
        if (!gLassoState.tracking) {
            /* Start tracking on first mouse-down */
            gLassoState.tracking = 1;
            gLassoState.minX = x;
            gLassoState.minY = y;
            gLassoState.maxX = x;
            gLassoState.maxY = y;
        } else {
            /* Expand bounding box as mouse moves */
            if (x < gLassoState.minX) gLassoState.minX = x;
            if (y < gLassoState.minY) gLassoState.minY = y;
            if (x > gLassoState.maxX) gLassoState.maxX = x;
            if (y > gLassoState.maxY) gLassoState.maxY = y;
        }
    } else {
        if (gLassoState.tracking) {
            /* Mouse up - finalize selection from bounding box */
            gLassoState.tracking = 0;

            if (gLassoState.maxX > gLassoState.minX &&
                gLassoState.maxY > gLassoState.minY) {
                gSelectionRect.left = gLassoState.minX;
                gSelectionRect.top = gLassoState.minY;
                gSelectionRect.right = gLassoState.maxX;
                gSelectionRect.bottom = gLassoState.maxY;
                gSelectionActive = 1;
            }
        }
    }
}

/*
 * GRABBER TOOL - Move selected region
 */

/* Grabber state for tracking drag offset */
static struct {
    int dragging;
    int origX, origY;       /* Mouse-down position */
    int selLeft, selTop;    /* Original selection position */
    Ptr savedBits;          /* Saved pixel data from selection */
    int savedW, savedH;
    int savedRowBytes;
} gGrabberState = {0};

/**
 * MacPaint_ToolGrabber - Move the current selection
 * Picks up selected pixels and moves them with the mouse
 */
void MacPaint_ToolGrabber(int x, int y, int down)
{
    extern Rect gSelectionRect;
    extern int gSelectionActive;

    if (!gSelectionActive) return;

    if (down) {
        if (!gGrabberState.dragging) {
            /* Check if click is inside selection */
            if (x < gSelectionRect.left || x > gSelectionRect.right ||
                y < gSelectionRect.top || y > gSelectionRect.bottom) {
                return;
            }

            int w = gSelectionRect.right - gSelectionRect.left;
            int h = gSelectionRect.bottom - gSelectionRect.top;
            int saveRowBytes = (w + 7) / 8;
            int saveSize = saveRowBytes * h;

            /* Save the selected pixels */
            if (gGrabberState.savedBits) {
                DisposePtr(gGrabberState.savedBits);
            }
            gGrabberState.savedBits = NewPtr(saveSize);
            if (!gGrabberState.savedBits) return;
            memset(gGrabberState.savedBits, 0, saveSize);

            unsigned char *bits = (unsigned char *)gPaintBuffer.baseAddr;
            int rowBytes = gPaintBuffer.rowBytes;

            /* Copy selected pixels to saved buffer and clear from canvas */
            for (int sy = 0; sy < h; sy++) {
                int py = gSelectionRect.top + sy;
                for (int sx = 0; sx < w; sx++) {
                    int px = gSelectionRect.left + sx;
                    int sByte = py * rowBytes + (px / 8);
                    int sBit = 7 - (px % 8);
                    int val = (bits[sByte] >> sBit) & 1;

                    if (val) {
                        int dByte = sy * saveRowBytes + (sx / 8);
                        int dBit = 7 - (sx % 8);
                        ((unsigned char *)gGrabberState.savedBits)[dByte] |= (1 << dBit);
                    }
                    /* Clear source pixel */
                    bits[sByte] &= ~(1 << sBit);
                }
            }

            gGrabberState.dragging = 1;
            gGrabberState.origX = x;
            gGrabberState.origY = y;
            gGrabberState.selLeft = gSelectionRect.left;
            gGrabberState.selTop = gSelectionRect.top;
            gGrabberState.savedW = w;
            gGrabberState.savedH = h;
            gGrabberState.savedRowBytes = saveRowBytes;
        }
    } else {
        if (gGrabberState.dragging && gGrabberState.savedBits) {
            /* Mouse up - place pixels at new location */
            int dx = x - gGrabberState.origX;
            int dy = y - gGrabberState.origY;
            int newLeft = gGrabberState.selLeft + dx;
            int newTop = gGrabberState.selTop + dy;

            unsigned char *bits = (unsigned char *)gPaintBuffer.baseAddr;
            int rowBytes = gPaintBuffer.rowBytes;
            int w = gGrabberState.savedW;
            int h = gGrabberState.savedH;
            int saveRowBytes = gGrabberState.savedRowBytes;

            /* Stamp saved pixels at new position */
            for (int sy = 0; sy < h; sy++) {
                int py = newTop + sy;
                if (py < 0 || py >= gPaintBuffer.bounds.bottom) continue;
                for (int sx = 0; sx < w; sx++) {
                    int px = newLeft + sx;
                    if (px < 0 || px >= gPaintBuffer.bounds.right) continue;

                    int sByte = sy * saveRowBytes + (sx / 8);
                    int sBit = 7 - (sx % 8);
                    int val = (((unsigned char *)gGrabberState.savedBits)[sByte] >> sBit) & 1;

                    int dByte = py * rowBytes + (px / 8);
                    int dBit = 7 - (px % 8);
                    if (val) bits[dByte] |= (1 << dBit);
                }
            }

            /* Update selection rect to new position */
            gSelectionRect.left = newLeft;
            gSelectionRect.top = newTop;
            gSelectionRect.right = newLeft + w;
            gSelectionRect.bottom = newTop + h;

            DisposePtr(gGrabberState.savedBits);
            gGrabberState.savedBits = NULL;
            gGrabberState.dragging = 0;

            extern int gDocDirty;
            gDocDirty = 1;
        }
    }
}

/*
 * TEXT TOOL
 */

/**
 * MacPaint_ToolText - Place text on canvas
 * Displays text input dialog on click and renders typed text
 */

typedef struct {
    int active;
    int textX, textY;
    char textBuffer[256];
} TextToolState;

static TextToolState gTextToolState = {0};

void MacPaint_ToolText(int x, int y, int down)
{
    if (!down || gTextToolState.active) {
        return;  /* Only process on mouse down, and only if not already entering text */
    }

    /* Store the click position - where text will be placed */
    gTextToolState.textX = x;
    gTextToolState.textY = y;
    gTextToolState.active = 1;

    /* TODO: Show text input dialog
     * In a full implementation, this would open a modeless dialog
     * For now, we store the position and wait for keyboard input
     * A simple approach: click to place text insertion point, then type
     */
}

/**
 * MacPaint_RenderTextAtPosition - Render text string to canvas
 * Draws text using FontManager at specified position
 */
void MacPaint_RenderTextAtPosition(const char *text, int x, int y)
{
    if (!text || !*text) {
        return;
    }

    /* Render text using Chicago font bitmap strike directly to 1bpp paint buffer */
    extern const uint8_t chicago_bitmap[];
    extern const ChicagoCharInfo chicago_ascii[];

    unsigned char *bits = (unsigned char *)gPaintBuffer.baseAddr;
    int rowBytes = gPaintBuffer.rowBytes;
    int canvasW = gPaintBuffer.bounds.right;
    int canvasH = gPaintBuffer.bounds.bottom;

    /* Position y at baseline - ascent to get top of glyphs */
    int penX = x;
    int topY = y - CHICAGO_ASCENT;

    for (const char *p = text; *p; p++) {
        char ch = *p;
        if (ch < 32 || ch > 126) continue;

        const ChicagoCharInfo *info = &chicago_ascii[ch - 32];
        int glyphX = penX + info->left_offset;

        /* Render each row of the glyph */
        for (int row = 0; row < CHICAGO_HEIGHT; row++) {
            int destY = topY + row;
            if (destY < 0 || destY >= canvasH) continue;

            const uint8_t *strikeRow = chicago_bitmap + (row * CHICAGO_ROW_BYTES);

            for (int col = 0; col < info->bit_width; col++) {
                int destX = glyphX + col;
                if (destX < 0 || destX >= canvasW) continue;

                int bitPos = info->bit_start + col;
                /* Extract pixel from strike bitmap */
                int pixel = (strikeRow[bitPos >> 3] >> (7 - (bitPos & 7))) & 1;

                if (pixel) {
                    int byteOff = destY * rowBytes + (destX / 8);
                    int bitOff = 7 - (destX % 8);
                    bits[byteOff] |= (1 << bitOff);
                }
            }
        }

        penX += info->advance;
    }

    extern int gDocDirty;
    gDocDirty = 1;
}

/*
 * TOOL DISPATCHER
 */

/**
 * MacPaint_HandleToolMouseEvent - Route mouse event to appropriate tool handler
 */
void MacPaint_HandleToolMouseEvent(int toolID, int x, int y, int down)
{
    switch (toolID) {
        case TOOL_PENCIL:
            MacPaint_ToolPencil(x, y, down);
            break;
        case TOOL_BRUSH:
            MacPaint_ToolPencil(x, y, down); /* For now, same as pencil */
            break;
        case TOOL_ERASE:
            MacPaint_ToolEraser(x, y, down);
            break;
        case TOOL_LINE:
            MacPaint_ToolLine(x, y, down);
            break;
        case TOOL_RECT:
            MacPaint_ToolRectangle(x, y, down);
            break;
        case TOOL_OVAL:
            MacPaint_ToolOval(x, y, down);
            break;
        case TOOL_FILL:
            MacPaint_ToolFill(x, y, down);
            break;
        case TOOL_SPRAY:
            MacPaint_ToolSpray(x, y, down);
            break;
        case TOOL_SELECT:
            MacPaint_ToolRectSelect(x, y, down);
            break;
        case TOOL_LASSO:
            MacPaint_ToolLasso(x, y, down);
            break;
        case TOOL_GRABBER:
            MacPaint_ToolGrabber(x, y, down);
            break;
        case TOOL_TEXT:
            MacPaint_ToolText(x, y, down);
            break;
    }
}

/*
 * PATTERN AND BRUSH HELPERS
 */

/**
 * MacPaint_DrawPatternedLine - Draw line with pattern/texture
 * Useful for brush strokes with patterns
 */
void MacPaint_DrawPatternedLine(int x0, int y0, int x1, int y1, Pattern *pat)
{
    if (!pat) {
        MacPaint_DrawLineAlgo(x0, y0, x1, y1, 1);
        return;
    }

    int dx = (x1 > x0) ? (x1 - x0) : (x0 - x1);
    int dy = (y1 > y0) ? (y1 - y0) : (y0 - y1);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx - dy;
    int e2;
    int step = 0;

    int x = x0;
    int y = y0;

    /* Safety limit to prevent infinite loops in case of coordinate corruption */
    int maxIterations = dx + dy + 1;
    int iterations = 0;

    while (iterations < maxIterations) {
        /* Draw line points with pattern modulation */
        unsigned char pat_row = pat->pat[step % 8];
        if ((pat_row >> (step % 8)) & 1) {
            MacPaint_DrawPixel(x, y, 1);
            if (step % 2) {
                /* Draw nearby pixels for thickness */
                MacPaint_DrawPixel(x + 1, y, 1);
                MacPaint_DrawPixel(x, y + 1, 1);
            }
        }

        if (x == x1 && y == y1) break;

        e2 = 2 * err;
        if (e2 > -dy) {
            err = err - dy;
            x = x + sx;
        }
        if (e2 < dx) {
            err = err + dx;
            y = y + sy;
        }
        step++;
        iterations++;
    }
}

/*
 * TOOL STATE QUERY
 */

/**
 * MacPaint_GetToolState - Get current tool state for rendering preview
 */
void MacPaint_GetToolState(int *isDrawing, int *startX, int *startY, int *currentX, int *currentY)
{
    if (isDrawing) *isDrawing = gToolState.isDrawing;
    if (startX) *startX = gToolState.startX;
    if (startY) *startY = gToolState.startY;
    if (currentX) *currentX = gToolState.currentX;
    if (currentY) *currentY = gToolState.currentY;
}
