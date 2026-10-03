/*
 * Pictures.c - QuickDraw Picture (PICT) Implementation
 *
 * Implements picture recording and playback for QuickDraw metafile format.
 * Pictures record drawing operations and can be played back with scaling.
 *
 * Copyright (c) 2025 - System 7.1 Portable Project
 * Derived from System 7 ROM analysis (Ghidra) QuickDraw
 */

#include "SystemTypes.h"
#include "QuickDraw/QuickDraw.h"
#include "QuickDraw/QDPictures.h"
#include "QuickDrawConstants.h"
#include "MemoryMgr/MemoryManager.h"
#include <string.h>

/* Current port from QuickDrawCore.c */

/* Picture opcodes (simplified PICT format subset) */
#define picOpNop        0x00
#define picOpClipRgn    0x01
#define picOpPnSize     0x07
#define picOpPnMode     0x08
#define picOpPnPat      0x09
#define picOpFillPat    0x0A
#define picOpLine       0x20
#define picOpLineFrom   0x21
#define picOpShortLine  0x22
#define picOpShortLineFrom 0x23
#define picOpFrameRect  0x30
#define picOpPaintRect  0x31
#define picOpEraseRect  0x32
#define picOpInvertRect 0x33
#define picOpFillRect   0x34
#define picOpFrameOval  0x50
#define picOpPaintOval  0x51
#define picOpEraseOval  0x52
#define picOpInvertOval 0x53
#define picOpFillOval   0x54
#define picOpFrameArc   0x60
#define picOpPaintArc   0x61
#define picOpEraseArc   0x62
#define picOpInvertArc  0x63
#define picOpFillArc    0x64
#define picOpFramePoly  0x70
#define picOpPaintPoly  0x71
#define picOpErasePoly  0x72
#define picOpInvertPoly 0x73
#define picOpFillPoly   0x74
#define picOpFrameRgn   0x80
#define picOpPaintRgn   0x81
#define picOpEraseRgn   0x82
#define picOpInvertRgn  0x83
#define picOpFillRgn    0x84
#define picOpEndPic     0xFF

/* Picture recording state */
typedef struct {
    Boolean recording;
    PicHandle currentPic;
    UInt8 *dataPtr;
    SInt32 dataSize;
    SInt32 dataCapacity;
} PictureState;

static PictureState g_pictureState = {false, NULL, NULL, 0, 0};

/* Helper: Write byte to picture data */
static void WriteByte(UInt8 byte) {
    if (!g_pictureState.recording || !g_pictureState.dataPtr) return;

    /* Expand capacity if needed */
    if (g_pictureState.dataSize >= g_pictureState.dataCapacity) {
        /* Calculate new capacity (grow by 256 bytes at a time) */
        SInt32 newCapacity = g_pictureState.dataCapacity + 256;
        SInt32 newHandleSize = sizeof(Picture) + newCapacity;

        /* Resize the picture handle */
        if (!SetHandleSize((Handle)g_pictureState.currentPic, newHandleSize)) {
            /* Resize failed - stop recording */
            return;
        }

        /* Update pointers since handle may have moved */
        g_pictureState.dataPtr = (UInt8 *)(*g_pictureState.currentPic) + sizeof(Picture);
        g_pictureState.dataCapacity = newCapacity;
    }

    g_pictureState.dataPtr[g_pictureState.dataSize++] = byte;
}

/* Helper: Write short to picture data (big-endian) */
static void WriteShort(SInt16 value) {
    WriteByte((value >> 8) & 0xFF);
    WriteByte(value & 0xFF);
}

/* Helper: Write rect to picture data */
static void WriteRect(const Rect *r) {
    WriteShort(r->top);
    WriteShort(r->left);
    WriteShort(r->bottom);
    WriteShort(r->right);
}

/* Helper: Write pattern to picture data */
/*
 * OpenPicture - Begin picture recording
 */
PicHandle OpenPicture(const Rect *picFrame) {
    if (!picFrame) return NULL;

    /* Allocate picture handle */
    PicHandle pic = (PicHandle)NewHandle(sizeof(Picture) + 1024);  /* Initial data size */
    if (!pic || !*pic) return NULL;

    /* The header big-endian, as a PICT is stored and DrawPicture reads it */
    UInt8 *raw = (UInt8 *)*pic;
    raw[0] = 0;
    raw[1] = sizeof(Picture);
    raw[2] = (UInt8)(picFrame->top >> 8);    raw[3] = (UInt8)picFrame->top;
    raw[4] = (UInt8)(picFrame->left >> 8);   raw[5] = (UInt8)picFrame->left;
    raw[6] = (UInt8)(picFrame->bottom >> 8); raw[7] = (UInt8)picFrame->bottom;
    raw[8] = (UInt8)(picFrame->right >> 8);  raw[9] = (UInt8)picFrame->right;

    /* Initialize recording state */
    g_pictureState.recording = true;
    g_pictureState.currentPic = pic;
    g_pictureState.dataPtr = (UInt8 *)(*pic) + sizeof(Picture);  /* Data follows header */
    g_pictureState.dataSize = 0;
    g_pictureState.dataCapacity = 1024;
    WriteByte(0x11);                    /* version 1 */
    WriteByte(0x01);

    return pic;
}

/*
 * ClosePicture - End picture recording
 */
void ClosePicture(void) {
    if (!g_pictureState.recording) return;

    /* Write end-of-picture opcode */
    WriteByte(picOpEndPic);

    /* Update picture size, big-endian */
    if (g_pictureState.currentPic && *g_pictureState.currentPic) {
        UInt16 size = (UInt16)(sizeof(Picture) + g_pictureState.dataSize);
        UInt8 *raw = (UInt8 *)*g_pictureState.currentPic;
        raw[0] = (UInt8)(size >> 8);
        raw[1] = (UInt8)size;
    }

    /* Stop recording */
    g_pictureState.recording = false;
    g_pictureState.currentPic = NULL;
    g_pictureState.dataPtr = NULL;
    g_pictureState.dataSize = 0;
}

/*
 * KillPicture - Dispose of picture
 */
void KillPicture(PicHandle myPicture) {
    if (myPicture) {
        DisposeHandle((Handle)myPicture);
    }
}

/*
 * PicComment - Add comment to picture
 */
void PicComment(SInt16 kind, SInt16 dataSize, Handle dataHandle) {
    if (!g_pictureState.recording) return;

    /* Write comment opcode (0xA1) */
    WriteByte(0xA1);
    WriteShort(kind);
    WriteShort(dataSize);

    /* Write comment data if provided */
    if (dataHandle && *dataHandle && dataSize > 0) {
        UInt8 *commentData = (UInt8 *)(*dataHandle);
        for (SInt16 i = 0; i < dataSize; i++) {
            WriteByte(commentData[i]);
        }
    }
}

/*
 * Picture Recording Hooks - Called by QuickDraw primitives
 */

void PictureRecordFrameRect(const Rect *r) {
    if (!g_pictureState.recording || !r) return;
    WriteByte(picOpFrameRect);
    WriteRect(r);
}

void PictureRecordPaintRect(const Rect *r) {
    if (!g_pictureState.recording || !r) return;
    WriteByte(picOpPaintRect);
    WriteRect(r);
}

void PictureRecordEraseRect(const Rect *r) {
    if (!g_pictureState.recording || !r) return;
    WriteByte(picOpEraseRect);
    WriteRect(r);
}

void PictureRecordInvertRect(const Rect *r) {
    if (!g_pictureState.recording || !r) return;
    WriteByte(picOpInvertRect);
    WriteRect(r);
}

void PictureRecordFrameOval(const Rect *r) {
    if (!g_pictureState.recording || !r) return;
    WriteByte(picOpFrameOval);
    WriteRect(r);
}

void PictureRecordPaintOval(const Rect *r) {
    if (!g_pictureState.recording || !r) return;
    WriteByte(picOpPaintOval);
    WriteRect(r);
}

void PictureRecordEraseOval(const Rect *r) {
    if (!g_pictureState.recording || !r) return;
    WriteByte(picOpEraseOval);
    WriteRect(r);
}

void PictureRecordInvertOval(const Rect *r) {
    if (!g_pictureState.recording || !r) return;
    WriteByte(picOpInvertOval);
    WriteRect(r);
}
