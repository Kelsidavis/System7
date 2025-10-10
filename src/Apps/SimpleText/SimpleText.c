/*
 * SimpleText.c - SimpleText Application Main Entry
 *
 * System 7.1-compatible text editor
 * Main application entry point and event loop
 */

#include <string.h>
#include "Apps/SimpleText.h"
#include "MemoryMgr/MemoryManager.h"
#include "SoundManager/SoundManager.h"

/* External functions */
extern UInt32 TickCount(void);

/* Utility macros for packing/unpacking longs */
#define HiWord(x) ((short)(((unsigned long)(x) >> 16) & 0xFFFF))
#define LoWord(x) ((short)((unsigned long)(x) & 0xFFFF))

/* Debug logging */
#include "System/SystemLogging.h"
#define ST_DEBUG 1

#if ST_DEBUG
void ST_Log(const char* fmt, ...) {
    char buf[256];
    int i = 0;
    const char* p = fmt;

    /* Simple format string - just handle %s, %d, %c */
    buf[0] = 'S'; buf[1] = 'T'; buf[2] = ':'; buf[3] = ' ';
    i = 4;

    while (*p && i < 250) {
        if (*p != '%') {
            buf[i++] = *p++;
        } else {
            p++; /* Skip % */
            if (*p) p++; /* Skip format specifier */
            buf[i++] = '*'; /* Placeholder */
        }
    }
    buf[i] = '\0';

    SYSTEM_LOG_DEBUG("%s", buf);
}
#else
#define ST_Log(...)
#endif

/* Global instance */
STGlobals g_ST = {0};

/* Static helper functions */
static void HandleMouseDown(EventRecord* event);
static void HandleKeyDown(EventRecord* event);
static void HandleUpdate(EventRecord* event);
static void HandleActivate(EventRecord* event);
static void HandleOSEvent(EventRecord* event);
static void AdjustMenus(void);
/* IsDialogEvent is declared in DialogManager */

/*
 * SimpleText_Init - Initialize SimpleText application
 */
void SimpleText_Init(void) {
    ST_Log("Initializing SimpleText\n");

    /* Initialize globals */
    g_ST.firstDoc = NULL;
    g_ST.activeDoc = NULL;
    g_ST.running = true;
    g_ST.hasColorQD = false;  /* Assume B&W for now */
    g_ST.lastCaretTime = 0;
    g_ST.caretVisible = true;
    g_ST.currentFont = 1;       /* Default font (geneva) */
    g_ST.currentSize = 12;      /* Default size */
    g_ST.currentStyle = 0;      /* Default style (normal) */

    /* Initialize TextEdit if not already done */
    TEInit();

    /* Initialize menus */
    STMenu_Init();

    /* Don't create initial untitled - let SimpleText_OpenFile create windows as needed */
}

/*
 * SimpleText_Run - Main event loop
 */
void SimpleText_Run(void) {
    EventRecord event;
    Boolean gotEvent;

    while (g_ST.running) {
        /* Get next event */
        gotEvent = WaitNextEvent(everyEvent, &event, 10, NULL);

        if (gotEvent) {
            SimpleText_HandleEvent(&event);
        } else {
            /* Null event - handle idle tasks */
            SimpleText_Idle();
        }
    }
}

/*
 * SimpleText_HandleEvent - Main event dispatcher
 */
void SimpleText_HandleEvent(EventRecord* event) {
    WindowPtr window;
    short part;

    /* Check if this is a dialog event first */
    if (IsDialogEvent(event)) {
        /* Let Dialog Manager handle it */
        return;
    }

    switch (event->what) {
        case mouseDown:
            HandleMouseDown(event);
            break;

        case keyDown:
        case autoKey:
            HandleKeyDown(event);
            break;

        case updateEvt:
            HandleUpdate(event);
            break;

        case activateEvt:
            HandleActivate(event);
            break;

        case osEvt:
            HandleOSEvent(event);
            break;

        case kHighLevelEvent:
            /* Handle Apple Events if implemented */
            break;
    }
}

/*
 * HandleMouseDown - Process mouse down events
 */
static void HandleMouseDown(EventRecord* event) {
    WindowPtr window;
    short part;
    long menuResult;
    STDocument* doc;

    part = FindWindow(event->where, &window);

    switch (part) {
        case inMenuBar:
            AdjustMenus();
            menuResult = MenuSelect(event->where);
            STMenu_Handle(menuResult);
            HiliteMenu(0);
            break;

        case inContent:
            if (window != FrontWindow()) {
                SelectWindow(window);
            } else {
                doc = STDoc_FindByWindow(window);
                if (doc) {
                    STView_Click(doc, event);
                }
            }
            break;

        case inDrag:
            {
                Rect dragRect = {0, 0, 480, 640};  /* Screen bounds */
                DragWindow(window, event->where, &dragRect);
            }
            break;

        case inGrow:
            {
                Rect sizeRect = {80, 80, 480, 640};
                long newSize = GrowWindow(window, event->where, &sizeRect);
                if (newSize) {
                    SizeWindow(window, LoWord(newSize), HiWord(newSize), true);
                    doc = STDoc_FindByWindow(window);
                    if (doc) {
                        STView_Resize(doc);
                    }
                }
            }
            break;

        case inGoAway:
            if (TrackGoAway(window, event->where)) {
                doc = STDoc_FindByWindow(window);
                if (doc) {
                    STDoc_Close(doc);
                }
            }
            break;
    }
}

/*
 * HandleKeyDown - Process keyboard events
 */
static void HandleKeyDown(EventRecord* event) {
    char key = event->message & charCodeMask;
    long menuResult;

    /* Check for command key */
    if (event->modifiers & cmdKey) {
        AdjustMenus();
        menuResult = MenuKey(key);
        STMenu_Handle(menuResult);
        HiliteMenu(0);
    } else {
        /* Regular key - pass to active document */
        if (g_ST.activeDoc) {
            STView_Key(g_ST.activeDoc, event);
            STDoc_SetDirty(g_ST.activeDoc, true);
        }
    }
}

/*
 * HandleUpdate - Process update events
 */
static void HandleUpdate(EventRecord* event) {
    WindowPtr window = (WindowPtr)event->message;
    STDocument* doc = STDoc_FindByWindow(window);

    if (doc) {
        BeginUpdate(window);
        STView_Draw(doc);
        EndUpdate(window);
    }
}

/*
 * HandleActivate - Process activate/deactivate events
 *
 * System 7 Design: Install our menus when our first window activates,
 * remove them when our last window deactivates (allowing Finder menus to show).
 */
static void HandleActivate(EventRecord* event) {
    WindowPtr window = (WindowPtr)event->message;
    STDocument* doc = STDoc_FindByWindow(window);
    Boolean wasActive;

    if (doc) {
        if (event->modifiers & activeFlag) {
            /* Window is being activated */
            wasActive = (g_ST.activeDoc != NULL);
            STDoc_Activate(doc);
            g_ST.activeDoc = doc;

            /* Install menus if this is the first active window */
            if (!wasActive) {
                extern void serial_puts(const char*);
                serial_puts("[ST] HandleActivate: First window activated - installing menus\n");
                STMenu_Install();
            }
        } else {
            /* Window is being deactivated */
            STDoc_Deactivate(doc);
            if (g_ST.activeDoc == doc) {
                g_ST.activeDoc = NULL;
            }

            /* Remove menus if no windows are active */
            if (g_ST.activeDoc == NULL) {
                extern void serial_puts(const char*);
                serial_puts("[ST] HandleActivate: Last window deactivated - removing menus\n");
                STMenu_Remove();
            }
        }
        STMenu_Update();
    }
}

/*
 * HandleOSEvent - Process suspend/resume events
 */
static void HandleOSEvent(EventRecord* event) {
    switch ((event->message >> 24) & 0xFF) {
        case suspendResumeMessage:
            if (event->message & resumeFlag) {
                /* Application resumed */
                if (g_ST.activeDoc) {
                    STDoc_Activate(g_ST.activeDoc);
                }
            } else {
                /* Application suspended */
                if (g_ST.activeDoc) {
                    STDoc_Deactivate(g_ST.activeDoc);
                }
            }
            break;
    }
}

/*
 * SimpleText_Idle - Handle idle time tasks
 */
void SimpleText_Idle(void) {
    UInt32 currentTime = TickCount();

    /* Blink caret every 30 ticks */
    if (g_ST.activeDoc && (currentTime - g_ST.lastCaretTime) > kCaretBlinkRate) {
        g_ST.caretVisible = !g_ST.caretVisible;
        g_ST.lastCaretTime = currentTime;
        STView_UpdateCaret(g_ST.activeDoc);
    }

    /* Give time to TextEdit idle */
    if (g_ST.activeDoc && g_ST.activeDoc->hTE) {
        TEIdle(g_ST.activeDoc->hTE);
    }
}

/*
 * SimpleText_Quit - Quit application
 */
void SimpleText_Quit(void) {
    STDocument* doc;
    STDocument* nextDoc;

    ST_Log("Quitting SimpleText\n");

    /* Close all documents */
    doc = g_ST.firstDoc;
    while (doc) {
        nextDoc = doc->next;

        /* Check for unsaved changes */
        if (doc->dirty) {
            if (!ST_ConfirmClose(doc)) {
                return;  /* User cancelled quit */
            }
        }

        STDoc_Close(doc);
        doc = nextDoc;
    }

    /* Dispose menus */
    STMenu_Dispose();

    /* Mark as not running */
    g_ST.running = false;
}

/*
 * SimpleText_IsRunning - Check if SimpleText is running
 */
Boolean SimpleText_IsRunning(void) {
    return g_ST.running;
}

/*
 * SimpleText_Launch - Launch SimpleText application
 */
void SimpleText_Launch(void) {
    if (!g_ST.running) {
        SimpleText_Init();
    }

    /* Bring to front if already running */
    if (g_ST.activeDoc) {
        SelectWindow(g_ST.activeDoc->window);
    } else if (g_ST.firstDoc) {
        SelectWindow(g_ST.firstDoc->window);
    }
}

/*
 * SimpleText_OpenFile - Open a file in SimpleText
 */
void SimpleText_OpenFile(const char* path) {
    STDocument* doc;

    /* Debug: log the exact path received */
    serial_logf(kLogModuleWindow, kLogLevelDebug,
               "[ST] SimpleText_OpenFile: path='%s' len=%d\n",
               path ? path : "(null)", path ? (int)strlen(path) : 0);

    ST_Log("Opening file: %s\n", path);

    /* Launch if not running */
    if (!g_ST.running) {
        SimpleText_Init();
    }

    /* Check if file is already open */
    for (doc = g_ST.firstDoc; doc; doc = doc->next) {
        if (strcmp(doc->filePath, path) == 0) {
            /* File already open - bring to front */
            SelectWindow(doc->window);
            return;
        }
    }

    /* Open new document */
    doc = STDoc_Open(path);
    if (doc) {
        SelectWindow(doc->window);
        ST_Log("Opened file successfully\n");
    } else {
        ST_ErrorAlert("Could not open file");
    }
}

/*
 * AdjustMenus - Enable/disable menu items based on state
 */
static void AdjustMenus(void) {
    STMenu_Update();
}

/*
 * IsDialogEvent - Check if event is for a dialog
 */
/* IsDialogEvent removed - use DialogManager version */

/*
 * ST_Beep - System beep
 */
void ST_Beep(void) {
    SysBeep(1);
}

/*
 * ST_ConfirmClose - Show close confirmation dialog
 */
Boolean ST_ConfirmClose(STDocument* doc) {
    /* TODO: Show proper dialog */
    /* For now, always allow close */
    ST_Log("Close confirmation for %s\n", doc->fileName);
    return true;
}

/*
 * ST_ShowAbout - Show About dialog
 */
void ST_ShowAbout(void) {
    ST_Log("About SimpleText\n");
    /* TODO: Show proper About dialog */
}

/*
 * ST_ErrorAlert - Show error alert
 */
void ST_ErrorAlert(const char* message) {
    ST_Log("Error: %s\n", message);
    ST_Beep();
    /* TODO: Show proper error dialog */
}

/*
 * ST_CenterWindow - Center a window on screen
 */
void ST_CenterWindow(WindowPtr window) {
    Rect windowRect;
    short screenWidth = 640;
    short screenHeight = 480;
    short windowWidth, windowHeight;
    short left, top;

    /* Get window bounds */
    windowRect = ((GrafPtr)window)->portRect;
    windowWidth = windowRect.right - windowRect.left;
    windowHeight = windowRect.bottom - windowRect.top;

    /* Calculate centered position */
    left = (screenWidth - windowWidth) / 2;
    top = (screenHeight - windowHeight) / 2;

    /* Account for menu bar */
    if (top < kMenuBarHeight) {
        top = kMenuBarHeight;
    }

    /* Move window */
    MoveWindow(window, left, top, false);
}