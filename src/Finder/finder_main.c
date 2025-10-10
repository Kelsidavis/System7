/* #include "SystemTypes.h" */
/*
 * RE-AGENT-BANNER
 * Main Finder Implementation
 *
 * Reverse-engineered from System 7 Finder.rsrc
 * Source:  3_resources/Finder.rsrc
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
#include "StandardFile/StandardFile.h"
#include "System71StdLib.h"
#include "ToolboxCompat.h"
#include "System71StdLib.h"
#include "Finder/AboutThisMac.h"
#include "Finder/FinderLogging.h"

/* External globals */
extern QDGlobals qd;  /* QuickDraw globals from main.c */

/* Global Variables */
static Boolean gFinderInitialized = false;
static Str255 gFinderVersion = "\033Macintosh Finder Version 7.1";
static MenuHandle gAppleMenu, gFileMenu, gEditMenu, gViewMenu, gLabelMenu, gSpecialMenu;

/* Forward Declarations */
OSErr InitializeFinder(void);  /* Made public for kernel integration */
static OSErr SetupMenus(void);
extern OSErr InitializeDesktopDB(void);  /* From desktop_manager.c */
extern OSErr InitializeTrashFolder(void);  /* From trash_manager.c */
static OSErr InitializeWindowManager(void);
/* HandleShutDown, HandleMenuChoice, HandleMouseDown, HandleKeyDown declared in #if 0 block below */
/* DoUpdate, DoActivate, DoBackgroundTasks declared in #if 0 block below */
/* MainEventLoop declared in #if 0 block below */
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
        /* ShowErrorDialog("\pCould not initialize Finder", err); */
        /* return err; */
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
 * OnVolumeMount - Callback when a volume is mounted
 * Adds the volume icon to the desktop
 */
void OnVolumeMount(VRefNum vref, const char* volName)
{
    extern OSErr Desktop_AddVolumeIcon(const char* name, VRefNum vref);

    FINDER_LOG_DEBUG("Finder: Volume '%s' (vRef %d) mounted - adding desktop icon\n", volName, vref);

    OSErr err = Desktop_AddVolumeIcon(volName, vref);
    if (err != noErr) {
        FINDER_LOG_DEBUG("Finder: Failed to add volume icon (err=%d)\n", err);
    }
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

    /* Set up volume mount callback */
    extern void VFS_SetMountCallback(void (*callback)(VRefNum, const char*));
    extern void OnVolumeMount(VRefNum vref, const char* volName);
    VFS_SetMountCallback(OnVolumeMount);
    serial_puts("Finder: Volume mount callback registered\n");

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
    err = InitializeVolumeIcon();
    if (err != noErr) {
        serial_puts("Finder: Failed to initialize volume icon\n");
        /* Non-fatal - continue */
    } else {
        serial_puts("Finder: Volume icon initialized\n");
    }

    /* Play classic System 7 startup chime */
    extern void StartupChime(void);
    serial_puts("Finder: SKIPPING startup chime (would trigger IRQ during delay)\n");
    // StartupChime();  // DISABLED: causes triple-fault when IRQ0 fires during busy-wait

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
    AppendMenu(gAppleMenu, "\025About This Macintosh");
    AppendMenu(gAppleMenu, "\002(-");
    AddResMenu(gAppleMenu, 'DRVR');
    AppendMenu(gAppleMenu, "\002(-");
    AppendMenu(gAppleMenu, "\011Shut Down");

    /* File Menu - Evidence: "Get Info", "Find", "Find Again" */
    static unsigned char fileTitle[] = {4, 'F', 'i', 'l', 'e'};  /* Pascal string: "File" */
    gFileMenu = NewMenu(129, fileTitle);
    AppendMenu(gFileMenu, "\015New Folder/N");
    AppendMenu(gFileMenu, "\007Open/O");
    AppendMenu(gFileMenu, "\010Print/P");
    AppendMenu(gFileMenu, "\010Close/W");
    AppendMenu(gFileMenu, "\002(-");
    AppendMenu(gFileMenu, "\013Get Info/I");
    AppendMenu(gFileMenu, "\013Sharing...");
    AppendMenu(gFileMenu, "\014Duplicate/D");
    AppendMenu(gFileMenu, "\012Make Alias");
    AppendMenu(gFileMenu, "\012Put Away/Y");
    AppendMenu(gFileMenu, "\002(-");
    AppendMenu(gFileMenu, "\011Find.../F");
    AppendMenu(gFileMenu, "\015Find Again/G");
    AppendMenu(gFileMenu, "\002(-");
    AppendMenu(gFileMenu, "\023[Test] Open File...");
    AppendMenu(gFileMenu, "\023[Test] Save File...");

    /* Edit Menu */
    static unsigned char editTitle[] = {4, 'E', 'd', 'i', 't'};  /* Pascal string: "Edit" */
    gEditMenu = NewMenu(130, editTitle);
    AppendMenu(gEditMenu, "\007Undo/Z");
    AppendMenu(gEditMenu, "\002(-");
    AppendMenu(gEditMenu, "\006Cut/X");
    AppendMenu(gEditMenu, "\007Copy/C");
    AppendMenu(gEditMenu, "\010Paste/V");
    AppendMenu(gEditMenu, "\005Clear");
    AppendMenu(gEditMenu, "\015Select All/A");

    /* View Menu - Evidence: "Icon Views", "List Views", "Clean Up" */
    static unsigned char viewTitle[] = {4, 'V', 'i', 'e', 'w'};  /* Pascal string: "View" */
    gViewMenu = NewMenu(131, viewTitle);
    AppendMenu(gViewMenu, "\007by Icon");
    AppendMenu(gViewMenu, "\007by Name");
    AppendMenu(gViewMenu, "\007by Size");
    AppendMenu(gViewMenu, "\007by Kind");
    AppendMenu(gViewMenu, "\010by Label");
    AppendMenu(gViewMenu, "\007by Date");
    AppendMenu(gViewMenu, "\002(-");
    AppendMenu(gViewMenu, "\017Clean Up Window");
    AppendMenu(gViewMenu, "\022Clean Up Selection");

    /* Label Menu */
    static unsigned char labelTitle[] = {5, 'L', 'a', 'b', 'e', 'l'};  /* Pascal string: "Label" */
    gLabelMenu = NewMenu(132, labelTitle);
    AppendMenu(gLabelMenu, "\004None");
    AppendMenu(gLabelMenu, "\011Essential");
    AppendMenu(gLabelMenu, "\003Hot");
    AppendMenu(gLabelMenu, "\013In Progress");
    AppendMenu(gLabelMenu, "\004Cool");
    AppendMenu(gLabelMenu, "\010Personal");
    AppendMenu(gLabelMenu, "\011Project 1");
    AppendMenu(gLabelMenu, "\011Project 2");

    /* Special Menu - Evidence: "Empty Trash", "Clean Up Desktop", "Eject" */
    static unsigned char specialTitle[] = {7, 'S', 'p', 'e', 'c', 'i', 'a', 'l'};  /* Pascal string: "Special" */
    gSpecialMenu = NewMenu(133, specialTitle);
    AppendMenu(gSpecialMenu, "\020Clean Up Desktop");
    AppendMenu(gSpecialMenu, "\013Empty Trash");
    AppendMenu(gSpecialMenu, "\002(-");
    AppendMenu(gSpecialMenu, "\010Eject/E");
    AppendMenu(gSpecialMenu, "\012Erase Disk");
    AppendMenu(gSpecialMenu, "\002(-");
    AppendMenu(gSpecialMenu, "\007Restart");

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

#if 0  /* Disabled - Event loop helper functions only used in standalone mode */
/*
 * DoUpdate - Handle window update events
 * System 7 Finder style: Draw window contents inside BeginUpdate/EndUpdate
 */
static void DoUpdate(WindowPtr w)
{
    FINDER_LOG_DEBUG("DoUpdate: called with window=0x%08x\n", (unsigned int)P2UL(w));

    if (!w) {
        FINDER_LOG_DEBUG("DoUpdate: window is NULL, returning\n");
        return;
    }

    GrafPtr savePort;
    GetPort(&savePort);
    SetPort(w);

    serial_logf(kLogModuleFinder, kLogLevelDebug, "[UPDATE] BeginUpdate START\n");
    BeginUpdate(w);
    serial_logf(kLogModuleFinder, kLogLevelDebug, "[UPDATE] BeginUpdate DONE\n");

    /* Draw only the content; chrome is the window def's job */
    /* We use the window refCon to decide what to render, like the Finder */

    /* Text drawing temporarily disabled until Font Manager is linked */

    if (w->refCon == 'TRSH' || w->refCon == 'DISK') {
        serial_logf(kLogModuleFinder, kLogLevelDebug, "[UPDATE] FolderWindow_Draw START\n");
        extern void FolderWindow_Draw(WindowPtr w);
        FolderWindow_Draw(w);
        serial_logf(kLogModuleFinder, kLogLevelDebug, "[UPDATE] FolderWindow_Draw DONE\n");
    } else {
        /* Generic doc window: clear content and draw sample text */
        Rect r = w->port.portRect;
        EraseRect(&r);

        /* Text drawing disabled - would draw "Window Content Here" at (10,30) */
    }

    serial_logf(kLogModuleFinder, kLogLevelDebug, "[UPDATE] EndUpdate START\n");
    EndUpdate(w);
    serial_logf(kLogModuleFinder, kLogLevelDebug, "[UPDATE] EndUpdate DONE\n");

    SetPort(savePort);
}
#endif

/*
 * Finder_OpenDesktopItem - Bulletproof window opener with immediate paint
 * Opens a desktop item window and ensures it draws immediately
 */
WindowPtr Finder_OpenDesktopItem(Boolean isTrash, ConstStr255Param title)
{
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

    FINDER_LOG_DEBUG("[WIN_OPEN] Starting, isTrash=%d\n", isTrash);

    /* Build title as local Pascal string (same method as AboutThisMac) */
    static unsigned char windowTitleBuf[256];
    ConstStr255Param windowTitle = title;

    if (!windowTitle || windowTitle[0] == 0) {
        /* Build default title as Pascal string */
        const char* titleText = isTrash ? "Trash" : "Macintosh HD";
        strcpy((char*)&windowTitleBuf[1], titleText);
        windowTitleBuf[0] = (unsigned char)strlen(titleText);
        windowTitle = windowTitleBuf;
        FINDER_LOG_DEBUG("[WIN_OPEN] Built title: len=%d, first_char=0x%02x\n",
                      windowTitleBuf[0], windowTitleBuf[1]);
    } else {
        FINDER_LOG_DEBUG("[WIN_OPEN] Using provided title: len=%d\n", windowTitle[0]);
    }

    FINDER_LOG_DEBUG("[WIN_OPEN] ABOUT TO CALL NewWindow: bounds=(%d,%d,%d,%d), title_len=%d, isTrash=%d\n",
                  r.top, r.left, r.bottom, r.right, windowTitle[0], isTrash);
    FINDER_LOG_DEBUG("[WIN_OPEN] NewWindow function ptr=%p\n", NewWindow);

    WindowPtr w = NewWindow(NULL, &r, windowTitle,
                            false, 0, (WindowPtr)-1L, true,
                            isTrash ? 0x54525348 : 0x4449534B);  /* 'TRSH' or 'DISK' */

    FINDER_LOG_DEBUG("[WIN_OPEN] NewWindow RETURNED: w=%p\n", w);

    if (!w) {
        FINDER_LOG_DEBUG("[WIN_OPEN] NewWindow returned NULL!\n");
        return NULL;
    }

    FINDER_LOG_DEBUG("[WIN_OPEN] NewWindow succeeded, calling ShowWindow\n");
    ShowWindow(w);
    FINDER_LOG_DEBUG("[WIN_OPEN] ShowWindow returned\n");

    /* Initialize folder state and populate contents from VFS */
    /* GetFolderState creates the state and calls InitializeFolderContents internally */
    extern void* GetFolderState(WindowPtr w);  /* Returns FolderWindowState* */
    FINDER_LOG_DEBUG("[WIN_OPEN] Calling GetFolderState to initialize contents\n");
    (void)GetFolderState(w);
    FINDER_LOG_DEBUG("[WIN_OPEN] GetFolderState returned\n");

    FINDER_LOG_DEBUG("[WIN_OPEN] Calling SelectWindow\n");
    SelectWindow(w);

    /* Window Manager will generate update event for content drawing */
    /* Application's update event handler (main.c) will call FolderWindowProc */

    FINDER_LOG_DEBUG("[WIN_OPEN] Complete, window created - content will be drawn via update event\n");
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

#if 0  /* Disabled - Background task functions only used in standalone mode */
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
            FINDER_LOG_DEBUG("Finder: Got event type=%d (updateEvt=%d)\n", event.what, updateEvt);
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
#endif

#if 0  /* Disabled - Standalone event handlers only used in standalone mode */
/*
 * HandleMouseDown - Process mouse down events

 */
static void HandleMouseDown(EventRecord *event)
{
    WindowPtr window;
    short part;
    long menuChoice;

    /* Function declarations */
    extern void SelectWindow(WindowPtr);
    extern void DragWindow(WindowPtr, Point, const Rect*);

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
            /* Select window before dragging (Mac OS standard behavior) */
            SelectWindow(window);
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

    /* Call the centralized menu command dispatcher */
    extern void DoMenuCommand(short menuID, short item);
    DoMenuCommand(menuID, menuItem);

#if 0  /* Old implementation - kept for reference */
    switch (menuID) {
        case 128:  // Apple Menu
            if (menuItem == 1) {
                AboutWindow_ShowOrToggle();
            } else {
                /* Get item text to check if it's Shut Down */
                Str255 itemName;
                GetMenuItemText(gAppleMenu, menuItem, itemName);

                /* Check if this is the Shut Down item */
                if (itemName[0] == 9 &&
                    itemName[1] == 'S' && itemName[2] == 'h' &&
                    itemName[3] == 'u' && itemName[4] == 't') {
                    /* Shut Down */
                    (void)HandleShutDown();
                } else {
                    /* Handle desk accessories */
                    OpenDeskAcc(itemName);
                }
            }
            break;

        case 129:  /* File Menu */
            switch (menuItem) {
                case 6:   /* Get Info */
                    (void)HandleGetInfo();
                    break;
                case 12:  /* Find */
                    (void)ShowFind();
                    break;
                case 13:  /* Find Again */
                    (void)FindAgain();
                    break;
                case 15:  /* [Test] Open File... */
                    {
                        StandardFileReply reply;
                        FINDER_LOG_DEBUG("[TEST] StandardGetFile called\n");
                        StandardGetFile(NULL, 0, NULL, &reply);
                        if (reply.sfGood) {
                            FINDER_LOG_DEBUG("[TEST] File selected: vRefNum=%d parID=%ld name='%.*s'\n",
                                         reply.sfFile.vRefNum, reply.sfFile.parID,
                                         reply.sfFile.name[0], reply.sfFile.name + 1);
                        } else {
                            FINDER_LOG_DEBUG("[TEST] User canceled\n");
                        }
                    }
                    break;
                case 16:  /* [Test] Save File... */
                    {
                        StandardFileReply reply;
                        FINDER_LOG_DEBUG("[TEST] StandardPutFile called\n");
                        StandardPutFile("\010Save As:", "\010Untitled", &reply);
                        if (reply.sfGood) {
                            FINDER_LOG_DEBUG("[TEST] Save location: vRefNum=%d parID=%ld name='%.*s'\n",
                                         reply.sfFile.vRefNum, reply.sfFile.parID,
                                         reply.sfFile.name[0], reply.sfFile.name + 1);
                        } else {
                            FINDER_LOG_DEBUG("[TEST] User canceled\n");
                        }
                    }
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
                    err = CleanUpDesktop();
                    break;
                case 2:   /* Empty Trash */
                    err = EmptyTrash(false);
                    break;
            }
            break;
    }
#endif  /* End of old implementation */

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
#endif  /* Event loop helper functions */

/*
 * ShowErrorDialog - Display error message to user

 */
OSErr ShowErrorDialog(StringPtr message, OSErr errorCode)
{
    Str255 errorText;

    /* Format error message */
    BlockMove(message, errorText, message[0] + 1);

    /* Show alert dialog */
    ParamText(errorText, "\000", "\000", "\000");
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
