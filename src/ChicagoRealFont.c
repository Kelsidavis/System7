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

/* Helper: map local (x,y) to framebuffer pixel coords using current port */
static inline void QD_LocalToPixel(int lx, int ly, int* px, int* py) {
    if (!g_currentPort) {
        *px = lx;
        *py = ly;
        return;
    }
    /* The port's portBits.bounds encodes the origin mapping */
    *px = lx + g_currentPort->portBits.bounds.left;
    *py = ly + g_currentPort->portBits.bounds.top;
}

/* QuickDraw text functions using real Chicago font */
void DrawChar(short ch) {
    extern void serial_printf(const char* fmt, ...);
    if (!framebuffer || ch < 32 || ch > 126) return;

    Point pen = g_currentPort ? g_currentPort->pnLoc : (Point){0,0};

    int px, py;
    QD_LocalToPixel(pen.h, pen.v - CHICAGO_ASCENT, &px, &py);

    serial_printf("DrawChar '%c': pen=(%d,%d) -> pixel=(%d,%d)\n",
                 ch, pen.h, pen.v, px, py);

    DrawRealChicagoChar(px, py, (char)ch, pack_color(0, 0, 0));

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
    if (!framebuffer || !g_currentPort || !textBuf || byteCount <= 0) return;

    const char* text = (const char*)textBuf;
    Point pen = g_currentPort->pnLoc;

    for (short i = 0; i < byteCount; i++) {
        unsigned char ch = text[firstByte + i];
        if (ch < 32 || ch > 126) continue;

        int px, py;
        QD_LocalToPixel(pen.h, pen.v - CHICAGO_ASCENT, &px, &py);
        DrawRealChicagoChar(px, py, (char)ch, pack_color(0, 0, 0));
        pen.h += chicago_ascii[ch - 32].advance;
    }
    g_currentPort->pnLoc = pen;  /* Still local */
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

