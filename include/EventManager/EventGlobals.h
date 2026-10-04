/*
 * EventGlobals.h - Event Manager Global Settings
 *
 * Provides classic System 7 event timing globals and utilities.
 */

#ifndef EVENT_GLOBALS_H
#define EVENT_GLOBALS_H

#include "SystemTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Double-click timing in ticks (default ~500ms at 60Hz) */
extern UInt32 gDoubleTimeTicks;

/* Double-click pixel slop (max distance between clicks) */
extern UInt16 gDoubleClickSlop;

/* Current button state (updated by ModernInput) */
extern volatile UInt8 gCurrentButtons;

/**
 * GetDblTime - Get current double-click time threshold
 * @return Double-click time in ticks
 */
UInt32 GetDblTime(void);

#ifdef __cplusplus
}
#endif

#endif /* EVENT_GLOBALS_H */
