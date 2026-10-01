/*
 * RE-AGENT-BANNER
 * Pattern Manager Header
 * System 7.1 Desktop Pattern Management
 *
 * Manages desktop patterns and background colors for System 7.1.
 * The Pattern Manager owns the background state that QuickDraw uses
 * when erasing regions and rectangles.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "SystemTypes.h"
#include "ResourceManager.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Desktop preference structure */
typedef struct DesktopPref {
    bool     usePixPat;   /* false = PAT (1-bit); true = ppat (PixPat) */
    int16_t  patID;       /* 'PAT ' resource ID if usePixPat == false */
    int16_t  ppatID;      /* 'ppat' resource ID if usePixPat == true */
    RGBColor backColor;   /* background color (0-bits or PixPat base) */
} DesktopPref;

/* Initialization */
void PM_Init(void);

/* Classic-style QuickDraw background setters */
void PM_SetBackPat(const Pattern *pat);              /* BackPat */
void PM_SetBackPixPat(Handle pixPatHandle);          /* BackPixPat (opaque blob for now) */
void PM_SetBackColor(const RGBColor *rgb);           /* RGBBackColor */

/* Get current background state */
void PM_GetBackPat(Pattern *pat);
void PM_GetBackColor(RGBColor *rgb);
bool PM_IsPixPatActive(void);

/* Persistence (Control Panel writes; Finder reads) */
DesktopPref PM_GetSavedDesktopPref(void);
void PM_SaveDesktopPref(const DesktopPref *p);

/* Apply a saved pref to the current desktop port */
bool PM_ApplyDesktopPref(const DesktopPref *p);

/* Paint the desktop again in the current pattern, around the windows. */
void PM_RedrawDesktop(void);

/* Resource helpers (like GetPattern/GetPixPat) */
bool PM_LoadPAT(int16_t id, Pattern *out);
Handle PM_LoadPPAT(int16_t id);  /* Raw 'ppat' handle; ownership to caller */

/* Get color pattern data if available */
bool PM_GetColorPattern(uint32_t** patternData);

#ifdef __cplusplus
}
#endif