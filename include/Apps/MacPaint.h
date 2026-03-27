/*
 * MacPaint.h - MacPaint Application Interface
 *
 * Public API for MacPaint 1.3 running on System 7.1 Portable
 * Adapted from original MacPaint by Bill Atkinson
 */

#ifndef MACPAINT_H
#define MACPAINT_H

#include "SystemTypes.h"
#include "QuickDraw/QuickDraw.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Tool Enumeration
 */
typedef enum {
    MacPaintTool_Lasso = 0,
    MacPaintTool_Select = 1,
    MacPaintTool_Grabber = 2,
    MacPaintTool_Text = 3,
    MacPaintTool_Fill = 4,
    MacPaintTool_Spray = 5,
    MacPaintTool_Brush = 6,
    MacPaintTool_Pencil = 7,
    MacPaintTool_Line = 8,
    MacPaintTool_Erase = 9,
    MacPaintTool_Oval = 10,
    MacPaintTool_Rect = 11
} MacPaintTool;

/* Tool ID Constants (matching enum values above) */
#define TOOL_LASSO 0
#define TOOL_SELECT 1
#define TOOL_GRABBER 2
#define TOOL_TEXT 3
#define TOOL_FILL 4
#define TOOL_SPRAY 5
#define TOOL_BRUSH 6
#define TOOL_PENCIL 7
#define TOOL_LINE 8
#define TOOL_ERASE 9
#define TOOL_OVAL 10
#define TOOL_RECT 11

/*
 * Constants
 */
#define MACPAINT_DOC_WIDTH 576
#define MACPAINT_DOC_HEIGHT 720
#define MACPAINT_PATTERN_COUNT 38
#define MACPAINT_TOOLBOX_WIDTH 74
#define MACPAINT_STATUS_HEIGHT 20

/*
 * Initialization and Shutdown
 */
OSErr MacPaint_Initialize(void);
void MacPaint_Shutdown(void);
int MacPaintMain(int argc, char **argv);
void MacPaint_Update(void);
OSErr MacPaint_LoadResources(void);
OSErr MacPaint_PromptSave(void);
void MacPaint_CheckMemory(void);

/*
 * Tool Operations
 */
void MacPaint_SelectTool(int toolID);
void MacPaint_SetLineSize(int size);
void MacPaint_SetPattern(int patternIndex);

/*
 * Drawing Operations (from MacPaint_Core.c)
 */
void MacPaint_DrawLine(int x1, int y1, int x2, int y2);
void MacPaint_FillRect(Rect *rect);
void MacPaint_DrawOval(Rect *rect);
void MacPaint_DrawRect(Rect *rect);

/*
 * Drawing Algorithms (from MacPaint_Tools.c)
 */
void MacPaint_DrawLineAlgo(int x0, int y0, int x1, int y1, int mode);
void MacPaint_DrawOvalAlgo(int cx, int cy, int rx, int ry, int filled, int mode);
void MacPaint_DrawRectAlgo(int x0, int y0, int x1, int y1, int filled, int mode);
void MacPaint_DrawPatternedLine(int x0, int y0, int x1, int y1, Pattern *pat);
void MacPaint_FloodFill(int x, int y);

/*
 * Tool Handlers (from MacPaint_Tools.c)
 */
void MacPaint_HandleToolMouseEvent(int toolID, int x, int y, int down);
void MacPaint_ToolPencil(int x, int y, int down);
void MacPaint_ToolEraser(int x, int y, int down);
void MacPaint_ToolLine(int x, int y, int down);
void MacPaint_ToolRectangle(int x, int y, int down);
void MacPaint_ToolOval(int x, int y, int down);
void MacPaint_ToolFill(int x, int y, int down);
void MacPaint_ToolSpray(int x, int y, int down);
void MacPaint_ToolRectSelect(int x, int y, int down);
void MacPaint_ToolLasso(int x, int y, int down);
void MacPaint_ToolGrabber(int x, int y, int down);
void MacPaint_ToolText(int x, int y, int down);
int MacPaint_TextToolHandleKey(int keyCode, int modifiers);
void MacPaint_RenderTextAtPosition(const char *text, int x, int y);
void MacPaint_GetToolState(int *isDrawing, int *startX, int *startY, int *currentX, int *currentY);

/*
 * Document Operations
 */
OSErr MacPaint_NewDocument(void);
OSErr MacPaint_SaveDocument(const char *filename);
OSErr MacPaint_SaveDocumentAs(const char *filename);
OSErr MacPaint_OpenDocument(const char *filename);
OSErr MacPaint_RevertDocument(void);

/*
 * File I/O and Compression (from MacPaint_FileIO.c)
 */
UInt32 MacPaint_PackBits(const unsigned char *src, int srcLen,
                         unsigned char *dst, int dstLen);
UInt32 MacPaint_UnpackBits(const unsigned char *src, int srcLen,
                           unsigned char *dst, int dstLen);
int MacPaint_IsDocumentDirty(void);
void MacPaint_GetDocumentInfo(char *filename, int *isDirty, int *modCount);
int MacPaint_PromptSaveChanges(void);
OSErr MacPaint_CreateBackup(void);
OSErr MacPaint_RestoreBackup(void);
int MacPaint_ValidateFile(const unsigned char *data, int dataLen);
OSErr MacPaint_ExportAsPICT(const char *filename);
OSErr MacPaint_ImportFromPICT(const char *filename);

/*
 * Low-level Bit Operations (ported from 68k assembly)
 */
int MacPaint_PixelTrue(int h, int v, BitMap *bits);
void MacPaint_SetPixel(int h, int v, BitMap *bits);
void MacPaint_ClearPixel(int h, int v, BitMap *bits);
void MacPaint_InvertBuf(BitMap *buf);
void MacPaint_ZeroBuf(BitMap *buf);
void MacPaint_ExpandPattern(Pattern pat, UInt32 *expanded);

/*
 * Rendering (from MacPaint_Rendering.c)
 */
void MacPaint_Render(void);
void MacPaint_InvalidateRect(Rect *rect);

/* Advanced Rendering */
void MacPaint_RenderPaintBuffer(void);
void MacPaint_RenderFatBits(void);
void MacPaint_DrawGridOverlay(void);
void MacPaint_DrawSelectionRectangle(void);
void MacPaint_RenderPatternEditorDialog(void);
void MacPaint_RenderBrushEditorDialog(void);
void MacPaint_DrawPatternPreview(const Rect *previewRect);
void MacPaint_DrawBrushPreview(const Rect *previewRect);
void MacPaint_DrawToolbox(void);
void MacPaint_HighlightActiveTool(void);
void MacPaint_DrawStatusBar(void);
void MacPaint_SetToolCursor(void);
void MacPaint_UpdateCursorPosition(int x, int y);
void MacPaint_InvalidateToolArea(void);
void MacPaint_InvalidateStatusArea(void);
void MacPaint_UpdateAnimations(void);
void MacPaint_SetFatBitsMode(int enabled, int zoomFactor);
int MacPaint_IsFatBitsMode(void);
void MacPaint_FullWindowUpdate(void);
void MacPaint_GetRenderStats(int *pixelsRendered, int *updateTime);
void MacPaint_ToggleGridDisplay(void);
void MacPaint_SetGridSpacing(int spacing);
void MacPaint_UpdateSelectionDisplay(const Rect *rect);
void MacPaint_AnimateSelection(void);

/*
 * Window Binding
 */
void MacPaint_RegisterMainWindow(WindowPtr window);

/*
 * Menu and Event System (from MacPaint_Menus.c)
 */
void MacPaint_InitializeMenus(void);
void MacPaint_UpdateMenus(void);
void MacPaint_HandleMenuCommand(int menuID, int menuItem);
void MacPaint_HandleMouseDown(int x, int y, int modifiers);
void MacPaint_HandleMouseDrag(int x, int y);
void MacPaint_HandleMouseUp(int x, int y);
void MacPaint_HandleKeyDown(int keyCode, int modifiers);
void MacPaint_GetMenuState(int *gridShown, int *fatBitsActive,
                          int *undoAvailable, int *selectionActive);
void MacPaint_SetMenuState(int gridShown, int fatBitsActive,
                          int undoAvailable, int selectionActive);
void MacPaint_SetClipboardState(int hasContent);
const char* MacPaint_GetWindowTitle(void);
int MacPaint_IsMenuItemAvailable(int menuID, int menuItem);

/* Aids Menu Operations */
void MacPaint_AidsToggleGrid(void);
void MacPaint_AidsToggleFatBits(void);
void MacPaint_AidsPatternEditor(void);
void MacPaint_AidsBrushEditor(void);
void MacPaint_AidsHelp(void);

/* Font and Style Menu Operations */
void MacPaint_FontSelect(int menuItem);
void MacPaint_StyleToggleBold(void);
void MacPaint_StyleToggleItalic(void);
void MacPaint_StyleToggleUnderline(void);
void MacPaint_StylePlain(void);
void MacPaint_GetTextStyle(short *font, short *size, Style *face);

/*
 * File Menu Operations
 */
void MacPaint_FileNew(void);
void MacPaint_FileOpen(void);
void MacPaint_FileClose(void);
void MacPaint_FileSave(void);
void MacPaint_FileSaveAs(void);
void MacPaint_FilePrint(void);
void MacPaint_FileQuit(void);

/*
 * Edit Menu Operations
 */
void MacPaint_EditUndo(void);
void MacPaint_EditCut(void);
void MacPaint_EditCopy(void);
void MacPaint_EditPaste(void);
void MacPaint_EditClear(void);
void MacPaint_EditInvert(void);
void MacPaint_EditFill(void);
void MacPaint_EditSelectAll(void);

/*
 * Advanced Features (from MacPaint_Advanced.c)
 */

/* Undo/Redo System */
OSErr MacPaint_InitializeUndo(void);
void MacPaint_ShutdownUndo(void);
OSErr MacPaint_SaveUndoState(const char *description);
int MacPaint_CanUndo(void);
int MacPaint_CanRedo(void);
OSErr MacPaint_Undo(void);
OSErr MacPaint_Redo(void);

/* Selection and Clipboard */
OSErr MacPaint_CreateSelection(int left, int top, int right, int bottom);
int MacPaint_GetSelection(Rect *bounds);
void MacPaint_ClearSelection(void);
OSErr MacPaint_CopySelectionToClipboard(void);
OSErr MacPaint_PasteFromClipboard(int x, int y);
OSErr MacPaint_CutSelection(void);

/* Pattern Editor */
OSErr MacPaint_OpenPatternEditor(void);
void MacPaint_ClosePatternEditor(void);
void MacPaint_SetPatternEditorPattern(int patternIndex);
Pattern MacPaint_GetPatternEditorPattern(void);
void MacPaint_PatternEditorPixelClick(int x, int y);

/* Brush Editor */
OSErr MacPaint_OpenBrushEditor(void);
void MacPaint_CloseBrushEditor(void);
void MacPaint_SetBrushSize(int diameter);
int MacPaint_GetBrushSize(void);

/* Advanced Drawing Modes */
void MacPaint_SetDrawingMode(int mode);
int MacPaint_GetDrawingMode(void);

/* Selection Transformations */
OSErr MacPaint_FlipSelectionHorizontal(void);
OSErr MacPaint_FlipSelectionVertical(void);
OSErr MacPaint_RotateSelectionCW(void);
OSErr MacPaint_RotateSelectionCCW(void);
OSErr MacPaint_ScaleSelection(int newWidth, int newHeight);

/* Advanced Fill Modes */
OSErr MacPaint_FillSelectionWithPattern(void);
OSErr MacPaint_GradientFill(void);
OSErr MacPaint_SmoothSelection(void);

/* State Queries */
int MacPaint_IsPatternEditorOpen(void);
int MacPaint_IsBrushEditorOpen(void);
int MacPaint_IsSelectionActive(void);
int MacPaint_HasClipboard(void);
const char* MacPaint_GetUndoDescription(void);
const char* MacPaint_GetRedoDescription(void);

/*
 * System Integration (from MacPaint_Integration.c)
 */

/* Window Management */
OSErr MacPaint_CreateWindow(void);
WindowPtr MacPaint_GetWindowPtr(void);
void MacPaint_GetPaintRect(Rect *rect);
void MacPaint_InvalidateWindow(void);
void MacPaint_CloseWindow(void);
void MacPaint_UpdateWindowTitle(void);
int MacPaint_IsWindowOpen(void);

/* Menu Manager */
OSErr MacPaint_InitializeMenuBar(void);
void MacPaint_HandleMenuSelection(int menuID, int itemID);
void MacPaint_AdjustMenus(void);
int MacPaint_GetMenuBarState(void);

/* Standard File Dialogs */
int MacPaint_DoOpenDialog(char *path, int pathLen);
int MacPaint_DoSaveDialog(char *path, int pathLen);

/* Print Manager */
int MacPaint_DoPrintDialog(void);
OSErr MacPaint_PrintDocument(void);

/* Dialog Editors */
OSErr MacPaint_CreatePatternEditorDialog(void);
int MacPaint_PatternEditorEventHandler(int itemHit);
void MacPaint_ClosePatternEditorDialog(void);
OSErr MacPaint_CreateBrushEditorDialog(void);
int MacPaint_BrushEditorEventHandler(int itemHit);
void MacPaint_CloseBrushEditorDialog(void);

/* Event Loop (from MacPaint_EventLoop.c) */
void MacPaint_RunEventLoop(void);
OSErr MacPaint_ExecuteMenuCommand(int menuID, int itemID);

/* Window Management (from MacPaint_EventLoop.c) */
OSErr MacPaint_CreateMainWindow(void);
void MacPaint_DrawPaintWindow(void);
void MacPaint_HandleWindowUpdate(WindowPtr window);
void MacPaint_HandleWindowClose(WindowPtr window);
void MacPaint_HandleWindowResize(WindowPtr window);

/* Mouse Event Handling */
void MacPaint_HandleMouseDownEvent(int x, int y, int modifiers);
void MacPaint_HandleMouseDragEvent(int x, int y);
void MacPaint_HandleMouseUpEvent(int x, int y);

/* Keyboard Event Handling */
void MacPaint_HandleKeyDownEvent(int keyCode, int modifiers);

/* Menu Event Handling */
void MacPaint_HandleMenuClickEvent(int menuID, int itemID);

/* Event Window Checking */
int MacPaint_IsEventInPaintWindow(int x, int y);

/* Window Invalidation and Update Management */
void MacPaint_InvalidateWindowArea(void);
void MacPaint_InvalidatePaintArea(void);
void MacPaint_InvalidateToolArea(void);
void MacPaint_InvalidateStatusArea(void);
void MacPaint_InvalidateRectArea(const Rect *rect);
int MacPaint_GetInvalidState(int *paintDirty, int *toolboxDirty, int *statusDirty);
void MacPaint_ClearInvalidState(void);

/* Event Loop State */
int MacPaint_IsMouseDown(void);
void MacPaint_GetLastMousePosition(int *x, int *y);
int MacPaint_ShouldQuit(void);
void MacPaint_RequestQuit(void);

/* Save Prompts */
int MacPaint_PromptSaveChanges(void);

/* Tool State */
void MacPaint_SetActiveTool(int toolID);
int MacPaint_GetActiveTool(void);

/* Idle Processing */
void MacPaint_ProcessIdleTime(void);

/* Clipboard Integration */
int MacPaint_PasteFromSystemClipboard(void);
int MacPaint_CopyToSystemClipboard(void);

/* Error Handling */
void MacPaint_HandleError(OSErr err, const char *context);

/* Shutdown Coordination */
void MacPaint_PrepareForShutdown(void);

/* Drag and Drop */
int MacPaint_CanAcceptDraggedFile(const char *filename);
OSErr MacPaint_HandleDroppedFile(const char *filename);

/* Resource Management */
OSErr MacPaint_LoadApplicationResources(void);
void MacPaint_ReleaseApplicationResources(void);

/* System Integration */
OSErr MacPaint_InitializeSystem(void);
void MacPaint_ShutdownSystem(void);

/* Document Management */
void MacPaint_SetDocumentName(const char *name);
const char* MacPaint_GetDocumentName(void);

/*
 * Application Launcher Wrapper Functions (for Finder integration)
 * These functions provide a consistent interface for the Finder to launch
 * and manage MacPaint, similar to SimpleText and other applications
 */
extern void MacPaint_Launch(void);           /* Launch MacPaint application */
extern void MacPaint_Init(void);             /* Initialize MacPaint */
extern void MacPaint_Quit(void);             /* Quit MacPaint */
extern Boolean MacPaint_IsRunning(void);     /* Check if MacPaint is running */
extern void MacPaint_OpenFile(const char* path); /* Open file in MacPaint */

/*
 * Global State Access (from MacPaint_Core.c)
 */
extern BitMap gPaintBuffer;
extern WindowPtr gPaintWindow;
extern char gDocName[64];
extern int gDocDirty;
extern int gCurrentTool;
extern Rect gSelectionRect;
extern int gSelectionActive;

#ifdef __cplusplus
}
#endif

#endif /* MACPAINT_H */
