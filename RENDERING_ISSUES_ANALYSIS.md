# Rendering Issues Investigation Report

## Issue 1: DESKTOP ICONS DISAPPEARING When Closing a Window

### Root Cause Analysis

**Location**: `/home/k/iteration2/src/WindowManager/WindowDisplay.c` - Lines 100-157 in `PaintOne()` function

**Problem**: 
The desktop window (which displays icons) is being erased with WHITE fill when any window needs repainting, causing desktop icons to vanish.

**Specific Code Issue**:
```c
// Lines 135-157 in WindowDisplay.c - PaintOne()
/* EXCEPTION: Skip filling windows with refCon=0 (desktop background window)
 * as filling it with white would erase desktop icons */
...
if (window->refCon != 0) {
    extern void FillRgn(RgnHandle rgn, const Pattern* pat);
    extern QDGlobals qd;
    FillRgn(window->contRgn, &qd.white);  // This fills with WHITE
}
```

**Why Icons Disappear**:
1. When a window closes, `HideWindow()` is called (line 1076)
2. `HideWindow()` calls `EraseRgn(clobberedRgn)` (line 1132) to erase the window area
3. Then it calls `PaintBehind(window->nextWindow, clobberedRgn)` (line 1147)
4. `PaintBehind()` iterates through ALL visible windows and calls `PaintOne()` on each (line 235)
5. **CRITICAL BUG**: When `PaintOne()` is called on the desktop window (refCon=0), the code at line 152 checks:
   ```c
   if (window->refCon != 0) {
       FillRgn(window->contRgn, &qd.white);  // Fills with WHITE!
   }
   ```
   This should SKIP filling for refCon=0, but the logic only prevents the fill.
   
6. **ACTUAL ISSUE**: The desktop window's visible region is NOT being erased with the desktop pattern - it's left untouched
7. BUT then `PaintBehind()` does NOT call the desktop hook `g_deskHook()` to redraw desktop icons!
8. Result: Desktop area is blank/white and icons are missing

**Secondary Issue**: 
- Line 1147 in `HideWindow()` calls `PaintBehind(window->nextWindow, ...)` which starts AFTER the hidden window
- If the desktop window is the only window left, it gets skipped entirely!
- Desktop icons never get redrawn because `g_deskHook()` is never called

**Line Numbers in WindowDisplay.c**:
- `HideWindow()`: Lines 1076-1157
- `PaintBehind()`: Lines 177-271
- `PaintOne()`: Lines 100-175
- Desktop fill check: Lines 135-157
- `EraseRgn()` call: Line 1132
- `PaintBehind()` call: Line 1147

---

## Issue 2: DROPDOWN MENUS APPEARING BLANK (White) Instead of Showing Menu Items

### Root Cause Analysis

**Location**: Multiple locations in menu rendering code

**Problem A - Menu Item Coordinates Wrong**:
**File**: `/home/k/iteration2/src/MenuManager/MenuDisplay.c` - Lines 1002-1060

**Specific Code Issue**:
```c
// Line 1027 - DrawMenuItemTextInternal()
short textY = itemRect->top + ((itemRect->bottom - itemRect->top) + 9) / 2;
```

**Why It's Wrong**:
- This formula calculates Y position INCORRECTLY for menu items
- Formula: `itemRect->top + (height + 9) / 2` 
- For itemRect height = 16: `top + (16 + 9) / 2 = top + 12.5 = top + 12`
- But this should be `top + height/2 + baseline_offset`
- The "+9" offset is arbitrary and wrong - causes text drawn at wrong Y coordinate
- Text gets drawn BELOW or ABOVE the menu item bounds where it's not visible

**Why Menu Appears Blank**:
1. When menu is drawn, `DrawMenu()` is called (MenuDisplay.c line 420)
2. This calls `DrawMenuItem()` for each item (line 473)
3. `DrawMenuItem()` calls `DrawMenuItemTextInternal()` (line 562)
4. The text position calculation uses the wrong Y coordinate (line 1027)
5. Text is drawn in the wrong location - outside visible menu bounds
6. Menu appears completely white with no text visible

**Problem B - Port Not Set Correctly**:
**Location**: `/home/k/iteration2/src/MenuManager/MenuDisplay.c` - Lines 160-285 in `DrawMenuTitle()`

**Specific Code Issue**:
```c
// Lines 189-192 - pnLoc reset attempt
if (qd.thePort) {
    qd.thePort->pnLoc.h = 0;
    qd.thePort->pnLoc.v = 0;
}
```

**Why It's Wrong**:
- The menu bar port (WMgrPort) has `portBits.bounds` set to GLOBAL screen coordinates
- BUT menu items are being drawn using coordinates relative to the menu rect
- The `pnLoc` position is not being reset BEFORE every MoveTo call
- **Critical Issue**: When `DrawString()` is called at line 1054, the text may be drawn at wrong position if pnLoc wasn't properly set by previous operations

**Problem C - Coordinate Transform Missing**:
**Location**: `/home/k/iteration2/src/MenuManager/MenuDisplay.c` - Lines 713-723 in `CalcMenuItemRect()`

**Issue**: 
```c
// Lines 719-722 - Menu item rect calculation
itemRect->left = menuRect->left + 4;     // Local offset only
itemRect->right = menuRect->right - 4;
itemRect->top = menuRect->top + 4 + (item - 1) * menuItemStdHeight;
itemRect->bottom = itemRect->top + menuItemStdHeight;
```

The rect calculation doesn't account for the port's coordinate system transformation when the menu is drawn in a port with offset bounds.

**Problem D - Port State Not Initialized for Drawing**:
**Location**: `/home/k/iteration2/src/MenuManager/MenuDisplay.c` - Lines 420-482 in `DrawMenu()`

**Issue**:
- When `DrawMenu()` is called, no explicit port setup happens
- Font/text settings may not be initialized
- No explicit MoveTo(0,0) or coordinate reset before drawing first item
- Leftover pen location from previous operations can affect text positioning

**Lines Affected**:
- `DrawMenu()`: Lines 420-482
- `DrawMenuItemTextInternal()`: Lines 1002-1060
- `CalcMenuItemRect()`: Lines 713-723
- `DrawMenuTitle()`: Lines 160-285
- Text position calculation with wrong formula: Line 1027

---

## Summary of Root Causes

### Issue 1 - Desktop Icons Vanish:
**Root Cause**: `PaintBehind()` does not call `g_deskHook()` to redraw desktop icons after window close
- Desktop window is skipped in PaintBehind iteration when it's the only window left
- OR desktop window is painted but icons aren't redrawn

**Root Cause Location**: 
- `WindowDisplay.c:1147` - `PaintBehind()` missing desktop hook call
- `WindowDisplay.c:177-271` - `PaintBehind()` doesn't handle desktop window specially

### Issue 2 - Menus Appear Blank:
**Root Cause A**: Menu item text Y coordinate calculation formula is wrong (line 1027)
- Formula adds arbitrary "+9" offset causing text to draw outside visible area
- Text gets drawn at wrong Y position, making it invisible

**Root Cause B**: Port and pen position not properly managed before text drawing
- `pnLoc` may be in wrong state from previous operations
- No guarantee that text will be drawn at correct screen location

**Root Cause C**: Coordinate transforms not applied correctly
- Menu rectangles calculated without accounting for port bounds offset
- Text coordinates calculated in wrong coordinate space

---

## Recommended Fixes

### For Desktop Icons Disappearing:

**Fix in PaintBehind() - WindowDisplay.c around line 220**:
```c
paint_windows:

// Add special handling for desktop window FIRST
// Desktop should be drawn even if it's not in the visible window list
extern DeskHookProc g_deskHook;
if (g_deskHook) {
    // Redraw desktop background and icons for all visible areas
    Rect screenRect;
    SetRect(&screenRect, 0, 20, 640, 480);  // Entire screen except menu bar
    RgnHandle screenRgn = NewRgn();
    RectRgn(screenRgn, &screenRect);
    g_deskHook(screenRgn);
    DisposeRgn(screenRgn);
}

// Then paint windows on top
for (int i = count - 1; i >= 0; i--) {
    // ... existing window paint code ...
}
```

### For Menu Items Appearing Blank:

**Fix 1 - Correct Y coordinate formula in DrawMenuItemTextInternal() - MenuDisplay.c line 1027**:
```c
// WRONG:
short textY = itemRect->top + ((itemRect->bottom - itemRect->top) + 9) / 2;

// CORRECT:
short textY = itemRect->top + ((itemRect->bottom - itemRect->top) / 2) + 4;
// Or better - use font metrics for proper baseline:
short textY = itemRect->top + 12;  // Baseline at 12 pixels from top for 16px item
```

**Fix 2 - Initialize port state in DrawMenu() - MenuDisplay.c around line 428**:
```c
// After line 427, add:
// Set up drawing context
GrafPtr savePort;
GetPort(&savePort);
SetPort(qd.thePort);  // Draw in screen port

// Reset pen position and font setup
MoveTo(0, 0);
TextFont(chicagoFont);
TextSize(12);
TextFace(normal);
PenNormal();

// ... rest of DrawMenu code ...
```

**Fix 3 - Ensure coordinate transforms in CalcMenuItemRect() - MenuDisplay.c line 719**:
```c
// Verify that menuRect is in correct coordinate space before using
// If menuRect is in global screen coords and port uses different origin,
// need to transform itemRect accordingly
itemRect->left = menuRect->left + 4;
itemRect->right = menuRect->right - 4;
itemRect->top = menuRect->top + 4 + (item - 1) * menuItemStdHeight;
itemRect->bottom = itemRect->top + menuItemStdHeight;
```

**Fix 4 - Reset pen location before each text draw in DrawMenuItemTextInternal() - MenuDisplay.c line 1038**:
```c
// After line 1037, ensure clean state:
MoveTo(textX, textY);

// Additionally, at start of function, capture current port state:
GrafPtr currentPort = NULL;
GetPort(&currentPort);
if (currentPort && currentPort->pnLoc.h != textX) {
    // pnLoc was in wrong position, reset it
    MoveTo(textX, textY);
}
```

