/*
 * MacPaint_Integration.c - System 7.1 Integration Layer
 *
 * Integrates MacPaint with System 7.1 managers:
 * - MenuManager: Load and manage application menus
 * - StandardFile: File open/save dialogs
 * - PrintManager: Print document support
 * - WindowManager: Window and document management
 * - DialogManager: Pattern/brush editor dialogs
 * - EventManager: Main event loop
 *
 * This layer bridges the portable MacPaint code with System 7.1 APIs
 */

#include "SystemTypes.h"
#include "Apps/MacPaint.h"
#include "MenuManager/MenuManager.h"
#include "WindowManager/WindowManager.h"
#include "DialogManager/DialogManager.h"
#include "StandardFile/StandardFile.h"
#include "System71StdLib.h"
#include "MemoryMgr/MemoryManager.h"
#include <string.h>

/*
 * WINDOW AND DOCUMENT MANAGEMENT
 */

typedef struct {
    WindowPtr window;           /* Main paint window */
    GrafPtr port;              /* Drawing port */
    Rect paintRect;            /* Canvas area within window */
    int windowOpen;            /* Window currently displayed */
    char documentName[256];    /* Full path or title */
} DocumentWindow;

static DocumentWindow gDocWindow = {0};
static void MacPaint_ResetDocumentWindow(void);
static void MacPaint_UpdateDocumentGeometry(void);

static void MacPaint_ResetDocumentWindow(void)
{
    gDocWindow.window = NULL;
    gDocWindow.port = NULL;
    gDocWindow.paintRect.left = 0;
    gDocWindow.paintRect.top = 0;
    gDocWindow.paintRect.right = MACPAINT_DOC_WIDTH;
    gDocWindow.paintRect.bottom = MACPAINT_DOC_HEIGHT;
    gDocWindow.windowOpen = 0;
}

static void MacPaint_UpdateDocumentGeometry(void)
{
    if (!gDocWindow.window) {
        gDocWindow.port = NULL;
        gDocWindow.paintRect.left = 0;
        gDocWindow.paintRect.top = 0;
        gDocWindow.paintRect.right = MACPAINT_DOC_WIDTH;
        gDocWindow.paintRect.bottom = MACPAINT_DOC_HEIGHT;
        return;
    }

    GrafPtr port = GetWindowPort(gDocWindow.window);
    gDocWindow.port = port;

    if (port) {
        gDocWindow.paintRect.left = port->portRect.left + MACPAINT_TOOLBOX_WIDTH;
        gDocWindow.paintRect.top = port->portRect.top;
        gDocWindow.paintRect.right = port->portRect.right;
        gDocWindow.paintRect.bottom = port->portRect.bottom - MACPAINT_STATUS_HEIGHT;

        if (gDocWindow.paintRect.right < gDocWindow.paintRect.left) {
            gDocWindow.paintRect.right = gDocWindow.paintRect.left;
        }

        if (gDocWindow.paintRect.bottom < gDocWindow.paintRect.top) {
            gDocWindow.paintRect.bottom = gDocWindow.paintRect.top;
        }
    } else {
        gDocWindow.paintRect.left = 0;
        gDocWindow.paintRect.top = 0;
        gDocWindow.paintRect.right = MACPAINT_DOC_WIDTH;
        gDocWindow.paintRect.bottom = MACPAINT_DOC_HEIGHT;
    }
}

/**
 * MacPaint_CreateWindow - Create main paint window
 */
OSErr MacPaint_CreateWindow(void)
{
    if (gDocWindow.windowOpen) {
        return noErr;  /* Already open */
    }

    MacPaint_ResetDocumentWindow();

    return noErr;
}

/**
 * MacPaint_GetWindowPtr - Get current window pointer
 */
WindowPtr MacPaint_GetWindowPtr(void)
{
    return gDocWindow.window;
}

/**
 * MacPaint_GetPaintRect - Get canvas drawing rectangle
 */
void MacPaint_GetPaintRect(Rect *rect)
{
    if (!rect) {
        return;
    }

    MacPaint_UpdateDocumentGeometry();
    *rect = gDocWindow.paintRect;
}

/**
 * MacPaint_RegisterMainWindow - Bind the QuickDraw window to document state
 */
void MacPaint_RegisterMainWindow(WindowPtr window)
{
    if (!window) {
        MacPaint_ResetDocumentWindow();
        return;
    }

    gDocWindow.window = window;
    gDocWindow.windowOpen = 1;
    MacPaint_UpdateDocumentGeometry();
}

/**
 * MacPaint_InvalidateWindow - Mark window for redraw
 */
void MacPaint_InvalidateWindow(void)
{
    if (gDocWindow.window) {
        InvalRect(&gDocWindow.paintRect);
    }
}

/**
 * MacPaint_CloseWindow - Close paint window
 */
void MacPaint_CloseWindow(void)
{
    if (gDocWindow.window) {
        DisposeWindow(gDocWindow.window);
        gDocWindow.window = NULL;
    }

    MacPaint_ResetDocumentWindow();
}

/**
 * MacPaint_UpdateWindowTitle - Update window title with document name
 */
void MacPaint_UpdateWindowTitle(void)
{
    if (!gDocWindow.window) {
        return;
    }

    /* Build Pascal string title from document name */
    const char *name = gDocWindow.documentName;
    if (!name || !name[0]) {
        name = "Untitled";
    }
    Str255 title;
    size_t len = strlen(name);
    if (len > 255) len = 255;
    title[0] = (unsigned char)len;
    memcpy(&title[1], name, len);
    SetWTitle(gDocWindow.window, title);
}

/*
 * MENU MANAGER INTEGRATION
 */

typedef struct {
    MenuHandle fileMenu;       /* MENU 129 */
    MenuHandle editMenu;       /* MENU 130 */
    MenuHandle fontMenu;       /* MENU 131 */
    MenuHandle styleMenu;      /* MENU 132 */
    MenuHandle aidsMenu;       /* MENU 133 */
    int menuBarInitialized;
} MenuBarState;

static MenuBarState gMenuBar = {0};

/**
 * MacPaint_InitializeMenuBar - Load and install menus from resources
 */
OSErr MacPaint_InitializeMenuBar(void)
{
    MenuHandle menu;

    /* Initialize MenuManager */
    InitMenus();

    /* Create Apple menu (128) - will contain desk accessories */
    { static unsigned char appleTitle[] = {1, 0x14};
    menu = NewMenu(128, appleTitle); }  /* Apple menu */
    if (menu) {
        AppendMenu(menu, "About MacPaint");
        gMenuBar.fileMenu = menu;  /* Reuse for now */
        InsertMenu(menu, 0);
    }

    /* Create File menu (129) */
    menu = NewMenu(129, "File");
    if (menu) {
        AppendMenu(menu, "New");
        AppendMenu(menu, "Open");
        AppendMenu(menu, "Close");
        AppendMenu(menu, "-");
        AppendMenu(menu, "Save");
        AppendMenu(menu, "Save As...");
        AppendMenu(menu, "-");
        AppendMenu(menu, "Print");
        AppendMenu(menu, "-");
        AppendMenu(menu, "Quit");
        gMenuBar.fileMenu = menu;
        InsertMenu(menu, 0);
    }

    /* Create Edit menu (130) */
    menu = NewMenu(130, "Edit");
    if (menu) {
        AppendMenu(menu, "Undo");
        AppendMenu(menu, "-");
        AppendMenu(menu, "Cut");
        AppendMenu(menu, "Copy");
        AppendMenu(menu, "Paste");
        AppendMenu(menu, "Clear");
        AppendMenu(menu, "-");
        AppendMenu(menu, "Select All");
        AppendMenu(menu, "Invert");
        gMenuBar.editMenu = menu;
        InsertMenu(menu, 0);
    }

    /* Create Font menu (131) */
    menu = NewMenu(131, "Font");
    if (menu) {
        AppendMenu(menu, "Chicago");
        AppendMenu(menu, "Geneva");
        AppendMenu(menu, "New York");
        gMenuBar.fontMenu = menu;
        InsertMenu(menu, 0);
    }

    /* Create Style menu (132) */
    menu = NewMenu(132, "Style");
    if (menu) {
        AppendMenu(menu, "Bold");
        AppendMenu(menu, "Italic");
        AppendMenu(menu, "Underline");
        AppendMenu(menu, "-");
        AppendMenu(menu, "Plain");
        gMenuBar.styleMenu = menu;
        InsertMenu(menu, 0);
    }

    /* Create Aids menu (133) - Tools and options */
    menu = NewMenu(133, "Aids");
    if (menu) {
        AppendMenu(menu, "Grid");
        AppendMenu(menu, "Fat Bits");
        AppendMenu(menu, "-");
        AppendMenu(menu, "Pattern Editor");
        AppendMenu(menu, "Brush Editor");
        AppendMenu(menu, "-");
        AppendMenu(menu, "About");
        gMenuBar.aidsMenu = menu;
        InsertMenu(menu, 0);
    }

    /* Initialize MacPaint menu state tracking */
    MacPaint_InitializeMenus();

    /* Draw the menu bar */
    DrawMenuBar();

    gMenuBar.menuBarInitialized = 1;

    return noErr;
}

/**
 * MacPaint_HandleMenuSelection - Process MenuManager menu selection
 */
void MacPaint_HandleMenuSelection(int menuID, int itemID)
{
    /* Dispatch to menu handler from MacPaint_Menus.c */
    MacPaint_HandleMenuCommand(menuID, itemID);
}

/**
 * MacPaint_AdjustMenus - Enable/disable menu items based on state
 */
void MacPaint_AdjustMenus(void)
{
    MacPaint_UpdateMenus();

    /* Update Edit menu items based on state */
    if (gMenuBar.editMenu) {
        if (MacPaint_IsMenuItemAvailable(130, 1)) {
            EnableItem(gMenuBar.editMenu, 1);  /* Undo */
        } else {
            DisableItem(gMenuBar.editMenu, 1);
        }

        /* Cut/Copy/Clear enabled only with selection */
        if (MacPaint_IsMenuItemAvailable(130, 3)) {
            EnableItem(gMenuBar.editMenu, 3);  /* Cut */
            EnableItem(gMenuBar.editMenu, 4);  /* Copy */
        } else {
            DisableItem(gMenuBar.editMenu, 3);
            DisableItem(gMenuBar.editMenu, 4);
        }

        if (MacPaint_IsMenuItemAvailable(130, 5)) {
            EnableItem(gMenuBar.editMenu, 5);  /* Paste */
        } else {
            DisableItem(gMenuBar.editMenu, 5);
        }

        if (MacPaint_IsMenuItemAvailable(130, 6)) {
            EnableItem(gMenuBar.editMenu, 6);  /* Clear */
        } else {
            DisableItem(gMenuBar.editMenu, 6);
        }
    }

    /* Update Aids menu for toggles */
    if (gMenuBar.aidsMenu) {
        int gridShown = 0, fatBitsActive = 0;
        MacPaint_GetMenuState(&gridShown, &fatBitsActive, NULL, NULL);

        if (gridShown) {
            CheckItem(gMenuBar.aidsMenu, 1, 1);  /* Grid */
        } else {
            CheckItem(gMenuBar.aidsMenu, 1, 0);
        }

        if (fatBitsActive) {
            CheckItem(gMenuBar.aidsMenu, 2, 1);  /* Fat Bits */
        } else {
            CheckItem(gMenuBar.aidsMenu, 2, 0);
        }
    }
}

/*
 * STANDARD FILE DIALOGS
 */

/**
 * MacPaint_DoOpenDialog - Show Standard File open dialog
 * Returns filename in path, or empty string if cancelled
 */
int MacPaint_DoOpenDialog(char *path, int pathLen)
{
    StandardFileReply reply;

    if (!path || pathLen <= 0) {
        return 0;
    }

    /* Show Standard File open dialog */
    StandardGetFile(NULL, -1, NULL, &reply);

    /* Check if user selected a file (not cancelled) */
    if (!reply.sfGood) {
        return 0;  /* User cancelled */
    }

    /* Copy filename to path buffer
     * sfFile.name is a Pascal string (length byte + ASCII data)
     */
    if (reply.sfFile.name[0] > 0 && reply.sfFile.name[0] < pathLen) {
        strncpy(path, (const char *)&reply.sfFile.name[1], reply.sfFile.name[0]);
        path[reply.sfFile.name[0]] = '\0';
        return 1;  /* Success */
    }

    return 0;
}

/**
 * MacPaint_DoSaveDialog - Show Standard File save dialog
 * Returns filename in path, or empty string if cancelled
 */
int MacPaint_DoSaveDialog(char *path, int pathLen)
{
    StandardFileReply reply;
    unsigned char promptStr[256];
    unsigned char defaultName[256];

    if (!path || pathLen <= 0) {
        return 0;
    }

    /* Convert C string to Pascal string for prompt
     * Format: length byte followed by ASCII string
     */
    const char *promptCStr = "Save Picture As:";
    int promptLen = strlen(promptCStr);
    if (promptLen > 255) promptLen = 255;
    promptStr[0] = promptLen;
    strncpy((char *)&promptStr[1], promptCStr, promptLen);

    /* Convert current path to Pascal string for default name */
    int pathLen2 = strlen(path);
    if (pathLen2 > 255) pathLen2 = 255;
    defaultName[0] = pathLen2;
    strncpy((char *)&defaultName[1], path, pathLen2);

    /* Show Standard File save dialog */
    StandardPutFile(promptStr, defaultName, &reply);

    /* Check if user selected a file (not cancelled) */
    if (!reply.sfGood) {
        return 0;  /* User cancelled */
    }

    /* Copy filename from sfFile.name (Pascal string) to path buffer */
    if (reply.sfFile.name[0] > 0 && reply.sfFile.name[0] < pathLen) {
        strncpy(path, (const char *)&reply.sfFile.name[1], reply.sfFile.name[0]);
        path[reply.sfFile.name[0]] = '\0';
        return 1;  /* Success */
    }

    return 0;
}

/*
 * PRINT MANAGER INTEGRATION
 */

/**
 * MacPaint_DoPrintDialog - Show print dialog and get settings
 */
int MacPaint_DoPrintDialog(void)
{
    /* TODO: Use PrintManager to show print dialog
     * PrOpen() - open printer driver
     * PrJobDialog(...) - show job settings
     * if cancelled: return 0
     * Store print record for printing
     */

    return 0;  /* Placeholder */
}

/**
 * MacPaint_PrintDocument - Print current document
 */
OSErr MacPaint_PrintDocument(void)
{
    /* TODO: Print the paint buffer
     * PrOpenDoc() - start document
     * PrOpenPage() - start page
     * Render bitmap to printer port
     * PrClosePage() - end page
     * PrCloseDoc() - end document
     * PrClose() - close printer
     */

    return noErr;  /* Placeholder */
}

/*
 * DIALOG FRAMEWORK FOR EDITORS
 */

typedef struct {
    DialogPtr dialog;           /* Dialog window pointer */
    int itemHit;               /* Last item clicked */
    int isModal;               /* Modal vs modeless dialog */
} DialogState;

static DialogState gPatternDlg = {0};
static DialogState gBrushDlg = {0};

/**
 * MacPaint_CreatePatternEditorDialog - Create pattern editor window
 */
OSErr MacPaint_CreatePatternEditorDialog(void)
{
    if (gPatternDlg.dialog) {
        return noErr;  /* Already open */
    }

    /* TODO: Use DialogManager to create modeless dialog
     * GetNewDialog(500, NULL, (WindowPtr)-1)  // Resource ID 500
     * Set up 8x8 grid of buttons/checkboxes for pixels
     * Add OK/Cancel/Reset buttons
     * ShowWindow(dialog)
     */

    MacPaint_OpenPatternEditor();
    return noErr;
}

/**
 * MacPaint_PatternEditorEventHandler - Handle dialog events
 */
int MacPaint_PatternEditorEventHandler(int itemHit)
{
    if (!gPatternDlg.dialog) {
        return 0;
    }

    switch (itemHit) {
        case 1: /* OK button */
            /* TODO: Save pattern and close */
            MacPaint_ClosePatternEditorDialog();
            return 1;

        case 2: /* Cancel button */
            MacPaint_ClosePatternEditorDialog();
            return 1;

        case 3: /* Reset button */
            /* TODO: Restore original pattern */
            return 1;

        default:
            if (itemHit >= 4 && itemHit <= 67) {
                /* Pixel grid items (64 pixels in 8x8 grid) */
                int x = (itemHit - 4) % 8;
                int y = (itemHit - 4) / 8;
                MacPaint_PatternEditorPixelClick(x, y);
                /* TODO: Redraw pattern preview */
                return 0;  /* Continue dialog */
            }
            break;
    }

    return 0;
}

/**
 * MacPaint_ClosePatternEditorDialog - Close pattern editor
 */
void MacPaint_ClosePatternEditorDialog(void)
{
    if (gPatternDlg.dialog) {
        DisposeDialog(gPatternDlg.dialog);
        gPatternDlg.dialog = NULL;
    }

    MacPaint_ClosePatternEditor();
}

/**
 * MacPaint_CreateBrushEditorDialog - Create brush editor window
 */
OSErr MacPaint_CreateBrushEditorDialog(void)
{
    if (gBrushDlg.dialog) {
        return noErr;  /* Already open */
    }

    /* TODO: Use DialogManager to create modeless dialog
     * GetNewDialog(501, NULL, (WindowPtr)-1)  // Resource ID 501
     * Set up brush shape radio buttons
     * Add size slider or spinner control
     * Add OK/Cancel buttons
     * ShowWindow(dialog)
     */

    MacPaint_OpenBrushEditor();
    return noErr;
}

/**
 * MacPaint_BrushEditorEventHandler - Handle dialog events
 */
int MacPaint_BrushEditorEventHandler(int itemHit)
{
    if (!gBrushDlg.dialog) {
        return 0;
    }

    switch (itemHit) {
        case 1: /* OK button */
            MacPaint_CloseBrushEditorDialog();
            return 1;

        case 2: /* Cancel button */
            MacPaint_CloseBrushEditorDialog();
            return 1;

        default:
            return 0;
    }
}

/**
 * MacPaint_CloseBrushEditorDialog - Close brush editor
 */
void MacPaint_CloseBrushEditorDialog(void)
{
    if (gBrushDlg.dialog) {
        DisposeDialog(gBrushDlg.dialog);
        gBrushDlg.dialog = NULL;
    }

    MacPaint_CloseBrushEditor();
}

/*
 * EVENT LOOP INTEGRATION
 * MacPaint_RunEventLoop is implemented in MacPaint_EventLoop.c
 * This provides the full WaitNextEvent-based event dispatch system
 */

/*
 * DRAG AND DROP SUPPORT
 */

/**
 * MacPaint_CanAcceptDraggedFile - Check if file can be dropped on window
 */
int MacPaint_CanAcceptDraggedFile(const char *filename)
{
    if (!filename) {
        return 0;
    }

    /* Find file extension */
    const char *ext = strrchr(filename, '.');
    if (!ext) {
        /* No extension - check if it could be a classic MacPaint file */
        /* Classic MacPaint files had no extension */
        return 1;
    }

    /* Check for supported extensions (case-insensitive) */
    ext++;  /* Skip the dot */

    /* MacPaint format */
    if (strcasecmp(ext, "paint") == 0 || strcasecmp(ext, "pnt") == 0) {
        return 1;
    }

    /* PICT format */
    if (strcasecmp(ext, "pict") == 0 || strcasecmp(ext, "pct") == 0) {
        return 1;
    }

    /* BMP format */
    if (strcasecmp(ext, "bmp") == 0) {
        return 1;
    }

    /* PNG format */
    if (strcasecmp(ext, "png") == 0) {
        return 1;
    }

    return 0;  /* Unsupported format */
}

/**
 * MacPaint_HandleDroppedFile - Process dropped file
 */
OSErr MacPaint_HandleDroppedFile(const char *filename)
{
    /* TODO: Open dropped file
     * Check if current document needs saving
     * Load file using MacPaint_OpenDocument
     */

    return noErr;  /* Placeholder */
}

/*
 * COMMAND ROUTING AND DISPATCH
 */

/**
 * MacPaint_ExecuteMenuCommand - Execute menu command via system integration
 * This wraps the lower-level menu handlers with system cleanup
 */
OSErr MacPaint_ExecuteMenuCommand(int menuID, int itemID)
{
    /* Call the menu command handler */
    MacPaint_HandleMenuCommand(menuID, itemID);

    /* Update window title if document was modified */
    MacPaint_UpdateWindowTitle();

    /* Adjust menus for new state */
    MacPaint_AdjustMenus();

    /* Redraw window if needed */
    if (gDocWindow.windowOpen) {
        MacPaint_InvalidateWindow();
    }

    return noErr;
}

/*
 * RESOURCE LOADING
 */

/**
 * MacPaint_LoadApplicationResources - Load all application resources
 */
OSErr MacPaint_LoadApplicationResources(void)
{
    /* TODO: Load resources from application file
     * OSErr err;
     * - Menus (MENU resources)
     * - Dialogs (DLOG resources)
     * - Icons (ICON resources)
     * - Patterns (PAT# resources)
     * - Cursors (CURS resources)
     * - Strings (STR resources)
     */

    return noErr;
}

/**
 * MacPaint_ReleaseApplicationResources - Unload resources
 */
void MacPaint_ReleaseApplicationResources(void)
{
    /* TODO: Release loaded resources
     * - Close resource file
     * - Flush resource cache if needed
     */
}

/*
 * INITIALIZATION AND CLEANUP
 */

/**
 * MacPaint_InitializeSystem - Set up System 7.1 integration
 */
OSErr MacPaint_InitializeSystem(void)
{
    OSErr err;

    /* Initialize window system */
    err = MacPaint_CreateWindow();
    if (err != noErr) {
        return err;
    }

    /* Load application resources */
    err = MacPaint_LoadApplicationResources();
    if (err != noErr) {
        return err;
    }

    /* Initialize menu bar */
    err = MacPaint_InitializeMenuBar();
    if (err != noErr) {
        return err;
    }

    /* Initial menu adjustment */
    MacPaint_AdjustMenus();

    return noErr;
}

/**
 * MacPaint_ShutdownSystem - Clean up System 7.1 integration
 */
void MacPaint_ShutdownSystem(void)
{
    /* Close any open dialogs */
    MacPaint_ClosePatternEditorDialog();
    MacPaint_CloseBrushEditorDialog();

    /* Close main window */
    MacPaint_CloseWindow();

    /* Release resources */
    MacPaint_ReleaseApplicationResources();
}

/*
 * STATE QUERIES FOR UI
 */

/**
 * MacPaint_IsWindowOpen - Check if main window is visible
 */
int MacPaint_IsWindowOpen(void)
{
    return gDocWindow.windowOpen;
}

/**
 * MacPaint_GetMenuBarState - Get menu bar initialization status
 */
int MacPaint_GetMenuBarState(void)
{
    return gMenuBar.menuBarInitialized;
}

/*
 * ACCESSORS FOR SYSTEM STATE
 */

/**
 * MacPaint_SetDocumentName - Update document name (for title bar, etc)
 */
void MacPaint_SetDocumentName(const char *name)
{
    if (name) {
        strncpy(gDocWindow.documentName, name, sizeof(gDocWindow.documentName) - 1);
        gDocWindow.documentName[sizeof(gDocWindow.documentName) - 1] = '\0';
        MacPaint_UpdateWindowTitle();
    }
}

/**
 * MacPaint_GetDocumentName - Get current document name
 */
const char* MacPaint_GetDocumentName(void)
{
    return gDocWindow.documentName;
}
