/*
 * STMenus.c - SimpleText Menu Management
 *
 * Handles all menu creation, updating, and command dispatch
 */

#include <string.h>
#include "Apps/SimpleText.h"
#include "MemoryMgr/MemoryManager.h"

/* Utility macros for packing/unpacking longs */
#define HiWord(x) ((short)(((unsigned long)(x) >> 16) & 0xFFFF))
#define LoWord(x) ((short)((unsigned long)(x) & 0xFFFF))

/* Menu item strings */
static const unsigned char kAppleMenuItems[] = {22, 'A','b','o','u','t',' ','S','i','m','p','l','e','T','e','x','t','.','.','.', ';','-'};
static const unsigned char kFileMenuItems[] = {89, 'N','e','w','/','N',';','O','p','e','n','.','.','.','/','O',';','-',';','C','l','o','s','e','/','W',';','S','a','v','e','/','S',';','S','a','v','e',' ','A','s','.','.','.','/','S',';','-',';','P','a','g','e',' ','S','e','t','u','p','.','.','.',';','P','r','i','n','t','.','.','.','/','P',';','-',';','Q','u','i','t','/','Q'};
static const unsigned char kEditMenuItems[] = {47, 'U','n','d','o','/','Z',';','-',';','C','u','t','/','X',';','C','o','p','y','/','C',';','P','a','s','t','e','/','V',';','C','l','e','a','r',';','-',';','S','e','l','e','c','t',' ','A','l','l','/','A'};
static const unsigned char kFontMenuItems[] = {22, 'M','o','n','a','c','o',';','G','e','n','e','v','a',';','C','h','i','c','a','g','o'};
static const unsigned char kSizeMenuItems[] = {62, '9',' ','P','o','i','n','t',';','1','0',' ','P','o','i','n','t',';','1','2',' ','P','o','i','n','t',';','1','4',' ','P','o','i','n','t',';','1','8',' ','P','o','i','n','t',';','2','4',' ','P','o','i','n','t'};
static const unsigned char kStyleMenuItems[] = {28, 'P','l','a','i','n',';','B','o','l','d',';','I','t','a','l','i','c',';','U','n','d','e','r','l','i','n','e'};

/* Static helper functions */
static void HandleAppleMenu(short item);
static void HandleFileMenu(short item);
static void HandleEditMenu(short item);
static void HandleFontMenu(short item);
static void HandleSizeMenu(short item);
static void HandleStyleMenu(short item);
static void UpdateFileMenu(void);
static void UpdateEditMenu(void);
static void UpdateFontMenu(void);
static void UpdateSizeMenu(void);
static void UpdateStyleMenu(void);

/*
 * STMenu_Init - Initialize all menus (create but don't install)
 *
 * System 7 Design: Applications create menus during init but do NOT insert them.
 * Menus are installed when the app's window becomes active, and removed when inactive.
 */
void STMenu_Init(void) {
    Handle menuBar;
    extern void serial_puts(const char*);

    serial_puts("[ST] STMenu_Init: Creating menus (not yet installing)\n");

    /* Create Apple menu - but DON'T insert yet */
    static unsigned char appleTitle[] = {1, 0x14};  /* Apple symbol */
    g_ST.appleMenu = NewMenu(mApple, appleTitle);
    if (g_ST.appleMenu) {
        AppendMenu(g_ST.appleMenu, kAppleMenuItems);
        /* DO NOT call InsertMenu here - menus installed on activate */
    }

    /* Create File menu - but DON'T insert yet */
    static unsigned char fileTitle[] = {4, 'F','i','l','e'};
    g_ST.fileMenu = NewMenu(mFile, fileTitle);
    if (g_ST.fileMenu) {
        AppendMenu(g_ST.fileMenu, kFileMenuItems);
        /* DO NOT call InsertMenu here - menus installed on activate */
    }

    /* Create Edit menu - but DON'T insert yet */
    static unsigned char editTitle[] = {4, 'E','d','i','t'};
    g_ST.editMenu = NewMenu(mEdit, editTitle);
    if (g_ST.editMenu) {
        AppendMenu(g_ST.editMenu, kEditMenuItems);
        /* DO NOT call InsertMenu here - menus installed on activate */
    }

    /* Create Font menu - but DON'T insert yet */
    static unsigned char fontTitle[] = {4, 'F','o','n','t'};
    g_ST.fontMenu = NewMenu(mFont, fontTitle);
    if (g_ST.fontMenu) {
        AppendMenu(g_ST.fontMenu, kFontMenuItems);
        /* DO NOT call InsertMenu here - menus installed on activate */
    }

    /* Create Size menu - but DON'T insert yet */
    static unsigned char sizeTitle[] = {4, 'S','i','z','e'};
    g_ST.sizeMenu = NewMenu(mSize, sizeTitle);
    if (g_ST.sizeMenu) {
        AppendMenu(g_ST.sizeMenu, kSizeMenuItems);
        /* DO NOT call InsertMenu here - menus installed on activate */
    }

    /* Create Style menu - but DON'T insert yet */
    static unsigned char styleTitle[] = {5, 'S','t','y','l','e'};
    g_ST.styleMenu = NewMenu(mStyle, styleTitle);
    if (g_ST.styleMenu) {
        AppendMenu(g_ST.styleMenu, kStyleMenuItems);
        /* DO NOT call InsertMenu here - menus installed on activate */
    }

    /* DO NOT call DrawMenuBar here - menus are drawn when installed on activate */

    serial_puts("[ST] STMenu_Init: Menus created successfully (not yet installed)\n");
}

/*
 * STMenu_Dispose - Dispose all menus
 */
void STMenu_Dispose(void) {
    ST_Log("Disposing menus\n");

    /* Menus are disposed automatically when app quits */
    /* But we can clear our references */
    g_ST.appleMenu = NULL;
    g_ST.fileMenu = NULL;
    g_ST.editMenu = NULL;
    g_ST.fontMenu = NULL;
    g_ST.sizeMenu = NULL;
    g_ST.styleMenu = NULL;
}

/*
 * STMenu_Handle - Handle menu command
 */
void STMenu_Handle(long menuResult) {
    short menuID = HiWord(menuResult);
    short item = LoWord(menuResult);

    if (menuID == 0 || item == 0) return;

    ST_Log("Menu command: menu=%d item=%d\n", menuID, item);

    switch (menuID) {
        case mApple:
            HandleAppleMenu(item);
            break;

        case mFile:
            HandleFileMenu(item);
            break;

        case mEdit:
            HandleEditMenu(item);
            break;

        case mFont:
            HandleFontMenu(item);
            break;

        case mSize:
            HandleSizeMenu(item);
            break;

        case mStyle:
            HandleStyleMenu(item);
            break;
    }
}

/*
 * STMenu_Update - Update menu enable states
 */
void STMenu_Update(void) {
    UpdateFileMenu();
    UpdateEditMenu();
    UpdateFontMenu();
    UpdateSizeMenu();
    UpdateStyleMenu();
}

/*
 * STMenu_Install - Install menus into the menu bar
 *
 * Called when a SimpleText window becomes active.
 * This follows System 7 design: each app installs its menus when active.
 */
void STMenu_Install(void) {
    extern void serial_puts(const char*);
    serial_puts("[ST] STMenu_Install: Installing SimpleText menus into menu bar\n");

    /* Insert menus in order: Apple, File, Edit, Font, Size, Style */
    if (g_ST.appleMenu) {
        InsertMenu(g_ST.appleMenu, 0);
    }
    if (g_ST.fileMenu) {
        InsertMenu(g_ST.fileMenu, 0);
    }
    if (g_ST.editMenu) {
        InsertMenu(g_ST.editMenu, 0);
    }
    if (g_ST.fontMenu) {
        InsertMenu(g_ST.fontMenu, 0);
    }
    if (g_ST.sizeMenu) {
        InsertMenu(g_ST.sizeMenu, 0);
    }
    if (g_ST.styleMenu) {
        InsertMenu(g_ST.styleMenu, 0);
    }

    /* Redraw menu bar to show our menus */
    DrawMenuBar();

    serial_puts("[ST] STMenu_Install: Menus installed successfully\n");
}

/*
 * STMenu_Remove - Remove menus from the menu bar
 *
 * Called when all SimpleText windows are deactivated.
 * This allows Finder (or other apps) to show their own menus.
 */
void STMenu_Remove(void) {
    extern void serial_puts(const char*);
    serial_puts("[ST] STMenu_Remove: Removing SimpleText menus from menu bar\n");

    /* Delete menus in reverse order */
    if (g_ST.styleMenu) {
        DeleteMenu(mStyle);
    }
    if (g_ST.sizeMenu) {
        DeleteMenu(mSize);
    }
    if (g_ST.fontMenu) {
        DeleteMenu(mFont);
    }
    if (g_ST.editMenu) {
        DeleteMenu(mEdit);
    }
    if (g_ST.fileMenu) {
        DeleteMenu(mFile);
    }
    if (g_ST.appleMenu) {
        DeleteMenu(mApple);
    }

    /* Redraw menu bar (will show Finder's menus or be empty) */
    DrawMenuBar();

    serial_puts("[ST] STMenu_Remove: Menus removed successfully\n");
}

/*
 * STMenu_EnableItem - Enable or disable menu item
 */
void STMenu_EnableItem(MenuHandle menu, short item, Boolean enable) {
    if (!menu) return;

    if (enable) {
        EnableItem(menu, item);
    } else {
        DisableItem(menu, item);
    }
}

/*
 * STMenu_CheckItem - Check or uncheck menu item
 */
void STMenu_CheckItem(MenuHandle menu, short item, Boolean check) {
    if (!menu) return;

    CheckItem(menu, item, check);
}

/* ============================================================================
 * Menu Handlers
 * ============================================================================ */

/*
 * HandleAppleMenu - Handle Apple menu commands
 */
static void HandleAppleMenu(short item) {
    switch (item) {
        case iAbout:
            ST_ShowAbout();
            break;
    }
}

/*
 * HandleFileMenu - Handle File menu commands
 */
static void HandleFileMenu(short item) {
    STDocument* doc = g_ST.activeDoc;
    char path[512];

    switch (item) {
        case iNew:
            STDoc_New();
            break;

        case iOpen:
            if (STIO_OpenDialog(path)) {
                SimpleText_OpenFile(path);
            }
            break;

        case iClose:
            if (doc) {
                STDoc_Close(doc);
            }
            break;

        case iSave:
            if (doc) {
                STDoc_Save(doc);
            }
            break;

        case iSaveAs:
            if (doc) {
                STDoc_SaveAs(doc);
            }
            break;

        case iPageSetup:
            /* TODO: Page setup dialog */
            ST_Log("Page Setup not implemented\n");
            break;

        case iPrint:
            /* TODO: Print dialog */
            ST_Log("Print not implemented\n");
            break;

        case iQuit:
            SimpleText_Quit();
            break;
    }
}

/*
 * HandleEditMenu - Handle Edit menu commands
 */
static void HandleEditMenu(short item) {
    STDocument* doc = g_ST.activeDoc;

    if (!doc) return;

    switch (item) {
        case iUndo:
            STClip_Undo(doc);
            break;

        case iCut:
            STClip_Cut(doc);
            break;

        case iCopy:
            STClip_Copy(doc);
            break;

        case iPaste:
            STClip_Paste(doc);
            break;

        case iClear:
            STClip_Clear(doc);
            break;

        case iSelectAll:
            STClip_SelectAll(doc);
            break;
    }
}

/*
 * HandleFontMenu - Handle Font menu commands
 */
static void HandleFontMenu(short item) {
    STDocument* doc = g_ST.activeDoc;
    SInt16 fontID = 0;

    if (!doc) return;

    switch (item) {
        case iMonaco:
            fontID = monaco;
            break;
        case iGeneva:
            fontID = geneva;
            break;
        case iChicago:
            fontID = 0 /* system font */;
            break;
    }

    if (fontID) {
        STView_SetStyle(doc, fontID, g_ST.currentSize, g_ST.currentStyle);
        g_ST.currentFont = fontID;
        UpdateFontMenu();
    }
}

/*
 * HandleSizeMenu - Handle Size menu commands
 */
static void HandleSizeMenu(short item) {
    STDocument* doc = g_ST.activeDoc;
    SInt16 size = 0;

    if (!doc) return;

    switch (item) {
        case iSize9:
            size = 9;
            break;
        case iSize10:
            size = 10;
            break;
        case iSize12:
            size = 12;
            break;
        case iSize14:
            size = 14;
            break;
        case iSize18:
            size = 18;
            break;
        case iSize24:
            size = 24;
            break;
    }

    if (size) {
        STView_SetStyle(doc, g_ST.currentFont, size, g_ST.currentStyle);
        g_ST.currentSize = size;
        UpdateSizeMenu();
    }
}

/*
 * HandleStyleMenu - Handle Style menu commands
 */
static void HandleStyleMenu(short item) {
    STDocument* doc = g_ST.activeDoc;
    Style newStyle = g_ST.currentStyle;

    if (!doc) return;

    switch (item) {
        case iPlain:
            newStyle = normal;
            break;

        case iBold:
            /* Toggle bold */
            if (newStyle & bold) {
                newStyle &= ~bold;
            } else {
                newStyle |= bold;
            }
            break;

        case iItalic:
            /* Toggle italic */
            if (newStyle & italic) {
                newStyle &= ~italic;
            } else {
                newStyle |= italic;
            }
            break;

        case iUnderline:
            /* Toggle underline */
            if (newStyle & underline) {
                newStyle &= ~underline;
            } else {
                newStyle |= underline;
            }
            break;
    }

    STView_SetStyle(doc, g_ST.currentFont, g_ST.currentSize, newStyle);
    g_ST.currentStyle = newStyle;
    UpdateStyleMenu();
}

/* ============================================================================
 * Menu Update Functions
 * ============================================================================ */

/*
 * UpdateFileMenu - Update File menu items
 */
static void UpdateFileMenu(void) {
    Boolean hasDoc = (g_ST.activeDoc != NULL);
    Boolean isDirty = hasDoc && g_ST.activeDoc->dirty;

    STMenu_EnableItem(g_ST.fileMenu, iClose, hasDoc);
    STMenu_EnableItem(g_ST.fileMenu, iSave, isDirty);
    STMenu_EnableItem(g_ST.fileMenu, iSaveAs, hasDoc);
    STMenu_EnableItem(g_ST.fileMenu, iPageSetup, false);  /* Not implemented */
    STMenu_EnableItem(g_ST.fileMenu, iPrint, false);      /* Not implemented */
}

/*
 * UpdateEditMenu - Update Edit menu items
 */
static void UpdateEditMenu(void) {
    Boolean hasDoc = (g_ST.activeDoc != NULL);
    Boolean hasSelection = false;
    Boolean canPaste = STClip_HasText();
    Boolean canUndo = false;

    if (hasDoc && g_ST.activeDoc->hTE) {
        SInt16 selStart = (*g_ST.activeDoc->hTE)->selStart;
        SInt16 selEnd = (*g_ST.activeDoc->hTE)->selEnd;
        hasSelection = (selStart != selEnd);
        canUndo = (g_ST.activeDoc->undoText != NULL);
    }

    STMenu_EnableItem(g_ST.editMenu, iUndo, canUndo);
    STMenu_EnableItem(g_ST.editMenu, iCut, hasSelection);
    STMenu_EnableItem(g_ST.editMenu, iCopy, hasSelection);
    STMenu_EnableItem(g_ST.editMenu, iPaste, canPaste);
    STMenu_EnableItem(g_ST.editMenu, iClear, hasSelection);
    STMenu_EnableItem(g_ST.editMenu, iSelectAll, hasDoc);
}

/*
 * UpdateFontMenu - Update Font menu checks
 */
static void UpdateFontMenu(void) {
    STMenu_CheckItem(g_ST.fontMenu, iMonaco, g_ST.currentFont == monaco);
    STMenu_CheckItem(g_ST.fontMenu, iGeneva, g_ST.currentFont == geneva);
    STMenu_CheckItem(g_ST.fontMenu, iChicago, g_ST.currentFont == 0 /* system font */);
}

/*
 * UpdateSizeMenu - Update Size menu checks
 */
static void UpdateSizeMenu(void) {
    STMenu_CheckItem(g_ST.sizeMenu, iSize9, g_ST.currentSize == 9);
    STMenu_CheckItem(g_ST.sizeMenu, iSize10, g_ST.currentSize == 10);
    STMenu_CheckItem(g_ST.sizeMenu, iSize12, g_ST.currentSize == 12);
    STMenu_CheckItem(g_ST.sizeMenu, iSize14, g_ST.currentSize == 14);
    STMenu_CheckItem(g_ST.sizeMenu, iSize18, g_ST.currentSize == 18);
    STMenu_CheckItem(g_ST.sizeMenu, iSize24, g_ST.currentSize == 24);
}

/*
 * UpdateStyleMenu - Update Style menu checks
 */
static void UpdateStyleMenu(void) {
    Boolean isPlain = (g_ST.currentStyle == normal);
    Boolean isBold = (g_ST.currentStyle & bold) != 0;
    Boolean isItalic = (g_ST.currentStyle & italic) != 0;
    Boolean isUnderline = (g_ST.currentStyle & underline) != 0;

    STMenu_CheckItem(g_ST.styleMenu, iPlain, isPlain);
    STMenu_CheckItem(g_ST.styleMenu, iBold, isBold);
    STMenu_CheckItem(g_ST.styleMenu, iItalic, isItalic);
    STMenu_CheckItem(g_ST.styleMenu, iUnderline, isUnderline);
}