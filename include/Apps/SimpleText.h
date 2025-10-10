/*
 * SimpleText.h - SimpleText Application Header
 *
 * System 7.1-compatible text editor using TextEdit API
 * Creator: 'ttxt', Document Type: 'TEXT'
 */

#ifndef SIMPLETEXT_H
#define SIMPLETEXT_H

#include "SystemTypes.h"
#include "TextEdit/TextEdit.h"
#include "WindowManager/WindowManager.h"
#include "MenuManager/MenuManager.h"
#include "EventManager/EventManager.h"
#include "DialogManager/DialogManager.h"
#include "ControlManager/ControlManager.h"
#include "QuickDraw/QuickDraw.h"

/* Define _FILE_DEFINED to prevent stdio FILE conflict */
#define _FILE_DEFINED

/* Constants */
#define kMaxDocuments 10
#define kMaxFileSize 32767  /* TextEdit 32K limit */
#define kMaxFileName 255
#define kScrollBarWidth 16
#define kMenuBarHeight 20
#define kDefaultFontSize 12
#define kCaretBlinkRate 30  /* ticks between blinks */

/* Menu IDs */
enum {
    mApple = 128,
    mFile = 129,
    mEdit = 130,
    mFont = 131,
    mSize = 132,
    mStyle = 133
};

/* Menu Commands */
enum {
    /* Apple Menu */
    iAbout = 1,

    /* File Menu */
    iNew = 1,
    iOpen = 2,
    iClose = 3,
    iSave = 4,
    iSaveAs = 5,
    iPageSetup = 6,
    iPrint = 7,
    iQuit = 9,

    /* Edit Menu */
    iUndo = 1,
    iCut = 3,
    iCopy = 4,
    iPaste = 5,
    iClear = 6,
    iSelectAll = 8,

    /* Font Menu - dynamic */
    iMonaco = 1,
    iGeneva = 2,
    iChicago = 3,

    /* Size Menu */
    iSize9 = 1,
    iSize10 = 2,
    iSize12 = 3,
    iSize14 = 4,
    iSize18 = 5,
    iSize24 = 6,

    /* Style Menu */
    iPlain = 1,
    iBold = 2,
    iItalic = 3,
    iUnderline = 4
};

/* Style Run structure for styled text */
typedef struct STStyleRun {
    SInt16 startChar;      /* Starting character position */
    SInt16 fontID;         /* Font family ID */
    SInt16 fontSize;       /* Font size */
    Style  fontStyle;      /* Style flags (bold, italic, underline) */
} STStyleRun;

typedef struct STStyleRunTable {
    SInt16    numRuns;
    Handle    hRuns;       /* Handle to array of STStyleRun */
} STStyleRunTable;

/* Document structure */
typedef struct STDocument {
    WindowPtr       window;         /* Document window */
    TEHandle        hTE;            /* TextEdit handle */
    Boolean         dirty;          /* Document modified flag */
    Boolean         untitled;       /* Is this "Untitled"? */
    Str255          fileName;       /* File name (Pascal string) */
    char            filePath[512];  /* Full file path */
    OSType          fileType;       /* File type ('TEXT') */
    OSType          fileCreator;    /* File creator ('ttxt') */
    ControlHandle   vScroll;        /* Vertical scrollbar */
    STStyleRunTable styles;         /* Style runs for styled text */
    SInt32          lastSaveLen;    /* Length at last save (for undo) */
    Handle          undoText;       /* Text for single-level undo */
    SInt16          undoStart;      /* Undo selection start */
    SInt16          undoEnd;        /* Undo selection end */
    struct STDocument* next;       /* Next document in list */
} STDocument;

/* Global state */
typedef struct STGlobals {
    STDocument*     firstDoc;      /* Linked list of documents */
    STDocument*     activeDoc;     /* Currently active document */
    MenuHandle      appleMenu;
    MenuHandle      fileMenu;
    MenuHandle      editMenu;
    MenuHandle      fontMenu;
    MenuHandle      sizeMenu;
    MenuHandle      styleMenu;
    Boolean         running;        /* Application running flag */
    Boolean         hasColorQD;     /* Color QuickDraw available */
    UInt32          lastCaretTime;  /* Last caret blink time */
    Boolean         caretVisible;   /* Caret visibility state */
    SInt16          currentFont;    /* Current font ID */
    SInt16          currentSize;    /* Current font size */
    Style           currentStyle;   /* Current style flags */
} STGlobals;

/* Function prototypes */

/* SimpleText.c - Main application */
extern void SimpleText_Init(void);
extern void SimpleText_Run(void);
extern void SimpleText_Quit(void);
extern void SimpleText_HandleEvent(EventRecord* event);
extern void SimpleText_Idle(void);
extern Boolean SimpleText_IsRunning(void);
extern void SimpleText_Launch(void);
extern void SimpleText_OpenFile(const char* path);

/* STDocument.c - Document management */
extern STDocument* STDoc_New(void);
extern STDocument* STDoc_Open(const char* path);
extern void STDoc_Close(STDocument* doc);
extern void STDoc_Save(STDocument* doc);
extern void STDoc_SaveAs(STDocument* doc);
extern void STDoc_SetDirty(STDocument* doc, Boolean dirty);
extern void STDoc_UpdateTitle(STDocument* doc);
extern STDocument* STDoc_FindByWindow(WindowPtr window);
extern void STDoc_Activate(STDocument* doc);
extern void STDoc_Deactivate(STDocument* doc);

/* STView.c - View/TextEdit handling */
extern void STView_Create(STDocument* doc);
extern void STView_Dispose(STDocument* doc);
extern void STView_Draw(STDocument* doc);
extern void STView_Click(STDocument* doc, EventRecord* event);
extern void STView_Key(STDocument* doc, EventRecord* event);
extern void STView_Resize(STDocument* doc);
extern void STView_Scroll(STDocument* doc, SInt16 dv, SInt16 dh);
extern void STView_SetStyle(STDocument* doc, SInt16 font, SInt16 size, Style style);
extern void STView_GetStyle(STDocument* doc, SInt16* font, SInt16* size, Style* style);
extern void STView_UpdateCaret(STDocument* doc);

/* STMenus.c - Menu management */
extern void STMenu_Init(void);
extern void STMenu_Dispose(void);
extern void STMenu_Handle(long menuResult);
extern void STMenu_Update(void);
extern void STMenu_Install(void);   /* Install menus when window activates */
extern void STMenu_Remove(void);    /* Remove menus when window deactivates */
extern void STMenu_EnableItem(MenuHandle menu, short item, Boolean enable);
extern void STMenu_CheckItem(MenuHandle menu, short item, Boolean check);

/* STFileIO.c - File operations */
extern Boolean STIO_ReadFile(STDocument* doc, const char* path);
extern Boolean STIO_WriteFile(STDocument* doc, const char* path);
extern Boolean STIO_SaveDialog(STDocument* doc, char* pathOut);
extern Boolean STIO_OpenDialog(char* pathOut);
extern void STIO_SetFileInfo(const char* path, OSType type, OSType creator);

/* STClipboard.c - Clipboard operations */
extern void STClip_Cut(STDocument* doc);
extern void STClip_Copy(STDocument* doc);
extern void STClip_Paste(STDocument* doc);
extern void STClip_Clear(STDocument* doc);
extern void STClip_SelectAll(STDocument* doc);
extern Boolean STClip_HasText(void);
extern void STClip_Undo(STDocument* doc);
extern void STClip_SaveUndo(STDocument* doc);

/* Utility functions */
extern void ST_Log(const char* fmt, ...);
extern void ST_Beep(void);
extern Boolean ST_ConfirmClose(STDocument* doc);
extern void ST_ShowAbout(void);
extern void ST_ErrorAlert(const char* message);
extern void ST_CenterWindow(WindowPtr window);

/* Global instance */
extern STGlobals g_ST;

#endif /* SIMPLETEXT_H */