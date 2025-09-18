/*
 * RE-AGENT-BANNER
 * QuickDraw Portable Extensions Header
 * Reimplemented from Apple System 7.1 QuickDraw Core
 *
 * Original binary: System.rsrc (SHA256: 78150ebb66707277e0947fbf81f6a27fc5d263a71bbf96df7084d5c3ec22a5ba)
 * Architecture: 68k Mac OS System 7
 * Evidence sources: String analysis found "DrawBatteryLevel" and "DrawCharger"
 *
 * This file contains Macintosh Portable-specific QuickDraw extensions
 * for drawing battery and charger indicators.
 */

#ifndef QUICKDRAW_PORTABLE_H
#define QUICKDRAW_PORTABLE_H

#include "quickdraw.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Portable-specific drawing functions */
/* Evidence: String "DrawBatteryLevel" found in System.rsrc */
void DrawBatteryLevel(short level, Boolean charging);

/* Evidence: String "DrawCharger" found in System.rsrc */
void DrawCharger(Boolean connected);

/* Battery level constants */
enum {
    kBatteryEmpty = 0,
    kBatteryLow = 1,
    kBatteryMedium = 2,
    kBatteryHigh = 3,
    kBatteryFull = 4
};

/* Drawing context for portable indicators */
typedef struct PortableContext {
    Rect batteryRect;    /* Battery indicator rectangle */
    Rect chargerRect;    /* Charger indicator rectangle */
    Pattern batteryPat;  /* Battery fill pattern */
    Pattern chargePat;   /* Charging pattern */
} PortableContext;

/* Initialize portable drawing context */
void InitPortableContext(PortableContext* context, const Rect* displayRect);

#ifdef __cplusplus
}
#endif

#endif /* QUICKDRAW_PORTABLE_H */

/*
 * RE-AGENT-TRAILER-JSON
 * {
 *   "type": "header_file",
 *   "name": "quickdraw_portable.h",
 *   "description": "Macintosh Portable-specific QuickDraw extensions",
 *   "evidence_sources": ["evidence.curated.quickdraw.json"],
 *   "confidence": 0.80,
 *   "functions_declared": 3,
 *   "system_specific": true,
 *   "target_system": "Macintosh Portable",
 *   "dependencies": ["quickdraw.h"]
 * }
 */