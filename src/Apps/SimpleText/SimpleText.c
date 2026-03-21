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
    STDocument* doc = NULL;

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
                GrafPtr oldPort;
                Point localPt;
                ControlHandle control = NULL;
                SInt16 controlPart;

                GetPort(&oldPort);
                SetPort((GrafPtr)window);

                localPt = event->where;
                GlobalToLocal(&localPt);
                controlPart = FindControl(localPt, window, &control);

                if (controlPart && control) {
                    if (doc == NULL) {
                        doc = STDoc_FindByWindow(window);
                    }

                    if (doc && doc->vScroll && control == doc->vScroll) {
                        SInt16 startValue = GetControlValue(control);
                        SInt16 delta = 0;
                        TrackScrollbar(control, localPt, controlPart, event->modifiers, &delta);
                        SInt16 endValue = GetControlValue(control);
                        if (delta == 0) {
                            delta = endValue - startValue;
                        }
                        if (delta != 0) {
                            STView_Scroll(doc, delta, 0);
                        }
                    } else {
                        TrackControl(control, localPt, NULL);
                    }

                    SetPort(oldPort);
                    break;
                }

                SetPort(oldPort);

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
                    extern void serial_puts(const char *str);
                    serial_puts("[ST] >>> Calling SizeWindow from SimpleText (after GrowWindow)\n");
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
    WindowPtr window = (WindowPtr)(uintptr_t)event->message;

    SimpleText_HandleWindowUpdate(window);
}

/*
 * HandleActivate - Process activate/deactivate events
 *
 * System 7 Design: Install our menus when our first window activates,
 * remove them when our last window deactivates (allowing Finder menus to show).
 */
static void HandleActivate(EventRecord* event) {
    WindowPtr window = (WindowPtr)(uintptr_t)event->message;
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

    /* Adjust cursor: I-beam over text area, arrow elsewhere.
     * This is classic Mac behavior — the cursor changes shape based on context. */
    {
        extern void GetMouse(Point* pt);
        extern void SetCursor(const Cursor* crsr);
        extern void InitCursor(void);
        extern const Cursor* CursorManager_GetIBeamCursor(void);
        extern WindowPtr FrontWindow(void);

        WindowPtr front = FrontWindow();
        STDocument* doc = front ? STDoc_FindByWindow(front) : NULL;

        if (doc && doc->hTE) {
            Point mouse;
            GetMouse(&mouse);

            /* Convert global to local */
            extern void GlobalToLocalWindow(WindowPtr window, Point *pt);
            GlobalToLocalWindow(front, &mouse);

            /* Check if mouse is in the text view rect */
            TERec* te = *doc->hTE;
            if (mouse.h >= te->viewRect.left && mouse.h < te->viewRect.right &&
                mouse.v >= te->viewRect.top  && mouse.v < te->viewRect.bottom) {
                /* Set I-beam cursor */
                const Cursor* ibeam = CursorManager_GetIBeamCursor();
                if (ibeam) {
                    SetCursor(ibeam);
                }
            } else {
                InitCursor();  /* Reset to arrow */
            }
        }
    }
}

Boolean SimpleText_DispatchEvent(EventRecord* event)
{
    if (!SimpleText_IsRunning() || !event) {
        return false;
    }

    switch (event->what) {
        case mouseDown: {
            WindowPtr window = NULL;
            short part = FindWindow(event->where, &window);
            if (part == inMenuBar) {
                WindowPtr front = FrontWindow();
                if (front && STDoc_FindByWindow(front)) {
                    SimpleText_HandleEvent(event);
                    return true;
                }
                return false;
            }
            if (window && STDoc_FindByWindow(window)) {
                SimpleText_HandleEvent(event);
                return true;
            }
            return false;
        }

        case keyDown:
        case autoKey: {
            WindowPtr front = FrontWindow();
            if (front && STDoc_FindByWindow(front)) {
                SimpleText_HandleEvent(event);
                return true;
            }
            return false;
        }

        case activateEvt: {
            WindowPtr window = (WindowPtr)(uintptr_t)event->message;
            if (window && STDoc_FindByWindow(window)) {
                SimpleText_HandleEvent(event);
                return true;
            }
            return false;
        }

        case osEvt:
            SimpleText_HandleEvent(event);
            return true;

        default:
            break;
    }

    return false;
}

Boolean SimpleText_HandleWindowUpdate(WindowPtr window) {
    if (!SimpleText_IsRunning() || window == NULL) {
        return false;
    }

    STDocument* doc = STDoc_FindByWindow(window);
    if (!doc) {
        return false;
    }

    BeginUpdate(window);
    STView_Draw(doc);
    EndUpdate(window);
    return true;
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

    if (!g_ST.firstDoc) {
        STDocument* doc = STDoc_New();
        if (!doc) {
            ST_ErrorAlert("Not enough memory to open document");
            return;
        }
        SelectWindow(doc->window);
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
 * ST_ConfirmClose - Show "Save changes?" confirmation dialog.
 *
 * Classic Mac OS dialog with three buttons:
 *   Save (1) - save then close
 *   Cancel (2) - abort close
 *   Don't Save (3) - discard changes and close
 *
 * Returns true if close should proceed, false if cancelled.
 */
Boolean ST_ConfirmClose(STDocument* doc) {
    extern DialogPtr NewDialog(void*, const Rect*, const unsigned char*, Boolean, SInt16,
                               WindowPtr, Boolean, SInt32, Handle);
    extern void DisposeDialog(DialogPtr);
    extern Boolean IsDialogEvent(const EventRecord*);
    extern Boolean DialogSelect(const EventRecord*, DialogPtr*, short*);
    extern void ShowWindow(WindowPtr);
    extern Boolean GetNextEvent(unsigned int, EventRecord*);
    extern void SystemTask(void);

    if (!doc) return true;

    ST_Log("Confirm close for '%s' (dirty=%d)\n", doc->fileName, doc->dirty);

    /* Build DITL: prompt (1=statText), Save (2=btn), Cancel (3=btn), Don't Save (4=btn) */
    Handle ditl = NewHandleClear(512);
    if (!ditl) return true;  /* Can't show dialog, allow close */

    HLock(ditl);
    unsigned char* p = (unsigned char*)*ditl;

    /* 4 items, count-1 = 3 */
    *p++ = 0; *p++ = 3;

    /* Item 1: Prompt static text */
    *p++ = 0; *p++ = 0; *p++ = 0; *p++ = 0;
    *p++ = 0; *p++ = 10;  /* top */
    *p++ = 0; *p++ = 10;  /* left */
    *p++ = 0; *p++ = 50;  /* bottom */
    *p++ = 1; *p++ = 30;  /* right = 286 */
    *p++ = 8;              /* statText */
    /* Build prompt: "Save changes to 'name' before closing?" */
    {
        char msg[200];
        int mlen = snprintf(msg, sizeof(msg),
                           "Save changes to \"%s\" before closing?", doc->fileName);
        if (mlen > 200) mlen = 200;
        *p++ = (unsigned char)mlen;
        memcpy(p, msg, mlen);
        p += mlen;
    }

    /* Item 2: Save button (default - bottom right) */
    *p++ = 0; *p++ = 0; *p++ = 0; *p++ = 0;
    *p++ = 0; *p++ = 60;  /* top */
    *p++ = 0; *p++ = 210; /* left */
    *p++ = 0; *p++ = 80;  /* bottom */
    *p++ = 1; *p++ = 30;  /* right = 286 */
    *p++ = 4;              /* ctrlItem + btnCtrl */
    *p++ = 4; *p++ = 'S'; *p++ = 'a'; *p++ = 'v'; *p++ = 'e';

    /* Item 3: Cancel button (bottom center) */
    *p++ = 0; *p++ = 0; *p++ = 0; *p++ = 0;
    *p++ = 0; *p++ = 60;
    *p++ = 0; *p++ = 110;
    *p++ = 0; *p++ = 80;
    *p++ = 0; *p++ = 200;
    *p++ = 4;
    *p++ = 6; *p++ = 'C'; *p++ = 'a'; *p++ = 'n'; *p++ = 'c'; *p++ = 'e'; *p++ = 'l';

    /* Item 4: Don't Save button (bottom left) */
    *p++ = 0; *p++ = 0; *p++ = 0; *p++ = 0;
    *p++ = 0; *p++ = 60;
    *p++ = 0; *p++ = 10;
    *p++ = 0; *p++ = 80;
    *p++ = 0; *p++ = 100;
    *p++ = 4;
    *p++ = 10;
    memcpy(p, "Don\xD5t Save", 10);  /* 0xD5 = curly apostrophe in Mac Roman */
    p += 10;

    HUnlock(ditl);

    Rect bounds = {140, 120, 240, 420};
    static unsigned char title[] = {0};  /* No title for alert-style dialog */
    DialogPtr dlg = NewDialog(NULL, &bounds, title, true, 1 /* dBoxProc */,
                              (WindowPtr)-1, false, 0, ditl);
    if (!dlg) {
        DisposeHandle(ditl);
        return true;
    }

    ShowWindow((WindowPtr)dlg);

    /* Modal event loop */
    short itemHit = 0;
    Boolean done = false;
    while (!done) {
        EventRecord event;
        if (GetNextEvent(0xFFFF, &event)) {
            if (IsDialogEvent(&event)) {
                DialogPtr whichDlg;
                short item;
                if (DialogSelect(&event, &whichDlg, &item)) {
                    if (whichDlg == dlg) {
                        itemHit = item;
                        done = (item >= 2 && item <= 4);
                    }
                }
            }
            /* Keyboard shortcuts */
            if (event.what == 3 /* keyDown */) {
                char ch = event.message & 0xFF;
                if (ch == '\r' || ch == 0x03) { itemHit = 2; done = true; }  /* Return = Save */
                if (ch == 0x1B) { itemHit = 3; done = true; }  /* Escape = Cancel */
                if (ch == 'd' || ch == 'D') { itemHit = 4; done = true; }  /* D = Don't Save */
            }
        }
        SystemTask();
    }

    DisposeDialog(dlg);

    ST_Log("Confirm close result: item=%d\n", itemHit);

    switch (itemHit) {
        case 2: {
            /* Save - save the document then proceed with close */
            extern void STDoc_Save(STDocument* doc);
            STDoc_Save(doc);
            return true;  /* Proceed with close */
        }
        case 3:
            /* Cancel - abort the close */
            return false;
        case 4:
            /* Don't Save - discard changes, proceed with close */
            doc->dirty = false;
            return true;
        default:
            return false;  /* Unknown = cancel */
    }
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
