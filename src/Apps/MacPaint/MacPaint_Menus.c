/*
 * MacPaint_Menus.c - MacPaint Menu System and Event Handling
 *
 * Implements complete menu system for MacPaint:
 * - File menu: New, Open, Close, Save, Save As, Print, Quit
 * - Edit menu: Undo, Cut, Copy, Paste, Clear, Invert, Fill
 * - Aids menu: Grid, Fat Bits, Pattern Editor, Brush Editor
 * - Font menu: Font selection
 * - Style menu: Text formatting
 *
 * Integrates with System 7.1 MenuManager for standard UI
 * Ported from original MacPaint.p menu definitions
 */

#include "SystemTypes.h"
#include "Apps/MacPaint.h"
#include "MenuManager/MenuManager.h"
#include "WindowManager/WindowManager.h"
#include "System71StdLib.h"
#include <string.h>

/*
 * MENU CONSTANTS AND DEFINITIONS
 */

/* Menu IDs (resource IDs for menus) */
#define MENU_APPLE      128     /* Apple menu (desk accessories) */
#define MENU_FILE       129     /* File menu */
#define MENU_EDIT       130     /* Edit menu */
#define MENU_FONT       131     /* Font selection menu */
#define MENU_STYLE      132     /* Text style menu */
#define MENU_AIDS       133     /* Aids/Tools menu */

/* File menu command codes */
#define CMD_FILE_NEW            1
#define CMD_FILE_OPEN           2
#define CMD_FILE_CLOSE          3
#define CMD_FILE_SAVE           4
#define CMD_FILE_SAVE_AS        5
#define CMD_FILE_PRINT          6
#define CMD_FILE_QUIT           7

/* Edit menu command codes */
#define CMD_EDIT_UNDO           1
#define CMD_EDIT_CUT            2
#define CMD_EDIT_COPY           3
#define CMD_EDIT_PASTE          4
#define CMD_EDIT_CLEAR          5
#define CMD_EDIT_INVERT         6
#define CMD_EDIT_FILL           7
#define CMD_EDIT_SELECT_ALL     8

/* Aids menu command codes */
#define CMD_AIDS_GRID           1
#define CMD_AIDS_FAT_BITS       2
#define CMD_AIDS_PATTERN_EDIT   3
#define CMD_AIDS_BRUSH_EDIT     4
#define CMD_AIDS_HELP           5

/* Font/Style command codes */
#define CMD_FONT_NEW            128     /* Font selection base */
#define CMD_STYLE_BOLD          1
#define CMD_STYLE_ITALIC        2
#define CMD_STYLE_UNDERLINE     3
#define CMD_STYLE_OUTLINE       4
#define CMD_STYLE_SHADOW        5
#define CMD_STYLE_PLAIN         6

/*
 * GLOBAL STATE FOR MENUS AND OPTIONS
 */

typedef struct {
    int showGrid;               /* Grid display toggle */
    int fatBitsMode;           /* Fat Bits zoom mode */
    int patternEditorOpen;     /* Pattern editor window state */
    int brushEditorOpen;       /* Brush editor window state */
    int undoAvailable;         /* Undo buffer has content */
    int clipboardHasContent;   /* Scrap/clipboard has data */
    int selectionActive;       /* Current selection exists */
} MenuState;

static MenuState gMenuState = {0, 0, 0, 0, 0, 0, 0};

/* External state from MacPaint_Core */
extern char gDocName[64];
extern int gDocDirty;
extern int gCurrentTool;

/*
 * MENU INITIALIZATION
 */

/**
 * MacPaint_InitializeMenus - Set up all application menus
 * Creates menu bar with File, Edit, Font, Style, Aids menus
 */
void MacPaint_InitializeMenus(void)
{
    /* TODO: Use MenuManager to create menus
     * Load from resources or create programmatically
     * Set initial menu states and enable/disable items
     */

    /* Initialize menu state */
    gMenuState.showGrid = 0;
    gMenuState.fatBitsMode = 0;
    gMenuState.undoAvailable = 0;
    gMenuState.clipboardHasContent = 0;
    gMenuState.selectionActive = 0;
}

/**
 * MacPaint_UpdateMenus - Update menu enable/disable states
 * Called after each operation to keep menus current
 */
void MacPaint_UpdateMenus(void)
{
    /* Update File menu */
    /* ENABLE: New, Open, Quit always available */
    /* ENABLE: Save/Save As only if dirty or no filename */
    /* ENABLE: Print if window open */
    /* ENABLE: Close if document open */

    /* Update Edit menu */
    /* ENABLE: Undo if gMenuState.undoAvailable */
    /* ENABLE: Cut/Copy if selection active */
    /* ENABLE: Paste if clipboard has content */
    /* ENABLE: Clear if selection active */
    /* DISABLE: Invert/Fill in some modes */

    /* Update Aids menu */
    /* Checkmark Grid if gMenuState.showGrid */
    /* Checkmark Fat Bits if gMenuState.fatBitsMode */
}

/*
 * FILE MENU OPERATIONS
 */

/**
 * MacPaint_FileNew - Create new blank document
 */
void MacPaint_FileNew(void)
{
    /* Check if current document needs saving */
    int promptResult = MacPaint_PromptSaveChanges();
    if (promptResult == 2) {
        return;  /* User cancelled */
    }
    if (promptResult == 1) {
        MacPaint_SaveDocument(gDocName);
    }

    /* Create new document */
    MacPaint_NewDocument();
}

/**
 * MacPaint_FileOpen - Open document from file
 */
void MacPaint_FileOpen(void)
{
    /* Check if current document needs saving */
    int promptResult = MacPaint_PromptSaveChanges();
    if (promptResult == 2) {
        return;  /* User cancelled */
    }
    if (promptResult == 1) {
        MacPaint_SaveDocument(gDocName);
    }

    /* Show Standard File dialog to select file */
    char filePath[256];
    filePath[0] = '\0';

    if (MacPaint_DoOpenDialog(filePath, sizeof(filePath))) {
        /* User selected a file - open it */
        MacPaint_OpenDocument(filePath);
        MacPaint_SetDocumentName(filePath);
        gDocDirty = 0;  /* Document is no longer dirty after loading */
    }
    /* If cancelled, just return without doing anything */
}

/**
 * MacPaint_FileClose - Close current document
 */
void MacPaint_FileClose(void)
{
    /* Check if needs saving */
    int promptResult = MacPaint_PromptSaveChanges();
    if (promptResult == 2) {
        return;  /* User cancelled */
    }
    if (promptResult == 1) {
        MacPaint_SaveDocument(gDocName);
    }

    /* Close window and clear document */
    strncpy(gDocName, "Untitled", sizeof(gDocName) - 1);
    gDocName[sizeof(gDocName) - 1] = '\0';
}

/**
 * MacPaint_FileSave - Save current document
 */
void MacPaint_FileSave(void)
{
    if (strcmp(gDocName, "Untitled") == 0) {
        /* No filename yet, prompt for Save As */
        MacPaint_FileSaveAs();
    } else {
        /* Save to existing filename */
        MacPaint_SaveDocument(gDocName);
    }
}

/**
 * MacPaint_FileSaveAs - Save with new filename
 */
void MacPaint_FileSaveAs(void)
{
    /* Show Standard File dialog for filename */
    char filePath[256];
    strncpy(filePath, gDocName, sizeof(filePath) - 1);
    filePath[sizeof(filePath) - 1] = '\0';

    if (MacPaint_DoSaveDialog(filePath, sizeof(filePath))) {
        /* User selected a filename - save with new name */
        MacPaint_SaveDocument(filePath);
        MacPaint_SetDocumentName(filePath);
        gDocDirty = 0;  /* Document is no longer dirty after saving */
    }
    /* If cancelled, just return without doing anything */
}

/**
 * MacPaint_FilePrint - Print current document
 */
void MacPaint_FilePrint(void)
{
    /* TODO: Implement print functionality
     * Use System 7.1 PrintManager to set up print job
     * Render bitmap to printer
     */
}

/**
 * MacPaint_FileQuit - Quit application
 */
void MacPaint_FileQuit(void)
{
    /* Check if needs saving */
    int promptResult = MacPaint_PromptSaveChanges();
    if (promptResult == 2) {
        return;  /* User cancelled */
    }
    if (promptResult == 1) {
        MacPaint_SaveDocument(gDocName);
    }

    /* Close the paint window to signal exit */
    extern WindowPtr gPaintWindow;
    if (gPaintWindow) {
        DisposeWindow(gPaintWindow);
        gPaintWindow = NULL;
    }
}

/*
 * EDIT MENU OPERATIONS
 */

/**
 * MacPaint_EditUndo - Undo last operation
 */
void MacPaint_EditUndo(void)
{
    MacPaint_Undo();
    MacPaint_UpdateMenus();
}

/**
 * MacPaint_EditCut - Cut selection to clipboard
 */
void MacPaint_EditCut(void)
{
    if (gMenuState.selectionActive) {
        MacPaint_SaveUndoState("Cut");
        MacPaint_CutSelection();
        gMenuState.clipboardHasContent = 1;
        MacPaint_InvalidatePaintArea();
    }
}

/**
 * MacPaint_EditCopy - Copy selection to clipboard
 */
void MacPaint_EditCopy(void)
{
    if (gMenuState.selectionActive) {
        MacPaint_CopySelectionToClipboard();
        gMenuState.clipboardHasContent = 1;
    }
}

/**
 * MacPaint_EditPaste - Paste from clipboard
 */
void MacPaint_EditPaste(void)
{
    if (gMenuState.clipboardHasContent) {
        MacPaint_SaveUndoState("Paste");
        /* Paste at center of canvas (convenient default position) */
        int centerX = (MACPAINT_DOC_WIDTH / 2) - (72 / 2);  /* Approximate center */
        int centerY = (MACPAINT_DOC_HEIGHT / 2) - (72 / 2);
        MacPaint_PasteFromClipboard(centerX, centerY);
        gMenuState.selectionActive = 1;
        MacPaint_InvalidatePaintArea();
    }
}

/**
 * MacPaint_EditClear - Delete selection
 */
void MacPaint_EditClear(void)
{
    if (gMenuState.selectionActive) {
        MacPaint_SaveUndoState("Clear");

        /* Clear selection area by erasing all pixels (setting to white/0) */
        unsigned char *bits = (unsigned char *)gPaintBuffer.baseAddr;
        int rowBytes = gPaintBuffer.rowBytes;

        Rect bounds;
        if (MacPaint_GetSelection(&bounds)) {
            int width = bounds.right - bounds.left;
            int height = bounds.bottom - bounds.top;

            for (int y = 0; y < height; y++) {
                for (int x = 0; x < width; x++) {
                    int srcX = bounds.left + x;
                    int srcY = bounds.top + y;

                    /* Clear this pixel (set to white) */
                    int byteOffset = srcY * rowBytes + (srcX / 8);
                    int bitOffset = 7 - (srcX % 8);
                    bits[byteOffset] &= ~(1 << bitOffset);
                }
            }

            gDocDirty = 1;
            MacPaint_InvalidatePaintArea();
        }
    }
}

/**
 * MacPaint_EditInvert - Invert colors in selection or whole document
 */
void MacPaint_EditInvert(void)
{
    if (gMenuState.selectionActive) {
        /* Invert pixels within selection rectangle */
        extern Rect gSelectionRect;
        unsigned char *bits = (unsigned char *)gPaintBuffer.baseAddr;
        int rowBytes = gPaintBuffer.rowBytes;

        for (int y = gSelectionRect.top; y < gSelectionRect.bottom; y++) {
            for (int x = gSelectionRect.left; x < gSelectionRect.right; x++) {
                int byteOff = y * rowBytes + (x / 8);
                int bitOff = 7 - (x % 8);
                bits[byteOff] ^= (1 << bitOff);
            }
        }
        gDocDirty = 1;
        MacPaint_InvalidatePaintArea();
    } else {
        /* Invert entire document */
        MacPaint_InvertBuf(&gPaintBuffer);
    }
}

/**
 * MacPaint_EditFill - Fill selection or bucket fill
 */
void MacPaint_EditFill(void)
{
    if (gMenuState.selectionActive) {
        /* Fill selection rectangle with current pattern */
        extern Rect gSelectionRect;
        extern Pattern gCurrentPattern;
        unsigned char *bits = (unsigned char *)gPaintBuffer.baseAddr;
        int rowBytes = gPaintBuffer.rowBytes;

        for (int y = gSelectionRect.top; y < gSelectionRect.bottom; y++) {
            unsigned char patRow = gCurrentPattern.pat[y % 8];
            for (int x = gSelectionRect.left; x < gSelectionRect.right; x++) {
                int patBit = 7 - (x % 8);
                int val = (patRow >> patBit) & 1;

                int byteOff = y * rowBytes + (x / 8);
                int bitOff = 7 - (x % 8);
                if (val) bits[byteOff] |= (1 << bitOff);
                else     bits[byteOff] &= ~(1 << bitOff);
            }
        }
        gDocDirty = 1;
        MacPaint_InvalidatePaintArea();
    }
}

/**
 * MacPaint_EditSelectAll - Select entire canvas
 */
void MacPaint_EditSelectAll(void)
{
    /* Create selection rectangle covering entire canvas */
    MacPaint_CreateSelection(0, 0, MACPAINT_DOC_WIDTH, MACPAINT_DOC_HEIGHT);
    gMenuState.selectionActive = 1;
    MacPaint_InvalidatePaintArea();
}

/*
 * AIDS MENU OPERATIONS (Tools and Options)
 */

/**
 * MacPaint_AidsToggleGrid - Toggle grid display
 */
void MacPaint_AidsToggleGrid(void)
{
    gMenuState.showGrid = !gMenuState.showGrid;
    MacPaint_ToggleGridDisplay();
}

/**
 * MacPaint_AidsToggleFatBits - Toggle Fat Bits zoom mode
 */
void MacPaint_AidsToggleFatBits(void)
{
    gMenuState.fatBitsMode = !gMenuState.fatBitsMode;
    MacPaint_SetFatBitsMode(gMenuState.fatBitsMode, 8);
}

/**
 * MacPaint_AidsPatternEditor - Open pattern editor dialog
 */
void MacPaint_AidsPatternEditor(void)
{
    gMenuState.patternEditorOpen = 1;
    /* TODO: Create modeless dialog for pattern editing
     * Show 8x8 grid of pixels
     * Allow drawing and preview
     */
}

/**
 * MacPaint_AidsBrushEditor - Open brush editor dialog
 */
void MacPaint_AidsBrushEditor(void)
{
    gMenuState.brushEditorOpen = 1;
    /* TODO: Create modeless dialog for brush design
     * Show available brush shapes
     * Allow custom pattern selection
     */
}

/**
 * MacPaint_AidsHelp - Show help information
 */
void MacPaint_AidsHelp(void)
{
    /* TODO: Show help window or alert with MacPaint info
     * Original included help pictures in resources
     */
}

/*
 * FONT AND STYLE MENU OPERATIONS
 */

/**
 * MacPaint_FontSelect - Handle font selection
 * menuItem: font ID from Font menu
 */
void MacPaint_FontSelect(int menuItem)
{
    /* TODO: Set current font for text tool
     * Update menu checkmark
     */
}

/**
 * MacPaint_StyleToggleBold - Toggle bold text style
 */
void MacPaint_StyleToggleBold(void)
{
    /* TODO: Toggle bold bit in text style flags */
}

/**
 * MacPaint_StyleToggleItalic - Toggle italic text style
 */
void MacPaint_StyleToggleItalic(void)
{
    /* TODO: Toggle italic bit in text style flags */
}

/**
 * MacPaint_StyleToggleUnderline - Toggle underline style
 */
void MacPaint_StyleToggleUnderline(void)
{
    /* TODO: Toggle underline bit in text style flags */
}

/**
 * MacPaint_StylePlain - Remove all text styling
 */
void MacPaint_StylePlain(void)
{
    /* TODO: Clear all text style flags */
}

/*
 * COMMAND DISPATCHER
 */

/**
 * MacPaint_HandleMenuCommand - Route menu selection to handler
 * menuID: which menu was selected
 * menuItem: which item in that menu
 */
void MacPaint_HandleMenuCommand(int menuID, int menuItem)
{
    switch (menuID) {
        case MENU_FILE:
            switch (menuItem) {
                case CMD_FILE_NEW:
                    MacPaint_FileNew();
                    break;
                case CMD_FILE_OPEN:
                    MacPaint_FileOpen();
                    break;
                case CMD_FILE_CLOSE:
                    MacPaint_FileClose();
                    break;
                case CMD_FILE_SAVE:
                    MacPaint_FileSave();
                    break;
                case CMD_FILE_SAVE_AS:
                    MacPaint_FileSaveAs();
                    break;
                case CMD_FILE_PRINT:
                    MacPaint_FilePrint();
                    break;
                case CMD_FILE_QUIT:
                    MacPaint_FileQuit();
                    break;
            }
            break;

        case MENU_EDIT:
            switch (menuItem) {
                case CMD_EDIT_UNDO:
                    MacPaint_EditUndo();
                    break;
                case CMD_EDIT_CUT:
                    MacPaint_EditCut();
                    break;
                case CMD_EDIT_COPY:
                    MacPaint_EditCopy();
                    break;
                case CMD_EDIT_PASTE:
                    MacPaint_EditPaste();
                    break;
                case CMD_EDIT_CLEAR:
                    MacPaint_EditClear();
                    break;
                case CMD_EDIT_INVERT:
                    MacPaint_EditInvert();
                    break;
                case CMD_EDIT_FILL:
                    MacPaint_EditFill();
                    break;
                case CMD_EDIT_SELECT_ALL:
                    MacPaint_EditSelectAll();
                    break;
            }
            break;

        case MENU_AIDS:
            switch (menuItem) {
                case CMD_AIDS_GRID:
                    MacPaint_AidsToggleGrid();
                    break;
                case CMD_AIDS_FAT_BITS:
                    MacPaint_AidsToggleFatBits();
                    break;
                case CMD_AIDS_PATTERN_EDIT:
                    MacPaint_AidsPatternEditor();
                    break;
                case CMD_AIDS_BRUSH_EDIT:
                    MacPaint_AidsBrushEditor();
                    break;
                case CMD_AIDS_HELP:
                    MacPaint_AidsHelp();
                    break;
            }
            break;

        case MENU_FONT:
            MacPaint_FontSelect(menuItem);
            break;

        case MENU_STYLE:
            switch (menuItem) {
                case CMD_STYLE_BOLD:
                    MacPaint_StyleToggleBold();
                    break;
                case CMD_STYLE_ITALIC:
                    MacPaint_StyleToggleItalic();
                    break;
                case CMD_STYLE_UNDERLINE:
                    MacPaint_StyleToggleUnderline();
                    break;
                case CMD_STYLE_PLAIN:
                    MacPaint_StylePlain();
                    break;
            }
            break;
    }

    /* Update menus after operation */
    MacPaint_UpdateMenus();
}

/*
 * EVENT HANDLING
 */

/**
 * MacPaint_HandleMouseDown - Process mouse click in window
 * x, y: mouse coordinates in local window coords
 * modifiers: keyboard modifiers (shift, option, command, etc)
 */
void MacPaint_HandleMouseDown(int x, int y, int modifiers)
{
    /* Get current tool and dispatch to tool handler */
    if (gCurrentTool <= TOOL_RECT) {
        MacPaint_HandleToolMouseEvent(gCurrentTool, x, y, 1);
    }
}

/**
 * MacPaint_HandleMouseDrag - Process mouse movement during drag
 * x, y: current mouse position
 */
void MacPaint_HandleMouseDrag(int x, int y)
{
    /* Continue tool operation with new position */
    if (gCurrentTool <= TOOL_RECT) {
        MacPaint_HandleToolMouseEvent(gCurrentTool, x, y, 1);
    }
}

/**
 * MacPaint_HandleMouseUp - Process mouse button release
 * x, y: final mouse position
 */
void MacPaint_HandleMouseUp(int x, int y)
{
    /* Finish tool operation */
    if (gCurrentTool <= TOOL_RECT) {
        MacPaint_HandleToolMouseEvent(gCurrentTool, x, y, 0);
    }
}

/**
 * MacPaint_HandleKeyDown - Process keyboard input
 * keyCode: Macintosh keyboard code
 * modifiers: modifier keys state
 */
void MacPaint_HandleKeyDown(int keyCode, int modifiers)
{
    /* If text tool is active, route printable keys to text input */
    if (gCurrentTool == TOOL_TEXT && MacPaint_TextToolHandleKey(keyCode, modifiers)) {
        return;
    }

    /* Handle keyboard shortcuts */
    if (modifiers & 0x100) {  /* Command key */
        switch (keyCode) {
            case 0x00: /* 'A' - Select All */
                MacPaint_EditSelectAll();
                break;
            case 0x06: /* 'Z' - Undo */
                MacPaint_EditUndo();
                break;
            case 0x07: /* 'X' - Cut */
                MacPaint_EditCut();
                break;
            case 0x08: /* 'C' - Copy */
                MacPaint_EditCopy();
                break;
            case 0x09: /* 'V' - Paste */
                MacPaint_EditPaste();
                break;
            case 0x02: /* 'S' - Save */
                MacPaint_FileSave();
                break;
            case 0x11: /* 'O' - Open */
                MacPaint_FileOpen();
                break;
            case 0x2E: /* 'N' - New */
                MacPaint_FileNew();
                break;
            case 0x0C: /* 'Q' - Quit */
                MacPaint_FileQuit();
                break;
        }
    }

    /* Handle tool selection with number keys (1-0) and letter shortcuts */
    switch (keyCode) {
        /* Number keys 1-0 for quick tool access */
        case 0x12: MacPaint_SelectTool(TOOL_LASSO);       break;    /* 1 = Lasso */
        case 0x13: MacPaint_SelectTool(TOOL_SELECT);      break;    /* 2 = Select */
        case 0x14: MacPaint_SelectTool(TOOL_GRABBER);     break;    /* 3 = Grabber */
        case 0x15: MacPaint_SelectTool(TOOL_TEXT);        break;    /* 4 = Text */
        case 0x17: MacPaint_SelectTool(TOOL_FILL);        break;    /* 5 = Fill */
        case 0x16: MacPaint_SelectTool(TOOL_SPRAY);       break;    /* 6 = Spray */
        case 0x1A: MacPaint_SelectTool(TOOL_BRUSH);       break;    /* 7 = Brush */
        case 0x1C: MacPaint_SelectTool(TOOL_PENCIL);      break;    /* 8 = Pencil */
        case 0x19: MacPaint_SelectTool(TOOL_LINE);        break;    /* 9 = Line */
        case 0x1D: MacPaint_SelectTool(TOOL_ERASE);       break;    /* 0 = Eraser */

        /* Letter shortcuts for tools */
        case 0x0F: MacPaint_SelectTool(TOOL_PENCIL);      break;    /* P = Pencil */
        case 0x02: MacPaint_SelectTool(TOOL_BRUSH);       break;    /* B = Brush */
        case 0x08: MacPaint_SelectTool(TOOL_ERASE);       break;    /* E = Eraser */
        case 0x09: MacPaint_SelectTool(TOOL_FILL);        break;    /* F = Fill */
        case 0x28: MacPaint_SelectTool(TOOL_LINE);        break;    /* L = Line */
        case 0x01: MacPaint_SelectTool(TOOL_SPRAY);       break;    /* S = Spray */
        case 0x32: MacPaint_SelectTool(TOOL_RECT);        break;    /* R = Rectangle */
        case 0x0A: MacPaint_SelectTool(TOOL_OVAL);        break;    /* G = oVal (circle) */
    }
}

/*
 * MENU STATE QUERIES
 */

/**
 * MacPaint_GetMenuState - Query current menu state
 */
void MacPaint_GetMenuState(int *gridShown, int *fatBitsActive,
                          int *undoAvailable, int *selectionActive)
{
    if (gridShown) *gridShown = gMenuState.showGrid;
    if (fatBitsActive) *fatBitsActive = gMenuState.fatBitsMode;
    if (undoAvailable) *undoAvailable = gMenuState.undoAvailable;
    if (selectionActive) *selectionActive = gMenuState.selectionActive;
}

/**
 * MacPaint_SetMenuState - Set menu state for specific conditions
 */
void MacPaint_SetMenuState(int gridShown, int fatBitsActive,
                          int undoAvailable, int selectionActive)
{
    if (gridShown >= 0) gMenuState.showGrid = gridShown;
    if (fatBitsActive >= 0) gMenuState.fatBitsMode = fatBitsActive;
    if (undoAvailable >= 0) gMenuState.undoAvailable = undoAvailable;
    if (selectionActive >= 0) gMenuState.selectionActive = selectionActive;
}

/**
 * MacPaint_SetClipboardState - Update clipboard availability
 */
void MacPaint_SetClipboardState(int hasContent)
{
    gMenuState.clipboardHasContent = hasContent;
}

/*
 * MENU DISPLAY HELPERS
 */

/**
 * MacPaint_GetWindowTitle - Generate window title from document name
 */
const char* MacPaint_GetWindowTitle(void)
{
    static char title[128];

    if (gDocDirty) {
        snprintf(title, sizeof(title), "%s*", gDocName);
    } else {
        strncpy(title, gDocName, sizeof(title) - 1);
        title[sizeof(title) - 1] = '\0';
    }

    return title;
}

/**
 * MacPaint_IsMenuItemAvailable - Check if menu item should be enabled
 */
int MacPaint_IsMenuItemAvailable(int menuID, int menuItem)
{
    switch (menuID) {
        case MENU_FILE:
            /* All file items always available */
            return 1;

        case MENU_EDIT:
            switch (menuItem) {
                case CMD_EDIT_UNDO:
                    return gMenuState.undoAvailable;
                case CMD_EDIT_CUT:
                case CMD_EDIT_COPY:
                case CMD_EDIT_CLEAR:
                    return gMenuState.selectionActive;
                case CMD_EDIT_PASTE:
                    return gMenuState.clipboardHasContent;
                default:
                    return 1;
            }

        case MENU_AIDS:
            return 1;

        default:
            return 1;
    }
}
