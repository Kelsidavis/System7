# Window Dragging and Repositioning Analysis

## Document Overview

This analysis explores the window manager's drag, repositioning, and bounds update mechanisms to identify potential issues with white infill appearing outside window bounds after drag operations.

**Repository:** /home/k/iteration2
**Key Files Examined:**
- `src/WindowManager/WindowDragging.c` - DragWindow implementation
- `src/WindowManager/WindowManagerCore.c` - MoveWindow, window initialization
- `src/WindowManager/WindowEvents.c` - BeginUpdate, region management
- `src/WindowManager/WindowDisplay.c` - Rendering after position change
- `src/WindowManager/WindowResizing.c` - SizeWindow, GrowWindow
- `src/QuickDraw/Coordinates.c` - Coordinate system documentation

---

## 1. DragWindow Implementation

**File:** `src/WindowManager/WindowDragging.c:260-591`

### How DragWindow Works

```
DragWindow(theWindow, startPt, boundsRect)
  1. Extract window bounds from strucRgn (GLOBAL coordinates)
  2. Enter modal drag loop with XOR outline feedback
  3. While mouse button down:
     - GetMouse() returns GLOBAL coordinates
     - Calculate new window position based on offset
     - Constrain position to dragBounds
     - Draw XOR outline at new position (by InvertRect XOR twice)
  4. After mouse up:
     - Call MoveWindow(theWindow, dragOutline.left, dragOutline.top, false)
     - Invalidate old and new positions
     - Repaint regions
```

### XOR Feedback Mechanism

**Lines 394-417: Outline Update Loop**
```c
if (outlineDrawn) {
    InvertRect(&dragOutline);      // Erase old by XOR
    QDPlatform_FlushScreen();
}

dragOutline.left = newLeft;
dragOutline.top = newTop;
dragOutline.right = newLeft + windowWidth;
dragOutline.bottom = newTop + windowHeight;

InvertRect(&dragOutline);          // Draw new
QDPlatform_FlushScreen();
```

### Critical Issue: Coordinate System During Drag

**Current State (Post-commit e87abba):**
- `startPt` and mouse coordinates from `GetMouse()` are in GLOBAL screen coordinates
- `frameG` extracted from `strucRgn->rgnBBox` is in GLOBAL coordinates
- XOR rectangles (`dragOutline`) are in GLOBAL coordinates
- This is **CORRECT**

---

## 2. MoveWindow Implementation

**File:** `src/WindowManager/WindowDragging.c:142-254`

### How MoveWindow Works

```
MoveWindow(theWindow, hGlobal, vGlobal, front)
  1. Get current global bounds from strucRgn (GLOBAL coords)
  2. Calculate delta: (hGlobal - left), (vGlobal - top)
  3. Validate new position
  4. Offset strucRgn, contRgn, updateRgn by delta
  5. **CRITICAL**: Update portBits.baseAddr with new framebuffer offset
  6. Update portBits.bounds (MUST STAY IN LOCAL COORDS: 0,0,width,height)
  7. Move platform window
  8. Invalidate old and new screen regions
```

### Critical Point: BaseAddr Update

**Lines 202-222: Direct Framebuffer Offset**
```c
/* CRITICAL: Update portBits.baseAddr for Direct Framebuffer approach */
if (theWindow->contRgn && *(theWindow->contRgn)) {
    Rect newContentRect = (*(theWindow->contRgn))->rgnBBox;
    
    extern void* framebuffer;
    extern uint32_t fb_pitch;
    uint32_t bytes_per_pixel = 4;
    uint32_t fbOffset = newContentRect.top * fb_pitch + 
                       newContentRect.left * bytes_per_pixel;
    
    theWindow->port.portBits.baseAddr = 
        (Ptr)((char*)framebuffer + fbOffset);
}

/* NOTE: Do NOT modify portBits.bounds - it must stay (0,0,width,height)! */
```

**Architecture:**
- `baseAddr` = framebuffer + offset to window content area
- `bounds` = (0, 0, contentWidth, contentHeight) in LOCAL coordinates
- When drawing code accesses pixel at (x,y), it uses: `baseAddr + (y * pitch + x * 4)`
- This automatically maps LOCAL coordinates to correct framebuffer position

---

## 3. Window Bounds and Region Structures

### Window Port Setup (Initialization)

**File:** `src/WindowManager/WindowManagerCore.c:725-876`

**portBits Fields:**
```c
window->port.portBits.baseAddr = (Ptr)framebuffer;  // Points to framebuffer start
window->port.portBits.bounds = {contentLeft, contentTop, 
                                contentLeft+w, contentTop+h};  // GLOBAL coords
window->port.portBits.rowBytes = fb_pitch | 0x8000;  // Pitch with 32-bit flag
```

**Wait - There's Ambiguity Here!**

Looking at lines 839-840 in WindowManagerCore.c:
```c
SetRect(&window->port.portBits.bounds, contentLeft, contentTop,
        contentLeft + contentWidth, contentTop + contentHeight);
```

But commit ee72cc4 says it changed from LOCAL to GLOBAL!

**ISSUE FOUND:** After commit ee72cc4, `portBits.bounds` was changed to GLOBAL coordinates for content area, but `baseAddr` still points to framebuffer base. 

This means:
- Drawing code does: `pixel_offset = (y - bounds.top) * pitch + (x - bounds.left) * 4`
- Then indexes into baseAddr, which is the full framebuffer
- **This is CORRECT** for the current architecture

### Window Regions

**Three regions per window:**
1. **strucRgn** - Structure region (frame + chrome + content) in GLOBAL coords
2. **contRgn** - Content region only in GLOBAL coords  
3. **updateRgn** - Areas needing update in GLOBAL coords

All three are updated by offset in MoveWindow.

---

## 4. BeginUpdate Fill Operation

**File:** `src/WindowManager/WindowEvents.c:482-563`

### The Recent Fix (e87abba)

This is the CRITICAL section that fills window with white:

```c
/* Erase update region to window background (direct framebuffer) */
if (theWindow->port.portBits.baseAddr) {
    extern uint32_t fb_pitch;
    uint32_t bytes_per_pixel = 4;

    /* Get window dimensions from portRect (LOCAL coords) */
    Rect portRect = theWindow->port.portRect;
    SInt16 width = portRect.right - portRect.left;
    SInt16 height = portRect.bottom - portRect.top;

    /* Calculate offset to window's position in framebuffer */
    Rect bounds = theWindow->port.portBits.bounds;
    uint32_t fbOffset = (uint32_t)bounds.top * fb_pitch + 
                       (uint32_t)bounds.left * bytes_per_pixel;
    UInt32* windowPixels = (UInt32*)((uint8_t*)theWindow->port.portBits.baseAddr + fbOffset);

    /* Fill window content area with white */
    for (SInt16 y = 0; y < height; y++) {
        for (SInt16 x = 0; x < width; x++) {
            windowPixels[y * (fb_pitch / bytes_per_pixel) + x] = 0xFFFFFFFF;
        }
    }
}
```

**This was fixed in e87abba** to calculate the correct framebuffer offset using `bounds.top` and `bounds.left`, which after ee72cc4 are in GLOBAL coordinates.

**Current Status (Post-fix):** The fill position should be correct now.

---

## 5. Issue Analysis: Potential White Infill Problems

### Scenario: Window Dragged

1. **Initial State:**
   - Window at (100, 100) global
   - strucRgn = (100, 100, 300, 300)
   - contRgn = (101, 121, 299, 299)
   - portBits.bounds = (101, 121, 299, 299) 
   - portBits.baseAddr = framebuffer + (121 * pitch + 101 * 4)

2. **Drag Operation:**
   - User drags window to (200, 150)
   - XOR outline drawn at correct screen position ✓
   - Mouse button released

3. **MoveWindow Called:**
   - Calculate offset: deltaH = 200-100=100, deltaV = 150-100=50
   - Offset regions: strucRgn becomes (200,150,400,400), etc.
   - Calculate new fbOffset: 150*pitch + 200*4
   - Update baseAddr = framebuffer + (150*pitch + 200*4)

4. **BeginUpdate Fill:**
   - Uses updated bounds (200, 150, 398, 398) ✓
   - Calculates fbOffset = 150*pitch + 200*4 ✓
   - Fills correct framebuffer region ✓

### Potential Issues Identified

#### Issue 1: Race Condition in Region Updates

In MoveWindow (lines 192-200):
```c
if (theWindow->strucRgn) {
    Platform_OffsetRgn(theWindow->strucRgn, deltaH, deltaV);
}
if (theWindow->contRgn) {
    Platform_OffsetRgn(theWindow->contRgn, deltaH, deltaV);
}
```

Then later (lines 209-217):
```c
if (theWindow->contRgn && *(theWindow->contRgn)) {
    Rect newContentRect = (*(theWindow->contRgn))->rgnBBox;
    /* ... calculate fbOffset ... */
}
```

**Problem:** `Platform_OffsetRgn` updates the region, then we read its bounds. If there's an issue in how regions are offset, the bounds will be wrong.

#### Issue 2: portBits.bounds Not Updated on Resize/Move

Looking at MoveWindow, there's a comment:
```c
/* NOTE: Do NOT modify portBits.bounds - it must stay (0,0,width,height)!
 * Only baseAddr needs to be updated. */
```

**But this contradicts the window initialization where bounds are set to GLOBAL!**

After commit ee72cc4, bounds ARE in GLOBAL coordinates, so they SHOULD be updated when window moves.

**CRITICAL BUG FOUND:** 
- In MoveWindow, portBits.bounds is NOT being updated
- But initialization sets it to GLOBAL coordinates
- This means bounds become stale after first move

#### Issue 3: SizeWindow Doesn't Update portBits.bounds Offset

Looking at SizeWindow (lines 108-250):

```c
/* CRITICAL: Update portBits.bounds for Direct Framebuffer approach */
SetRect(&(theWindow)->port.portBits.bounds, 0, 0, w, h);
```

**This sets bounds to LOCAL coordinates!** But after ee72cc4, they should be in GLOBAL!

The code doesn't update bounds to include window position.

#### Issue 4: Stale Window Position During Repaint

In DragWindow after MoveWindow (lines 470-530):

```c
MoveWindow(theWindow, dragOutline.left, dragOutline.top, false);
CalcVis(theWindow);
// ... create regions ...
PaintBehind(theWindow->nextWindow, uncoveredRgn);
PaintOne(theWindow, NULL);
```

If PaintOne calls DrawWindowFrame, which uses strucRgn for positioning:

```c
// From WindowDisplay.c:432
Rect frame = (*window->strucRgn)->rgnBBox;
```

This should be correct since strucRgn was updated.

---

## 6. Paint/Redraw Pipeline After Move

### PaintOne Function

**File:** `src/WindowManager/WindowDisplay.c:100-175`

```c
PaintOne(WindowPtr window, RgnHandle clobberedRgn) {
    // ... fill contRgn with white using FillRgn
    if (window->refCon != 0) {
        FillRgn(window->contRgn, &qd.white);
    }
    
    // Draw chrome
    DrawWindowFrame(window);
    DrawWindowControls(window);
}
```

**Issue:** FillRgn is called on contRgn in WMgrPort context. This should work correctly since contRgn is in GLOBAL coordinates.

### DrawWindowFrame Function

**Lines 385-794:**

Uses strucRgn directly:
```c
Rect frame = (*window->strucRgn)->rgnBBox;
```

Then draws using framebuffer direct writes (lines 445-688):
```c
uint32_t* fb = (uint32_t*)framebuffer;
int pitch = fb_pitch / 4;
// ... draws using frame.left, frame.top, frame.right, frame.bottom directly
```

**This is CORRECT** - using global coordinates directly into framebuffer.

---

## 7. Root Cause Analysis

### The Architecture

**GLOBAL Coordinate System:**
- `strucRgn`, `contRgn`, `updateRgn` are all in GLOBAL screen coordinates
- Drawing operations that use these regions are correct

**Direct Framebuffer Approach:**
- `portBits.baseAddr` = framebuffer base address
- `portBits.bounds` = GLOBAL content coordinates (after ee72cc4)
- Drawing code: `offset = (y - bounds.top) * pitch + (x - bounds.left) * 4`

### Found Inconsistency

**In SizeWindow (line 146):**
```c
SetRect(&(theWindow)->port.portBits.bounds, 0, 0, w, h);
```

This sets bounds to LOCAL (0,0,width,height) but after ee72cc4 they should be GLOBAL!

**Expected (after ee72cc4):**
```c
if (oldStrucRgn && *(oldStrucRgn)) {
    Rect oldStrucRect = (**oldStrucRgn).rgnBBox;
    SInt16 contentLeft = oldStrucRect.left + kBorder;
    SInt16 contentTop = oldStrucRect.top + kTitleBar + kSeparator;
    
    SetRect(&(theWindow)->port.portBits.bounds, 
            contentLeft, contentTop,
            contentLeft + w, contentTop + h);
}
```

**Missing in MoveWindow:**
portBits.bounds is NOT updated when window moves, even though:
1. It's set to GLOBAL coordinates at initialization  
2. Regions are offset
3. baseAddr is updated

This creates a stale bounds field.

### Issue Impact

**Scenario with the bugs:**

1. Window created at (100, 100): bounds = (101, 121, 299, 299) ✓
2. Window resized: bounds set to (0, 0, 199, 179) ✗ (LOCAL instead of GLOBAL)
3. Window moved to (200, 150): bounds NOT updated, still (0, 0, 199, 179) ✗
4. BeginUpdate uses: fbOffset = 0*pitch + 0*4 = 0 ✗
5. **Fill happens at screen (0,0) instead of (200, 150)** ✗

---

## 8. Coordinate System Documentation

**From `src/QuickDraw/Coordinates.c:65-117`:**

```c
void LocalToGlobal(Point *pt) {
    if (g_currentPort) {
        // With Direct Framebuffer, bounds is (0,0,w,h)
        // so this is effectively a no-op
        pt->h += g_currentPort->portBits.bounds.left;
        pt->v += g_currentPort->portBits.bounds.top;
    }
}

void GlobalToLocalWindow(WindowPtr window, Point *pt) {
    if (window->contRgn && *(window->contRgn)) {
        Rect contentGlobal = (*(window->contRgn))->rgnBBox;
        pt->h -= contentGlobal.left;
        pt->v -= contentGlobal.top;
    }
}
```

The comment is INCORRECT! It says "bounds is (0,0,w,h)" but after ee72cc4, bounds ARE GLOBAL!

This comment indicates the actual design intent and existing confusion in the codebase.

---

## 9. Issues Summary

### Critical Bugs Found

| # | File | Lines | Issue | Impact |
|---|------|-------|-------|--------|
| 1 | WindowResizing.c | 146 | `portBits.bounds` set to LOCAL (0,0,w,h) instead of GLOBAL | After resize, bounds are stale |
| 2 | WindowDragging.c | 192-217 | `portBits.bounds` never updated during MoveWindow | After drag, bounds are stale |
| 3 | QuickDraw/Coordinates.c | 70-76 | Comment says bounds is (0,0,w,h) but it's actually GLOBAL | Developer confusion |
| 4 | WindowManagerCore.c | 839-840 | Comment doesn't explain post-ee72cc4 GLOBAL bounds | Developer confusion |
| 5 | WindowEvents.c | 536-558 | BeginUpdate assumes portBits.bounds is correct | Works only if bounds updated correctly |

### Symptom: White Infill Outside Window

When window is:
1. **Resized:** bounds set to LOCAL, then becomes stale after future moves
2. **Moved:** bounds not updated, stays at old position
3. **BeginUpdate fill:** uses stale bounds, fills wrong framebuffer location

Result: White pixels appear at screen (0,0) or old window position

---

## 10. Required Fixes

### Fix 1: SizeWindow - Update bounds to GLOBAL

**File:** `src/WindowManager/WindowResizing.c:146`

Replace:
```c
SetRect(&(theWindow)->port.portBits.bounds, 0, 0, w, h);
```

With:
```c
if (oldStrucRgn && *(oldStrucRgn)) {
    Rect oldStrucRect = (**oldStrucRgn).rgnBBox;
    SInt16 contentLeft = oldStrucRect.left + kBorder;
    SInt16 contentTop = oldStrucRect.top + kTitleBar + kSeparator;
    
    SetRect(&(theWindow)->port.portBits.bounds, 
            contentLeft, contentTop,
            contentLeft + w, contentTop + h);
}
```

### Fix 2: MoveWindow - Update bounds on move

**File:** `src/WindowManager/WindowDragging.c:220-222`

After updating baseAddr, add:
```c
/* CRITICAL: Update portBits.bounds to reflect new GLOBAL position */
if (theWindow->contRgn && *(theWindow->contRgn)) {
    Rect newContentRect = (*(theWindow->contRgn))->rgnBBox;
    SetRect(&(theWindow)->port.portBits.bounds,
            newContentRect.left, newContentRect.top,
            newContentRect.right, newContentRect.bottom);
}
```

### Fix 3: Document coordinate system

**File:** `src/QuickDraw/Coordinates.c:70-76`

Update comments:
```c
void LocalToGlobal(Point *pt) {
    if (g_currentPort) {
        /* NOTE (Post-ee72cc4): portBits.bounds is in GLOBAL screen coordinates.
         * For windows, bounds represents the content area in global coords.
         * For drawing, we use bounds to map local to global. */
        pt->h += g_currentPort->portBits.bounds.left;
        pt->v += g_currentPort->portBits.bounds.top;
    }
}
```

### Fix 4: Add verification in BeginUpdate

**File:** `src/WindowManager/WindowEvents.c:545-550`

After calculating fbOffset, add validation:
```c
/* Verify offset is within framebuffer bounds */
extern uint32_t fb_width, fb_height;
if (fbOffset < 0 || fbOffset > ((uint32_t)fb_height * fb_pitch)) {
    serial_printf("[ERROR] BeginUpdate: Invalid fbOffset=0x%x for bounds (%d,%d,%d,%d)\n",
                  (unsigned)fbOffset, bounds.left, bounds.top, bounds.right, bounds.bottom);
    return;  // Abort fill to prevent corruption
}
```

---

## Summary

The window manager's drag and reposition system has a **coordinate system mismatch** bug:

1. Regions (strucRgn, contRgn, updateRgn) are correctly in GLOBAL coordinates
2. After commit ee72cc4, portBits.bounds was intended to be GLOBAL coordinates  
3. **But SizeWindow still sets bounds to LOCAL**, and
4. **MoveWindow never updates bounds after repositioning**

This causes `portBits.bounds` to become stale, leading to:
- Incorrect framebuffer offset calculation in BeginUpdate
- White fill appearing at wrong screen position
- Visual artifacts after drag/resize operations

**Recommended Action:** Apply Fixes 1-3 immediately, and Fix 4 for defensive validation.

