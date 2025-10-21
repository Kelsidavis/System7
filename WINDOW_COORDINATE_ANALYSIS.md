# Window Manager Coordinate System and Rendering Analysis

## Executive Summary

The window manager uses a **"Direct Framebuffer" coordinate approach** where:
- **portBits.bounds** contains GLOBAL screen coordinates (e.g., 1,21,801,621 for a window at screen position (1,21))
- **portRect** contains LOCAL coordinates (0,0,width,height)
- **baseAddr** points to the full framebuffer (not offset)
- **Drawing coordinates** are transformed by subtracting portBits.bounds to get local offsets

This design allows windows to be positioned at any screen location while keeping QuickDraw code simple (drawing at local coords automatically maps to global position through portBits.bounds).

## 1. Window Positioning and Region Setup

### Window Creation (WindowManagerCore.c, InitializeWindowRecord)

**Key Code (lines 773-876):**
```c
// Chrome dimensions
const SInt16 kBorder = 1;
const SInt16 kTitleBar = 20;
const SInt16 kSeparator = 1;

// Calculate window dimensions from boundsRect (GLOBAL frame coords)
SInt16 contentLeft = clampedBounds.left + kBorder;  // Skip left border
SInt16 contentTop = clampedBounds.top + kTitleBar + kSeparator;  // Skip title bar

// Content width/height (excluding chrome on all sides)
SInt16 contentWidth = fullWidth - 3;    // 1px left + 2px right
SInt16 contentHeight = fullHeight - (kTitleBar + kSeparator + 2);

// portRect uses LOCAL coordinates (relative to window)
SetRect(&window->port.portRect, 0, 0, contentWidth, contentHeight);

// portBits.bounds uses GLOBAL coordinates for content area
SetRect(&window->port.portBits.bounds, contentLeft, contentTop,
        contentLeft + contentWidth, contentTop + contentHeight);
```

### Region Initialization (WindowParts.c, WM_CalculateStandardWindowRegions)

**Lines 401-431:**
```c
// strucRgn = full window including chrome (GLOBAL coords)
Rect structRect;
Platform_GetWindowFrameRect(window, &structRect);
Platform_SetRectRgn(window->strucRgn, &structRect);

// contRgn = content area only (GLOBAL coords)
Rect contentRect;
Platform_GetWindowContentRect(window, &contentRect);
Platform_SetRectRgn(window->contRgn, &contentRect);
```

### ContentRect Calculation (WindowPlatform.c, Platform_GetWindowContentRect)

**Lines 397-425:**
```c
// Extract global window bounds from strucRgn
Rect strucRect = (**(window->strucRgn)).rgnBBox;

// Calculate content area by subtracting chrome from frame
const SInt16 kBorder = 1, kTitleBar = 20, kSeparator = 1;
rect->left = strucRect.left + kBorder;
rect->top = strucRect.top + kTitleBar + kSeparator;
rect->right = strucRect.right - (kBorder + 1);  // 2px right border
rect->bottom = strucRect.bottom - kBorder;
```

## 2. Coordinate Systems

### portBits.bounds Semantics

**Current Implementation: GLOBAL Screen Coordinates**
```
For window at screen position (x=100, y=50) with size 400x300:

portBits.bounds = {101, 71, 501, 371}  // GLOBAL coords (left + border, top + title, ...)
portRect = {0, 0, 397, 277}            // LOCAL coords (content dimensions only)
```

**Purpose:** Maps local coordinates to global screen position:
- When drawing at local point (10, 20)
- Actual screen pixel = (10 + portBits.bounds.left, 20 + portBits.bounds.top)
- = (111, 91) on screen

### Glyph Drawing Code (QuickDrawPlatform.c, QDPlatform_DrawGlyphBitmap)

**Lines 1195-1291:**
```c
// pen position comes in GLOBAL screen coordinates
void QDPlatform_DrawGlyphBitmap(GrafPtr port, Point pen, ...)
{
    // Convert global pen to local bitmap coordinates
    SInt16 destX = pen.h - destBits->bounds.left;
    SInt16 destY = pen.v - destBits->bounds.top;
    
    // Draw to framebuffer using local offset
    SInt32 pixelOffset = destY * pixelPitch + destX;
    pixels[pixelOffset] = fgColor;  // pixels = (uint32_t*)baseAddr
}
```

**Key Issue:** If portBits.bounds contains incorrect values, the transformation `pen - bounds` will produce wrong offsets.

## 3. Drawing and Update Handling

### BeginUpdate/EndUpdate (WindowEvents.c)

**BeginUpdate (lines 482-557):**
```c
// Set current port to window for drawing
Platform_SetCurrentPort(&theWindow->port);

// Set clipping to prevent overdraw
if (theWindow->contRgn && theWindow->updateRgn) {
    RgnHandle updateClip = Platform_NewRgn();
    Platform_IntersectRgn(theWindow->contRgn, theWindow->updateRgn, updateClip);
    Platform_SetClipRgn(&theWindow->port, updateClip);
}

// CRITICAL: Erase content with white using LOCAL coordinates
if (theWindow->port.portBits.baseAddr) {
    Rect portRect = theWindow->port.portRect;  // LOCAL: (0,0,width,height)
    for (SInt16 y = 0; y < height; y++) {
        for (SInt16 x = 0; x < width; x++) {
            pixels[y * (fb_pitch / bytes_per_pixel) + x] = 0xFFFFFFFF;
        }
    }
}
```

**EndUpdate (lines 559-616):**
```c
// Clear update region
Platform_SetEmptyRgn(theWindow->updateRgn);

// Restore clip region
Platform_SetClipRgn(&theWindow->port, theWindow->contRgn);
```

### PaintOne/ShowWindow (WindowDisplay.c)

**PaintOne - White Fill (lines 100-175):**
```c
// Fill content area with white background
if (window->contRgn && *(window->contRgn)) {
    Region* rgn = *(window->contRgn);
    if (window->refCon != 0) {  // Skip for desktop window
        extern void FillRgn(RgnHandle rgn, const Pattern* pat);
        FillRgn(window->contRgn, &qd.white);  // Uses GLOBAL coords
    }
}
```

**ShowWindow - Region Invalidation (lines 971-1062):**
```c
// Calculate visible region
CalcVis(window);

// Redraw desktop icons before window
if (g_deskHook && window->strucRgn) {
    g_deskHook(windowRgn);
}

// Paint chrome
PaintOne(window, NULL);

// Invalidate content to trigger update event
if (window->contRgn) {
    CopyRgn(window->contRgn, window->port.clipRgn);
    InvalRgn(window->contRgn);
}
```

## 4. Identified Potential Issues with Extra White Space

### Issue #1: portBits.bounds Offset Discrepancy

**Location:** WindowManagerCore.c lines 837-840
```c
// portBits.bounds set to content area coordinates
SetRect(&window->port.portBits.bounds, contentLeft, contentTop,
        contentLeft + contentWidth, contentTop + contentHeight);
```

**Problem:** The comment says "Direct Framebuffer" but the implementation appears to put GLOBAL coords in portBits.bounds. However, the code later accesses pixels with local coordinates in BeginUpdate:

```c
// BeginUpdate uses LOCAL indexing directly
pixels[y * (fb_pitch / 4) + x] = 0xFFFFFFFF;  // y,x are 0 to height/width
```

**Impact:** If portBits.baseAddr is full framebuffer AND portBits.bounds has global coords, then glyph drawing will work correctly. BUT if drawing code expects baseAddr to be offset to content area, then text will appear offset.

### Issue #2: Coordinate Transform in Glyph Drawing

**Location:** QuickDrawPlatform.c lines 1211-1213
```c
SInt16 destX = pen.h - destBits->bounds.left;  // Convert global to local
SInt16 destY = pen.v - destBits->bounds.top;
```

**Potential Issue:** If pen position comes from title bar text drawing (DrawWindowFrame line 765):
```c
MoveTo(textLeft, textBaseline);  // These are GLOBAL coords
DrawString(titleStr);              // Calls drawing functions with global coords
```

Then the transform works correctly IF portBits.bounds = (contentLeft, contentTop, ...).

BUT this assumes:
1. portBits.bounds.top = contentTop = window.top + titleBar + separator
2. Text baseline calculation is correct relative to frame

### Issue #3: White Space Caused by Chrome Offset

**Potential Source:** WindowManagerCore.c lines 784-797
```c
const SInt16 kBorder = 1;
const SInt16 kTitleBar = 20;
const SInt16 kSeparator = 1;

SInt16 contentWidth = fullWidth - 3;
SInt16 contentHeight = fullHeight - kTitleBar - kSeparator - 2;

SetRect(&window->port.portRect, 0, 0, contentWidth, contentHeight);
```

**Problem:** If portRect is set to exclude chrome, BUT contRgn is calculated from portBits.bounds (which includes chrome), then:
- portRect = (0, 0, 397, 277)
- portBits.bounds = (101, 71, 501, 371) -- includes full area!
- contRgn = (101, 71, 501, 371) -- matches portBits.bounds
- FillRgn fills the entire content area (CORRECT)
- BUT EraseRect in BeginUpdate only fills portRect size (MISSING BORDER!)

### Issue #4: Mismatch Between portBits.bounds and Actual Content Area

**Location:** WindowPlatform.c lines 397-425 (Platform_GetWindowContentRect)
```c
rect->left = strucRect.left + kBorder;
rect->right = strucRect.right - (kBorder + 1);  // -2 for 2px right border
```

**vs.** WindowManagerCore.c lines 788-797:
```c
SInt16 contentWidth = fullWidth - 3;  // -3 = 1 left + 2 right
SetRect(&window->port.portBits.bounds, contentLeft, contentTop,
        contentLeft + contentWidth, contentTop + contentHeight);
```

**Analysis:**
- strucRect.right - strucRect.left = fullWidth
- contentWidth = fullWidth - 3
- portBits.bounds.right = contentLeft + contentWidth
  = (left + 1) + (fullWidth - 3)
  = left + 1 + (right - left) - 3
  = right - 2

- Platform_GetWindowContentRect calculates:
  = strucRect.right - (1 + 1)  // kBorder + 1
  = right - 2

**MATCH:** Both calculate right edge as `frame.right - 2`. So this is consistent.

### Issue #5: Direct Framebuffer Offset Not Calculated

**Location:** WindowManagerCore.c lines 831-835
```c
uint32_t bytes_per_pixel = 4;
uint32_t fbOffset = contentTop * fb_pitch + contentLeft * bytes_per_pixel;

// Set baseAddr to framebuffer base (not offset!)
window->port.portBits.baseAddr = (Ptr)framebuffer;
```

**Critical Issue:** Code calculates fbOffset but NEVER USES IT!
```c
// Should be: window->port.portBits.baseAddr = (Ptr)framebuffer + fbOffset;
// But actually: window->port.portBits.baseAddr = (Ptr)framebuffer;
```

**Consequence:** When BeginUpdate does:
```c
pixels[y * (fb_pitch / 4) + x] = 0xFFFFFFFF;
```

It fills pixel (y,x) relative to framebuffer start (0,0), NOT relative to content area!
This causes filling at WRONG screen position IF portBits.bounds is supposed to map to a different part of screen.

### Issue #6: Pixel Drawing in BeginUpdate Using Wrong Coordinates

**Location:** WindowEvents.c lines 536-552
```c
// Erase update region to window background
if (theWindow->port.portBits.baseAddr) {
    extern uint32_t fb_pitch;
    uint32_t bytes_per_pixel = 4;
    
    // Get window dimensions from portRect (LOCAL coords)
    Rect portRect = theWindow->port.portRect;
    SInt16 width = portRect.right - portRect.left;
    SInt16 height = portRect.bottom - portRect.top;
    
    // Fill with opaque white
    UInt32* pixels = (UInt32*)theWindow->port.portBits.baseAddr;
    for (SInt16 y = 0; y < height; y++) {
        for (SInt16 x = 0; x < width; x++) {
            pixels[y * (fb_pitch / bytes_per_pixel) + x] = 0xFFFFFFFF;
        }
    }
}
```

**CRITICAL BUG:** This code:
1. Gets portBits.baseAddr = full framebuffer
2. Iterates y from 0 to portRect.height
3. Accesses pixels[y * pitch + x]

This fills pixels (0,0) to (width, height) on the SCREEN, not at the window's actual position!

**FIX NEEDED:** Should use portBits.bounds offset or baseAddr offset to actual content area.

## 5. Chrome Rendering Issues

### Title Bar Rendering (WindowDisplay.c, DrawWindowFrame)

**Lines 476-791:** Directly manipulates framebuffer:
```c
Rect titleBar;
titleBar.left = frame.left + 1;     // frame from strucRgn (GLOBAL)
titleBar.top = frame.top + 1;       // GLOBAL coords

// Draw to framebuffer using GLOBAL coordinates
for (int y = titleBar.top; y < titleBar.bottom; y++) {
    for (int x = titleBar.left; x < titleBar.right; x++) {
        fb[y * pitch + x] = lightGrey;
    }
}
```

**This is CORRECT** - uses strucRgn (GLOBAL) and indexes framebuffer directly.

## 6. Summary of Coordinate System Design

### Working Correctly:
1. **Region calculations** - strucRgn and contRgn correctly calculated as GLOBAL coords
2. **Chrome drawing** - Uses strucRgn directly, accesses framebuffer with global coords
3. **Glyph transform** - Correctly converts global pen to local offset via portBits.bounds

### Problem Areas:
1. **BeginUpdate white fill** - Uses baseAddr (full framebuffer) with local (y,x) coordinates
   - SHOULD: `baseAddr[offset + y*pitch + x]` where offset = contentTop*pitch + contentLeft*4
   - OR: Use FillRgn with contRgn instead of direct pixel loop

2. **fbOffset calculation** - Calculated but not used
   - Line 832: `uint32_t fbOffset = ...` never assigned to baseAddr

3. **Mismatch in coordinate philosophy:**
   - Chrome drawing uses: strucRgn (GLOBAL) + framebuffer direct access (GLOBAL)
   - Content filling uses: portRect (LOCAL) + framebuffer (assumes GLOBAL offset!)

## 7. Likely Cause of Extra White Space at Top/Left

When a window is created at position (x, y):
1. **contentLeft = x + 1** (accounting for left border)
2. **contentTop = y + 20 + 1** (accounting for title bar + separator)
3. **portBits.bounds = (contentLeft, contentTop, contentLeft + width, contentTop + height)**
4. **BeginUpdate fills pixels (0..width, 0..height)** -- which is SCREEN (0,0)!

Result: **White rectangle appears at top-left (0,0) of screen, size (window.width, window.height)**
Actual content fills correctly because:
- Application draws to window with local coordinates
- QuickDraw converts to global: `global = local + portBits.bounds`
- Glyph drawing: `local = global - portBits.bounds`
- Works correctly!

But BeginUpdate's direct fill:
- Uses local coordinates (0..width, 0..height)  
- Directly indexes framebuffer without offset
- Fills at GLOBAL position (0,0) instead of (contentLeft, contentTop)

## 8. Recommended Fixes

1. **Fix BeginUpdate white fill:**
   ```c
   // Option A: Use portBits.bounds offset
   uint32_t fbOffset = contentTop * fb_pitch + contentLeft * bytes_per_pixel;
   UInt32* pixels = (UInt32*)(theWindow->port.portBits.baseAddr + fbOffset);
   
   // Option B: Use QuickDraw FillRgn instead of direct pixel loop
   FillRgn(window->contRgn, &qd.white);
   ```

2. **Ensure portBits.baseAddr includes offset if using local coords:**
   ```c
   window->port.portBits.baseAddr = (Ptr)framebuffer + fbOffset;
   // Then local (y,x) access works: pixels[y*pitch + x]
   ```

3. **Or use global coordinates throughout:**
   ```c
   // Don't use portRect at all in BeginUpdate
   // Use contRgn (GLOBAL) instead
   ```

