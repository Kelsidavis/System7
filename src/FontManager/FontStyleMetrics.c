/*
 * FontStyleMetrics.c - Font Style Metrics
 *
 * Computes style-adjusted character and string widths and text bounds.
 */

#include "FontManager/FontManager.h"
#include "FontManager/FontTypes.h"
#include "FontManager/FontStyleMetrics.h"
#include "FontManager/FontInternal.h"
#include "SystemTypes.h"
#include "FontManager/FontLogging.h"

/* Debug logging */
#define FSS_DEBUG 1

#if FSS_DEBUG
#define FSS_LOG(...) FONT_LOG_DEBUG("FSS: " __VA_ARGS__)
#else
#define FSS_LOG(...)
#endif

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
 * Bold Width
 * ============================================================================ */

/* Bold adds BOLD_OFFSET pixels to the normal width. */
short FM_GetBoldWidth(short normalWidth) {
    return normalWidth + BOLD_OFFSET;
}

/* ============================================================================
 * Italic Width
 * ============================================================================ */

/* Italic width includes the estimated shear for the font height. */
short FM_GetItalicWidth(short normalWidth, short height) {
    /* Width increases by height/ITALIC_SHEAR_RATIO */
    return normalWidth + (height / ITALIC_SHEAR_RATIO);
}

/* ============================================================================
 * Shadow Width
 * ============================================================================ */

/* Shadow adds its horizontal offset to the normal width. */
short FM_GetShadowWidth(short normalWidth) {
    return normalWidth + SHADOW_OFFSET_X;
}

/* ============================================================================
 * Outline Width
 * ============================================================================ */

/* Outline adds thickness on both sides of the normal width. */
short FM_GetOutlineWidth(short normalWidth) {
    return normalWidth + (OUTLINE_THICKNESS * 2);
}

/* ============================================================================
 * Condense/Extend Width
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

/*
 * FM_GetStyledCharWidth - Calculate width with all styles applied
 */
short FM_GetStyledCharWidth(unsigned char ch, Style face) {
    /* Not CharWidth: that consults the port's style and would call straight
     * back here for any styled text, recursing until the stack gave out. */
    short width = FM_GetPlainCharWidth((short)(unsigned char)ch);

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
