/*
 * MacPaint_Rendering.c - UI Rendering and Display
 *
 * Implements all visual rendering for MacPaint:
 * - Paint window rendering (bitmap display)
 * - Dialog UI rendering (pattern/brush editors)
 * - Grid overlay
 * - Selection rectangle display
 * - Tool cursor management
 * - Toolbox and interface elements
 *
 * Uses System 7.1 QuickDraw for all rendering operations
 */

#include "SystemTypes.h"
#include "Apps/MacPaint.h"
#include "QuickDraw/QuickDraw.h"
#include "QuickDrawConstants.h"
#include "WindowManager/WindowManager.h"
#include "FontManager/FontManager.h"
#include "System71StdLib.h"
#include <string.h>

/*
 * RENDERING STATE
 */

typedef struct {
    int showGrid;               /* Grid overlay enabled */
    int gridSpacing;           /* Pixels between grid lines */
    int gridColor;             /* Grid line color */
    int fatBitsMode;           /* Fat Bits zoom display */
    int fatBitsZoom;           /* Zoom factor (2, 4, 8, etc) */
    int showSelectionRect;     /* Selection rectangle visible */
    Rect selectionRect;        /* Current selection bounds */
    int selectionMarching;     /* Marching ants animation */
} RenderState;

static RenderState gRenderState = {
    .showGrid = 0,
    .gridSpacing = 16,
    .gridColor = 0xCCCCCCCC,
    .fatBitsMode = 0,
    .fatBitsZoom = 2,
    .showSelectionRect = 0,
    .selectionRect = {0, 0, 0, 0},
    .selectionMarching = 0
};

/* External state from MacPaint modules */
extern BitMap gPaintBuffer;
extern WindowPtr gPaintWindow;
extern int gCurrentTool;

/*
 * UPDATE REGION TRACKING
 */

typedef struct {
    Rect paintRect;         /* Canvas area bounds */
    Rect toolboxRect;       /* Toolbox area bounds */
    Rect statusRect;        /* Status bar area bounds */
    int paintDirty;         /* Canvas needs redraw */
    int toolboxDirty;       /* Toolbox needs redraw */
    int statusDirty;        /* Status bar needs redraw */
} InvalidationState;

static InvalidationState gInvalidState = {
    .paintRect = {0, 0, 0, 0},
    .toolboxRect = {0, 0, 0, 0},
    .statusRect = {0, 0, 0, 0},
    .paintDirty = 0,
    .toolboxDirty = 0,
    .statusDirty = 0
};

/**
 * MacPaint_UpdateInvalidationRects - Update cached rectangle positions
 */
static void MacPaint_UpdateInvalidationRects(void)
{
    if (!gPaintWindow) {
        return;
    }

    GrafPtr port = GetWindowPort(gPaintWindow);
    if (!port) {
        return;
    }

    /* Paint canvas area (right side, minus left toolbox and bottom status) */
    gInvalidState.paintRect.left = port->portRect.left + MACPAINT_TOOLBOX_WIDTH;
    gInvalidState.paintRect.top = port->portRect.top;
    gInvalidState.paintRect.right = port->portRect.right;
    gInvalidState.paintRect.bottom = port->portRect.bottom - MACPAINT_STATUS_HEIGHT;

    /* Toolbox area (right side) */
    gInvalidState.toolboxRect.left = port->portRect.left;
    gInvalidState.toolboxRect.top = port->portRect.top;
    gInvalidState.toolboxRect.right = port->portRect.left + MACPAINT_TOOLBOX_WIDTH;
    gInvalidState.toolboxRect.bottom = port->portRect.bottom - MACPAINT_STATUS_HEIGHT;

    /* Status bar area (bottom) */
    gInvalidState.statusRect.left = port->portRect.left;
    gInvalidState.statusRect.top = port->portRect.bottom - MACPAINT_STATUS_HEIGHT;
    gInvalidState.statusRect.right = port->portRect.right;
    gInvalidState.statusRect.bottom = port->portRect.bottom;
}

/*
 * PAINT WINDOW RENDERING
 */

/**
 * MacPaint_RenderPaintBuffer - Draw the paint buffer to window
 * This is the main rendering routine called on every update
 */
void MacPaint_RenderPaintBuffer(void)
{
    if (!gPaintWindow) {
        return;
    }

    GrafPtr port = GetWindowPort(gPaintWindow);
    if (!port) {
        return;
    }

    SetPort(port);
    PenNormal();

    /* Render paint buffer using appropriate mode */
    if (gRenderState.fatBitsMode) {
        MacPaint_RenderFatBits();
    } else {
        /* Normal 1:1 rendering using CopyBits */
        Rect dstRect;
        MacPaint_GetPaintRect(&dstRect);
        CopyBits(&gPaintBuffer, &port->portBits,
                 &gPaintBuffer.bounds, &dstRect,
                 srcCopy, NULL);
    }

    /* Render overlay elements */
    if (gRenderState.showGrid) {
        MacPaint_DrawGridOverlay();
    }

    if (gRenderState.showSelectionRect) {
        MacPaint_DrawSelectionRectangle();
    }

    MacPaint_DrawToolbox();
    MacPaint_DrawStatusBar();
}

/**
 * MacPaint_RenderFatBits - Render in Fat Bits zoom mode
 * Shows each pixel as a larger block for pixel-level editing
 */
void MacPaint_RenderFatBits(void)
{
    GrafPtr port = GetWindowPort(gPaintWindow);
    if (!port) {
        return;
    }

    SetPort(port);
    int zoom = gRenderState.fatBitsZoom;
    int x, y;
    Rect pixelRect;
    Rect dstRect;
    MacPaint_GetPaintRect(&dstRect);

    PenNormal();
    PenSize(1, 1);

    /* Draw each pixel as a zoom block */
    for (y = 0; y < gPaintBuffer.bounds.bottom; y++) {
        for (x = 0; x < gPaintBuffer.bounds.right; x++) {
            pixelRect.top = dstRect.top + (y * zoom);
            pixelRect.left = dstRect.left + (x * zoom);
            pixelRect.bottom = pixelRect.top + zoom;
            pixelRect.right = pixelRect.left + zoom;

            if (MacPaint_PixelTrue(x, y, &gPaintBuffer)) {
                PaintRect(&pixelRect);
            } else {
                FrameRect(&pixelRect);
            }
        }
    }

    /* Draw pixel grid lines */
    PenMode(patXor);
    for (x = 0; x <= gPaintBuffer.bounds.right; x++) {
        int screenX = dstRect.left + (x * zoom);
        MoveTo(screenX, dstRect.top);
        LineTo(screenX, dstRect.bottom);
    }
    for (y = 0; y <= gPaintBuffer.bounds.bottom; y++) {
        int screenY = dstRect.top + (y * zoom);
        MoveTo(dstRect.left, screenY);
        LineTo(dstRect.right, screenY);
    }
    PenNormal();
}

/*
 * GRID RENDERING
 */

/**
 * MacPaint_DrawGridOverlay - Draw grid overlay on canvas
 */
void MacPaint_DrawGridOverlay(void)
{
    if (!gRenderState.showGrid || !gPaintWindow) {
        return;
    }

    GrafPtr port = GetWindowPort(gPaintWindow);
    if (!port) {
        return;
    }

    SetPort(port);

    Rect paintRect;
    MacPaint_GetPaintRect(&paintRect);

    /* Use XOR pen mode so grid is visible on any background */
    PenMode(patXor);
    PenSize(1, 1);

    int spacing = gRenderState.gridSpacing;

    /* Draw vertical grid lines */
    for (int x = 0; x < paintRect.right; x += spacing) {
        MoveTo(paintRect.left + x, paintRect.top);
        LineTo(paintRect.left + x, paintRect.bottom);
    }

    /* Draw horizontal grid lines */
    for (int y = 0; y < paintRect.bottom; y += spacing) {
        MoveTo(paintRect.left, paintRect.top + y);
        LineTo(paintRect.right, paintRect.top + y);
    }

    PenNormal();
}

/**
 * MacPaint_ToggleGridDisplay - Turn grid on/off
 */
void MacPaint_ToggleGridDisplay(void)
{
    gRenderState.showGrid = !gRenderState.showGrid;
    MacPaint_InvalidateWindowArea();
}

/**
 * MacPaint_SetGridSpacing - Set grid line spacing in pixels
 */
void MacPaint_SetGridSpacing(int spacing)
{
    if (spacing >= 4 && spacing <= 64) {
        gRenderState.gridSpacing = spacing;
        MacPaint_InvalidateWindowArea();
    }
}

/*
 * SELECTION RECTANGLE RENDERING
 */

/**
 * MacPaint_DrawSelectionRectangle - Draw marching ants selection outline
 */
void MacPaint_DrawSelectionRectangle(void)
{
    if (!gRenderState.showSelectionRect || !gPaintWindow) {
        return;
    }

    GrafPtr port = GetWindowPort(gPaintWindow);
    if (!port) {
        return;
    }

    SetPort(port);

    /* Use XOR pen mode so visible on any background */
    PenMode(patXor);
    PenSize(1, 1);

    /* Create marching ants pattern with animation offset */
    UInt8 marchPattern[8];
    int offset = gRenderState.selectionMarching % 8;
    for (int i = 0; i < 8; i++) {
        marchPattern[i] = (i >= offset) ? 0xFF : 0x00;
    }

    PenPat((Pattern *)marchPattern);

    /* Draw outline with marching ants effect */
    FrameRect(&gRenderState.selectionRect);

    PenNormal();
}

/**
 * MacPaint_UpdateSelectionDisplay - Update selection rectangle display
 */
void MacPaint_UpdateSelectionDisplay(const Rect *rect)
{
    if (!rect) {
        gRenderState.showSelectionRect = 0;
    } else {
        gRenderState.selectionRect = *rect;
        gRenderState.showSelectionRect = 1;
    }
    MacPaint_InvalidateWindowArea();
}

/**
 * MacPaint_AnimateSelection - Animate marching ants
 * Called periodically to update marching ants animation
 */
void MacPaint_AnimateSelection(void)
{
    if (!gRenderState.showSelectionRect) {
        return;
    }

    /* Update animation frame for marching ants */
    gRenderState.selectionMarching++;
    if (gRenderState.selectionMarching > 7) {
        gRenderState.selectionMarching = 0;
    }
    MacPaint_InvalidateWindowArea();
}

/*
 * PATTERN EDITOR DIALOG RENDERING
 */

/**
 * MacPaint_RenderPatternEditorDialog - Draw pattern editor window
 */
void MacPaint_RenderPatternEditorDialog(void)
{
    /* Pattern editor dialog layout (modeless dialog):
     * - 8x8 pixel grid for editing (16x16 pixels display)
     * - Preview area showing current pattern
     * - OK, Cancel, Reset buttons
     *
     * This is called when DialogManager handles dialog events.
     * The actual dialog window pointer and positioning is handled by DialogManager.
     *
     * Note: Full implementation requires DialogManager and DLOG resources.
     * For now, we'll provide the rendering framework.
     */

    /* In a full implementation, would:
     * 1. Get pattern from MacPaint_GetPatternEditorPattern()
     * 2. Draw 8x8 grid of pixel boxes
     * 3. Draw preview of pattern
     * 4. Draw buttons
     * 5. Handle clicks on pixels to toggle pattern bits
     */
}

/**
 * MacPaint_DrawPatternPreview - Draw preview of edited pattern
 */
void MacPaint_DrawPatternPreview(const Rect *previewRect)
{
    if (!previewRect) {
        return;
    }

    /* Draw frame around preview area */
    FrameRect(previewRect);

    /* Get pattern being edited */
    Pattern editPat = MacPaint_GetPatternEditorPattern();

    /* Tile pattern to fill preview rect, with 16x magnification */
    int x, y, px, py;
    for (y = previewRect->top; y < previewRect->bottom; y += 128) {
        for (x = previewRect->left; x < previewRect->right; x += 128) {
            /* Draw magnified 8x8 pattern (16 pixels per pattern pixel) */
            for (py = 0; py < 8; py++) {
                for (px = 0; px < 8; px++) {
                    /* Check if this pattern bit is set */
                    if ((editPat.pat[py] >> (7 - px)) & 1) {
                        /* Draw 16x16 pixel block for this pattern pixel */
                        Rect pixelRect;
                        pixelRect.left = x + (px * 16);
                        pixelRect.top = y + (py * 16);
                        pixelRect.right = pixelRect.left + 16;
                        pixelRect.bottom = pixelRect.top + 16;

                        /* Clip to preview rect */
                        if (pixelRect.right > previewRect->right)
                            pixelRect.right = previewRect->right;
                        if (pixelRect.bottom > previewRect->bottom)
                            pixelRect.bottom = previewRect->bottom;

                        /* Draw filled pixel block */
                        PaintRect(&pixelRect);
                        FrameRect(&pixelRect);
                    }
                }
            }
        }
    }
}

/*
 * BRUSH EDITOR DIALOG RENDERING
 */

/**
 * MacPaint_RenderBrushEditorDialog - Draw brush editor window
 */
void MacPaint_RenderBrushEditorDialog(void)
{
    /* Brush editor dialog layout (modeless dialog):
     * - 5 brush shape radio buttons
     * - Size slider (1-64 pixels)
     * - Preview area showing brush shape
     * - OK, Cancel buttons
     *
     * Brush shapes:
     *   0 - Circle (filled)
     *   1 - Square (filled)
     *   2 - Diamond
     *   3 - Spray
     *   4 - Custom Pattern
     *
     * Note: Full implementation requires DialogManager and DLOG resources.
     * For now, we'll provide the rendering framework.
     */

    /* In a full implementation, would:
     * 1. Draw radio button groups for brush shapes
     * 2. Draw size slider control
     * 3. Draw preview of current brush
     * 4. Draw buttons
     */
}

/**
 * MacPaint_DrawBrushPreview - Draw preview of edited brush
 */
void MacPaint_DrawBrushPreview(const Rect *previewRect)
{
    if (!previewRect) {
        return;
    }

    /* Draw frame around preview area */
    FrameRect(previewRect);

    int size = MacPaint_GetBrushSize();

    /* Calculate centered preview position */
    int centerX = (previewRect->left + previewRect->right) / 2;
    int centerY = (previewRect->top + previewRect->bottom) / 2;

    /* Draw brush preview based on current brush shape
     * For now, we'll draw a simple filled circle as default
     */

    Rect brushRect;
    brushRect.left = centerX - (size / 2);
    brushRect.top = centerY - (size / 2);
    brushRect.right = centerX + (size / 2);
    brushRect.bottom = centerY + (size / 2);

    /* Clip to preview rect */
    if (brushRect.left < previewRect->left)
        brushRect.left = previewRect->left;
    if (brushRect.top < previewRect->top)
        brushRect.top = previewRect->top;
    if (brushRect.right > previewRect->right)
        brushRect.right = previewRect->right;
    if (brushRect.bottom > previewRect->bottom)
        brushRect.bottom = previewRect->bottom;

    /* Draw brush shape preview
     * Default: circle shape
     */
    PaintOval(&brushRect);
    FrameOval(&brushRect);
}

/*
 * TOOLBOX RENDERING
 */

/**
 * MacPaint_DrawToolbox - Draw tool palette on screen
 */
void MacPaint_DrawToolbox(void)
{
    if (!gPaintWindow) {
        return;
    }

    GrafPtr port = GetWindowPort(gPaintWindow);
    if (!port) {
        return;
    }

    SetPort(port);
    PenNormal();

    /* Toolbox area on left side of window */
    Rect toolboxRect;
    toolboxRect.left = port->portRect.left;
    toolboxRect.right = port->portRect.left + MACPAINT_TOOLBOX_WIDTH;    /* 74 pixels wide for 2x36+2 */
    toolboxRect.top = port->portRect.top;
    toolboxRect.bottom = port->portRect.bottom - MACPAINT_STATUS_HEIGHT; /* Leave space for status bar */

    /* Draw toolbox frame */
    FrameRect(&toolboxRect);

    /* Draw 12 tool buttons in 2x6 grid (30x30 pixels each + 2 pixel border) */
    int toolSize = 30;
    int spacing = 2;
    int col, row, toolID;
    Rect toolRect;

    for (row = 0; row < 6; row++) {
        for (col = 0; col < 2; col++) {
            toolID = row * 2 + col;
            if (toolID >= 12) break;

            /* Calculate tool button rectangle */
            toolRect.left = toolboxRect.left + spacing + (col * (toolSize + spacing));
            toolRect.top = toolboxRect.top + spacing + (row * (toolSize + spacing));
            toolRect.right = toolRect.left + toolSize;
            toolRect.bottom = toolRect.top + toolSize;

            /* Draw tool button frame */
            FrameRect(&toolRect);

            /* Highlight active tool with inverted box */
            if (toolID == gCurrentTool) {
                InvertRect(&toolRect);
            }
        }
    }
}

/**
 * MacPaint_HighlightActiveTool - Draw highlight on currently selected tool
 */
void MacPaint_HighlightActiveTool(void)
{
    if (!gPaintWindow || gCurrentTool < 0 || gCurrentTool >= 12) {
        return;
    }

    GrafPtr port = GetWindowPort(gPaintWindow);
    if (!port) {
        return;
    }

    SetPort(port);

    /* Calculate highlighted tool position */
    Rect toolboxRect;
    toolboxRect.left = port->portRect.left;
    toolboxRect.right = port->portRect.left + MACPAINT_TOOLBOX_WIDTH;
    toolboxRect.top = port->portRect.top;
    toolboxRect.bottom = port->portRect.bottom - MACPAINT_STATUS_HEIGHT;

    int toolSize = 30;
    int spacing = 2;
    int col = gCurrentTool % 2;
    int row = gCurrentTool / 2;

    Rect toolRect;
    toolRect.left = toolboxRect.left + spacing + (col * (toolSize + spacing));
    toolRect.top = toolboxRect.top + spacing + (row * (toolSize + spacing));
    toolRect.right = toolRect.left + toolSize;
    toolRect.bottom = toolRect.top + toolSize;

    /* Draw highlight border */
    PenMode(patXor);
    PenSize(2, 2);
    FrameRect(&toolRect);
    PenNormal();
}

/*
 * STATUS BAR RENDERING
 */

/**
 * MacPaint_DrawStatusBar - Draw status information at bottom
 */
void MacPaint_DrawStatusBar(void)
{
    if (!gPaintWindow) {
        return;
    }

    GrafPtr port = GetWindowPort(gPaintWindow);
    if (!port) {
        return;
    }

    SetPort(port);

    /* Calculate status bar area (bottom 20 pixels of window) */
    Rect statusRect;
    statusRect.top = port->portRect.bottom - MACPAINT_STATUS_HEIGHT;
    statusRect.left = port->portRect.left;
    statusRect.right = port->portRect.right;
    statusRect.bottom = port->portRect.bottom;

    /* Draw frame around status bar */
    FrameRect(&statusRect);

    /* Draw separator line */
    MoveTo(statusRect.left, statusRect.top);
    LineTo(statusRect.right, statusRect.top);

    /* Draw status bar text */
    PenNormal();
    TextFont(0);  /* Chicago */
    TextSize(10);
    ForeColor(blackColor);

    /* Tool name */
    static const char* toolNames[] = {
        "Lasso", "Select", "Grabber", "Text", "Fill", "Spray",
        "Brush", "Pencil", "Line", "Eraser", "Oval", "Rect"
    };
    const char* toolName = "Unknown";
    if (gCurrentTool >= 0 && gCurrentTool < 12) {
        toolName = toolNames[gCurrentTool];
    }

    /* Build status string: "Tool: [name]  |  X: [x]  Y: [y]" */
    MoveTo(statusRect.left + 4, statusRect.bottom - 5);

    /* Draw tool label */
    unsigned char pStr[64];
    int len = 0;
    const char *prefix = "Tool: ";
    while (*prefix && len < 62) pStr[1 + len++] = *prefix++;
    while (*toolName && len < 62) pStr[1 + len++] = *toolName++;
    pStr[0] = (unsigned char)len;
    DrawString(pStr);

    /* Draw coordinates */
    extern int gDocDirty;
    extern char gDocName[64];
    MoveTo(statusRect.left + 120, statusRect.bottom - 5);
    int nameLen = 0;
    while (gDocName[nameLen] && nameLen < 40) nameLen++;
    if (nameLen > 0) {
        len = 0;
        for (int i = 0; i < nameLen && len < 60; i++)
            pStr[1 + len++] = gDocName[i];
        if (gDocDirty) pStr[1 + len++] = '*';
        pStr[0] = (unsigned char)len;
        DrawString(pStr);
    }
}

/*
 * CURSOR MANAGEMENT
 */

/**
 * MacPaint_SetToolCursor - Update cursor based on current tool
 */
void MacPaint_SetToolCursor(void)
{
    /* Set cursor shape based on current tool selection
     *
     * Tool cursor mapping:
     * - Drawing tools (Pencil, Line, Rect, Oval): crosshair
     * - Brush: brush shape
     * - Eraser: eraser
     * - Fill: paint bucket
     * - Spray: airbrush
     * - Lasso: lasso shape
     * - Select: rectangle select
     * - Grabber: hand
     * - Text: i-beam
     *
     * For now, use default arrow cursor for all tools.
     * Full implementation would load CURS resources from application file.
     */

    /* Use built-in system cursors for each tool category.
     * CursHandle is void*, cast to Cursor** for dereferencing. */
    switch (gCurrentTool) {
        case TOOL_TEXT:
        {
            Cursor **ibeam = (Cursor **)GetCursor(1);  /* iBeamCursor */
            if (ibeam && *ibeam) SetCursor(*ibeam);
            else InitCursor();
            break;
        }

        case TOOL_PENCIL:
        case TOOL_LINE:
        case TOOL_RECT:
        case TOOL_OVAL:
        case TOOL_BRUSH:
        case TOOL_SPRAY:
        {
            Cursor **cross = (Cursor **)GetCursor(2);  /* crossCursor */
            if (cross && *cross) SetCursor(*cross);
            else InitCursor();
            break;
        }

        case TOOL_GRABBER:
        {
            Cursor **hand = (Cursor **)GetCursor(4);  /* plusCursor */
            if (hand && *hand) SetCursor(*hand);
            else InitCursor();
            break;
        }

        case TOOL_FILL:
        case TOOL_ERASE:
        case TOOL_LASSO:
        case TOOL_SELECT:
        default:
            InitCursor();
            break;
    }
}

/**
 * MacPaint_UpdateCursorPosition - Update cursor on mouse move
 */
void MacPaint_UpdateCursorPosition(int x, int y)
{
    /* Update cursor appearance based on mouse position and context
     *
     * Considerations:
     * - Over canvas: show tool-specific cursor
     * - Over toolbox: show hand/pointer
     * - Over selection: show move cursor
     * - Over window frame: show resize cursor
     *
     * For now, just ensure tool cursor is set.
     * Full implementation would check regions and change cursor dynamically.
     */

    MacPaint_SetToolCursor();
}

/*
 * INVALIDATION AND REDRAW COORDINATION
 */

/**
 * MacPaint_InvalidateToolArea - Mark toolbox for redraw
 */
void MacPaint_InvalidateToolArea(void)
{
    if (!gPaintWindow) {
        return;
    }

    MacPaint_UpdateInvalidationRects();
    gInvalidState.toolboxDirty = 1;

    /* Mark toolbox rectangle for update */
    InvalRect(&gInvalidState.toolboxRect);
}

/**
 * MacPaint_InvalidateStatusArea - Mark status bar for redraw
 */
void MacPaint_InvalidateStatusArea(void)
{
    if (!gPaintWindow) {
        return;
    }

    MacPaint_UpdateInvalidationRects();
    gInvalidState.statusDirty = 1;

    /* Mark status bar rectangle for update */
    InvalRect(&gInvalidState.statusRect);
}

/**
 * MacPaint_InvalidateWindowArea - Mark entire window for redraw
 */
void MacPaint_InvalidateWindowArea(void)
{
    if (!gPaintWindow) {
        return;
    }

    GrafPtr port = GetWindowPort(gPaintWindow);
    if (!port) {
        return;
    }

    MacPaint_UpdateInvalidationRects();
    gInvalidState.paintDirty = 1;
    gInvalidState.toolboxDirty = 1;
    gInvalidState.statusDirty = 1;

    /* Mark entire window for update */
    InvalRect(&port->portRect);
}

/**
 * MacPaint_InvalidatePaintArea - Mark canvas area for redraw
 */
void MacPaint_InvalidatePaintArea(void)
{
    if (!gPaintWindow) {
        return;
    }

    MacPaint_UpdateInvalidationRects();
    gInvalidState.paintDirty = 1;

    /* Mark paint canvas rectangle for update */
    InvalRect(&gInvalidState.paintRect);
}

/**
 * MacPaint_GetInvalidState - Check if areas need redrawing
 */
int MacPaint_GetInvalidState(int *paintDirty, int *toolboxDirty, int *statusDirty)
{
    if (paintDirty) *paintDirty = gInvalidState.paintDirty;
    if (toolboxDirty) *toolboxDirty = gInvalidState.toolboxDirty;
    if (statusDirty) *statusDirty = gInvalidState.statusDirty;

    return (gInvalidState.paintDirty || gInvalidState.toolboxDirty || gInvalidState.statusDirty);
}

/**
 * MacPaint_ClearInvalidState - Clear dirty flags after redraw
 */
void MacPaint_ClearInvalidState(void)
{
    gInvalidState.paintDirty = 0;
    gInvalidState.toolboxDirty = 0;
    gInvalidState.statusDirty = 0;
}

/*
 * ANIMATED ELEMENTS
 */

/**
 * MacPaint_UpdateAnimations - Update all animated elements
 * Called periodically during idle time
 */
void MacPaint_UpdateAnimations(void)
{
    if (gRenderState.showSelectionRect) {
        MacPaint_AnimateSelection();
    }

    /* TODO: Update other animations
     * - Tool cursor animation
     * - Blinking text cursor in text tool
     * - Spray particle animation
     */
}

/*
 * RENDERING OPTIONS
 */

/**
 * MacPaint_SetFatBitsMode - Enable/disable Fat Bits zoom display
 */
void MacPaint_SetFatBitsMode(int enabled, int zoomFactor)
{
    if (enabled) {
        gRenderState.fatBitsMode = 1;
        gRenderState.fatBitsZoom = (zoomFactor >= 1 && zoomFactor <= 16) ? zoomFactor : 2;
    } else {
        gRenderState.fatBitsMode = 0;
    }
    MacPaint_InvalidateWindowArea();
}

/**
 * MacPaint_IsFatBitsMode - Check if in Fat Bits mode
 */
int MacPaint_IsFatBitsMode(void)
{
    return gRenderState.fatBitsMode;
}

/*
 * FULL WINDOW UPDATE
 */

/**
 * MacPaint_FullWindowUpdate - Complete redraw of entire window
 */
void MacPaint_FullWindowUpdate(void)
{
    if (!gPaintWindow) {
        return;
    }

    BeginUpdate(gPaintWindow);

    GrafPtr port = GetWindowPort(gPaintWindow);
    if (port) {
        SetPort(port);

        /* Erase background with white */
        BackColor(whiteColor);
        EraseRect(&port->portRect);

        /* Render all elements in order */
        MacPaint_RenderPaintBuffer();
        MacPaint_DrawToolbox();
        MacPaint_DrawStatusBar();
    }

    EndUpdate(gPaintWindow);
}

/*
 * RENDERING STATISTICS (DEBUG)
 */

/**
 * MacPaint_GetRenderStats - Get rendering performance stats
 */
void MacPaint_GetRenderStats(int *pixelsRendered, int *updateTime)
{
    /* TODO: Track rendering performance
     * - Total pixels rendered per frame
     * - Time spent in rendering code
     * - Invalidated regions
     */
    if (pixelsRendered) *pixelsRendered = 0;
    if (updateTime) *updateTime = 0;
}
