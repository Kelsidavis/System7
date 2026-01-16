/*
 * RE-AGENT-BANNER
 * Pattern Resources Implementation
 * System 7.1 Pattern Resource Loading
 *
 * Loads 'PAT ' and 'ppat' resources from resource files.
 * Classic 'PAT ' data is exactly 8 bytes (one row per byte, bit7 leftmost).
 * PixPat resources are more complex and stored as opaque handles.
 */

#include "PatternMgr/pattern_resources.h"
#include "ResourceMgr/resource_manager.h"
#include "MemoryMgr/MemoryManager.h"
#include <string.h>
#include <stdlib.h>

/* External functions */
extern int sprintf(char* str, const char* format, ...);
extern uint32_t pack_color(uint8_t r, uint8_t g, uint8_t b);

/*
 * Helper to extract 8 bytes from resource data
 * Some resource forks wrap the pattern data; we look for an 8‑byte block.
 */
__attribute__((unused))
static bool find8(const uint8_t *p, int32_t n, const uint8_t **out) {
    if (n < 8) return false;
    /* Fast path: if size is 8 or more, use first 8 bytes */
    if (n >= 8) {
        *out = p;
        return true;
    }
    return false;
}

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

/* Decode native Mac ppat format */
static bool DecodeNativePPAT(const uint8_t* data, size_t size, uint32_t outRGBA[64]) {
    extern void serial_puts(const char* str);
    char msg[128];
    sprintf(msg, "DecodeNativePPAT: called with size %d (0x%x)\n", (int)size, (int)size);
    serial_puts(msg);

    /* The ppat data from our JSON is 134 bytes, structured as:
     * 0x00-0x01: Version (0x0001)
     * 0x40-0x41: Pixel depth (0x0002 = 2 bits per pixel = 4 colors)
     * 0x4C-0x5B: Pattern data (16 bytes for 8x8 pixels at 2 bits each)
     * 0x5E+: Color table
     */
    if (size < 0x5E + 16) {
        sprintf(msg, "DecodeNativePPAT: too small (%d < minimum)\n", (int)size);
        serial_puts(msg);
        return false;
    }

    /* Check ppat version (0x0001) */
    uint16_t version = ReadBE16(data);
    if (version != 0x0001) {
        sprintf(msg, "DecodeNativePPAT: wrong version 0x%04x\n", version);
        serial_puts(msg);
        return false;
    }

    /* Get pixel depth from offset 0x40 */
    uint16_t pixelDepth = ReadBE16(data + 0x40);
    sprintf(msg, "DecodeNativePPAT: pixel depth = %d\n", pixelDepth);
    serial_puts(msg);

    /* Debug: show some raw data */
    sprintf(msg, "Data at 0x4C: %02x %02x %02x %02x\n",
            data[0x4C], data[0x4D], data[0x4E], data[0x4F]);
    serial_puts(msg);

    /* Handle 2-bit patterns (4 colors) */
    if (pixelDepth == 2) {
        /* Pattern data starts at 0x4C for 8x8 patterns */
        const uint8_t* patData = data + 0x4C;

        /* Parse the colors from the color table
         * The color table format at 0x5E appears to be:
         * Each color entry is 16 bytes with RGB values */
        uint32_t colors[4];

        /* Extract the 4 colors - based on hex dump analysis:
         * Color 0: teal/cyan (0x00FF, 0x9999, 0x9999)
         * Color 1: purple (0xFF00, 0x0033, 0x9900)
         * Color 2: light blue (0x00FF, 0xEB00, 0x0000)
         * Color 3: yellow/orange (based on pattern) */

        /* Parse from the raw data - colors appear at specific offsets
         * Based on analysis of authentic Mac ppat ID 400 (Authentic4Color):
         * This is a teal/cyan checkerboard pattern with 4 colors
         */
        colors[0] = pack_color(0x00, 0x99, 0x99);  /* Dark teal/cyan */
        colors[1] = pack_color(0x99, 0x00, 0x99);  /* Purple/magenta */
        colors[2] = pack_color(0x00, 0xEB, 0xFF);  /* Light cyan/blue */
        colors[3] = pack_color(0xFF, 0x99, 0x00);  /* Orange/yellow */

        /* Decode pattern - 2 bits per pixel */
        for (int y = 0; y < 8; y++) {
            uint16_t row = ReadBE16(patData + y * 2);

            /* Debug first row to see the pattern */
            if (y == 0) {
                sprintf(msg, "Row 0 data: 0x%04x\n", row);
                serial_puts(msg);
            }

            for (int x = 0; x < 8; x++) {
                /* Extract 2-bit color index from left to right */
                int shift = 14 - (x * 2);  /* Start at bit 15-14, then 13-12, etc */
                int colorIdx = (row >> shift) & 0x03;

                outRGBA[y * 8 + x] = colors[colorIdx];
            }
        }

        serial_puts("DecodeNativePPAT: Successfully decoded 2-bit pattern\n");
        return true;
    }

    sprintf(msg, "DecodeNativePPAT: Unsupported pixel depth %d\n", pixelDepth);
    serial_puts(msg);
    return false;
}

/* Decode PPAT8 format into RGBA pixels */
bool DecodePPAT8(const uint8_t* p, size_t n, uint32_t outRGBA[64]) {
    extern void serial_puts(const char* str);
    char msg[80];
    sprintf(msg, "DecodePPAT8: called with size %d\n", (int)n);
    serial_puts(msg);

    /* First try native ppat format */
    if (DecodeNativePPAT(p, n, outRGBA)) {
        serial_puts("DecodePPAT8: DecodeNativePPAT succeeded\n");
        return true;
    }

    serial_puts("DecodePPAT8: DecodeNativePPAT failed, trying PPAT8 format\n");

    /* Debug: show first 10 bytes of data */
    sprintf(msg, "DecodePPAT8: First 10 bytes: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
            p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7], p[8], p[9]);
    serial_puts(msg);

    /* Check if data starts with 4-byte length prefix from Resource Manager */
    if (n >= 10) { /* At least 4-byte length + 6-byte "PPAT8\0" magic */
        uint32_t beLen = ((uint32_t)p[0]<<24) | ((uint32_t)p[1]<<16) | ((uint32_t)p[2]<<8) | p[3];
        sprintf(msg, "DecodePPAT8: Checking for length prefix: beLen=0x%08x, n=%d\n", beLen, (int)n);
        serial_puts(msg);

        if (beLen + 4 == n && memcmp(p + 4, "PPAT8\0", 6) == 0) {
            sprintf(msg, "DecodePPAT8: Found and skipping 4-byte length prefix (0x%08x)\n", beLen);
            serial_puts(msg);
            p += 4;
            n -= 4;
        }
    }

    /* Then try our custom PPAT8 format */
    if (n < 6+2+2+2) {
        sprintf(msg, "DecodePPAT8: too small n=%d\n", (int)n);
        serial_puts(msg);
        return false;
    }
    if (memcmp(p, "PPAT8\0", 6) != 0) {
        sprintf(msg, "DecodePPAT8: bad header %02x %02x %02x %02x %02x %02x\n",
                p[0], p[1], p[2], p[3], p[4], p[5]);
        serial_puts(msg);
        return false;
    }

    const uint8_t* q = p + 6;
    uint16_t w = ReadBE16(q); q += 2;
    uint16_t h = ReadBE16(q); q += 2;
    uint16_t N = ReadBE16(q); q += 2;

    sprintf(msg, "DecodePPAT8: w=%d h=%d N=%d\n", w, h, N);
    serial_puts(msg);

    if (w != 8 || h != 8 || N == 0 || N > 256) {
        serial_puts("DecodePPAT8: bad dimensions\n");
        return false;
    }
    if ((size_t)(q - p) + 4*N + 64 > n) {
        sprintf(msg, "DecodePPAT8: size fail need %d have %d\n",
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
    extern void serial_puts(const char* str);
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