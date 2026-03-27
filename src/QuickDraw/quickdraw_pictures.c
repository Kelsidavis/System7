/* #include "SystemTypes.h" */
#include "QuickDraw/QuickDrawInternal.h"
/*
 * QuickDraw Picture playback
 *
 * Implements a minimal-but-functional PICT 1/2 interpreter capable of
 * rendering the monochrome PackBitsRect pictures used throughout the
 * System 7 resource set. Implementation derived from public documentation
 * (Inside Macintosh, TN1035) and clean-room investigation of the ROM.
 */

#include "QuickDraw/QuickDraw.h"
#include "QuickDraw/quickdraw_types.h"
#include "SystemTypes.h"
#include "QuickDrawConstants.h"
#include "MemoryMgr/MemoryManager.h"
#include "FontManager/FontManager.h"

#include <string.h>

/* Global current port - from QuickDrawCore.c */
extern GrafPtr g_currentPort;
#define thePort g_currentPort

typedef struct {
    const UInt8* ptr;
    const UInt8* end;
} PictStream;

static Boolean pict_read_u8(PictStream* s, UInt8* value) {
    if (s->ptr >= s->end) {
        return false;
    }
    *value = *s->ptr++;
    return true;
}

static Boolean pict_read_s16(PictStream* s, SInt16* value) {
    if ((s->end - s->ptr) < 2) {
        return false;
    }
    *value = (SInt16)((s->ptr[0] << 8) | s->ptr[1]);
    s->ptr += 2;
    return true;
}

static Boolean pict_read_u16(PictStream* s, UInt16* value) {
    return pict_read_s16(s, (SInt16*)value);
}

static Boolean pict_read_u32(PictStream* s, UInt32* value) {
    if ((s->end - s->ptr) < 4) {
        return false;
    }
    *value = ((UInt32)s->ptr[0] << 24) |
             ((UInt32)s->ptr[1] << 16) |
             ((UInt32)s->ptr[2] << 8) |
             (UInt32)s->ptr[3];
    s->ptr += 4;
    return true;
}

static Boolean pict_read_rect(PictStream* s, Rect* r) {
    return pict_read_s16(s, &r->top) &&
           pict_read_s16(s, &r->left) &&
           pict_read_s16(s, &r->bottom) &&
           pict_read_s16(s, &r->right);
}

static Rect pict_scale_rect(const Rect* src, const Rect* picFrame, const Rect* dstRect,
                            SInt32 scaleX, SInt32 scaleY) {
    Rect result;
    result.left   = (SInt16)((((SInt32)(src->left   - picFrame->left)  * scaleX) >> 16) + dstRect->left);
    result.top    = (SInt16)((((SInt32)(src->top    - picFrame->top)   * scaleY) >> 16) + dstRect->top);
    result.right  = (SInt16)((((SInt32)(src->right  - picFrame->left)  * scaleX) >> 16) + dstRect->left);
    result.bottom = (SInt16)((((SInt32)(src->bottom - picFrame->top)   * scaleY) >> 16) + dstRect->top);
    return result;
}

static void pict_scale_point(Point* pt, const Rect* picFrame, const Rect* dstRect,
                             SInt32 scaleX, SInt32 scaleY) {
    pt->h = (SInt16)((((SInt32)(pt->h - picFrame->left) * scaleX) >> 16) + dstRect->left);
    pt->v = (SInt16)((((SInt32)(pt->v - picFrame->top)  * scaleY) >> 16) + dstRect->top);
}

static Boolean pict_skip_bytes(PictStream* s, SInt32 count) {
    if (count < 0 || (s->end - s->ptr) < count) {
        return false;
    }
    s->ptr += count;
    return true;
}

static Boolean pict_unpack_packbits_row(PictStream* s, UInt8* dst, SInt32 expected) {
    UInt8* out = dst;
    UInt8* outEnd = dst + expected;
    while (out < outEnd) {
        UInt8 control;
        if (!pict_read_u8(s, &control)) {
            return false;
        }
        SInt8 signedControl = (SInt8)control;
        if (signedControl >= 0) {
            SInt32 literalCount = (SInt32)signedControl + 1;
            if ((outEnd - out) < literalCount || (s->end - s->ptr) < literalCount) {
                return false;
            }
            memcpy(out, s->ptr, (size_t)literalCount);
            s->ptr += literalCount;
            out += literalCount;
        } else if (signedControl >= -127) {
            SInt32 repeatCount = 1 - (SInt32)signedControl;
            UInt8 value;
            if (!pict_read_u8(s, &value)) {
                return false;
            }
            if ((outEnd - out) < repeatCount) {
                return false;
            }
            memset(out, value, (size_t)repeatCount);
            out += repeatCount;
        } else {
            /* -128: no-op */
        }
    }
    return true;
}

static Boolean pict_unpack_packbits(PictStream* s, UInt8* buffer, SInt16 rowBytes,
                                    SInt16 height) {
    UInt8* rowPtr = buffer;
    for (SInt16 row = 0; row < height; ++row) {
        if (rowBytes < 8) {
            UInt8 count;
            if (!pict_read_u8(s, &count)) {
                return false;
            }
        } else {
            UInt16 count16;
            if (!pict_read_u16(s, &count16)) {
                return false;
            }
        }
        if (!pict_unpack_packbits_row(s, rowPtr, rowBytes)) {
            return false;
        }
        rowPtr += rowBytes;
    }
    return true;
}

static void pict_apply_clip(const Rect* r) {
    if (!thePort->clipRgn) {
        thePort->clipRgn = NewRgn();
    }
    if (thePort->clipRgn) {
        RectRgn(thePort->clipRgn, r);
    }
}

static Boolean pict_handle_bits_rect(PictStream* s, const Rect* picFrame,
                                     const Rect* dstRect, SInt32 scaleX, SInt32 scaleY,
                                     Boolean packBits) {
    UInt16 rawRowBytes;
    if (!pict_read_u16(s, &rawRowBytes)) {
        return false;
    }

    Boolean hasPixMap = (rawRowBytes & 0x8000u) != 0;
    SInt16 rowBytes = (SInt16)(rawRowBytes & 0x7FFFu);

    Rect srcRect;
    Rect dstLocal;
    if (!pict_read_rect(s, &srcRect) || !pict_read_rect(s, &dstLocal)) {
        return false;
    }

    dstLocal = pict_scale_rect(&dstLocal, picFrame, dstRect, scaleX, scaleY);

    SInt16 mode;
    if (!pict_read_s16(s, &mode)) {
        return false;
    }

    if (hasPixMap) {
        /* Skip PixMap header we do not currently use */
        UInt16 pmVersion, packType, pixelType, pixelSize, cmpCount, cmpSize;
        UInt32 packSize, hRes, vRes, planeBytes, pmTable, pmReserved;
        if (!pict_read_u16(s, &pmVersion) ||
            !pict_read_u16(s, &packType) ||
            !pict_read_u32(s, &packSize) ||
            !pict_read_u32(s, &hRes) ||
            !pict_read_u32(s, &vRes) ||
            !pict_read_u16(s, &pixelType) ||
            !pict_read_u16(s, &pixelSize) ||
            !pict_read_u16(s, &cmpCount) ||
            !pict_read_u16(s, &cmpSize) ||
            !pict_read_u32(s, &planeBytes) ||
            !pict_read_u32(s, &pmTable) ||
            !pict_read_u32(s, &pmReserved)) {
            return false;
        }

        /* This simple implementation only supports 1-bit monochrome data */
        if (pixelSize != 1 || cmpCount != 1 || cmpSize != 1) {
            return false;
        }
    }

    SInt16 height = (SInt16)(srcRect.bottom - srcRect.top);
    if (rowBytes <= 0 || height <= 0) {
        return false;
    }

    /* Check for integer overflow before multiplication */
    if (rowBytes > (SInt16)(0x7FFF / height)) {
        return false;
    }

    Size bufferSize = (Size)rowBytes * height;
    Ptr pixelData = NewPtr(bufferSize);
    if (!pixelData) {
        return false;
    }

    Boolean ok = true;
    if (packBits) {
        ok = pict_unpack_packbits(s, (UInt8*)pixelData, rowBytes, height);
    } else {
        /* Cast to SInt32 before multiplication to avoid overflow (already validated above) */
        SInt32 expected = (SInt32)rowBytes * (SInt32)height;
        if ((s->end - s->ptr) < expected) {
            ok = false;
        } else {
            memcpy(pixelData, s->ptr, (size_t)expected);
            s->ptr += expected;
        }
    }

    if (!ok) {
        DisposePtr(pixelData);
        return false;
    }

    BitMap srcBits;
    srcBits.baseAddr = pixelData;
    srcBits.rowBytes = rowBytes;
    srcBits.bounds = srcRect;

    BitMap dstBits = thePort->portBits;

    CopyBits(&srcBits, &dstBits, &srcRect, &dstLocal, mode, NULL);

    DisposePtr(pixelData);
    return true;
}

/*
 * DrawPicture - draw picture resource
 */
void DrawPicture(PicHandle myPicture, const Rect* dstRect) {
    if (!thePort || !myPicture || !dstRect || !*myPicture) {
        return;
    }

    /* Validate picture handle size before reading header */
    Size handleSize = GetHandleSize((Handle)myPicture);
    if (handleSize < 10) {
        return;  /* Picture too small to contain valid header */
    }

    const UInt8* raw = (const UInt8*)(*myPicture);
    SInt16 picSize = (SInt16)((raw[0] << 8) | raw[1]);

    /* Validate picSize against actual handle size */
    if (picSize < 10 || picSize > handleSize) {
        return;  /* Invalid picture size */
    }
    Rect picFrame;
    picFrame.top = (raw[2] << 8) | raw[3];
    picFrame.left = (raw[4] << 8) | raw[5];
    picFrame.bottom = (raw[6] << 8) | raw[7];
    picFrame.right = (raw[8] << 8) | raw[9];

    PictStream stream;
    stream.ptr = raw + 10;
    stream.end = raw + picSize;

    SInt16 picWidth = (SInt16)(picFrame.right - picFrame.left);
    SInt16 picHeight = (SInt16)(picFrame.bottom - picFrame.top);
    if (picWidth <= 0 || picHeight <= 0) {
        return;
    }

    SInt16 dstWidth = (SInt16)(dstRect->right - dstRect->left);
    SInt16 dstHeight = (SInt16)(dstRect->bottom - dstRect->top);
    if (dstWidth <= 0 || dstHeight <= 0) {
        return;
    }

    SInt32 scaleX = ((SInt32)dstWidth << 16) / picWidth;
    SInt32 scaleY = ((SInt32)dstHeight << 16) / picHeight;

    Boolean done = false;
    while (!done && stream.ptr < stream.end) {
        UInt8 opcode;
        if (!pict_read_u8(&stream, &opcode)) {
            break;
        }

        switch (opcode) {
            case 0x00: /* NOP */
                break;

            case 0x01: { /* Clip */
                UInt16 regionSize;
                if (!pict_read_u16(&stream, &regionSize)) {
                    done = true;
                    break;
                }
                if (!pict_skip_bytes(&stream, regionSize - 2)) {
                    done = true;
                }
                pict_apply_clip(dstRect);
                break;
            }

            case 0x07: { /* Pen size */
                Point pnSize;
                if (!pict_read_s16(&stream, &pnSize.v) ||
                    !pict_read_s16(&stream, &pnSize.h)) {
                    done = true;
                    break;
                }
                PenSize(pnSize.h, pnSize.v);
                break;
            }

            case 0x08: { /* Pen mode */
                SInt16 mode;
                if (!pict_read_s16(&stream, &mode)) {
                    done = true;
                    break;
                }
                PenMode(mode);
                break;
            }

            case 0x09: { /* Pen pattern */
                if (!pict_skip_bytes(&stream, 8)) {
                    done = true;
                    break;
                }
                Pattern pat;
                memcpy(pat.pat, stream.ptr - 8, sizeof(pat.pat));
                PenPat(&pat);
                break;
            }

            case 0x0A: { /* Fill pattern */
                if (!pict_skip_bytes(&stream, 8)) {
                    done = true;
                    break;
                }
                memcpy(thePort->fillPat.pat, stream.ptr - 8, sizeof(thePort->fillPat.pat));
                break;
            }

            case 0x03: { /* Text font */
                SInt16 fontID;
                if (!pict_read_s16(&stream, &fontID)) {
                    done = true;
                    break;
                }
                TextFont(fontID);
                break;
            }

            case 0x04: { /* Text face */
                UInt8 face;
                if (!pict_read_u8(&stream, &face)) {
                    done = true;
                    break;
                }
                TextFace(face);
                break;
            }

            case 0x0C: { /* Origin */
                SInt16 dh, dv;
                if (!pict_read_s16(&stream, &dh) || !pict_read_s16(&stream, &dv)) {
                    done = true;
                    break;
                }
                SetOrigin(dh, dv);
                break;
            }

            case 0x0D: { /* Text size */
                SInt16 size;
                if (!pict_read_s16(&stream, &size)) {
                    done = true;
                    break;
                }
                TextSize(size);
                break;
            }

            case 0x0E: /* ForeColor */
            case 0x0F: /* BackColor */
                if (!pict_skip_bytes(&stream, 4)) {
                    done = true;
                }
                break;

            case 0x10: /* Text ratio */
                if (!pict_skip_bytes(&stream, 8)) {
                    done = true;
                }
                break;

            case 0x11: /* Version */
                if (!pict_skip_bytes(&stream, 2)) {
                    done = true;
                }
                break;

            case 0x20: { /* Line */
                Point pt;
                if (!pict_read_s16(&stream, &pt.v) || !pict_read_s16(&stream, &pt.h)) {
                    done = true;
                    break;
                }
                pict_scale_point(&pt, &picFrame, dstRect, scaleX, scaleY);
                LineTo(pt.h, pt.v);
                break;
            }

            case 0x21: { /* LineFrom */
                Point pt;
                if (!pict_read_s16(&stream, &pt.v) || !pict_read_s16(&stream, &pt.h)) {
                    done = true;
                    break;
                }
                pict_scale_point(&pt, &picFrame, dstRect, scaleX, scaleY);
                MoveTo(pt.h, pt.v);
                break;
            }

            case 0x28: { /* DrawString */
                UInt8 len;
                if (!pict_read_u8(&stream, &len)) {
                    done = true;
                    break;
                }
                if (stream.ptr + len > stream.end) {
                    done = true;
                    break;
                }
                DrawText((const char*)stream.ptr, 0, len);
                stream.ptr += len;
                /* Pad to even byte boundary */
                if (len % 2 == 0) {
                    stream.ptr++;
                }
                break;
            }

            case 0x2C: /* FrameOval */
            case 0x2D: /* PaintOval */
            case 0x2E: /* EraseOval */
            case 0x2F: { /* InvertOval */
                Rect r;
                if (!pict_read_rect(&stream, &r)) {
                    done = true;
                    break;
                }
                Rect scaled = pict_scale_rect(&r, &picFrame, dstRect, scaleX, scaleY);
                if (opcode == 0x2C) FrameOval(&scaled);
                else if (opcode == 0x2D) PaintOval(&scaled);
                else if (opcode == 0x2E) EraseOval(&scaled);
                else if (opcode == 0x2F) InvertOval(&scaled);
                break;
            }

            case 0x30: /* FrameRect */
            case 0x31: /* PaintRect */
            case 0x32: /* EraseRect */
            case 0x33: /* InvertRect */
            case 0x34: { /* FillRect */
                Rect r;
                if (!pict_read_rect(&stream, &r)) {
                    done = true;
                    break;
                }
                Rect scaled = pict_scale_rect(&r, &picFrame, dstRect, scaleX, scaleY);
                if (opcode == 0x30) FrameRect(&scaled);
                else if (opcode == 0x31) PaintRect(&scaled);
                else if (opcode == 0x32) EraseRect(&scaled);
                else if (opcode == 0x33) InvertRect(&scaled);
                else FillRect(&scaled, &thePort->fillPat);
                break;
            }

            case 0x40: /* FrameRoundRect */
            case 0x41: /* PaintRoundRect */
            case 0x42: /* EraseRoundRect */
            case 0x43: { /* InvertRoundRect */
                Rect r;
                if (!pict_read_rect(&stream, &r)) {
                    done = true;
                    break;
                }
                Rect scaled = pict_scale_rect(&r, &picFrame, dstRect, scaleX, scaleY);
                /* Use fixed corner radius of 16 pixels */
                if (opcode == 0x40) FrameRoundRect(&scaled, 16, 16);
                else if (opcode == 0x41) PaintRoundRect(&scaled, 16, 16);
                else if (opcode == 0x42) EraseRoundRect(&scaled, 16, 16);
                else if (opcode == 0x43) InvertRoundRect(&scaled, 16, 16);
                break;
            }

            case 0x50: /* FrameArc */
            case 0x51: /* PaintArc */
            case 0x52: /* EraseArc */
            case 0x53: { /* InvertArc */
                Rect r;
                SInt16 startAngle, arcAngle;
                if (!pict_read_rect(&stream, &r) ||
                    !pict_read_s16(&stream, &startAngle) ||
                    !pict_read_s16(&stream, &arcAngle)) {
                    done = true;
                    break;
                }
                Rect scaled = pict_scale_rect(&r, &picFrame, dstRect, scaleX, scaleY);
                if (opcode == 0x50) FrameArc(&scaled, startAngle, arcAngle);
                else if (opcode == 0x51) PaintArc(&scaled, startAngle, arcAngle);
                else if (opcode == 0x52) EraseArc(&scaled, startAngle, arcAngle);
                else if (opcode == 0x53) InvertArc(&scaled, startAngle, arcAngle);
                break;
            }

            case 0x70: /* FramePoly */
            case 0x71: { /* PaintPoly */
                UInt16 polySize;
                if (!pict_read_u16(&stream, &polySize)) {
                    done = true;
                    break;
                }
                /* Skip polygon data for now - just advance stream */
                if (!pict_skip_bytes(&stream, polySize - 2)) {
                    done = true;
                }
                break;
            }

            case 0x90: { /* BitsRect (uncompressed) */
                if (!pict_handle_bits_rect(&stream, &picFrame, dstRect, scaleX, scaleY, false)) {
                    done = true;
                }
                break;
            }

            case 0x98: { /* PackBitsRect */
                if (!pict_handle_bits_rect(&stream, &picFrame, dstRect, scaleX, scaleY, true)) {
                    done = true;
                }
                break;
            }

            case 0xA0: /* ShortComment */
                if (!pict_skip_bytes(&stream, 2)) {
                    done = true;
                }
                break;

            case 0xA1: { /* LongComment */
                SInt16 kind;
                UInt16 length;
                if (!pict_read_s16(&stream, &kind) ||
                    !pict_read_u16(&stream, &length) ||
                    !pict_skip_bytes(&stream, length)) {
                    done = true;
                }
                break;
            }

            case 0xFF: /* EndPic */
                done = true;
                break;

            default:
                /* Unknown opcode – bail out to keep parser safe */
                done = true;
                break;
        }
    }
}
