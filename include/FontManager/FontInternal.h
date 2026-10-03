/* Font Manager Internal Functions */
#ifndef FONT_INTERNAL_H
#define FONT_INTERNAL_H

#include "SystemTypes.h"
#include "QuickDraw/QuickDrawInternal.h"

/* Font drawing internals */
void FM_DrawChicagoCharInternal(short x, short y, unsigned char ch, uint32_t color);
void DrawChar(SInt16 ch);

#endif /* FONT_INTERNAL_H */
