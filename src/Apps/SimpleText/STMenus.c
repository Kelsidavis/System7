/*
 * STMenus.c - SimpleText Menu Management
 *
 * Handles all menu creation, updating, and command dispatch
 */

#include "DialogManager/DITLBuilder.h"
#include <string.h>
#include "System71StdLib.h"
#include "Apps/SimpleText.h"
#include "MemoryMgr/MemoryManager.h"
#include "EventManager/EventManager.h"
#include "DialogManager/DialogManager.h"
#include "SoundManager/SoundManager.h"

/* Utility macros for packing/unpacking longs */
#define HiWord(x) ((short)(((unsigned long)(x) >> 16) & 0xFFFF))
#define LoWord(x) ((short)((unsigned long)(x) & 0xFFFF))

/* Menu item strings */
/*
 * Menu item lists and item numbers.
 *
 * An item's number is its position in the list, so the list and the numbers
 * are one fact and are written once, here. They used to be two: a hand-built
 * byte array of text in this file and a separate enum of numbers in the
 * header. Both had drifted. Every one of the six arrays carried a
 * hand-counted Pascal length that no longer matched its text, so AppendMenu
 * read past the end of all of them; and the File menu's numbers had been
 * written without counting its separator lines, so Command-S - which MenuKey
 * correctly resolved to item 5 - arrived at the handler as Save As and always
 * put up a dialog.
 *
 * Each X entry is (name, text). Separators are ordinary items and take a
 * number, which is exactly why they have to appear in the same list.
 * "\311" is the ellipsis in the Mac Roman set; a leading "(" disables an
 * item, so "(-" is a disabled separator line.
 */
#define ST_APPLE_MENU_ITEMS(X) \
    X(iAbout,       "About SimpleText\311") \
    X(iAppleSep1,   "(-")

#define ST_FILE_MENU_ITEMS(X) \
    X(iNew,         "New/N") \
    X(iOpen,        "Open\311/O") \
    X(iFileSep1,    "(-") \
    X(iClose,       "Close/W") \
    X(iSave,        "Save/S") \
    X(iSaveAs,      "Save As\311") \
    X(iFileSep2,    "(-") \
    X(iPageSetup,   "Page Setup\311") \
    X(iPrint,       "Print\311/P") \
    X(iFileSep3,    "(-") \
    X(iQuit,        "Quit/Q")

#define ST_EDIT_MENU_ITEMS(X) \
    X(iUndo,        "Undo/Z") \
    X(iEditSep1,    "(-") \
    X(iCut,         "Cut/X") \
    X(iCopy,        "Copy/C") \
    X(iPaste,       "Paste/V") \
    X(iClear,       "Clear") \
    X(iEditSep2,    "(-") \
    X(iSelectAll,   "Select All/A") \
    X(iEditSep3,    "(-") \
    X(iFind,        "Find\311/F") \
    X(iFindAgain,   "Find Again/G")

#define ST_FONT_MENU_ITEMS(X) \
    X(iMonaco,      "Monaco") \
    X(iGeneva,      "Geneva") \
    X(iChicago,     "Chicago")

#define ST_SIZE_MENU_ITEMS(X) \
    X(iSize9,       "9 Point") \
    X(iSize10,      "10 Point") \
    X(iSize12,      "12 Point") \
    X(iSize14,      "14 Point") \
    X(iSize18,      "18 Point") \
    X(iSize24,      "24 Point")

#define ST_STYLE_MENU_ITEMS(X) \
    X(iPlain,       "Plain") \
    X(iBold,        "Bold") \
    X(iItalic,      "Italic") \
    X(iUnderline,   "Underline")

/* Item numbers start at 1 and count every entry, separators included. */
#define ST_MENU_ITEM_ENUM(name, text)  name,
#define ST_MENU_ITEM_TEXT(name, text)  text ";"

enum { ST_APPLE_ITEM_BASE = 0, ST_APPLE_MENU_ITEMS(ST_MENU_ITEM_ENUM) };
enum { ST_FILE_ITEM_BASE  = 0, ST_FILE_MENU_ITEMS(ST_MENU_ITEM_ENUM)  };
enum { ST_EDIT_ITEM_BASE  = 0, ST_EDIT_MENU_ITEMS(ST_MENU_ITEM_ENUM)  };
enum { ST_FONT_ITEM_BASE  = 0, ST_FONT_MENU_ITEMS(ST_MENU_ITEM_ENUM)  };
enum { ST_SIZE_ITEM_BASE  = 0, ST_SIZE_MENU_ITEMS(ST_MENU_ITEM_ENUM)  };
enum { ST_STYLE_ITEM_BASE = 0, ST_STYLE_MENU_ITEMS(ST_MENU_ITEM_ENUM) };

/* AppendMenu takes the whole list at once, ";"-separated. The trailing ";"
 * each entry contributes is harmless - it is stripped below. */
static const char* const kAppleMenuItems = ST_APPLE_MENU_ITEMS(ST_MENU_ITEM_TEXT);
static const char* const kFileMenuItems  = ST_FILE_MENU_ITEMS(ST_MENU_ITEM_TEXT);
static const char* const kEditMenuItems  = ST_EDIT_MENU_ITEMS(ST_MENU_ITEM_TEXT);
static const char* const kFontMenuItems  = ST_FONT_MENU_ITEMS(ST_MENU_ITEM_TEXT);
static const char* const kSizeMenuItems  = ST_SIZE_MENU_ITEMS(ST_MENU_ITEM_TEXT);
static const char* const kStyleMenuItems = ST_STYLE_MENU_ITEMS(ST_MENU_ITEM_TEXT);

/*
 * Append a ";"-separated item list to a menu.
 *
 * AppendMenu wants a Pascal string; this is the only place that has to know
 * that, and the length comes from the text rather than from a second copy of
 * it kept alongside.
 */
static void STMenu_AppendItems(MenuHandle menu, const char* items)
{
    Str255 pstr;
    size_t len;

    if (menu == NULL || items == NULL) return;

    len = strlen(items);
    while (len > 0 && items[len - 1] == ';') {
        len--;   /* the list builder leaves one behind */
    }
    if (len > 255) len = 255;
    pstr[0] = (unsigned char)len;
    memcpy(&pstr[1], items, len);
    AppendMenu(menu, pstr);
}

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
    extern void serial_puts(const char*);

    serial_puts("[ST] STMenu_Init: Creating menus (not yet installing)\n");

    /* Create Apple menu - but DON'T insert yet */
    static unsigned char appleTitle[] = {1, 0x14};  /* Apple symbol */
    g_ST.appleMenu = NewMenu(mApple, appleTitle);
    if (g_ST.appleMenu) {
        STMenu_AppendItems(g_ST.appleMenu, kAppleMenuItems);
        /* DO NOT call InsertMenu here - menus installed on activate */
    }

    /* Create File menu - but DON'T insert yet */
    static Str255 fileTitle;
    c2pstrcpy(fileTitle, "File");
    g_ST.fileMenu = NewMenu(mFile, fileTitle);
    if (g_ST.fileMenu) {
        STMenu_AppendItems(g_ST.fileMenu, kFileMenuItems);
        /* DO NOT call InsertMenu here - menus installed on activate */
    }

    /* Create Edit menu - but DON'T insert yet */
    static Str255 editTitle;
    c2pstrcpy(editTitle, "Edit");
    g_ST.editMenu = NewMenu(mEdit, editTitle);
    if (g_ST.editMenu) {
        STMenu_AppendItems(g_ST.editMenu, kEditMenuItems);
        /* DO NOT call InsertMenu here - menus installed on activate */
    }

    /* Create Font menu - but DON'T insert yet */
    static Str255 fontTitle;
    c2pstrcpy(fontTitle, "Font");
    g_ST.fontMenu = NewMenu(mFont, fontTitle);
    if (g_ST.fontMenu) {
        STMenu_AppendItems(g_ST.fontMenu, kFontMenuItems);
        /* DO NOT call InsertMenu here - menus installed on activate */
    }

    /* Create Size menu - but DON'T insert yet */
    static Str255 sizeTitle;
    c2pstrcpy(sizeTitle, "Size");
    g_ST.sizeMenu = NewMenu(mSize, sizeTitle);
    if (g_ST.sizeMenu) {
        STMenu_AppendItems(g_ST.sizeMenu, kSizeMenuItems);
        /* DO NOT call InsertMenu here - menus installed on activate */
    }

    /* Create Style menu - but DON'T insert yet */
    static Str255 styleTitle;
    c2pstrcpy(styleTitle, "Style");
    g_ST.styleMenu = NewMenu(mStyle, styleTitle);
    if (g_ST.styleMenu) {
        STMenu_AppendItems(g_ST.styleMenu, kStyleMenuItems);
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
/* Whether SimpleText's menus are the ones currently in the menu bar.
 *
 * This used to be inferred from g_ST.activeDoc ("install if no document was
 * active"), but the document record is set when a document opens, well before
 * its activate event arrives - so by the time HandleActivate ran, activeDoc
 * was already non-NULL and the install was skipped every time. SimpleText's
 * menus never reached the bar and Save, Close, Font, Size and Style were
 * unreachable. Whether the menus are installed is a fact about the menu bar,
 * so it is recorded here, next to the code that changes it. */
static Boolean gSTMenusInstalled = false;

Boolean STMenu_IsInstalled(void) {
    return gSTMenusInstalled;
}

void STMenu_Install(void) {
    extern void serial_puts(const char*);

    if (gSTMenusInstalled) {
        return;
    }
    serial_puts("[ST] STMenu_Install: Installing SimpleText menus into menu bar\n");

    /* Only one application owns the menu bar at a time. The Finder's menus
     * come out before SimpleText's go in, or the bar would carry two File
     * menus and two Edit menus. */
    ClearMenuBar();

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

    gSTMenusInstalled = true;
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

    if (!gSTMenusInstalled) {
        return;
    }
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

    gSTMenusInstalled = false;

    /* Hand the bar back to the Finder. Deleting SimpleText's menus on its own
     * would leave the bar empty - the Finder's menus were cleared when
     * SimpleText took over, and something has to put them back. */
    {
        extern void Finder_InstallMenuBar(void);
        Finder_InstallMenuBar();
    }

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
        {
            /* Show page setup info alert */
            unsigned char msg[] = "\x2APage Setup: US Letter, Portrait, 1\" margins";
            unsigned char empty[] = "\x00";
            ParamText(msg, empty, empty, empty);
            NoteAlert(130, NULL);
            break;
        }

        case iPrint:
        {
            /* Show print info alert - printing not available on bare metal */
            unsigned char msg[] = "\x22Printing is not available on this system.";
            unsigned char empty[] = "\x00";
            ParamText(msg, empty, empty, empty);
            NoteAlert(130, NULL);
            break;
        }

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

        case iFind:
            STFind_ShowDialog(doc);
            break;

        case iFindAgain:
            STFind_Again(doc);
            break;
    }
}

/*
 * STFind_ShowDialog - Show a simple Find dialog and search for text.
 * Uses a modal dialog with a text prompt. On OK, searches forward from
 * the current selection end and highlights the match.
 */
void STFind_ShowDialog(STDocument* doc) {
    if (!doc || !doc->hTE) return;

    extern void ShowWindow(WindowPtr);
    extern void SystemTask(void);

    /*
     * Built through DITLBuilder rather than by hand.
     *
     * The edit field is pre-filled with the previous search text, so its
     * length is whatever the user last typed. An item whose data length is odd
     * needs a pad byte, and without it the parser reads the following item's
     * header one byte off - so the "Find:" label after it appeared or vanished
     * according to the parity of the last search.
     */
    DITLBuilder b;
    if (!DITL_Begin(&b, 512)) return;

    DITL_AddButton(&b, 70, 180, 90, 266, "Find");
    DITL_AddButton(&b, 70, 90, 90, 170, "Cancel");
    DITL_AddEditText(&b, 32, 10, 52, 266, g_ST.searchText);
    DITL_AddText(&b, 10, 10, 26, 266, "Find:");

    Handle ditl = DITL_Finish(&b);
    if (!ditl) return;

    Rect bounds = {120, 120, 230, 400};
    static Str255 title;
    c2pstrcpy(title, "Find");
    DialogPtr dlg = NewDialog(NULL, &bounds, title, true, 1 /* dBoxProc */,
                              (WindowPtr)-1, false, 0, ditl);
    if (!dlg) {
        DisposeHandle(ditl);
        return;
    }

    /*
     * Run through the shared modal runner rather than a loop of our own.
     *
     * The runner draws the dialog before waiting, because nothing guarantees
     * an update event arrives for a window that was just created, and it
     * pumps the input devices while it runs - the main event loop is the only
     * other caller of ProcessModernInput and it is blocked behind us. The
     * loop that used to be here did neither, and reimplemented the key
     * handling besides.
     *
     * This does not fix reopening Find after a search that found something,
     * which still shows nothing. The dialog is created and its window is
     * marked visible in that case too; the trigger is TESetSelect on the
     * match, since a search that finds nothing reopens fine.
     */
    short itemHit = RunModalDialogBox(dlg, 1 /* Find */, 2 /* Cancel */);

    /* Extract search text from edit field (item 3) */
    if (itemHit == 1) {
        SInt16 itemType;
        Handle itemH;
        Rect itemBox;
        extern void GetDialogItem(DialogPtr, SInt16, SInt16*, Handle*, Rect*);
        extern void GetDialogItemText(Handle, unsigned char*);
        GetDialogItem(dlg, 3, &itemType, &itemH, &itemBox);
        if (itemH) {
            unsigned char pstr[256];
            GetDialogItemText(itemH, pstr);
            int len = pstr[0];
            if (len > 255) len = 255;
            memcpy(g_ST.searchText, &pstr[1], len);
            g_ST.searchText[len] = '\0';
            g_ST.searchOffset = 0;
        }
    }

    DisposeDialog(dlg);

    /* Perform the search if Find was clicked */
    if (itemHit == 1 && g_ST.searchText[0]) {
        /* Start searching from current selection end */
        TERec* te = *doc->hTE;
        g_ST.searchOffset = te->selEnd;
        STFind_Again(doc);
    }
}

/*
 * STFind_Again - Find next occurrence of the search text.
 * Searches forward from searchOffset, wrapping to beginning if needed.
 */
void STFind_Again(STDocument* doc) {
    if (!doc || !doc->hTE || !g_ST.searchText[0]) {
        SysBeep(10);
        return;
    }

    TERec* te = *doc->hTE;
    Handle hText = te->hText;
    if (!hText || !*hText) {
        SysBeep(10);
        return;
    }

    const char* text = (const char*)*hText;
    SInt16 textLen = te->teLength;
    int searchLen = 0;
    while (g_ST.searchText[searchLen]) searchLen++;

    if (searchLen == 0 || searchLen > textLen) {
        SysBeep(10);
        return;
    }

    /* Search forward from current offset */
    SInt16 startPos = g_ST.searchOffset;
    if (startPos < 0) startPos = 0;
    if (startPos > textLen) startPos = 0;

    Boolean found = false;
    SInt16 foundPos = -1;

    /* Search from startPos to end */
    for (SInt16 i = startPos; i <= textLen - searchLen; i++) {
        Boolean match = true;
        for (int j = 0; j < searchLen; j++) {
            if (text[i + j] != g_ST.searchText[j]) {
                match = false;
                break;
            }
        }
        if (match) {
            foundPos = i;
            found = true;
            break;
        }
    }

    /* Wrap around: search from beginning to startPos */
    if (!found && startPos > 0) {
        SInt16 limit = startPos - 1;
        if (limit > textLen - searchLen) limit = textLen - searchLen;
        for (SInt16 i = 0; i <= limit; i++) {
            Boolean match = true;
            for (int j = 0; j < searchLen; j++) {
                if (text[i + j] != g_ST.searchText[j]) {
                    match = false;
                    break;
                }
            }
            if (match) {
                foundPos = i;
                found = true;
                break;
            }
        }
    }

    if (found) {
        TESetSelect(foundPos, foundPos + searchLen, doc->hTE);
        g_ST.searchOffset = foundPos + searchLen;
        /* Scroll to show selection */
        extern void TESelView(TEHandle hTE);
        TESelView(doc->hTE);
    } else {
        SysBeep(10);  /* Not found beep */
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
