/*
 * RE-AGENT-BANNER
 * PRAM Desktop Preferences Implementation
 * System 7.1 Desktop Pattern Persistence
 *
 * Provides PRAM-style persistent storage for desktop patterns.
 * In real System 7, this would use Parameter RAM; we use a simple
 * in-memory storage that simulates PRAM.
 */

#include "PatternMgr/pram_prefs.h"
#include <string.h>

/* Static storage for desktop preferences (simulates PRAM) */
static DesktopPref gStoredPref = {
    .usePixPat = false,
    .patID = 16,  /* Default to pattern ID 16 */
    .ppatID = 0,
    .backColor = { 0xC000, 0xC000, 0xC000 }  /* Light gray */
};
static bool gPrefInitialized = false;

bool PRAM_LoadDesktopPref(DesktopPref *out) {
    if (!out) return false;

    /* Copy stored preference */
    *out = gStoredPref;
    gPrefInitialized = true;

    return true;
}

bool PRAM_SaveDesktopPref(const DesktopPref *in) {
    if (!in) return false;

    /* Store preference */
    gStoredPref = *in;
    gPrefInitialized = true;

    return true;
}