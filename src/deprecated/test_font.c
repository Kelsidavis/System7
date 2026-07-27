/*
 * Test font rendering to debug character display
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

void TestFontRendering(void) {
    if (!framebuffer) return;

    /* Clear test area */
    uint32_t *fb = (uint32_t*)framebuffer;
    uint32_t white = pack_color(255, 255, 255);
    uint32_t black = pack_color(0, 0, 0);

    /* Draw white background for test area */
    for (int y = 100; y < 200; y++) {
        for (int x = 50; x < 750; x++) {
            int fb_offset = y * (fb_pitch / 4) + x;
            fb[fb_offset] = white;
        }
    }

    /* Test drawing ASCII characters */
    int x_pos = 60;
    int y_pos = 120;

    /* Draw alphabet */
    for (char ch = 'A'; ch <= 'Z'; ch++) {
        DrawCharAt(x_pos, y_pos, ch);
        x_pos += 20;
        if (x_pos > 700) {
            x_pos = 60;
            y_pos += 20;
        }
    }

    /* Draw lowercase */
    x_pos = 60;
    y_pos = 140;
    for (char ch = 'a'; ch <= 'z'; ch++) {
        DrawCharAt(x_pos, y_pos, ch);
        x_pos += 20;
        if (x_pos > 700) {
            x_pos = 60;
            y_pos += 20;
        }
    }

    /* Draw numbers */
    x_pos = 60;
    y_pos = 160;
    for (char ch = '0'; ch <= '9'; ch++) {
        DrawCharAt(x_pos, y_pos, ch);
        x_pos += 20;
    }

    /* Draw test strings */
    x_pos = 60;
    y_pos = 180;
    const char* testStr = "File Edit View Special";
    for (int i = 0; testStr[i]; i++) {
        DrawCharAt(x_pos, y_pos, testStr[i]);
        x_pos += CharWidth(testStr[i]);
    }
}

/* Draw a single character at specific position for debugging */
void DrawCharAt(short x, short y, char ch) {
    if (!framebuffer) return;

    /* Bounds check */
    if (x < 0 || y < 0) return;
    if (y + CHICAGO_HEIGHT > fb_height) return;

    /* Get character info */
    int char_idx;
    if (ch < CHICAGO_FIRST_CHAR || ch > CHICAGO_LAST_CHAR) {
        char_idx = 0; /* Use missing char glyph */
    } else {
        char_idx = ch - CHICAGO_FIRST_CHAR + 1;
    }

    CharInfo info = chicago_chars[char_idx];

    /* Draw each row of the character */
    uint32_t *fb = (uint32_t*)framebuffer;
    for (int row = 0; row < CHICAGO_HEIGHT; row++) {
        if (y + row >= fb_height) break;

        /* Get row bitmap from font data */
        int row_offset = row * CHICAGO_ROW_WORDS * 2;

        /* Extract bits for this character */
        for (int col = 0; col < info.width; col++) {
            if (x + col >= fb_width) break;

            /* Calculate bit position in the row */
            int bit_pos = info.offset + col;
            int byte_idx = row_offset + (bit_pos / 8);
            int bit_idx = 7 - (bit_pos % 8);  /* MSB first */

            /* Check if pixel is set */
            if (byte_idx < sizeof(chicago_bitmap) &&
                (chicago_bitmap[byte_idx] & (1 << bit_idx))) {
                /* Set pixel in framebuffer */
                int fb_offset = (y + row) * (fb_pitch / 4) + (x + col);
                fb[fb_offset] = pack_color(0, 0, 0);
            }
        }
    }
}

short CharWidth(short ch) {
    int char_idx;
    if (ch < CHICAGO_FIRST_CHAR || ch > CHICAGO_LAST_CHAR) {
        char_idx = 0;
    } else {
        char_idx = ch - CHICAGO_FIRST_CHAR + 1;
    }
    return chicago_chars[char_idx].width;
}