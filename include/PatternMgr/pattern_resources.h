/*
 * RE-AGENT-BANNER
 * Pattern Resources Header
 * System 7.1 Pattern Resource Loading
 *
 * Loads 'PAT ' and 'ppat' resources from resource files.
 * Mirrors the pattern resource loading in classic Mac OS.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "SystemTypes.h"
#include "ResourceManager.h"

#ifdef __cplusplus
extern "C" {
#endif

#define kPatternResourceType  FOURCC('P', 'A', 'T', ' ')
#define kPixPatternResourceType FOURCC('p', 'p', 'a', 't')

/* Load an 8×8 1‑bit classic Pattern (8 bytes), mapped into QuickDraw Pattern */
bool LoadPATResource(int16_t id, Pattern *outPat);

/* Load a PixPat blob as a Handle (opaque to callers, interpreted by QuickDraw) */
Handle LoadPPATResource(int16_t id);

/* Decode PPAT8 format into RGBA pixels */
bool DecodePPAT8(const uint8_t* p, size_t n, uint32_t outRGBA[64]);

/* Get built-in pattern data by ID */
const uint8_t* GetBuiltInPatternData(int16_t patternID);

#ifdef __cplusplus
}
#endif
