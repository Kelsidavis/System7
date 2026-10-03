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

#endif /* FONT_INTERNAL_H */
