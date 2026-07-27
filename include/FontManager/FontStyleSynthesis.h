/*
 * FontStyleSynthesis.h - Font Style Synthesis API
 *
 * Style generation functions for System 7.1 Font Manager
 */

#ifndef FONT_STYLE_SYNTHESIS_H
#define FONT_STYLE_SYNTHESIS_H

#include "SystemTypes.h"
#include "FontTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Individual Style Synthesis - internal functions, not exported */

/* Width Calculations for Styles */
short FM_GetBoldWidth(short normalWidth);
short FM_GetItalicWidth(short normalWidth, short height);
short FM_GetShadowWidth(short normalWidth);
short FM_GetOutlineWidth(short normalWidth);
short FM_GetCondensedWidth(short normalWidth);
short FM_GetExtendedWidth(short normalWidth);

/* Combined Style Support */
short FM_GetStyledCharWidth(unsigned char ch, Style face);

/* String Operations with Styles */
short FM_MeasureStyledString(ConstStr255Param s, Style face);

/* Style Metrics */
short FM_GetStyleExtraWidth(Style face, short baseSize);
short FM_GetStyleExtraHeight(Style face);

#ifdef __cplusplus
}
#endif

#endif /* FONT_STYLE_SYNTHESIS_H */