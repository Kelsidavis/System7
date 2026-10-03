/* Persistent desktop pattern preferences. */

#pragma once

#include <stdbool.h>
#include "PatternMgr/pattern_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Load desktop preference from persistent storage */
bool PRAM_LoadDesktopPref(DesktopPref *out);

/* Save desktop preference to persistent storage */
bool PRAM_SaveDesktopPref(const DesktopPref *in);

#ifdef __cplusplus
}
#endif
