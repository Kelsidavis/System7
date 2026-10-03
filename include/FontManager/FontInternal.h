/* Font Manager Internal Functions */
#ifndef FONT_INTERNAL_H
#define FONT_INTERNAL_H

#include "SystemTypes.h"

/* Font drawing internals */
void FM_DrawChicagoCharInternal(short x, short y, unsigned char ch, uint32_t color);
void QD_LocalToPixel(short localX, short localY, short* pixelX, short* pixelY);
void DrawChar(SInt16 ch);
void DrawString(ConstStr255Param s);
void DrawText(const void *textBuf, SInt16 firstByte, SInt16 byteCount);

/* Font style synthesis - see FontStyleSynthesis.h for the actual API */
/* These are internal buffer-based operations not currently implemented */
SInt16 FM_GetBoldWidth(SInt16 baseWidth);
SInt16 FM_GetShadowWidth(SInt16 baseWidth);
SInt16 FM_GetOutlineWidth(SInt16 baseWidth);
SInt16 FM_GetCondensedWidth(SInt16 baseWidth);
SInt16 FM_GetExtendedWidth(SInt16 baseWidth);
SInt16 FM_GetStyleExtraHeight(Style style);

#endif /* FONT_INTERNAL_H */
