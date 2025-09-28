# T-WM-wdef-01: WDEF Hit Test Matrix

**Provenance**: IM:Windows Vol I pp. 2-88 to 2-95 - "Window Definition Procedures"

## Test Description

This test validates the Window Definition Procedure (WDEF) hit testing by sweeping
a grid of points across a window and verifying that FindWindow returns the correct
window part codes for each region.

## Window Part Codes

From IM:Windows Vol I p. 2-90:

- `wNoHit` (0): Not in window
- `wInContent` (1): In content region
- `wInDrag` (2): In drag region (title bar)
- `wInGrow` (3): In grow box
- `wInGoAway` (4): In close box
- `wInZoomIn` (5): In zoom box (unzoomed state)
- `wInZoomOut` (6): In zoom box (zoomed state)

## Test Setup

Create a standard document window:
```c
Rect bounds = {50, 50, 350, 550};  // Height: 300px, Width: 500px
WindowPtr w = NewWindow(NULL, &bounds, "\pTest Window", true,
                        documentProc, (WindowPtr)-1L, true, 0L);
```

## Test Grid

Sweep a 5x5 grid across key window regions:

### Region 1: Close Box (Go-Away Box)
- Location: Upper-left corner of title bar
- Expected: `wInGoAway` (4)
- Test Points:
  - (60, 58)  - Center of close box
  - (55, 55)  - Top-left of close box
  - (65, 61)  - Bottom-right of close box

### Region 2: Title Bar Drag Area
- Location: Center of title bar
- Expected: `wInDrag` (2)
- Test Points:
  - (200, 58) - Center of title bar
  - (150, 58) - Left of center
  - (250, 58) - Right of center

### Region 3: Zoom Box
- Location: Upper-right corner of title bar
- Expected: `wInZoomIn` (5) when not zoomed, `wInZoomOut` (6) when zoomed
- Test Points:
  - (535, 58) - Center of zoom box
  - (530, 55) - Top-left of zoom box
  - (540, 61) - Bottom-right of zoom box

### Region 4: Content Area
- Location: Below title bar, within window bounds
- Expected: `wInContent` (1)
- Test Points:
  - (200, 100) - Center of content
  - (100, 100) - Left side of content
  - (400, 200) - Right side of content
  - (200, 300) - Bottom of content

### Region 5: Grow Box (if present)
- Location: Lower-right corner
- Expected: `wInGrow` (3)
- Test Points:
  - (540, 340) - Center of grow box
  - (535, 335) - Upper-left of grow box
  - (545, 345) - Lower-right of grow box

### Region 6: Outside Window
- Location: Beyond window bounds
- Expected: `wNoHit` (0)
- Test Points:
  - (40, 40)   - Above and left
  - (560, 40)  - Above and right
  - (40, 360)  - Below and left
  - (560, 360) - Below and right

## Test Implementation

```c
typedef struct {
    Point pt;
    short expectedPart;
    const char* description;
} HitTestCase;

HitTestCase tests[] = {
    // Close box
    {{60, 58}, wInGoAway, "Close box center"},
    {{55, 55}, wInGoAway, "Close box top-left"},
    {{65, 61}, wInGoAway, "Close box bottom-right"},

    // Title bar
    {{200, 58}, wInDrag, "Title bar center"},
    {{150, 58}, wInDrag, "Title bar left"},
    {{250, 58}, wInDrag, "Title bar right"},

    // Zoom box
    {{535, 58}, wInZoomIn, "Zoom box center (unzoomed)"},
    {{530, 55}, wInZoomIn, "Zoom box top-left"},
    {{540, 61}, wInZoomIn, "Zoom box bottom-right"},

    // Content
    {{200, 100}, wInContent, "Content center"},
    {{100, 100}, wInContent, "Content left"},
    {{400, 200}, wInContent, "Content right"},
    {{200, 300}, wInContent, "Content bottom"},

    // Grow box
    {{540, 340}, wInGrow, "Grow box center"},
    {{535, 335}, wInGrow, "Grow box upper-left"},
    {{545, 345}, wInGrow, "Grow box lower-right"},

    // Outside
    {{40, 40}, wNoHit, "Outside upper-left"},
    {{560, 40}, wNoHit, "Outside upper-right"},
    {{40, 360}, wNoHit, "Outside lower-left"},
    {{560, 360}, wNoHit, "Outside lower-right"}
};

for (int i = 0; i < sizeof(tests)/sizeof(tests[0]); i++) {
    short part;
    WindowPtr hitWindow = FindWindow(tests[i].pt, &part);

    if (part != tests[i].expectedPart) {
        printf("FAIL: %s - expected %d, got %d\n",
               tests[i].description, tests[i].expectedPart, part);
    }
}
```

## WDEF Message Protocol

The WDEF is called with message `wHit` (1) to perform hit testing:

```c
long result = CallWDEF(window, wHit, globalPoint);
// result = window part code (wInGoAway, wInDrag, etc.)
```

## Expected Behavior

1. **Title Bar Regions**: Close box, zoom box, and drag area all distinct
2. **Content Region**: Everything below title bar, above grow box
3. **Grow Box**: Lower-right 15x15 pixel area (if window is growable)
4. **Precision**: Hit testing accurate to pixel level
5. **WDEF Independence**: Different window types (document, dialog, etc.) have different hit test logic

## Validation Points

1. Close box hit test returns `wInGoAway` only within close box bounds
2. Zoom box hit test returns `wInZoomIn`/`wInZoomOut` only within zoom box bounds
3. Title bar drag area returns `wInDrag` excluding close/zoom boxes
4. Content region returns `wInContent` for all points in content area
5. Grow box returns `wInGrow` only when window is growable
6. Points outside window return `wNoHit`

## Test ID

- **ID**: T-WM-wdef-01
- **Category**: Window Manager / WDEF / Hit Testing
- **Priority**: Critical
- **Status**: Specification Complete

## Related Tests

- T-WM-wdef-02: WDEF region calculation (wCalcRgns message)
- T-WM-wdef-03: WDEF frame drawing (wDraw message)
- T-WM-wdef-04: WDEF grow feedback (wGrow message)