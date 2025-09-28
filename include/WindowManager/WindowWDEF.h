/*
 * WindowWDEF.h - Window Definition Procedure (WDEF) Constants
 *
 * [WM-054] Single source of truth for WDEF message constants
 * Provenance: IM:Windows Vol I pp. 2-88 to 2-95 "Window Definition Procedures"
 *
 * This header defines the canonical WDEF message codes used by window definition
 * procedures (WDEFs) to handle window frame drawing, hit testing, and region
 * calculation. All Window Manager code that uses WDEF messages must include
 * this header to prevent duplicate definitions.
 *
 * WDEF Message Protocol (IM:Windows Vol I p. 2-90):
 * - wDraw (0): Draw the window frame in its current state
 * - wHit (1): Determine which part of the window contains a given point
 * - wCalcRgns (2): Calculate structure and content regions
 * - wNew (3): Perform initialization for a newly created window
 * - wDispose (4): Perform cleanup before window disposal
 * - wGrow (5): Draw feedback during window grow operation
 * - wDrawGIcon (6): Draw the grow icon in the window's lower-right corner
 *
 * Copyright (c) 2025 - System 7.1 Portable Project
 * Based on Apple Macintosh System Software 7.1 Window Manager
 */

#ifndef WINDOW_WDEF_H
#define WINDOW_WDEF_H

/* [WM-054] WDEF Message Constants from IM:Windows Vol I pp. 2-88 to 2-95 */
#ifndef wDraw
#define wDraw        0  /* Draw window frame */
#endif

#ifndef wHit
#define wHit         1  /* Hit test window parts */
#endif

#ifndef wCalcRgns
#define wCalcRgns    2  /* Calculate window regions */
#endif

#ifndef wNew
#define wNew         3  /* Initialize new window */
#endif

#ifndef wDispose
#define wDispose     4  /* Dispose window */
#endif

#ifndef wGrow
#define wGrow        5  /* Grow window */
#endif

#ifndef wDrawGIcon
#define wDrawGIcon   6  /* Draw grow icon */
#endif

#endif /* WINDOW_WDEF_H */