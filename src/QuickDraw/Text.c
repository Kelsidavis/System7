/*
 * Text.c - QuickDraw Text Drawing Implementation
 *
 * Implementation of text drawing, font management, and character
 * width calculations for QuickDraw.
 *
 * Copyright (c) 2025 - System 7.1 Portable Project
 * Based on Apple Macintosh System Software 7.1 QuickDraw
 */

#include "../include/QuickDraw/QuickDraw.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* Platform abstraction layer */
#include "QuickDrawPlatform.h"

/* Font information cache */
typedef struct FontCache {
    int16_t fontID;
    int16_t fontSize;
    Style fontStyle;
    FontInfo fontInfo;
    bool valid;
} FontCache;

static FontCache g_fontCache = {0, 0, 0, {0, 0, 0, 0}, false};

/* Forward declarations */
static void UpdateFontCache(int16_t fontID, int16_t fontSize, Style fontStyle);
static void DrawTextString(const char *text, int16_t length, Point startPt);
static int16_t MeasureTextWidth(const char *text, int16_t length);
static void ApplyTextStyle(Style style);

/* ================================================================
 * TEXT ATTRIBUTE MANAGEMENT
 * ================================================================ */

void TextFont(int16_t font) {
    extern GrafPtr g_currentPort; /* From QuickDrawCore.c */
    if (g_currentPort) {
        g_currentPort->txFont = font;
        g_fontCache.valid = false; /* Invalidate cache */
    }
}

void TextFace(Style face) {
    extern GrafPtr g_currentPort; /* From QuickDrawCore.c */
    if (g_currentPort) {
        g_currentPort->txFace = face;
        g_fontCache.valid = false; /* Invalidate cache */
    }
}

void TextMode(int16_t mode) {
    extern GrafPtr g_currentPort; /* From QuickDrawCore.c */
    if (g_currentPort) {
        g_currentPort->txMode = mode;
    }
}

void TextSize(int16_t size) {
    extern GrafPtr g_currentPort; /* From QuickDrawCore.c */
    if (g_currentPort) {
        g_currentPort->txSize = size;
        g_fontCache.valid = false; /* Invalidate cache */
    }
}

/* ================================================================
 * TEXT DRAWING
 * ================================================================ */

void DrawChar(int16_t ch) {
    extern GrafPtr g_currentPort; /* From QuickDrawCore.c */
    if (!g_currentPort) return;

    char str[2] = {(char)ch, '\0'};
    DrawTextString(str, 1, g_currentPort->pnLoc);

    /* Advance pen */
    g_currentPort->pnLoc.h += CharWidth(ch);
}

void DrawString(ConstStr255Param s) {
    extern GrafPtr g_currentPort; /* From QuickDrawCore.c */
    if (!g_currentPort || !s) return;

    int16_t length = s[0]; /* Pascal string length */
    if (length > 0) {
        DrawTextString((const char *)(s + 1), length, g_currentPort->pnLoc);

        /* Advance pen */
        g_currentPort->pnLoc.h += StringWidth(s);
    }
}

void DrawText(const void *textBuf, int16_t firstByte, int16_t byteCount) {
    extern GrafPtr g_currentPort; /* From QuickDrawCore.c */
    if (!g_currentPort || !textBuf || byteCount <= 0) return;

    const char *text = (const char *)textBuf + firstByte;
    DrawTextString(text, byteCount, g_currentPort->pnLoc);

    /* Advance pen */
    g_currentPort->pnLoc.h += TextWidth(textBuf, firstByte, byteCount);
}

/* ================================================================
 * TEXT MEASUREMENT
 * ================================================================ */

int16_t CharWidth(int16_t ch) {
    extern GrafPtr g_currentPort; /* From QuickDrawCore.c */
    if (!g_currentPort) return 0;

    /* Update font cache if needed */
    UpdateFontCache(g_currentPort->txFont, g_currentPort->txSize, g_currentPort->txFace);

    /* Simple character width calculation */
    /* In a real implementation, this would look up actual character widths */
    int16_t baseWidth = g_fontCache.fontInfo.widMax / 2;

    /* Adjust for common characters */
    if (ch >= '0' && ch <= '9') return baseWidth;           /* Digits */
    if (ch >= 'A' && ch <= 'Z') return baseWidth + 1;      /* Uppercase */
    if (ch >= 'a' && ch <= 'z') return baseWidth;          /* Lowercase */
    if (ch == ' ') return baseWidth / 2;                   /* Space */
    if (ch == 'i' || ch == 'l' || ch == '1') return baseWidth / 2; /* Narrow chars */
    if (ch == 'W' || ch == 'M') return baseWidth + 2;      /* Wide chars */

    return baseWidth;
}

int16_t StringWidth(ConstStr255Param s) {
    if (!s) return 0;

    int16_t length = s[0]; /* Pascal string length */
    int16_t totalWidth = 0;

    for (int16_t i = 1; i <= length; i++) {
        totalWidth += CharWidth(s[i]);
    }

    return totalWidth;
}

int16_t TextWidth(const void *textBuf, int16_t firstByte, int16_t byteCount) {
    if (!textBuf || byteCount <= 0) return 0;

    const char *text = (const char *)textBuf + firstByte;
    int16_t totalWidth = 0;

    for (int16_t i = 0; i < byteCount; i++) {
        totalWidth += CharWidth(text[i]);
    }

    return totalWidth;
}

void GetFontInfo(FontInfo *info) {
    extern GrafPtr g_currentPort; /* From QuickDrawCore.c */
    if (!info) return;

    if (g_currentPort) {
        UpdateFontCache(g_currentPort->txFont, g_currentPort->txSize, g_currentPort->txFace);
        *info = g_fontCache.fontInfo;
    } else {
        /* Default font info */
        info->ascent = 12;
        info->descent = 3;
        info->widMax = 8;
        info->leading = 2;
    }
}

/* ================================================================
 * INTERNAL HELPER FUNCTIONS
 * ================================================================ */

static void UpdateFontCache(int16_t fontID, int16_t fontSize, Style fontStyle) {
    /* Check if cache is valid */
    if (g_fontCache.valid &&
        g_fontCache.fontID == fontID &&
        g_fontCache.fontSize == fontSize &&
        g_fontCache.fontStyle == fontStyle) {
        return;
    }

    /* Update cache */
    g_fontCache.fontID = fontID;
    g_fontCache.fontSize = fontSize;
    g_fontCache.fontStyle = fontStyle;

    /* Calculate font metrics */
    int16_t baseSize = (fontSize > 0) ? fontSize : 12; /* Default size */

    /* Simple font metric calculation */
    g_fontCache.fontInfo.ascent = (baseSize * 3) / 4;
    g_fontCache.fontInfo.descent = baseSize / 4;
    g_fontCache.fontInfo.widMax = (baseSize * 2) / 3;
    g_fontCache.fontInfo.leading = baseSize / 6;

    /* Adjust for style */
    if (fontStyle & bold) {
        g_fontCache.fontInfo.widMax++;
    }
    if (fontStyle & italic) {
        g_fontCache.fontInfo.widMax++;
    }
    if (fontStyle & extend) {
        g_fontCache.fontInfo.widMax = (g_fontCache.fontInfo.widMax * 3) / 2;
    }
    if (fontStyle & condense) {
        g_fontCache.fontInfo.widMax = (g_fontCache.fontInfo.widMax * 2) / 3;
    }

    g_fontCache.valid = true;
}

static void DrawTextString(const char *text, int16_t length, Point startPt) {
    extern GrafPtr g_currentPort; /* From QuickDrawCore.c */
    if (!g_currentPort || !text || length <= 0) return;

    /* Update font cache */
    UpdateFontCache(g_currentPort->txFont, g_currentPort->txSize, g_currentPort->txFace);

    /* Apply text styling */
    ApplyTextStyle(g_currentPort->txFace);

    /* Draw each character */
    Point currentPt = startPt;
    for (int16_t i = 0; i < length; i++) {
        char ch = text[i];

        /* Get character width */
        int16_t charWidth = CharWidth(ch);

        /* Draw character using platform layer */
        QDPlatform_DrawCharacter(g_currentPort, ch, currentPt,
                                g_currentPort->txFont, g_currentPort->txSize,
                                g_currentPort->txFace, g_currentPort->txMode);

        /* Advance to next character position */
        currentPt.h += charWidth;

        /* Add extra space if specified */
        if (g_currentPort->spExtra != 0) {
            currentPt.h += (int16_t)(g_currentPort->spExtra >> 16); /* Convert Fixed to int */
        }
    }

    /* Handle underline */
    if (g_currentPort->txFace & underline) {
        int16_t underlineY = startPt.v + g_fontCache.fontInfo.ascent + 1;
        int16_t textWidth = MeasureTextWidth(text, length);

        Point underlineStart = {startPt.v, startPt.h};
        Point underlineEnd = {startPt.v, startPt.h + textWidth};

        underlineStart.v = underlineEnd.v = underlineY;

        QDPlatform_DrawLine(g_currentPort, underlineStart, underlineEnd,
                           &g_currentPort->pnPat, g_currentPort->txMode);
    }

    /* Handle shadow */
    if (g_currentPort->txFace & shadow) {
        Point shadowPt = {startPt.v + 1, startPt.h + 1};
        /* Draw shadow text at offset position */
        /* This would typically be drawn in a shadow color */
    }
}

static int16_t MeasureTextWidth(const char *text, int16_t length) {
    int16_t totalWidth = 0;
    extern GrafPtr g_currentPort; /* From QuickDrawCore.c */

    for (int16_t i = 0; i < length; i++) {
        totalWidth += CharWidth(text[i]);

        /* Add extra space if specified */
        if (g_currentPort && g_currentPort->spExtra != 0) {
            totalWidth += (int16_t)(g_currentPort->spExtra >> 16);
        }
    }

    return totalWidth;
}

static void ApplyTextStyle(Style style) {
    /* This function would set up platform-specific text styling */
    /* For now, it's a placeholder */

    /* In a real implementation, this would:
     * - Set bold rendering for (style & bold)
     * - Set italic skewing for (style & italic)
     * - Set outline mode for (style & outline)
     * - etc.
     */
}

/* Additional platform function declaration needed */
void QDPlatform_DrawCharacter(GrafPtr port, char ch, Point pt,
                             int16_t fontID, int16_t fontSize,
                             Style fontStyle, int16_t mode) {
    /* Platform-specific character drawing implementation would go here */
    /* For now, this is a stub */
}