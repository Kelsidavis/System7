/*
 * WindowRegions.h - Explicit ownership helpers for QuickDraw regions
 *
 * This header defines ownership wrappers for QuickDraw regions. They make
 * ownership explicit, but callers must dispose owned regions themselves.
 *
 * OWNERSHIP MODEL:
 * The wrapper records whether its region is owned and provides a common
 * disposal operation. It does not provide automatic cleanup: every owning
 * caller must call WM_DisposeAutoRgn() on all exit paths.
 *
 * Copyright (c) 2025 - System 7.1 Portable Project
 */

#ifndef __WINDOW_REGIONS_H__
#define __WINDOW_REGIONS_H__

#include "SystemTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Owned Region Handle
 * ============================================================================ */

/*
 * AutoRgnHandle - region handle with explicit ownership tracking
 *
 * This structure tracks whether a region handle is "owned" and should be
 * disposed when no longer needed. Call WM_DisposeAutoRgn() on every exit path
 * that owns the region; this C wrapper does not clean up automatically.
 *
 * Example usage:
 *   void MyFunction(void) {
 *       AutoRgnHandle tempRgn = WM_NewAutoRgn();
 *       if (!tempRgn.rgn) {
 *           return;  // No leak - rgn is NULL
 *       }
 *
 *       // Use tempRgn.rgn for work...
 *       RectRgn(tempRgn.rgn, &someRect);
 *
 *       // An early return must dispose the region first
 *       if (someError) {
 *           WM_DisposeAutoRgn(&tempRgn);
 *           return;  // No leak
 *       }
 *
 *       // Normal cleanup
 *       WM_DisposeAutoRgn(&tempRgn);
 *   }
 */
typedef struct AutoRgnHandle {
    RgnHandle rgn;      /* The actual region handle (may be NULL) */
    Boolean owned;      /* True if we should dispose this region */
} AutoRgnHandle;

/* ============================================================================
 * Region Ownership Functions
 * ============================================================================ */

/*
 * WM_NewAutoRgn - Create a region and mark it as owned
 *
 * Allocates a new region and records that the returned wrapper owns it.
 *
 * Returns: AutoRgnHandle with owned=true and rgn=NewRgn() result
 *          If allocation fails, rgn will be NULL but structure is still valid
 *
 * IMPORTANT: Always call WM_DisposeAutoRgn() when done, even if rgn is NULL
 */
AutoRgnHandle WM_NewAutoRgn(void);

/*
 * WM_WrapRgn - Wrap an existing region with explicit ownership
 *
 * Wraps an existing region handle and records whether the wrapper owns it.
 * Useful when taking ownership of a region from another function.
 *
 * Parameters:
 *   rgn - Existing region handle to wrap
 *   takeOwnership - If true, rgn will be disposed by WM_DisposeAutoRgn()
 *
 * Returns: AutoRgnHandle wrapping the provided region
 */
AutoRgnHandle WM_WrapRgn(RgnHandle rgn, Boolean takeOwnership);

/*
 * WM_DisposeAutoRgn - Dispose an owned region
 *
 * Disposes the region if owned, then marks as no longer owned.
 * Safe to call multiple times on the same AutoRgnHandle.
 * Safe to call with NULL rgn.
 *
 * Parameters:
 *   handle - AutoRgnHandle to dispose (will be modified to owned=false)
 */
void WM_DisposeAutoRgn(AutoRgnHandle* handle);

/*
 * WM_ReleaseAutoRgn - Release ownership without disposing
 *
 * Marks the region as no longer owned, so WM_DisposeAutoRgn() will not
 * dispose it. Useful when transferring ownership to another function.
 *
 * Parameters:
 *   handle - AutoRgnHandle to release (will be modified to owned=false)
 *
 * Returns: The region handle (caller now responsible for disposal)
 */
RgnHandle WM_ReleaseAutoRgn(AutoRgnHandle* handle);

/* ============================================================================
 * Convenience Macros for Common Patterns
 * ============================================================================ */

/*
 * WM_WITH_AUTO_RGN - Declare an owned region wrapper in function scope
 *
 * Declares an AutoRgnHandle. The wrapper is not automatically disposed when
 * the function returns; pair it with WM_CLEANUP_AUTO_RGN() on each exit path.
 *
 * Example:
 *   void MyFunction(void) {
 *       WM_WITH_AUTO_RGN(tempRgn);
 *       if (!tempRgn.rgn) return;
 *
 *       // Use tempRgn.rgn...
 *
 *       WM_CLEANUP_AUTO_RGN(tempRgn);  // Explicit cleanup
 *   }
 */
#define WM_WITH_AUTO_RGN(name) AutoRgnHandle name = WM_NewAutoRgn()

/*
 * WM_CLEANUP_AUTO_RGN - Clean up auto region
 *
 * Disposes an auto region created with WM_WITH_AUTO_RGN.
 */
#define WM_CLEANUP_AUTO_RGN(name) WM_DisposeAutoRgn(&(name))

/* ============================================================================
 * Region Operation Helpers
 * ============================================================================ */

/*
 * WM_CopyToAutoRgn - Copy region into an owned handle
 *
 * Creates a new owned region and copies source region into it.
 * Handles allocation failure gracefully.
 *
 * Parameters:
 *   srcRgn - Source region to copy from
 *
 * Returns: AutoRgnHandle containing copy (rgn may be NULL if allocation failed)
 */
AutoRgnHandle WM_CopyToAutoRgn(RgnHandle srcRgn);

/*
 * WM_RectToAutoRgn - Create an owned rectangular region
 *
 * Creates a new owned region from a rectangle.
 *
 * Parameters:
 *   rect - Rectangle to convert to region
 *
 * Returns: AutoRgnHandle containing rectangle region
 */
AutoRgnHandle WM_RectToAutoRgn(const Rect* rect);

#ifdef __cplusplus
}
#endif

#endif /* __WINDOW_REGIONS_H__ */
