/* Font Manager Internal Functions */
#ifndef FONT_INTERNAL_H
#define FONT_INTERNAL_H

#include "SystemTypes.h"
#include "QuickDraw/QuickDrawInternal.h"

/* Font drawing internals */
void FM_DrawChicagoCharInternal(short x, short y, unsigned char ch, uint32_t color);
void FM_DrawChicagoCharAtSize(short x, short y, unsigned char ch, short size, uint32_t color);
short FM_GetChicagoCharWidthAtSize(unsigned char ch, short size);
void FM_GetChicagoMetricsAtSize(short size, FMetricRec* metrics);
void DrawChar(SInt16 ch);

#endif /* FONT_INTERNAL_H */
