/* QuickDraw compatibility constants and shared type includes. */

#ifndef QUICKDRAW_TYPES_H
#define QUICKDRAW_TYPES_H

#include "SystemTypes.h"

#include "EventManager/EventTypes.h"  /* Must include before WindowTypes.h to avoid activeFlag conflict */
#include "WindowManager/WindowTypes.h"

/* Constants */
#define kQDMaxColors 256
#define kQDPatternSize 8
#define kQDCursorSize 16

#endif /* QUICKDRAW_TYPES_H */
