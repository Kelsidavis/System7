/* #include "SystemTypes.h" */
/*
 * RE-AGENT-BANNER
 * Main Finder Implementation
 *
 * Reverse-engineered from System 7 Finder.rsrc
 * Source:  3_resources/Finder.rsrc
 * ROM disassembly
 *
 * Evidence sources:
 * - String analysis: "Macintosh Finder Version 7.1", "About The Finder"
 * - Functionality analysis from evidence.curated.json
 * - API mappings from mappings.json
 *
 * This is the main entry point and initialization code for the Finder.
 */

// #include "CompatibilityFix.h" // Removed
#include "SystemTypes.h"

#include "Finder/finder.h"
#include "Finder/finder_types.h"
/* Use local headers instead of system headers */
#include "MemoryMgr/memory_manager_types.h"
#include "ResourceManager.h"
#include "EventManager/EventTypes.h"
#include "MenuManager/MenuTypes.h"
#include "DialogManager/DialogTypes.h"
#include "WindowManager/WindowTypes.h"
#include "FS/vfs.h"


/* External globals */
extern QDGlobals qd;  /* QuickDraw globals from main.c */

/* Global Variables */
static Boolean gFinderInitialized = false;
static Str255 gFinderVersion = "\pMacintosh Finder Version 7.1"; /*
static MenuHandle gAppleMenu, gFileMenu, gEditMenu, gViewMenu, gLabelMenu, gSpecialMenu;

/* Forward Declarations */
OSErr InitializeFinder(void);  /* Made public for kernel integration */
static OSErr SetupMenus(void);
extern OSErr InitializeDesktopDB(void);  /* From desktop_manager.c */
static OSErr InitializeWindowManager(void);
static OSErr InitializeTrash(void);
static OSErr HandleShutDown(void);
static void HandleMenuChoice(long menuChoice);
static void HandleMouseDown(EventRecord *event);
static void HandleKeyDown(EventRecord *event);
static void MainEventLoop(void);
static void DoUpdate(WindowPtr w);
static void DoActivate(WindowPtr w, Boolean becomingActive);
static void DoBackgroundTasks(void);
extern void DrawFolderWindowContents(WindowPtr window, Boolean isTrash);

#if 0  /* Disabled - Finder is now integrated into kernel, not standalone */
/*
 * main - Finder entry point

 */
int main(void)
{
    OSErr err;

    /* Initialize the Finder subsystems */
    err = InitializeFinder();
    if (err != noErr) {
        /*
        ShowErrorDialog("\pCould not initialize Finder", err);
        return err;
    }

    /* Enter the main event loop */
    MainEventLoop();

    return noErr;
}
#endif

/*
 * InitializeWindowManager - Initialize window management for Finder

 */
static OSErr InitializeWindowManager(void)
{
    /* Window Manager is already initialized by the kernel */
    /* This would set up Finder-specific windows like desktop */
    return noErr;
}

/*
 * InitializeFinder - Initialize all Finder subsystems

 * Made non-static for kernel integration
 */
OSErr InitializeFinder(void)
{
    OSErr err = noErr;

    if (gFinderInitialized) {
        return noErr;
    }

    /* Toolbox managers already initialized by kernel */
    /* InitGraf(&qd.thePort); -- already done */
    /* InitFonts(); -- already done */
    FlushEvents(everyEvent, 0);
    /* InitWindows(); -- already done */
    /* InitMenus(); -- already done */
    /* TEInit(); -- already done */
    /* InitDialogs(nil); -- already done */
    InitCursor();

    /* Set up menus - Evidence: Menu structure from string analysis */
    extern void serial_puts(const char* str);
    serial_puts("Finder: Before SetupMenus\n");
    err = SetupMenus();
    serial_puts("Finder: After SetupMenus\n");
    if (err != noErr) {
        serial_puts("Finder: SetupMenus failed!\n");
        return err;
    }

    /* Initialize desktop database - Evidence: "Rebuilding the desktop file" */
    serial_puts("Finder: About to call InitializeDesktopDB\n");
    err = InitializeDesktopDB();
    serial_puts("Finder: InitializeDesktopDB returned\n");
    if (err != noErr) return err;

    /* Initialize window management */
    err = InitializeWindowManager();
    if (err != noErr) return err;

    /* Initialize trash folder - Evidence: "Empty Trash" functionality */
    err = InitializeTrashFolder();
    if (err != noErr) {
        serial_puts("Finder: Failed to initialize trash folder (non-fatal)\n");
        /* Non-fatal - continue without trash */
    }

    /* Initialize volume icon on desktop */
    extern OSErr InitializeVolumeIcon(void);
    err = InitializeVolumeIcon();
    if (err != noErr) {
        serial_puts("Finder: Failed to initialize volume icon\n");
        /* Non-fatal - continue */
    } else {
        serial_puts("Finder: Volume icon initialized\n");
    }

    gFinderInitialized = true;
    return noErr;
}

/*
 * SetupMenus - Create Finder menu bar

 */
static OSErr SetupMenus(void)
{
    /* Apple Menu - Evidence: "About The Finder" */
    static unsigned char appleTitle[] = {1, 0x14};  /* Pascal string: Apple symbol */
    gAppleMenu = NewMenu(128, appleTitle);
    AppendMenu(gAppleMenu, "\pAbout This Macintosh");
    AppendMenu(gAppleMenu, "\p(-");
    AddResMenu(gAppleMenu, 'DRVR');
    AppendMenu(gAppleMenu, "\p(-");
    AppendMenu(gAppleMenu, "\pShut Down");

    /* File Menu - Evidence: "Get Info", "Find", "Find Again" */
    static unsigned char fileTitle[] = {4, 'F', 'i', 'l', 'e'};  /* Pascal string: "File" */
    gFileMenu = NewMenu(129, fileTitle);
    AppendMenu(gFileMenu, "\pNew Folder/N");
    AppendMenu(gFileMenu, "\pOpen/O");
    AppendMenu(gFileMenu, "\pPrint/P");
    AppendMenu(gFileMenu, "\pClose/W");
    AppendMenu(gFileMenu, "\p(-");
    AppendMenu(gFileMenu, "\pGet Info/I");
    AppendMenu(gFileMenu, "\pSharing...");
    AppendMenu(gFileMenu, "\pDuplicate/D");
    AppendMenu(gFileMenu, "\pMake Alias");
    AppendMenu(gFileMenu, "\pPut Away/Y");
    AppendMenu(gFileMenu, "\p(-");
    AppendMenu(gFileMenu, "\pFind.../F");
    AppendMenu(gFileMenu, "\pFind Again/G");

    /* Edit Menu */
    static unsigned char editTitle[] = {4, 'E', 'd', 'i', 't'};  /* Pascal string: "Edit" */
    gEditMenu = NewMenu(130, editTitle);
    AppendMenu(gEditMenu, "\pUndo/Z");
    AppendMenu(gEditMenu, "\p(-");
    AppendMenu(gEditMenu, "\pCut/X");
    AppendMenu(gEditMenu, "\pCopy/C");
    AppendMenu(gEditMenu, "\pPaste/V");
    AppendMenu(gEditMenu, "\pClear");
    AppendMenu(gEditMenu, "\pSelect All/A");

    /* View Menu - Evidence: "Icon Views", "List Views", "Clean Up" */
    static unsigned char viewTitle[] = {4, 'V', 'i', 'e', 'w'};  /* Pascal string: "View" */
    gViewMenu = NewMenu(131, viewTitle);
    AppendMenu(gViewMenu, "\pby Icon");
    AppendMenu(gViewMenu, "\pby Name");
    AppendMenu(gViewMenu, "\pby Size");
    AppendMenu(gViewMenu, "\pby Kind");
    AppendMenu(gViewMenu, "\pby Label");
    AppendMenu(gViewMenu, "\pby Date");
    AppendMenu(gViewMenu, "\p(-");
    AppendMenu(gViewMenu, "\pClean Up Window");
    AppendMenu(gViewMenu, "\pClean Up Selection");

    /* Label Menu */
    static unsigned char labelTitle[] = {5, 'L', 'a', 'b', 'e', 'l'};  /* Pascal string: "Label" */
    gLabelMenu = NewMenu(132, labelTitle);
    AppendMenu(gLabelMenu, "\pNone");
    AppendMenu(gLabelMenu, "\pEssential");
    AppendMenu(gLabelMenu, "\pHot");
    AppendMenu(gLabelMenu, "\pIn Progress");
    AppendMenu(gLabelMenu, "\pCool");
    AppendMenu(gLabelMenu, "\pPersonal");
    AppendMenu(gLabelMenu, "\pProject 1");
    AppendMenu(gLabelMenu, "\pProject 2");

    /* Special Menu - Evidence: "Empty Trash", "Clean Up Desktop", "Eject" */
    static unsigned char specialTitle[] = {7, 'S', 'p', 'e', 'c', 'i', 'a', 'l'};  /* Pascal string: "Special" */
    gSpecialMenu = NewMenu(133, specialTitle);
    AppendMenu(gSpecialMenu, "\pClean Up Desktop");
    AppendMenu(gSpecialMenu, "\pEmpty Trash");
    AppendMenu(gSpecialMenu, "\p(-");
    AppendMenu(gSpecialMenu, "\pEject/E");
    AppendMenu(gSpecialMenu, "\pErase Disk");
    AppendMenu(gSpecialMenu, "\p(-");
    AppendMenu(gSpecialMenu, "\pRestart");

    /* Insert menus into menu bar in correct order: Apple, File, Edit, View, Label, Special */
    /* InsertMenu with 0 adds to end, so insert in order */
    InsertMenu(gAppleMenu, 0);    /* Apple at position 0 */
    InsertMenu(gFileMenu, 0);      /* File at position 1 */
    InsertMenu(gEditMenu, 0);      /* Edit at position 2 */
    InsertMenu(gViewMenu, 0);      /* View at position 3 */
    InsertMenu(gLabelMenu, 0);     /* Label at position 4 */
    InsertMenu(gSpecialMenu, 0);   /* Special at position 5 */

    extern void serial_puts(const char* str);
    serial_puts("Finder: About to call DrawMenuBar\n");
    DrawMenuBar();
    serial_puts("Finder: DrawMenuBar returned\n");

    /* Temporary workaround: manually setup default menus */
    extern void SetupDefaultMenus(void);
    serial_puts("Finder: Calling SetupDefaultMenus\n");
    SetupDefaultMenus();
    serial_puts("Finder: SetupDefaultMenus returned\n");

    return noErr;
}

/*
 * DoUpdate - Handle window update events
 * System 7 Finder style: Draw window contents inside BeginUpdate/EndUpdate
 */
static void DoUpdate(WindowPtr w)
{
    extern void serial_printf(const char* fmt, ...);
    serial_printf("DoUpdate: called with window=%p\n", w);

    if (!w) {
        serial_printf("DoUpdate: window is NULL, returning\n");
        return;
    }

    GrafPtr savePort;
    GetPort(&savePort);
    SetPort(w);

    serial_printf("DoUpdate: calling BeginUpdate\n");
    BeginUpdate(w);

    /* Draw only the content; chrome is the window def's job */
    /* We use the window refCon to decide what to render, like the Finder */
    serial_printf("DoUpdate: refCon='%c%c%c%c'\n",
                 (char)(w->refCon >> 24), (char)(w->refCon >> 16),
                 (char)(w->refCon >> 8), (char)w->refCon);

    if (w->refCon == 'TRSH') {
        serial_printf("DoUpdate: drawing trash window contents\n");
        DrawFolderWindowContents(w, true);
    } else if (w->refCon == 'DISK') {
        serial_printf("DoUpdate: drawing volume window contents\n");
        DrawFolderWindowContents(w, false);
    } else {
        /* Generic doc window: clear content so it doesn't tear */
        serial_printf("DoUpdate: generic window, erasing content\n");
        Rect r = w->port.portRect;
        InsetRect(&r, 1, 21); /* skip title bar area */
        EraseRect(&r);
    }

    serial_printf("DoUpdate: calling EndUpdate\n");
    EndUpdate(w);

    SetPort(savePort);
}

/*
 * Finder_OpenDesktopItem - Bulletproof window opener with immediate paint
 * Opens a desktop item window and ensures it draws immediately
 */
WindowPtr Finder_OpenDesktopItem(Boolean isTrash, ConstStr255Param title)
{
    extern void serial_printf(const char* fmt, ...);
    extern WindowPtr NewWindow(void *, const Rect *, ConstStr255Param, Boolean, short,
                               WindowPtr, Boolean, long);
    extern void ShowWindow(WindowPtr);
    extern void SelectWindow(WindowPtr);
    extern void DrawFolderWindowContents(WindowPtr w, Boolean isTrash);

    static Rect r;
    r.left = 10;
    r.top = 80;
    r.right = 490;
    r.bottom = 420;

    serial_printf("[WIN_OPEN] Starting, isTrash=%d\n", isTrash);

    WindowPtr w = NewWindow(NULL, &r, isTrash ? "\pTrash" : "\pMacintosh HD",
                            true, 0, (WindowPtr)-1L, true,
                            isTrash ? 'TRSH' : 'DISK');

    if (!w) {
        serial_printf("[WIN_OPEN] NewWindow returned NULL!\n");
        return NULL;
    }

    serial_printf("[WIN_OPEN] NewWindow succeeded, calling ShowWindow\n");
    ShowWindow(w);

    serial_printf("[WIN_OPEN] Calling SelectWindow\n");
    SelectWindow(w);

    /* Window Manager draws chrome automatically via ShowWindow */
    /* Content will be drawn via update event handled by DoUpdate() */

    serial_printf("[WIN_OPEN] Complete, window created\n");
    return w;
}

/*
 * DoActivate - Handle window activation events
 */
static void DoActivate(WindowPtr w, Boolean becomingActive)
{
    if (!w) return;

    /* Standard Finder activate handling */
    if (becomingActive) {
        /* Activate the window - highlight controls, etc. */
    } else {
        /* Deactivate - unhighlight */
    }
}

/*
 * DoBackgroundTasks - Perform idle-time tasks
 */
static void DoBackgroundTasks(void)
{
    /* Background tasks like checking for disk insertions */
}

/*
 * MainEventLoop - Main event processing loop

 */
static void MainEventLoop(void)
{
    EventRecord event;
    Boolean gotEvent;

    while (true) {
        gotEvent = WaitNextEvent(everyEvent, &event, 0L, nil);

        if (gotEvent) {
            switch (event.what) {
                case mouseDown:
                    HandleMouseDown(&event);
                    break;

                case keyDown:
                case autoKey:
                    HandleKeyDown(&event);
                    break;

                case updateEvt:
                    DoUpdate((WindowPtr)event.message);
                    break;

                case activateEvt:
                    DoActivate((WindowPtr)event.message,
                              (event.modifiers & activeFlag) != 0);
                    break;

                case diskEvt:
                    /* Handle disk insertion */
                    break;

                case osEvt:
                    /* Handle suspend/resume events */
                    break;
            }
        }

        /* Background processing */
        DoBackgroundTasks();
    }
}

/*
 * HandleMouseDown - Process mouse down events

 */
static void HandleMouseDown(EventRecord *event)
{
    WindowPtr window;
    short part;
    long menuChoice;

    part = FindWindow(event->where, &window);

    switch (part) {
        case inMenuBar:
            menuChoice = MenuSelect(event->where);
            HandleMenuChoice(menuChoice);
            break;

        case inSysWindow:
            SystemClick(event, window);
            break;

        case inContent:
            HandleContentClick(window, event);
            break;

        case inDrag:
            DragWindow(window, event->where, &qd.screenBits.bounds);
            break;

        case inGrow:
            HandleGrowWindow(window, event);
            break;

        case inGoAway:
            if (TrackGoAway(window, event->where)) {
                CloseFinderWindow(window);
            }
            break;

        case inZoomIn:
        case inZoomOut:
            if (TrackBox(window, event->where, part)) {
                ZoomWindow(window, part, true);
            }
            break;
    }
}

/*
 * HandleMenuChoice - Process menu selections

 */
static void HandleMenuChoice(long menuChoice)
{
    short menuID = HiWord(menuChoice);
    short menuItem = LoWord(menuChoice);
    OSErr err;

    switch (menuID) {
        case 128:  /* Apple Menu */
            if (menuItem == 1) {
                ShowAboutFinder();  /*
            } else {
                /* Get item text to check if it's Shut Down */
                Str255 itemName;
                GetMenuItemText(gAppleMenu, menuItem, itemName);

                /* Check if this is the Shut Down item */
                if (itemName[0] == 9 &&
                    itemName[1] == 'S' && itemName[2] == 'h' &&
                    itemName[3] == 'u' && itemName[4] == 't') {
                    /* Shut Down */
                    err = HandleShutDown();
                } else {
                    /* Handle desk accessories */
                    OpenDeskAcc(itemName);
                }
            }
            break;

        case 129:  /* File Menu */
            switch (menuItem) {
                case 6:   /* Get Info */
                    err = HandleGetInfo();  /*
                    break;
                case 12:  /* Find */
                    err = ShowFind();       /*
                    break;
                case 13:  /* Find Again */
                    err = FindAgain();      /*
                    break;
            }
            break;

        case 131:  /* View Menu */
            switch (menuItem) {
                case 8:   /* Clean Up Window */
                    err = CleanUpWindow(FrontWindow(), kCleanUpByName);
                    break;
                case 9:   /* Clean Up Selection */
                    err = CleanUpSelection(FrontWindow());
                    break;
            }
            break;

        case 133:  /* Special Menu */
            switch (menuItem) {
                case 1:   /* Clean Up Desktop */
                    err = CleanUpDesktop();  /*
                    break;
                case 2:   /* Empty Trash */
                    err = EmptyTrash(false); /*
                    break;
            }
            break;
    }

    HiliteMenu(0);
}

/*
 * HandleShutDown - Handle Shut Down menu command
 */
static OSErr HandleShutDown(void)
{
    /* Log shutdown request */
    extern void serial_puts(const char* str);
    serial_puts("Finder: Shutting down system\n");

    /* Halt the CPU */
    __asm__ volatile("cli; hlt");

    return noErr;  /* Never reached */
}

/*
 * ShowErrorDialog - Display error message to user

 */
OSErr ShowErrorDialog(StringPtr message, OSErr errorCode)
{
    Str255 errorText;

    /* Format error message */
    BlockMoveData(message, errorText, message[0] + 1);

    /* Show alert dialog */
    ParamText(errorText, "\p", "\p", "\p");
    Alert(128, nil);  /* Error alert dialog */

    return noErr;
}

/*
 * GetFinderVersion - Return current Finder version

 */
StringPtr GetFinderVersion(void)
{
    return gFinderVersion;
}

/*
