/*
 * System 7.1 Portable - ExpandMem (Extended Memory Globals)
 *
 * ExpandMem provides extended global variable space beyond the original
 * Mac OS low memory globals. It's essential for System 7.1's extended
 * functionality including international support, extended file system
 * features, and process management.
 *
 * Copyright (c) 2024 - Portable Mac OS Project
 */

#ifndef EXPANDMEM_H

#include "SystemTypes.h"
#define EXPANDMEM_H

#include "SystemTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ExpandMem version and sizing */
#define EM_CURRENT_VERSION      0x0200     /* Version 2.0 for System 7.1 */
#define EM_MIN_SIZE            0x1000      /* Minimum ExpandMem size (4KB) */
#define EM_STANDARD_SIZE       0x2000      /* Standard size (8KB) */
#define EM_EXTENDED_SIZE       0x4000      /* Extended size for future (16KB) */

/* Key cache constants for keyboard support */
#define KEY_CACHE_MIN          0x400       /* Minimum key cache size */
#define KEY_CACHE_SLOP         0x100       /* Extra space for safety */
#define KEY_DEAD_STATE_SIZE    16          /* Dead key state buffer */

/* ExpandMem Record Structure */

/* ExpandMem API Functions */


/**
 * Dump ExpandMem contents for debugging
 *
 * @param em ExpandMem pointer
 * @param output_func Function to output debug text
 */
void ExpandMemDump(const ExpandMemRec* em,
                   void (*output_func)(const char* text));


#ifdef __cplusplus
}
#endif

#endif /* EXPANDMEM_H */