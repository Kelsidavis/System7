/*
 * ChicagoRealFont.c - Real Chicago font from System 7.1 resources
 * Uses the actual NFNT resource data, with corrected spacing.
 */

#include "SystemTypes.h"
#include "QuickDraw/QuickDraw.h"
#include "chicago_font.h"

/* External framebuffer from multiboot */
extern void* framebuffer;
extern uint32_t fb_width;
extern uint32_t fb_height;
extern uint32_t fb_pitch;
extern uint32_t pack_color(uint8_t r, uint8_t g, uint8_t b);

/* Current port from QuickDrawCore.c */
extern GrafPtr g_currentPort;

/* Helper to get a bit from MSB-first bitmap */
static inline uint8_t get_bit(const uint8_t *row, int bitOff) {
    return (row[bitOff >> 3] >> (7 - (bitOff & 7))) & 1;
}

/* Draw a character from the real Chicago font */
static void DrawRealChicagoChar(short x, short y, char ch, uint32_t color) {
    if (!framebuffer) return;
    if (ch < 32 || ch > 126) return;

    ChicagoCharInfo info = chicago_ascii[ch - 32];
    x += info.left_offset;

    uint32_t *fb = (uint32_t*)framebuffer;

    for (int row = 0; row < CHICAGO_HEIGHT; row++) {
        if (y + row >= fb_height) break;
        const uint8_t *strike_row = chicago_bitmap + (row * CHICAGO_ROW_BYTES);

        for (int col = 0; col < info.bit_width; col++) {
            if (x + col >= fb_width) break;
            int bit_position = info.bit_start + col;
            if (get_bit(strike_row, bit_position)) {
                int fb_offset = (y + row) * (fb_pitch / 4) + (x + col);
                fb[fb_offset] = color;
            }
        }
    }
}

/* QuickDraw text functions using real Chicago font */
void DrawChar(short ch) {
    extern void serial_printf(const char* fmt, ...);
    if (!framebuffer || ch < 32 || ch > 126) {
        serial_printf("DrawChar: skipping ch=%d (no fb=%d, out of range=%d)\n",
                     ch, !framebuffer, (ch < 32 || ch > 126));
        return;
    }

    Point penPos = {0, 0};
    if (g_currentPort) penPos = g_currentPort->pnLoc;

    int draw_y = penPos.v - CHICAGO_ASCENT;
    serial_printf("DrawChar '%c': pen=(%d,%d) -> draw at (%d,%d)\n",
                 ch, penPos.h, penPos.v, penPos.h, draw_y);

    DrawRealChicagoChar(penPos.h, draw_y, (char)ch, pack_color(0, 0, 0));

    /* Advance by NFNT metrics */
    if (g_currentPort) {
        g_currentPort->pnLoc.h += chicago_ascii[ch - 32].advance;
    }
}

void DrawString(ConstStr255Param s) {
    if (!s || s[0] == 0) return;
    for (int i = 1; i <= s[0]; i++) {
        DrawChar(s[i]);
    }
}

void DrawText(const void* textBuf, short firstByte, short byteCount) {
    extern void serial_printf(const char* fmt, ...);
    if (!textBuf || byteCount <= 0) {
        serial_printf("DrawText: invalid params textBuf=%p, byteCount=%d\n", textBuf, byteCount);
        return;
    }
    const char* text = (const char*)textBuf;

    /* Don't print if not in debug mode */
    #if 0
    serial_printf("DrawText called: firstByte=%d, byteCount=%d, text='", firstByte, byteCount);
    for (short i = 0; i < byteCount; i++) {
        char ch = text[firstByte + i];
        if (ch >= 32 && ch < 127) {
            serial_printf("%c", ch);
        } else {
            serial_printf("\\x%02x", ch);
        }
    }
    serial_printf("'\n");
    #endif

    if (!g_currentPort) {
        serial_printf("DrawText: ERROR: g_currentPort is NULL!\n");
        return;
    }

    /* Always draw directly if we have a framebuffer */
    if (framebuffer) {
        Point penPos = g_currentPort->pnLoc;
        for (short i = 0; i < byteCount; i++) {
            char ch = text[firstByte + i];
            if (ch == 0 || ch < 32 || ch > 126) continue;

            /* Draw character directly */
            int draw_y = penPos.v - CHICAGO_ASCENT;
            DrawRealChicagoChar(penPos.h, draw_y, ch, pack_color(0, 0, 0));

            /* Advance pen position */
            penPos.h += chicago_ascii[ch - 32].advance;
        }
        /* Update port's pen position */
        g_currentPort->pnLoc = penPos;
    }
}

/* Font measurement functions */
short CharWidth(short ch) {
    if (ch >= 32 && ch <= 126) {
        return chicago_ascii[ch - 32].advance;
    }
    return 0;
}

short StringWidth(ConstStr255Param s) {
    if (!s || s[0] == 0) return 0;
    short width = 0;
    for (int i = 1; i <= s[0]; i++) {
        width += CharWidth(s[i]);
    }
    return width;
}

short TextWidth(const void* textBuf, short firstByte, short byteCount) {
    if (!textBuf || byteCount <= 0) return 0;
    const char* text = (const char*)textBuf;
    short width = 0;
    for (short i = 0; i < byteCount; i++) {
        width += CharWidth(text[firstByte + i]);
    }
    return width;
}

/* Utility function for debugging */
void DrawCharAt(short x, short y, char ch) {
    DrawRealChicagoChar(x, y, ch, pack_color(0, 0, 0));
}

