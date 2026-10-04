/*
 * WindowRegions.c - Implementation of Safe Region Management
 *
 * Copyright (c) 2025 - System 7.1 Portable Project
 */

#include "WindowManager/WindowRegions.h"
#include "QuickDraw/QuickDraw.h"
#include <stddef.h>

/* ============================================================================
 * Region Ownership Functions
 * ============================================================================ */

AutoRgnHandle WM_NewAutoRgn(void) {
    AutoRgnHandle handle;
    handle.rgn = NewRgn();
    handle.owned = true;
    return handle;
}

AutoRgnHandle WM_WrapRgn(RgnHandle rgn, Boolean takeOwnership) {
    AutoRgnHandle handle;
    handle.rgn = rgn;
    handle.owned = takeOwnership;
    return handle;
}

void WM_DisposeAutoRgn(AutoRgnHandle* handle) {
    if (handle && handle->owned && handle->rgn) {
        DisposeRgn(handle->rgn);
        handle->rgn = NULL;
    }
    if (handle) {
        handle->owned = false;
    }
}

RgnHandle WM_ReleaseAutoRgn(AutoRgnHandle* handle) {
    RgnHandle rgn = NULL;
    if (handle) {
        rgn = handle->rgn;
        handle->owned = false;
    }
    return rgn;
}

/* ============================================================================
 * Region Operation Helpers
 * ============================================================================ */

AutoRgnHandle WM_CopyToAutoRgn(RgnHandle srcRgn) {
    AutoRgnHandle handle = WM_NewAutoRgn();
    if (handle.rgn && srcRgn) {
        CopyRgn(srcRgn, handle.rgn);
    }
    return handle;
}

AutoRgnHandle WM_RectToAutoRgn(const Rect* rect) {
    AutoRgnHandle handle = WM_NewAutoRgn();
    if (handle.rgn && rect) {
        RectRgn(handle.rgn, rect);
    }
    return handle;
}
