/* Load classic Pattern and PixPat resources. */

#include "PatternMgr/pattern_resources.h"
#include "Platform/Framebuffer.h"
#include "System71StdLib.h"
#include "ResourceManager.h"
#include "MemoryMgr/MemoryManager.h"
#include <string.h>
#include <stdlib.h>

/* External functions */
extern int snprintf(char* str, size_t size, const char* format, ...);

/* External function from pattern_data.c */
extern const uint8_t* GetBuiltInPatternData(int16_t patternID);

bool LoadPATResource(int16_t id, Pattern *outPat) {
    if (!outPat) return false;

    /* First try to get from resource manager */
    Handle h = GetResource(kPatternResourceType, id);

    if (!h) {
        /* Fall back to built-in patterns */
        const uint8_t* builtInData = GetBuiltInPatternData(id);
        if (builtInData) {
            memset(outPat, 0, sizeof(*outPat));
            memcpy(outPat->pat, builtInData, 8);
            return true;
        }
        /* No pattern found */
        return false;
    }

    /* Handle was found - extract pattern data carefully */
    HLock(h);

    /* Verify handle is still valid */
    if (!*h) {
        HUnlock(h);
        /* DON'T call ReleaseResource - Resource Manager owns this handle */
        return false;
    }

    const uint8_t *p = (const uint8_t *)*h;
    Size sz = GetHandleSize(h);

    /* Ensure we have at least 8 bytes */
    if (sz < 8) {
        HUnlock(h);
        /* DON'T call ReleaseResource - Resource Manager owns this handle */
        return false;
    }

    /* Copy exactly 8 bytes - use first 8 bytes if larger */
    memset(outPat, 0, sizeof(*outPat));
    memcpy(outPat->pat, p, 8);

    HUnlock(h);

    /* DON'T call ReleaseResource - The Resource Manager manages resource lifetimes
     * and will automatically dispose of the handle. Calling ReleaseResource here
     * can corrupt the resource cache state when the same resource is accessed
     * multiple times (as happens when redrawing the pattern grid). */

    return true;
}

/* Parse big-endian 16-bit value */
static uint16_t ReadBE16(const uint8_t* p) {
    return (p[0] << 8) | p[1];
}

static uint32_t ReadBE32(const uint8_t* p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

/*
 * DecodeNativePPAT - a 'ppat' resource as the Mac stores it (Inside Macintosh:
 * Imaging With QuickDraw, 4-104): a PixPat record, the PixMap it points to,
 * the pixel data, and the colour table the PixMap points to, every offset
 * from the start of the resource.
 *
 *   PixPat   patType(2) patMap(4) patData(4) patXData(4) patXValid(2)
 *            patXMap(4) pat1Data(8)
 *   PixMap   baseAddr(4) rowBytes(2) bounds(8) pmVersion(2) packType(2)
 *            packSize(4) hRes(4) vRes(4) pixelType(2) pixelSize(2)
 *            cmpCount(2) cmpSize(2) planeBytes(4) pmTable(4) pmReserved(4)
 *   CTab     ctSeed(4) ctFlags(2) ctSize(2), then ctSize+1 of
 *            value(2) red(2) green(2) blue(2)
 *
 * Only full-colour (patType 1) 8 by 8 patterns of 1, 2, 4 or 8 bits a pixel,
 * which is what a desktop pattern is. This used to read the pixels two bytes
 * early and paint them in four colours written into the code, whatever the
 * pattern's own colour table said.
 */
static bool DecodeNativePPAT(const uint8_t* d, size_t n, uint32_t out[64]) {
    if (n < 28 || ReadBE16(d) != 1) return false;
    uint32_t mapOff = ReadBE32(d + 2);
    uint32_t dataOff = ReadBE32(d + 6);
    if (mapOff + 50 > n) return false;

    const uint8_t* pm = d + mapOff;
    uint16_t rowBytes = ReadBE16(pm + 4) & 0x3FFF;
    int16_t top = (int16_t)ReadBE16(pm + 6), left = (int16_t)ReadBE16(pm + 8);
    int16_t bottom = (int16_t)ReadBE16(pm + 10), right = (int16_t)ReadBE16(pm + 12);
    uint16_t pixelSize = ReadBE16(pm + 32);
    uint32_t ctOff = ReadBE32(pm + 42);

    if (bottom - top != 8 || right - left != 8) return false;
    if (pixelSize != 1 && pixelSize != 2 && pixelSize != 4 && pixelSize != 8) return false;
    /* a row of 8 pixels is pixelSize bytes */
    if (rowBytes < pixelSize || dataOff + rowBytes * 8u > n) return false;
    if (ctOff + 8 > n) return false;

    uint16_t ctFlags = ReadBE16(d + ctOff + 4);
    uint16_t ctCount = (uint16_t)(ReadBE16(d + ctOff + 6) + 1);
    if (ctOff + 8 + ctCount * 8u > n) return false;
    const uint8_t* ct = d + ctOff + 8;

    for (int y = 0; y < 8; y++) {
        const uint8_t* row = d + dataOff + y * rowBytes;
        for (int x = 0; x < 8; x++) {
            int bit = x * pixelSize;
            unsigned index = (row[bit >> 3] >> (8 - pixelSize - (bit & 7))) & ((1u << pixelSize) - 1);
            /* A device table (high bit of ctFlags) is indexed by position;
             * otherwise each entry names its pixel value. */
            const uint8_t* entry = NULL;
            if (ctFlags & 0x8000) {
                if (index < ctCount) entry = ct + index * 8;
            } else {
                for (unsigned k = 0; k < ctCount && !entry; k++) {
                    if (ReadBE16(ct + k * 8) == index) entry = ct + k * 8;
                }
            }
            out[y * 8 + x] = entry ? pack_color(entry[2], entry[4], entry[6])
                                   : pack_color(0, 0, 0);
        }
    }
    return true;
}

/* Decode PPAT8 format into RGBA pixels */
bool DecodePPAT8(const uint8_t* p, size_t n, uint32_t outRGBA[64]) {
    char msg[128];
    snprintf(msg, sizeof(msg), "DecodePPAT8: called with size %d\n", (int)n);
    serial_puts(msg);

    /* First try native ppat format */
    if (DecodeNativePPAT(p, n, outRGBA)) {
        serial_puts("DecodePPAT8: DecodeNativePPAT succeeded\n");
        return true;
    }

    serial_puts("DecodePPAT8: DecodeNativePPAT failed, trying PPAT8 format\n");

    /* Debug: show first 10 bytes of data */
    snprintf(msg, sizeof(msg), "DecodePPAT8: First 10 bytes: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
            p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7], p[8], p[9]);
    serial_puts(msg);

    /* Check if data starts with 4-byte length prefix from Resource Manager */
    if (n >= 10) { /* At least 4-byte length + 6-byte "PPAT8\0" magic */
        uint32_t beLen = ((uint32_t)p[0]<<24) | ((uint32_t)p[1]<<16) | ((uint32_t)p[2]<<8) | p[3];
        snprintf(msg, sizeof(msg), "DecodePPAT8: Checking for length prefix: beLen=0x%08lx, n=%d\n", (unsigned long)beLen, (int)n);
        serial_puts(msg);

        if (beLen + 4 == n && memcmp(p + 4, "PPAT8\0", 6) == 0) {
            snprintf(msg, sizeof(msg), "DecodePPAT8: Found and skipping 4-byte length prefix (0x%08lx)\n", (unsigned long)beLen);
            serial_puts(msg);
            p += 4;
            n -= 4;
        }
    }

    /* Then try our custom PPAT8 format */
    if (n < 6+2+2+2) {
        snprintf(msg, sizeof(msg), "DecodePPAT8: too small n=%d\n", (int)n);
        serial_puts(msg);
        return false;
    }
    if (memcmp(p, "PPAT8\0", 6) != 0) {
        snprintf(msg, sizeof(msg), "DecodePPAT8: bad header %02x %02x %02x %02x %02x %02x\n",
                p[0], p[1], p[2], p[3], p[4], p[5]);
        serial_puts(msg);
        return false;
    }

    const uint8_t* q = p + 6;
    uint16_t w = ReadBE16(q); q += 2;
    uint16_t h = ReadBE16(q); q += 2;
    uint16_t N = ReadBE16(q); q += 2;

    snprintf(msg, sizeof(msg), "DecodePPAT8: w=%d h=%d N=%d\n", w, h, N);
    serial_puts(msg);

    if (w != 8 || h != 8 || N == 0 || N > 256) {
        serial_puts("DecodePPAT8: bad dimensions\n");
        return false;
    }
    if ((size_t)(q - p) + 4*N + 64 > n) {
        snprintf(msg, sizeof(msg), "DecodePPAT8: size fail need %d have %d\n",
                (int)((q - p) + 4*N + 64), (int)n);
        serial_puts(msg);
        return false;
    }

    /* Read palette */
    const uint8_t* pal = q;
    q += 4*N;
    const uint8_t* idx = q;

    /* Convert to RGBA pixels */
    for (int i = 0; i < 64; i++) {
        uint8_t k = idx[i];
        if (k >= N) return false;
        const uint8_t* c = pal + 4*k;
        /* Store as ARGB for compatibility with pack_color */
        outRGBA[i] = pack_color(c[0], c[1], c[2]);
    }

    return true;
}

Handle LoadPPATResource(int16_t id) {
    extern void uart_flush(void);

    /* CRITICAL FIX: Use serial_puts instead of sprintf to avoid ARM64 hang */
    serial_puts("LoadPPATResource: Loading ppat\n");
    uart_flush();

    Handle h = GetResource(kPixPatternResourceType, id);
    if (!h) {
        serial_puts("LoadPPATResource: Failed to get ppat\n");
        uart_flush();
        return NULL;
    }

    serial_puts("LoadPPATResource: Got ppat handle\n");
    uart_flush();

    /* Return a duplicate the caller owns; leave original in the resource map */
    Size sz = GetHandleSize(h);
    serial_puts("LoadPPATResource: Got size\n");
    uart_flush();

    Handle dup = NewHandle(sz);
    if (!dup) {
        serial_puts("LoadPPATResource: Failed to allocate handle\n");
        ReleaseResource(h);
        return NULL;
    }

    HLock(h);
    HLock(dup);
    memcpy(*dup, *h, sz);
    HUnlock(dup);
    HUnlock(h);
    ReleaseResource(h);

    serial_puts("LoadPPATResource: Success\n");
    return dup;
}
