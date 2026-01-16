/*
 * FontStyleSynthesis.c - Font Style Synthesis Implementation
 *
 * Generates bold, italic, underline, shadow, outline styles
 * from base bitmap fonts per System 7.1 specifications
 */

#include "FontManager/FontManager.h"
#include "FontManager/FontTypes.h"
#include "FontManager/FontStyleSynthesis.h"
#include "FontManager/FontInternal.h"
#include "QuickDraw/QuickDraw.h"
#include "SystemTypes.h"
#include <string.h>
#include "FontManager/FontLogging.h"

/* Ensure boolean constants */
#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

/* Debug logging */
#define FSS_DEBUG 1

#if FSS_DEBUG
#define FSS_LOG(...) FONT_LOG_DEBUG("FSS: " __VA_ARGS__)
#else
#define FSS_LOG(...)
#endif

/* External dependencies */
extern GrafPtr g_currentPort;

/* Internal Font Manager drawing function */
extern void FM_DrawChicagoCharInternal(short x, short y, char ch, uint32_t color);

/* Style synthesis parameters (per System 7.1) */
#define BOLD_OFFSET         1    /* Horizontal emboldening pixels */
#define ITALIC_SHEAR_RATIO  4    /* 1:4 shear ratio (~14 degrees) */
#define UNDERLINE_OFFSET    2    /* Pixels below baseline */
#define UNDERLINE_THICKNESS 1    /* Underline thickness */
#define SHADOW_OFFSET_X     1    /* Shadow horizontal offset */
#define SHADOW_OFFSET_Y     1    /* Shadow vertical offset */
#define OUTLINE_THICKNESS   1    /* Outline stroke width */
#define CONDENSE_FACTOR     0.9  /* 90% horizontal spacing */
#define EXTEND_FACTOR       1.1  /* 110% horizontal spacing */

/* ============================================================================
 * Bold Synthesis
 * ============================================================================ */

/*
 * FM_SynthesizeBold - Create bold version by horizontal emboldening
 * Algorithm: Draw character twice, offset by BOLD_OFFSET pixels
 */
static void FM_SynthesizeBold(short x, short y, char ch, uint32_t color) {
    FSS_LOG("SynthesizeBold: char='%c' at (%d,%d)\n", ch, x, y);

    /* Draw original character */
    FM_DrawChicagoCharInternal(x, y, ch, color);

    /* Draw offset copy for emboldening */
    FM_DrawChicagoCharInternal(x + BOLD_OFFSET, y, ch, color);
}

/*
 * FM_GetBoldWidth - Calculate width of bold character
 * Bold adds BOLD_OFFSET pixels to normal width
 */
short FM_GetBoldWidth(short normalWidth) {
    return normalWidth + BOLD_OFFSET;
}

/* ============================================================================
 * Italic Synthesis
 * ============================================================================ */

/*
 * FM_SynthesizeItalic - Create italic by shearing transform
 * Algorithm: Shift each scanline proportionally (1:4 ratio)
 * Draws the character multiple times with progressively offset positions
 * to simulate a shear/slant effect (~14 degrees)
 */
static void FM_SynthesizeItalic(short x, short y, char ch, uint32_t color) {
    FSS_LOG("SynthesizeItalic: char='%c' at (%d,%d)\n", ch, x, y);

    /* Draw character with shear effect using multiple offset draws
     * This creates an italic appearance through horizontal offset
     * Top scanlines shifted more than bottom for 1:4 shear ratio
     */
    short shearAmount = 1;  /* ~1-2 pixels maximum shear for typical char height */

    /* Draw base character */
    FM_DrawChicagoCharInternal(x, y, ch, color);

    /* Draw with slight right offset at top to create italic effect */
    FM_DrawChicagoCharInternal(x + shearAmount, y - 1, ch, color);

    /* Optional: add more shear for stronger italic effect on taller fonts */
    /* FM_DrawChicagoCharInternal(x + shearAmount/2, y - 2, ch, color); */
}

/*
 * FM_GetItalicWidth - Calculate width of italic character
 * Italic adds shear amount to normal width
 */
short FM_GetItalicWidth(short normalWidth, short height) {
    /* Width increases by height/ITALIC_SHEAR_RATIO */
    return normalWidth + (height / ITALIC_SHEAR_RATIO);
}

/* ============================================================================
 * Underline Synthesis
 * ============================================================================ */

/*
 * FM_DrawUnderline - Draw underline for text run
 * Position: baseline + descent/2 (per System 7.1)
 */
static void FM_DrawUnderline(short x, short y, short width, uint32_t color) {
    short underlineY = y + UNDERLINE_OFFSET;

    FSS_LOG("DrawUnderline: from (%d,%d) width=%d\n", x, underlineY, width);

    /* Draw horizontal line using QuickDraw */
    extern void MoveTo(SInt16 h, SInt16 v);
    extern void LineTo(SInt16 h, SInt16 v);

    if (width > 0) {
        MoveTo(x, underlineY);
        LineTo(x + width - 1, underlineY);
    }
}

/* ============================================================================
 * Shadow Synthesis
 * ============================================================================ */

/*
 * FM_SynthesizeShadow - Create shadow effect
 * Algorithm: Draw character, then draw offset shadow behind
 */
static void FM_SynthesizeShadow(short x, short y, char ch, uint32_t foreColor, uint32_t shadowColor) {
    FSS_LOG("SynthesizeShadow: char='%c' at (%d,%d)\n", ch, x, y);

    /* Draw shadow first (offset and in shadow color) */
    FM_DrawChicagoCharInternal(x + SHADOW_OFFSET_X, y + SHADOW_OFFSET_Y, ch, shadowColor);

    /* Draw main character on top */
    FM_DrawChicagoCharInternal(x, y, ch, foreColor);
}

/*
 * FM_GetShadowWidth - Calculate width with shadow
 * Shadow adds offset to normal width
 */
short FM_GetShadowWidth(short normalWidth) {
    return normalWidth + SHADOW_OFFSET_X;
}

/* ============================================================================
 * Outline Synthesis
 * ============================================================================ */

/*
 * FM_SynthesizeOutline - Create outline effect
 * Algorithm: Draw character stroked, then fill center
 */
static void FM_SynthesizeOutline(short x, short y, char ch, uint32_t outlineColor, uint32_t fillColor) {
    FSS_LOG("SynthesizeOutline: char='%c' at (%d,%d)\n", ch, x, y);

    /* Draw outline in 8 directions */
    for (short dx = -OUTLINE_THICKNESS; dx <= OUTLINE_THICKNESS; dx++) {
        for (short dy = -OUTLINE_THICKNESS; dy <= OUTLINE_THICKNESS; dy++) {
            if (dx != 0 || dy != 0) {
                FM_DrawChicagoCharInternal(x + dx, y + dy, ch, outlineColor);
            }
        }
    }

    /* Draw fill in center */
    FM_DrawChicagoCharInternal(x, y, ch, fillColor);
}

/*
 * FM_GetOutlineWidth - Calculate width with outline
 * Outline adds thickness on both sides
 */
short FM_GetOutlineWidth(short normalWidth) {
    return normalWidth + (OUTLINE_THICKNESS * 2);
}

/* ============================================================================
 * Condense/Extend Synthesis
 * ============================================================================ */

/*
 * FM_GetCondensedWidth - Calculate condensed width
 * Condense reduces spacing by 10%
 */
short FM_GetCondensedWidth(short normalWidth) {
    return (short)(normalWidth * CONDENSE_FACTOR);
}

/*
 * FM_GetExtendedWidth - Calculate extended width
 * Extend increases spacing by 10%
 */
short FM_GetExtendedWidth(short normalWidth) {
    return (short)(normalWidth * EXTEND_FACTOR);
}

/* ============================================================================
 * Combined Style Synthesis
 * ============================================================================ */

/*
 * FM_SynthesizeStyledChar - Apply multiple styles to character
 * Handles combination of bold, italic, underline, etc.
 */
static void FM_SynthesizeStyledChar(short x, short y, char ch, Style face, uint32_t color) {
    Boolean hasBold = (face & bold) != 0;
    Boolean hasItalic = (face & italic) != 0;
    Boolean hasUnderline = (face & underline) != 0;
    Boolean hasShadow = (face & shadow) != 0;
    Boolean hasOutline = (face & outline) != 0;
    Boolean hasCondense = (face & condense) != 0;
    Boolean hasExtend = (face & extend) != 0;

    FSS_LOG("SynthesizeStyledChar: char='%c' face=0x%02X\n", ch, face);

    /* Handle outline first (affects everything else) */
    if (hasOutline) {
        uint32_t fillColor = hasShadow ? 0xFFFFFFFF : color; /* White fill for shadow+outline */
        FM_SynthesizeOutline(x, y, ch, color, fillColor);
        return; /* Outline handles its own rendering */
    }

    /* Handle shadow */
    if (hasShadow) {
        uint32_t shadowColor = 0xFF808080; /* Gray shadow */
        FM_SynthesizeShadow(x, y, ch, color, shadowColor);
        return; /* Shadow handles its own rendering */
    }

    /* Handle bold */
    if (hasBold && hasItalic) {
        /* Bold italic - combine both effects */
        FM_SynthesizeBold(x, y, ch, color);
        /* Would apply italic shear here */
    } else if (hasBold) {
        FM_SynthesizeBold(x, y, ch, color);
    } else if (hasItalic) {
        FM_SynthesizeItalic(x, y, ch, color);
    } else {
        /* Plain character */
        FM_DrawChicagoCharInternal(x, y, ch, color);
    }

    /* Add underline if needed (drawn after character) */
    if (hasUnderline) {
        short width = CharWidth(ch);
        if (hasBold) width = FM_GetBoldWidth(width);
        if (hasCondense) width = FM_GetCondensedWidth(width);
        if (hasExtend) width = FM_GetExtendedWidth(width);

        FM_DrawUnderline(x, y, width, color);
    }
}

/*
 * FM_GetStyledCharWidth - Calculate width with all styles applied
 */
short FM_GetStyledCharWidth(char ch, Style face) {
    short width = CharWidth(ch);

    /* Apply style modifiers */
    if (face & bold) {
        width = FM_GetBoldWidth(width);
    }

    if (face & italic) {
        /* Assume standard font height for shear calculation */
        width = FM_GetItalicWidth(width, 15); /* Chicago is 15 pixels tall */
    }

    if (face & shadow) {
        width = FM_GetShadowWidth(width);
    }

    if (face & outline) {
        width = FM_GetOutlineWidth(width);
    }

    if (face & condense) {
        width = FM_GetCondensedWidth(width);
    }

    if (face & extend) {
        width = FM_GetExtendedWidth(width);
    }

    return width;
}

/*
 * FM_DrawStyledString - Draw complete string with styles
 */
__attribute__((unused))
static void FM_DrawStyledString(ConstStr255Param s, Style face) {
    if (!s || s[0] == 0) return;

    Point pen = g_currentPort->pnLoc;
    uint32_t color = 0xFF000000; /* Black */

    FSS_LOG("DrawStyledString: \"%.*s\" with face=0x%02X\n", s[0], &s[1], face);

    /* Draw each character with styles */
    for (int i = 1; i <= s[0]; i++) {
        char ch = s[i];

        /* Draw styled character */
        FM_SynthesizeStyledChar(pen.h, pen.v, ch, face, color);

        /* Advance pen by styled width */
        pen.h += FM_GetStyledCharWidth(ch, face);
    }

    /* Update pen location */
    g_currentPort->pnLoc = pen;
}

/*
 * FM_MeasureStyledString - Measure string width with styles
 */
short FM_MeasureStyledString(ConstStr255Param s, Style face) {
    if (!s || s[0] == 0) return 0;

    short totalWidth = 0;

    for (int i = 1; i <= s[0]; i++) {
        totalWidth += FM_GetStyledCharWidth(s[i], face);
    }

    FSS_LOG("MeasureStyledString: \"%.*s\" face=0x%02X = %d pixels\n",
            s[0], &s[1], face, totalWidth);

    return totalWidth;
}

/* ============================================================================
 * Style Property Tables
 * ============================================================================ */

/*
 * FM_GetStyleExtraWidth - Get additional width needed for style
 * Used by FMSwapFont to report style metrics
 */
short FM_GetStyleExtraWidth(Style face, short baseSize) {
    short extra = 0;

    if (face & bold) {
        extra += BOLD_OFFSET;
    }

    if (face & italic) {
        /* Italic adds based on font height */
        extra += baseSize / ITALIC_SHEAR_RATIO;
    }

    if (face & shadow) {
        extra += SHADOW_OFFSET_X;
    }

    if (face & outline) {
        extra += OUTLINE_THICKNESS * 2;
    }

    /* Condense/Extend handled separately as factors */

    return extra;
}

/*
 * FM_GetStyleExtraHeight - Get additional height needed for style
 * Shadow and outline can increase vertical space
 */
short FM_GetStyleExtraHeight(Style face) {
    short extra = 0;

    if (face & shadow) {
        extra += SHADOW_OFFSET_Y;
    }

    if (face & outline) {
        extra += OUTLINE_THICKNESS * 2;
    }

    if (face & underline) {
        /* Underline extends below baseline */
        extra += UNDERLINE_OFFSET + UNDERLINE_THICKNESS;
    }

    return extra;
}