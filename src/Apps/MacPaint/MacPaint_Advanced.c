/*
 * MacPaint_Advanced.c - Advanced Features: Undo/Redo, Selections, Editors
 *
 * Implements advanced MacPaint features:
 * - Undo/Redo system with circular buffer
 * - Selection and clipboard operations
 * - Pattern editor
 * - Brush editor
 * - Additional drawing modes (XOR, OR, AND)
 *
 * Ported from original MacPaint undo system and advanced tools
 */

#include "SystemTypes.h"
#include "Apps/MacPaint.h"
#include "System71StdLib.h"
#include "MemoryMgr/MemoryManager.h"
#include "FileManagerTypes.h"
#include <string.h>

/*
 * UNDO/REDO SYSTEM - Circular Buffer Implementation
 */

#define MAX_UNDO_BUFFERS 3      /* Maximum undo levels (reduced for memory) */
#define UNDO_BUFFER_SIZE 32768  /* Size of each undo buffer (32KB) */

typedef struct {
    Ptr data;                   /* Bitmap snapshot */
    UInt32 dataSize;           /* Compressed size */
    UInt32 timestamp;          /* When this state was saved */
    char description[32];      /* Operation description */
} UndoFrame;

typedef struct {
    UndoFrame frames[MAX_UNDO_BUFFERS];
    int currentFrame;          /* Current position in circular buffer */
    int frameCount;            /* Number of valid frames */
    int undoPosition;          /* Position for undo navigation */
    Ptr compressionBuffer;     /* Temporary compression buffer */
} UndoBuffer;

static UndoBuffer gUndoBuffer = {0};

/* External paint buffer and state from MacPaint_Core */
extern BitMap gPaintBuffer;
extern int gDocDirty;

/**
 * MacPaint_InitializeUndo - Set up undo system
 */
OSErr MacPaint_InitializeUndo(void)
{
    /* Allocate compression buffer for undo operations */
    gUndoBuffer.compressionBuffer = NewPtr(UNDO_BUFFER_SIZE + 1024);
    if (!gUndoBuffer.compressionBuffer) {
        return memFullErr;
    }

    /* Initialize undo buffer array */
    for (int i = 0; i < MAX_UNDO_BUFFERS; i++) {
        gUndoBuffer.frames[i].data = NewPtr(UNDO_BUFFER_SIZE);
        if (!gUndoBuffer.frames[i].data) {
            return memFullErr;
        }
        gUndoBuffer.frames[i].dataSize = 0;
        gUndoBuffer.frames[i].timestamp = 0;
        gUndoBuffer.frames[i].description[0] = '\0';
    }

    gUndoBuffer.currentFrame = 0;
    gUndoBuffer.frameCount = 0;
    gUndoBuffer.undoPosition = 0;

    return noErr;
}

/**
 * MacPaint_ShutdownUndo - Clean up undo buffers
 */
void MacPaint_ShutdownUndo(void)
{
    for (int i = 0; i < MAX_UNDO_BUFFERS; i++) {
        if (gUndoBuffer.frames[i].data) {
            DisposePtr(gUndoBuffer.frames[i].data);
            gUndoBuffer.frames[i].data = NULL;
        }
    }

    if (gUndoBuffer.compressionBuffer) {
        DisposePtr(gUndoBuffer.compressionBuffer);
        gUndoBuffer.compressionBuffer = NULL;
    }
}

/**
 * MacPaint_SaveUndoState - Save current bitmap state for undo
 * Returns: noErr if saved, error code otherwise
 */
OSErr MacPaint_SaveUndoState(const char *description)
{
    int uncompressedSize = gPaintBuffer.rowBytes *
                          (gPaintBuffer.bounds.bottom - gPaintBuffer.bounds.top);

    /* Compress bitmap data */
    UInt32 compressedSize = MacPaint_PackBits(
        (unsigned char *)gPaintBuffer.baseAddr,
        uncompressedSize,
        (unsigned char *)gUndoBuffer.compressionBuffer,
        UNDO_BUFFER_SIZE + 1024
    );

    if (compressedSize == 0 || compressedSize > UNDO_BUFFER_SIZE) {
        return ioErr;  /* Compression failed or too large */
    }

    /* Move to next frame in circular buffer */
    gUndoBuffer.currentFrame = (gUndoBuffer.currentFrame + 1) % MAX_UNDO_BUFFERS;
    if (gUndoBuffer.frameCount < MAX_UNDO_BUFFERS) {
        gUndoBuffer.frameCount++;
    }

    /* Save compressed data */
    UndoFrame *frame = &gUndoBuffer.frames[gUndoBuffer.currentFrame];
    memcpy(frame->data, gUndoBuffer.compressionBuffer, compressedSize);
    frame->dataSize = compressedSize;

    /* Get current tick count for undo frame timestamp */
    extern UInt32 TickCount(void);
    frame->timestamp = TickCount();

    if (description) {
        strncpy(frame->description, description, sizeof(frame->description) - 1);
        frame->description[sizeof(frame->description) - 1] = '\0';
    } else {
        frame->description[0] = '\0';
    }

    gUndoBuffer.undoPosition = gUndoBuffer.currentFrame;
    return noErr;
}

/**
 * MacPaint_CanUndo - Check if undo is available
 */
int MacPaint_CanUndo(void)
{
    return (gUndoBuffer.frameCount > 0);
}

/**
 * MacPaint_CanRedo - Check if redo is available
 */
int MacPaint_CanRedo(void)
{
    /* Redo available if we're not at the most recent state */
    if (gUndoBuffer.frameCount == 0) {
        return 0;
    }

    int prevPosition = (gUndoBuffer.undoPosition + 1) % MAX_UNDO_BUFFERS;
    return (prevPosition != gUndoBuffer.currentFrame &&
            gUndoBuffer.frames[prevPosition].dataSize > 0);
}

/**
 * MacPaint_Undo - Restore previous state
 */
OSErr MacPaint_Undo(void)
{
    if (!MacPaint_CanUndo()) {
        return noErr;  /* Nothing to undo */
    }

    /* Move to previous frame */
    gUndoBuffer.undoPosition = (gUndoBuffer.undoPosition - 1 + MAX_UNDO_BUFFERS) % MAX_UNDO_BUFFERS;
    UndoFrame *frame = &gUndoBuffer.frames[gUndoBuffer.undoPosition];

    if (frame->dataSize == 0) {
        return noErr;  /* Frame is empty */
    }

    /* Decompress bitmap data */
    int uncompressedSize = gPaintBuffer.rowBytes *
                          (gPaintBuffer.bounds.bottom - gPaintBuffer.bounds.top);

    UInt32 decompressedSize = MacPaint_UnpackBits(
        (unsigned char *)frame->data,
        frame->dataSize,
        (unsigned char *)gPaintBuffer.baseAddr,
        uncompressedSize
    );

    if (decompressedSize != uncompressedSize) {
        return ioErr;  /* Decompression failed */
    }

    gDocDirty = 1;
    return noErr;
}

/**
 * MacPaint_Redo - Restore next state
 */
OSErr MacPaint_Redo(void)
{
    if (!MacPaint_CanRedo()) {
        return noErr;  /* Nothing to redo */
    }

    /* Move to next frame */
    gUndoBuffer.undoPosition = (gUndoBuffer.undoPosition + 1) % MAX_UNDO_BUFFERS;
    UndoFrame *frame = &gUndoBuffer.frames[gUndoBuffer.undoPosition];

    if (frame->dataSize == 0) {
        return noErr;  /* Frame is empty */
    }

    /* Decompress bitmap data */
    int uncompressedSize = gPaintBuffer.rowBytes *
                          (gPaintBuffer.bounds.bottom - gPaintBuffer.bounds.top);

    UInt32 decompressedSize = MacPaint_UnpackBits(
        (unsigned char *)frame->data,
        frame->dataSize,
        (unsigned char *)gPaintBuffer.baseAddr,
        uncompressedSize
    );

    if (decompressedSize != uncompressedSize) {
        return ioErr;  /* Decompression failed */
    }

    gDocDirty = 1;
    return noErr;
}

/*
 * SELECTION AND CLIPBOARD SYSTEM
 */

typedef struct {
    int active;                 /* Selection is active */
    Rect bounds;               /* Selection rectangle */
    Ptr clipboardData;         /* Clipboard bitmap */
    UInt32 clipboardSize;      /* Clipboard size */
    int clipboardWidth;        /* Clipboard dimensions */
    int clipboardHeight;
} SelectionState;

static SelectionState gSelection = {0};

/**
 * MacPaint_CreateSelection - Create rectangular selection
 */
OSErr MacPaint_CreateSelection(int left, int top, int right, int bottom)
{
    if (left >= right || top >= bottom) {
        return paramErr;
    }

    gSelection.bounds.left = left;
    gSelection.bounds.top = top;
    gSelection.bounds.right = right;
    gSelection.bounds.bottom = bottom;
    gSelection.active = 1;

    return noErr;
}

/**
 * MacPaint_GetSelection - Get current selection rectangle
 */
int MacPaint_GetSelection(Rect *bounds)
{
    if (!bounds) {
        return 0;
    }

    if (gSelection.active) {
        *bounds = gSelection.bounds;
        return 1;
    }

    return 0;
}

/**
 * MacPaint_ClearSelection - Remove current selection
 */
void MacPaint_ClearSelection(void)
{
    gSelection.active = 0;
}

/**
 * MacPaint_CopySelectionToClipboard - Copy selected region to clipboard
 */
OSErr MacPaint_CopySelectionToClipboard(void)
{
    if (!gSelection.active) {
        return paramErr;
    }

    int width = gSelection.bounds.right - gSelection.bounds.left;
    int height = gSelection.bounds.bottom - gSelection.bounds.top;

    /* Calculate bitmap size for selection */
    int bytesPerRow = (width + 7) / 8;
    UInt32 bitmapSize = bytesPerRow * height;

    /* Allocate clipboard buffer if needed */
    if (!gSelection.clipboardData || gSelection.clipboardSize < bitmapSize) {
        if (gSelection.clipboardData) {
            DisposePtr(gSelection.clipboardData);
        }

        gSelection.clipboardData = NewPtr(bitmapSize + 1024);
        if (!gSelection.clipboardData) {
            return memFullErr;
        }

        gSelection.clipboardSize = bitmapSize + 1024;
    }

    /* Copy selected pixels from paint buffer to clipboard
     * Iterate through each pixel in the selection rectangle
     * Convert from source coordinates to clipboard coordinates
     */
    unsigned char *srcBits = (unsigned char *)gPaintBuffer.baseAddr;
    unsigned char *dstBits = (unsigned char *)gSelection.clipboardData;
    int srcRowBytes = gPaintBuffer.rowBytes;

    /* Zero the clipboard buffer first */
    memset(dstBits, 0, bitmapSize);

    /* Copy pixel data from selection */
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            /* Source pixel in paint buffer */
            int srcX = gSelection.bounds.left + x;
            int srcY = gSelection.bounds.top + y;

            /* Get source pixel */
            int srcByteOffset = srcY * srcRowBytes + (srcX / 8);
            int srcBitOffset = 7 - (srcX % 8);
            int srcPixel = (srcBits[srcByteOffset] >> srcBitOffset) & 1;

            /* Destination pixel in clipboard */
            int dstByteOffset = y * bytesPerRow + (x / 8);
            int dstBitOffset = 7 - (x % 8);

            if (srcPixel) {
                dstBits[dstByteOffset] |= (1 << dstBitOffset);
            }
        }
    }

    gSelection.clipboardWidth = width;
    gSelection.clipboardHeight = height;

    return noErr;
}

/**
 * MacPaint_PasteFromClipboard - Paste clipboard at position
 * x, y: position to paste at
 */
OSErr MacPaint_PasteFromClipboard(int x, int y)
{
    if (!gSelection.clipboardData) {
        return paramErr;
    }

    /* Paste clipboard bitmap at (x, y)
     * Copy from clipboard buffer back to paint buffer
     */
    unsigned char *dstBits = (unsigned char *)gPaintBuffer.baseAddr;
    unsigned char *srcBits = (unsigned char *)gSelection.clipboardData;
    int dstRowBytes = gPaintBuffer.rowBytes;

    int width = gSelection.clipboardWidth;
    int height = gSelection.clipboardHeight;
    int srcBytesPerRow = (width + 7) / 8;

    /* Paste pixels from clipboard to paint buffer at (x, y)
     * Bounds check to prevent writing outside canvas
     */
    for (int py = 0; py < height; py++) {
        int dstY = y + py;

        /* Skip rows that are outside the canvas */
        if (dstY < 0 || dstY >= gPaintBuffer.bounds.bottom - gPaintBuffer.bounds.top) {
            continue;
        }

        for (int px = 0; px < width; px++) {
            int dstX = x + px;

            /* Skip pixels that are outside the canvas */
            if (dstX < 0 || dstX >= gPaintBuffer.bounds.right - gPaintBuffer.bounds.left) {
                continue;
            }

            /* Source pixel in clipboard */
            int srcByteOffset = py * srcBytesPerRow + (px / 8);
            int srcBitOffset = 7 - (px % 8);
            int srcPixel = (srcBits[srcByteOffset] >> srcBitOffset) & 1;

            /* Destination pixel in paint buffer */
            int dstByteOffset = dstY * dstRowBytes + (dstX / 8);
            int dstBitOffset = 7 - (dstX % 8);

            if (srcPixel) {
                /* Paint black pixel (set bit) */
                dstBits[dstByteOffset] |= (1 << dstBitOffset);
            } else {
                /* Erase white pixel (clear bit) */
                dstBits[dstByteOffset] &= ~(1 << dstBitOffset);
            }
        }
    }

    /* Mark document as dirty and create new selection at paste location */
    gDocDirty = 1;
    MacPaint_CreateSelection(x, y, x + width, y + height);

    return noErr;
}

/**
 * MacPaint_CutSelection - Cut (copy and clear) selection
 */
OSErr MacPaint_CutSelection(void)
{
    OSErr err = MacPaint_CopySelectionToClipboard();
    if (err != noErr) {
        return err;
    }

    /* Clear selection area by erasing all pixels (setting to white/0) */
    unsigned char *bits = (unsigned char *)gPaintBuffer.baseAddr;
    int rowBytes = gPaintBuffer.rowBytes;
    int width = gSelection.bounds.right - gSelection.bounds.left;
    int height = gSelection.bounds.bottom - gSelection.bounds.top;

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int srcX = gSelection.bounds.left + x;
            int srcY = gSelection.bounds.top + y;

            /* Clear this pixel (set to white) */
            int byteOffset = srcY * rowBytes + (srcX / 8);
            int bitOffset = 7 - (srcX % 8);
            bits[byteOffset] &= ~(1 << bitOffset);
        }
    }

    gDocDirty = 1;
    return noErr;
}

/*
 * PATTERN EDITOR
 */

typedef struct {
    int open;                   /* Editor window open */
    Pattern editPattern;        /* Pattern being edited */
    int selectedPattern;        /* Which pattern slot */
    Rect editorBounds;         /* Window bounds */
} PatternEditor;

static PatternEditor gPatternEditor = {0};

/**
 * MacPaint_OpenPatternEditor - Open pattern editor window
 */
OSErr MacPaint_OpenPatternEditor(void)
{
    if (gPatternEditor.open) {
        return noErr;
    }

    /* Copy current pattern into editor for editing */
    extern Pattern gCurrentPattern;
    gPatternEditor.editPattern = gCurrentPattern;
    gPatternEditor.open = 1;

    /* Set editor bounds (8x8 grid at 8x magnification = 64x64 pixels) */
    gPatternEditor.editorBounds.left = 200;
    gPatternEditor.editorBounds.top = 100;
    gPatternEditor.editorBounds.right = 200 + 64;
    gPatternEditor.editorBounds.bottom = 100 + 64;

    return noErr;
}

/**
 * MacPaint_ClosePatternEditor - Close pattern editor and apply
 */
void MacPaint_ClosePatternEditor(void)
{
    if (gPatternEditor.open) {
        /* Apply edited pattern as current pattern */
        extern Pattern gCurrentPattern;
        gCurrentPattern = gPatternEditor.editPattern;
    }
    gPatternEditor.open = 0;
}

/**
 * MacPaint_SetPatternEditorPattern - Load pattern into editor
 */
void MacPaint_SetPatternEditorPattern(int patternIndex)
{
    if (patternIndex >= 0 && patternIndex < 38) {
        gPatternEditor.selectedPattern = patternIndex;
        /* Load the current active pattern into editor */
        extern Pattern gCurrentPattern;
        gPatternEditor.editPattern = gCurrentPattern;
    }
}

/**
 * MacPaint_GetPatternEditorPattern - Get edited pattern
 */
Pattern MacPaint_GetPatternEditorPattern(void)
{
    return gPatternEditor.editPattern;
}

/**
 * MacPaint_PatternEditorPixelClick - Toggle pixel in pattern grid
 * x, y: pixel coordinates within the 8x8 pattern (0-7 each)
 */
void MacPaint_PatternEditorPixelClick(int x, int y)
{
    if (!gPatternEditor.open) return;
    if (x < 0 || x > 7 || y < 0 || y > 7) return;

    /* Toggle the bit at (x, y) in the pattern */
    int bitOff = 7 - x;
    gPatternEditor.editPattern.pat[y] ^= (1 << bitOff);
}

/**
 * MacPaint_IsPatternEditorOpen - Check if pattern editor is active
 */
int MacPaint_IsPatternEditorOpen(void)
{
    return gPatternEditor.open;
}

/*
 * BRUSH EDITOR
 */

typedef struct {
    int open;                   /* Editor window open */
    int selectedBrush;         /* Current brush style */
    Rect editorBounds;         /* Window bounds */
    int brushSize;             /* Brush diameter */
} BrushEditor;

static BrushEditor gBrushEditor = {0};

/**
 * MacPaint_OpenBrushEditor - Open brush editor window
 */
OSErr MacPaint_OpenBrushEditor(void)
{
    if (gBrushEditor.open) {
        return noErr;
    }

    /* Initialize with defaults */
    if (gBrushEditor.brushSize == 0) {
        gBrushEditor.brushSize = 1;
    }
    gBrushEditor.open = 1;

    return noErr;
}

/**
 * MacPaint_CloseBrushEditor - Close brush editor and apply
 */
void MacPaint_CloseBrushEditor(void)
{
    if (gBrushEditor.open) {
        /* Apply selected brush size to the line size */
        extern void MacPaint_SetLineSize(int size);
        MacPaint_SetLineSize(gBrushEditor.brushSize);
    }
    gBrushEditor.open = 0;
}

/**
 * MacPaint_SetBrushSize - Set brush diameter
 */
void MacPaint_SetBrushSize(int diameter)
{
    if (diameter >= 1 && diameter <= 64) {
        gBrushEditor.brushSize = diameter;
    }
}

/**
 * MacPaint_GetBrushSize - Get current brush size
 */
int MacPaint_GetBrushSize(void)
{
    return gBrushEditor.brushSize;
}

/*
 * ADVANCED DRAWING MODES
 */

/* Global drawing mode state */
static int gDrawingMode = 0;  /* 0=replace, 1=OR, 2=XOR, 3=AND */

/**
 * MacPaint_SetDrawingMode - Set pixel blending mode
 * mode: 0=replace, 1=OR, 2=XOR, 3=AND (clear)
 */
void MacPaint_SetDrawingMode(int mode)
{
    if (mode >= 0 && mode <= 3) {
        gDrawingMode = mode;
    }
}

/**
 * MacPaint_GetDrawingMode - Get current drawing mode
 */
int MacPaint_GetDrawingMode(void)
{
    return gDrawingMode;
}

/*
 * SELECTION TRANSFORMATIONS
 */

/**
 * MacPaint_FlipSelectionHorizontal - Mirror selection horizontally
 */
OSErr MacPaint_FlipSelectionHorizontal(void)
{
    if (!gSelection.active) {
        return paramErr;
    }

    unsigned char *bits = (unsigned char *)gPaintBuffer.baseAddr;
    int rowBytes = gPaintBuffer.rowBytes;
    int width = gSelection.bounds.right - gSelection.bounds.left;
    int height = gSelection.bounds.bottom - gSelection.bounds.top;

    for (int y = 0; y < height; y++) {
        int srcY = gSelection.bounds.top + y;
        /* Swap pixels from left and right edges inward */
        for (int x = 0; x < width / 2; x++) {
            int leftX = gSelection.bounds.left + x;
            int rightX = gSelection.bounds.right - 1 - x;

            /* Read left pixel */
            int lByte = srcY * rowBytes + (leftX / 8);
            int lBit = 7 - (leftX % 8);
            int leftVal = (bits[lByte] >> lBit) & 1;

            /* Read right pixel */
            int rByte = srcY * rowBytes + (rightX / 8);
            int rBit = 7 - (rightX % 8);
            int rightVal = (bits[rByte] >> rBit) & 1;

            /* Write left pixel with right value */
            if (rightVal) bits[lByte] |= (1 << lBit);
            else          bits[lByte] &= ~(1 << lBit);

            /* Write right pixel with left value */
            if (leftVal) bits[rByte] |= (1 << rBit);
            else         bits[rByte] &= ~(1 << rBit);
        }
    }

    gDocDirty = 1;
    return noErr;
}

/**
 * MacPaint_FlipSelectionVertical - Mirror selection vertically
 */
OSErr MacPaint_FlipSelectionVertical(void)
{
    if (!gSelection.active) {
        return paramErr;
    }

    unsigned char *bits = (unsigned char *)gPaintBuffer.baseAddr;
    int rowBytes = gPaintBuffer.rowBytes;
    int width = gSelection.bounds.right - gSelection.bounds.left;
    int height = gSelection.bounds.bottom - gSelection.bounds.top;

    for (int y = 0; y < height / 2; y++) {
        int topY = gSelection.bounds.top + y;
        int botY = gSelection.bounds.bottom - 1 - y;

        for (int x = 0; x < width; x++) {
            int px = gSelection.bounds.left + x;

            /* Read top pixel */
            int tByte = topY * rowBytes + (px / 8);
            int tBit = 7 - (px % 8);
            int topVal = (bits[tByte] >> tBit) & 1;

            /* Read bottom pixel */
            int bByte = botY * rowBytes + (px / 8);
            int bBit = 7 - (px % 8);
            int botVal = (bits[bByte] >> bBit) & 1;

            /* Swap */
            if (botVal) bits[tByte] |= (1 << tBit);
            else        bits[tByte] &= ~(1 << tBit);

            if (topVal) bits[bByte] |= (1 << bBit);
            else        bits[bByte] &= ~(1 << bBit);
        }
    }

    gDocDirty = 1;
    return noErr;
}

/**
 * MacPaint_RotateSelectionCW - Rotate selection 90° clockwise
 */
OSErr MacPaint_RotateSelectionCW(void)
{
    if (!gSelection.active) {
        return paramErr;
    }

    unsigned char *bits = (unsigned char *)gPaintBuffer.baseAddr;
    int rowBytes = gPaintBuffer.rowBytes;
    int width = gSelection.bounds.right - gSelection.bounds.left;
    int height = gSelection.bounds.bottom - gSelection.bounds.top;

    /* Allocate temp buffer for the rotated pixels */
    int tmpRowBytes = (height + 7) / 8;
    int tmpSize = tmpRowBytes * width;
    Ptr tmpBuf = NewPtr(tmpSize);
    if (!tmpBuf) return memFullErr;
    memset(tmpBuf, 0, tmpSize);

    /* Copy pixels into rotated positions: dst(x,y) = src(y, width-1-x) */
    for (int sy = 0; sy < height; sy++) {
        for (int sx = 0; sx < width; sx++) {
            int srcPx = gSelection.bounds.left + sx;
            int srcPy = gSelection.bounds.top + sy;
            int sByte = srcPy * rowBytes + (srcPx / 8);
            int sBit = 7 - (srcPx % 8);
            int val = (bits[sByte] >> sBit) & 1;

            /* Rotated CW: new_x = height-1-sy, new_y = sx */
            int dx = height - 1 - sy;
            int dy = sx;
            int dByte = dy * tmpRowBytes + (dx / 8);
            int dBit = 7 - (dx % 8);
            if (val) ((unsigned char *)tmpBuf)[dByte] |= (1 << dBit);
        }
    }

    /* Clear original selection area */
    for (int sy = 0; sy < height; sy++) {
        for (int sx = 0; sx < width; sx++) {
            int px = gSelection.bounds.left + sx;
            int py = gSelection.bounds.top + sy;
            int byteOff = py * rowBytes + (px / 8);
            int bitOff = 7 - (px % 8);
            bits[byteOff] &= ~(1 << bitOff);
        }
    }

    /* Write rotated pixels back (new dimensions: height x width) */
    int newWidth = height;
    int newHeight = width;
    for (int dy = 0; dy < newHeight; dy++) {
        for (int dx = 0; dx < newWidth; dx++) {
            int px = gSelection.bounds.left + dx;
            int py = gSelection.bounds.top + dy;
            /* Bounds check against canvas */
            if (px >= gPaintBuffer.bounds.right || py >= gPaintBuffer.bounds.bottom)
                continue;
            int dByte = dy * tmpRowBytes + (dx / 8);
            int dBit = 7 - (dx % 8);
            int val = (((unsigned char *)tmpBuf)[dByte] >> dBit) & 1;
            if (val) {
                int byteOff = py * rowBytes + (px / 8);
                int bitOff = 7 - (px % 8);
                bits[byteOff] |= (1 << bitOff);
            }
        }
    }

    /* Update selection bounds */
    gSelection.bounds.right = gSelection.bounds.left + newWidth;
    gSelection.bounds.bottom = gSelection.bounds.top + newHeight;

    DisposePtr(tmpBuf);
    gDocDirty = 1;
    return noErr;
}

/**
 * MacPaint_RotateSelectionCCW - Rotate selection 90° counter-clockwise
 */
OSErr MacPaint_RotateSelectionCCW(void)
{
    if (!gSelection.active) {
        return paramErr;
    }

    unsigned char *bits = (unsigned char *)gPaintBuffer.baseAddr;
    int rowBytes = gPaintBuffer.rowBytes;
    int width = gSelection.bounds.right - gSelection.bounds.left;
    int height = gSelection.bounds.bottom - gSelection.bounds.top;

    /* Allocate temp buffer for the rotated pixels */
    int tmpRowBytes = (height + 7) / 8;
    int tmpSize = tmpRowBytes * width;
    Ptr tmpBuf = NewPtr(tmpSize);
    if (!tmpBuf) return memFullErr;
    memset(tmpBuf, 0, tmpSize);

    /* Copy pixels into rotated positions: dst(x,y) = src(width-1-y, x) */
    for (int sy = 0; sy < height; sy++) {
        for (int sx = 0; sx < width; sx++) {
            int srcPx = gSelection.bounds.left + sx;
            int srcPy = gSelection.bounds.top + sy;
            int sByte = srcPy * rowBytes + (srcPx / 8);
            int sBit = 7 - (srcPx % 8);
            int val = (bits[sByte] >> sBit) & 1;

            /* Rotated CCW: new_x = sy, new_y = width-1-sx */
            int dx = sy;
            int dy = width - 1 - sx;
            int dByte = dy * tmpRowBytes + (dx / 8);
            int dBit = 7 - (dx % 8);
            if (val) ((unsigned char *)tmpBuf)[dByte] |= (1 << dBit);
        }
    }

    /* Clear original selection area */
    for (int sy = 0; sy < height; sy++) {
        for (int sx = 0; sx < width; sx++) {
            int px = gSelection.bounds.left + sx;
            int py = gSelection.bounds.top + sy;
            int byteOff = py * rowBytes + (px / 8);
            int bitOff = 7 - (px % 8);
            bits[byteOff] &= ~(1 << bitOff);
        }
    }

    /* Write rotated pixels back (new dimensions: height x width) */
    int newWidth = height;
    int newHeight = width;
    for (int dy = 0; dy < newHeight; dy++) {
        for (int dx = 0; dx < newWidth; dx++) {
            int px = gSelection.bounds.left + dx;
            int py = gSelection.bounds.top + dy;
            if (px >= gPaintBuffer.bounds.right || py >= gPaintBuffer.bounds.bottom)
                continue;
            int dByte = dy * tmpRowBytes + (dx / 8);
            int dBit = 7 - (dx % 8);
            int val = (((unsigned char *)tmpBuf)[dByte] >> dBit) & 1;
            if (val) {
                int byteOff = py * rowBytes + (px / 8);
                int bitOff = 7 - (px % 8);
                bits[byteOff] |= (1 << bitOff);
            }
        }
    }

    /* Update selection bounds */
    gSelection.bounds.right = gSelection.bounds.left + newWidth;
    gSelection.bounds.bottom = gSelection.bounds.top + newHeight;

    DisposePtr(tmpBuf);
    gDocDirty = 1;
    return noErr;
}

/**
 * MacPaint_ScaleSelection - Scale selection to new size
 */
OSErr MacPaint_ScaleSelection(int newWidth, int newHeight)
{
    if (!gSelection.active || newWidth <= 0 || newHeight <= 0) {
        return paramErr;
    }

    unsigned char *bits = (unsigned char *)gPaintBuffer.baseAddr;
    int rowBytes = gPaintBuffer.rowBytes;
    int srcWidth = gSelection.bounds.right - gSelection.bounds.left;
    int srcHeight = gSelection.bounds.bottom - gSelection.bounds.top;

    /* Allocate temp buffer for scaled result */
    int tmpRowBytes = (newWidth + 7) / 8;
    int tmpSize = tmpRowBytes * newHeight;
    Ptr tmpBuf = NewPtr(tmpSize);
    if (!tmpBuf) return memFullErr;
    memset(tmpBuf, 0, tmpSize);

    /* Nearest-neighbor scaling */
    for (int dy = 0; dy < newHeight; dy++) {
        int sy = (dy * srcHeight) / newHeight;
        int srcPy = gSelection.bounds.top + sy;

        for (int dx = 0; dx < newWidth; dx++) {
            int sx = (dx * srcWidth) / newWidth;
            int srcPx = gSelection.bounds.left + sx;

            /* Read source pixel */
            int sByte = srcPy * rowBytes + (srcPx / 8);
            int sBit = 7 - (srcPx % 8);
            int val = (bits[sByte] >> sBit) & 1;

            if (val) {
                int dByte = dy * tmpRowBytes + (dx / 8);
                int dBit = 7 - (dx % 8);
                ((unsigned char *)tmpBuf)[dByte] |= (1 << dBit);
            }
        }
    }

    /* Clear the larger of old and new areas */
    int clearW = (newWidth > srcWidth) ? newWidth : srcWidth;
    int clearH = (newHeight > srcHeight) ? newHeight : srcHeight;
    for (int y = 0; y < clearH; y++) {
        int py = gSelection.bounds.top + y;
        if (py >= gPaintBuffer.bounds.bottom) break;
        for (int x = 0; x < clearW; x++) {
            int px = gSelection.bounds.left + x;
            if (px >= gPaintBuffer.bounds.right) break;
            int byteOff = py * rowBytes + (px / 8);
            int bitOff = 7 - (px % 8);
            bits[byteOff] &= ~(1 << bitOff);
        }
    }

    /* Write scaled result back */
    for (int dy = 0; dy < newHeight; dy++) {
        int py = gSelection.bounds.top + dy;
        if (py >= gPaintBuffer.bounds.bottom) break;
        for (int dx = 0; dx < newWidth; dx++) {
            int px = gSelection.bounds.left + dx;
            if (px >= gPaintBuffer.bounds.right) break;
            int dByte = dy * tmpRowBytes + (dx / 8);
            int dBit = 7 - (dx % 8);
            int val = (((unsigned char *)tmpBuf)[dByte] >> dBit) & 1;
            if (val) {
                int byteOff = py * rowBytes + (px / 8);
                int bitOff = 7 - (px % 8);
                bits[byteOff] |= (1 << bitOff);
            }
        }
    }

    /* Update selection bounds */
    gSelection.bounds.right = gSelection.bounds.left + newWidth;
    gSelection.bounds.bottom = gSelection.bounds.top + newHeight;

    DisposePtr(tmpBuf);
    gDocDirty = 1;
    return noErr;
}

/*
 * ADVANCED FILL MODES
 */

/**
 * MacPaint_FillSelectionWithPattern - Fill selection with current pattern
 */
OSErr MacPaint_FillSelectionWithPattern(void)
{
    if (!gSelection.active) {
        return paramErr;
    }

    extern Pattern gCurrentPattern;
    unsigned char *bits = (unsigned char *)gPaintBuffer.baseAddr;
    int rowBytes = gPaintBuffer.rowBytes;
    int width = gSelection.bounds.right - gSelection.bounds.left;
    int height = gSelection.bounds.bottom - gSelection.bounds.top;

    for (int y = 0; y < height; y++) {
        int py = gSelection.bounds.top + y;
        unsigned char patRow = gCurrentPattern.pat[py % 8];

        for (int x = 0; x < width; x++) {
            int px = gSelection.bounds.left + x;
            int patBit = 7 - (px % 8);
            int val = (patRow >> patBit) & 1;

            int byteOff = py * rowBytes + (px / 8);
            int bitOff = 7 - (px % 8);
            if (val) bits[byteOff] |= (1 << bitOff);
            else     bits[byteOff] &= ~(1 << bitOff);
        }
    }

    gDocDirty = 1;
    return noErr;
}

/**
 * MacPaint_GradientFill - Fill with gradient (light to dark)
 */
OSErr MacPaint_GradientFill(void)
{
    if (!gSelection.active) {
        return paramErr;
    }

    unsigned char *bits = (unsigned char *)gPaintBuffer.baseAddr;
    int rowBytes = gPaintBuffer.rowBytes;
    int width = gSelection.bounds.right - gSelection.bounds.left;
    int height = gSelection.bounds.bottom - gSelection.bounds.top;

    if (height <= 0) return paramErr;

    /* Create top-to-bottom dithered gradient using ordered dithering */
    /* 4x4 Bayer matrix for ordered dithering */
    static const int bayer4[4][4] = {
        { 0,  8,  2, 10},
        {12,  4, 14,  6},
        { 3, 11,  1,  9},
        {15,  7, 13,  5}
    };

    for (int y = 0; y < height; y++) {
        int py = gSelection.bounds.top + y;
        /* Gradient intensity: 0 (white/top) to 15 (black/bottom) */
        int intensity = (y * 16) / height;
        if (intensity > 15) intensity = 15;

        for (int x = 0; x < width; x++) {
            int px = gSelection.bounds.left + x;
            int threshold = bayer4[y % 4][x % 4];
            int val = (intensity > threshold) ? 1 : 0;

            int byteOff = py * rowBytes + (px / 8);
            int bitOff = 7 - (px % 8);
            if (val) bits[byteOff] |= (1 << bitOff);
            else     bits[byteOff] &= ~(1 << bitOff);
        }
    }

    gDocDirty = 1;
    return noErr;
}

/**
 * MacPaint_SmoothSelection - Smooth selection edges with anti-aliasing
 */
OSErr MacPaint_SmoothSelection(void)
{
    if (!gSelection.active) {
        return paramErr;
    }

    unsigned char *bits = (unsigned char *)gPaintBuffer.baseAddr;
    int rowBytes = gPaintBuffer.rowBytes;
    int width = gSelection.bounds.right - gSelection.bounds.left;
    int height = gSelection.bounds.bottom - gSelection.bounds.top;

    /* Allocate temp buffer for smoothed result */
    int tmpRowBytes = (width + 7) / 8;
    int tmpSize = tmpRowBytes * height;
    Ptr tmpBuf = NewPtr(tmpSize);
    if (!tmpBuf) return memFullErr;
    memset(tmpBuf, 0, tmpSize);

    /* 3x3 box filter: pixel is set if majority of neighbors are set */
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int count = 0;
            /* Count set pixels in 3x3 neighborhood */
            for (int dy = -1; dy <= 1; dy++) {
                for (int dx = -1; dx <= 1; dx++) {
                    int nx = x + dx;
                    int ny = y + dy;
                    if (nx < 0 || nx >= width || ny < 0 || ny >= height) continue;
                    int px = gSelection.bounds.left + nx;
                    int py = gSelection.bounds.top + ny;
                    int byteOff = py * rowBytes + (px / 8);
                    int bitOff = 7 - (px % 8);
                    if ((bits[byteOff] >> bitOff) & 1) count++;
                }
            }
            /* Threshold: 5 or more of 9 neighbors set → pixel on */
            if (count >= 5) {
                int dByte = y * tmpRowBytes + (x / 8);
                int dBit = 7 - (x % 8);
                ((unsigned char *)tmpBuf)[dByte] |= (1 << dBit);
            }
        }
    }

    /* Write smoothed result back to canvas */
    for (int y = 0; y < height; y++) {
        int py = gSelection.bounds.top + y;
        for (int x = 0; x < width; x++) {
            int px = gSelection.bounds.left + x;
            int sByte = y * tmpRowBytes + (x / 8);
            int sBit = 7 - (x % 8);
            int val = (((unsigned char *)tmpBuf)[sByte] >> sBit) & 1;
            int byteOff = py * rowBytes + (px / 8);
            int bitOff = 7 - (px % 8);
            if (val) bits[byteOff] |= (1 << bitOff);
            else     bits[byteOff] &= ~(1 << bitOff);
        }
    }

    DisposePtr(tmpBuf);
    gDocDirty = 1;
    return noErr;
}

/*
 * STATE QUERIES
 */

/**
 * MacPaint_IsBrushEditorOpen - Check if brush editor is open
 */
int MacPaint_IsBrushEditorOpen(void)
{
    return gBrushEditor.open;
}

/**
 * MacPaint_IsSelectionActive - Check if selection exists
 */
int MacPaint_IsSelectionActive(void)
{
    return gSelection.active;
}

/**
 * MacPaint_HasClipboard - Check if clipboard has content
 */
int MacPaint_HasClipboard(void)
{
    return (gSelection.clipboardData != NULL);
}

/*
 * OPERATION DESCRIPTIONS FOR UNDO
 */

/**
 * MacPaint_GetUndoDescription - Get description of undo item
 */
const char* MacPaint_GetUndoDescription(void)
{
    if (!MacPaint_CanUndo()) {
        return "(no undo available)";
    }

    return gUndoBuffer.frames[gUndoBuffer.undoPosition].description;
}

/**
 * MacPaint_GetRedoDescription - Get description of redo item
 */
const char* MacPaint_GetRedoDescription(void)
{
    if (!MacPaint_CanRedo()) {
        return "(no redo available)";
    }

    int redoPos = (gUndoBuffer.undoPosition + 1) % MAX_UNDO_BUFFERS;
    return gUndoBuffer.frames[redoPos].description;
}
