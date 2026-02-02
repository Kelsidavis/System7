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
#include "Platform/Halt.h"
#include "Finder/finder_types.h"
/* Use local headers instead of system headers */
#include "MemoryMgr/memory_manager_types.h"
#include "ResourceManager.h"
#include "LocaleManager/LocaleManager.h"
#include "LocaleManager/StringIDs.h"
#include "EventManager/EventTypes.h"
#include "MenuManager/MenuTypes.h"
#include "MenuManager/MenuManager.h"
#include "DialogManager/DialogTypes.h"
#include "WindowManager/WindowTypes.h"
#include "WindowManager/WindowManager.h"
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
static MenuHandle gAppleMenu, gFileMenu, gEditMenu, gViewMenu, gLabelMenu, gSpecialMenu, gControlPanelsMenu;

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
    serial_puts("Finder: Playing System 7 startup chime\n");
    StartupChime();

    gFinderInitialized = true;
    return noErr;
}

/*
 * SetupMenus - Create Finder menu bar

 */
static OSErr SetupMenus(void)
{
    Str255 menuStr;

    /* Apple Menu */
    static unsigned char appleTitle[] = {1, 0x14};  /* Pascal string: Apple symbol */
    gAppleMenu = NewMenu(128, appleTitle);
    GetLocalizedString(menuStr, kSTRListFinderAppleMenu, kStrAboutThisMacintosh);
    AppendMenu(gAppleMenu, menuStr);
    AppendMenu(gAppleMenu, "\002(-");
    GetLocalizedString(menuStr, kSTRListFinderAppleMenu, kStrControlPanelsSubmenu);
    AppendMenu(gAppleMenu, menuStr);
    GetLocalizedString(menuStr, kSTRListFinderAppleMenu, kStrNotepad);
    AppendMenu(gAppleMenu, menuStr);
    AppendMenu(gAppleMenu, "\002(-");
    AddResMenu(gAppleMenu, 'DRVR');

    /* Control Panels submenu - ID 134 */
    GetLocalizedString(menuStr, kSTRListFinderMenuTitles, kStrMenuControlPanels);
    gControlPanelsMenu = NewMenu(134, menuStr);
    GetLocalizedString(menuStr, kSTRListFinderControlPanels, kStrCPDesktopPatterns);
    AppendMenu(gControlPanelsMenu, menuStr);
    GetLocalizedString(menuStr, kSTRListFinderControlPanels, kStrCPDateTime);
    AppendMenu(gControlPanelsMenu, menuStr);
    GetLocalizedString(menuStr, kSTRListFinderControlPanels, kStrCPSound);
    AppendMenu(gControlPanelsMenu, menuStr);
    GetLocalizedString(menuStr, kSTRListFinderControlPanels, kStrCPMouse);
    AppendMenu(gControlPanelsMenu, menuStr);
    GetLocalizedString(menuStr, kSTRListFinderControlPanels, kStrCPKeyboard);
    AppendMenu(gControlPanelsMenu, menuStr);
    GetLocalizedString(menuStr, kSTRListFinderControlPanels, kStrCPControlStrip);
    AppendMenu(gControlPanelsMenu, menuStr);

    /* Connect Control Panels submenu to Apple menu item 3 (Control Panels>) */
    /* Note: Do NOT insert into menu bar - keep as submenu only */
    SetItemSubmenu(gAppleMenu, 3, 134);

    /* File Menu */
    GetLocalizedString(menuStr, kSTRListFinderMenuTitles, kStrMenuFile);
    gFileMenu = NewMenu(129, menuStr);
    GetLocalizedString(menuStr, kSTRListFinderFileMenu, kStrNewFolder);
    AppendMenu(gFileMenu, menuStr);
    GetLocalizedString(menuStr, kSTRListFinderFileMenu, kStrOpen);
    AppendMenu(gFileMenu, menuStr);
    GetLocalizedString(menuStr, kSTRListFinderFileMenu, kStrPrint);
    AppendMenu(gFileMenu, menuStr);
    GetLocalizedString(menuStr, kSTRListFinderFileMenu, kStrClose);
    AppendMenu(gFileMenu, menuStr);
    AppendMenu(gFileMenu, "\002(-");
    GetLocalizedString(menuStr, kSTRListFinderFileMenu, kStrGetInfo);
    AppendMenu(gFileMenu, menuStr);
    GetLocalizedString(menuStr, kSTRListFinderFileMenu, kStrSharing);
    AppendMenu(gFileMenu, menuStr);
    GetLocalizedString(menuStr, kSTRListFinderFileMenu, kStrDuplicate);
    AppendMenu(gFileMenu, menuStr);
    GetLocalizedString(menuStr, kSTRListFinderFileMenu, kStrMakeAlias);
    AppendMenu(gFileMenu, menuStr);
    GetLocalizedString(menuStr, kSTRListFinderFileMenu, kStrPutAway);
    AppendMenu(gFileMenu, menuStr);
    AppendMenu(gFileMenu, "\002(-");
    GetLocalizedString(menuStr, kSTRListFinderFileMenu, kStrFindEllipsis);
    AppendMenu(gFileMenu, menuStr);
    GetLocalizedString(menuStr, kSTRListFinderFileMenu, kStrFindAgain);
    AppendMenu(gFileMenu, menuStr);

    /* Edit Menu */
    GetLocalizedString(menuStr, kSTRListFinderMenuTitles, kStrMenuEdit);
    gEditMenu = NewMenu(130, menuStr);
    GetLocalizedString(menuStr, kSTRListFinderEditMenu, kStrUndo);
    AppendMenu(gEditMenu, menuStr);
    AppendMenu(gEditMenu, "\002(-");
    GetLocalizedString(menuStr, kSTRListFinderEditMenu, kStrCut);
    AppendMenu(gEditMenu, menuStr);
    GetLocalizedString(menuStr, kSTRListFinderEditMenu, kStrCopy);
    AppendMenu(gEditMenu, menuStr);
    GetLocalizedString(menuStr, kSTRListFinderEditMenu, kStrPaste);
    AppendMenu(gEditMenu, menuStr);
    GetLocalizedString(menuStr, kSTRListFinderEditMenu, kStrClear);
    AppendMenu(gEditMenu, menuStr);
    GetLocalizedString(menuStr, kSTRListFinderEditMenu, kStrSelectAll);
    AppendMenu(gEditMenu, menuStr);

    /* View Menu */
    GetLocalizedString(menuStr, kSTRListFinderMenuTitles, kStrMenuView);
    gViewMenu = NewMenu(131, menuStr);
    GetLocalizedString(menuStr, kSTRListFinderViewMenu, kStrByIcon);
    AppendMenu(gViewMenu, menuStr);
    GetLocalizedString(menuStr, kSTRListFinderViewMenu, kStrByName);
    AppendMenu(gViewMenu, menuStr);
    GetLocalizedString(menuStr, kSTRListFinderViewMenu, kStrBySize);
    AppendMenu(gViewMenu, menuStr);
    GetLocalizedString(menuStr, kSTRListFinderViewMenu, kStrByKind);
    AppendMenu(gViewMenu, menuStr);
    GetLocalizedString(menuStr, kSTRListFinderViewMenu, kStrByLabel);
    AppendMenu(gViewMenu, menuStr);
    GetLocalizedString(menuStr, kSTRListFinderViewMenu, kStrByDate);
    AppendMenu(gViewMenu, menuStr);
    AppendMenu(gViewMenu, "\002(-");
    GetLocalizedString(menuStr, kSTRListFinderViewMenu, kStrCleanUpWindow);
    AppendMenu(gViewMenu, menuStr);
    GetLocalizedString(menuStr, kSTRListFinderViewMenu, kStrCleanUpSelection);
    AppendMenu(gViewMenu, menuStr);

    /* Label Menu */
    GetLocalizedString(menuStr, kSTRListFinderMenuTitles, kStrMenuLabel);
    gLabelMenu = NewMenu(132, menuStr);
    GetLocalizedString(menuStr, kSTRListFinderLabelMenu, kStrLabelNone);
    AppendMenu(gLabelMenu, menuStr);
    GetLocalizedString(menuStr, kSTRListFinderLabelMenu, kStrLabelEssential);
    AppendMenu(gLabelMenu, menuStr);
    GetLocalizedString(menuStr, kSTRListFinderLabelMenu, kStrLabelHot);
    AppendMenu(gLabelMenu, menuStr);
    GetLocalizedString(menuStr, kSTRListFinderLabelMenu, kStrLabelInProgress);
    AppendMenu(gLabelMenu, menuStr);
    GetLocalizedString(menuStr, kSTRListFinderLabelMenu, kStrLabelCool);
    AppendMenu(gLabelMenu, menuStr);
    GetLocalizedString(menuStr, kSTRListFinderLabelMenu, kStrLabelPersonal);
    AppendMenu(gLabelMenu, menuStr);
    GetLocalizedString(menuStr, kSTRListFinderLabelMenu, kStrLabelProject1);
    AppendMenu(gLabelMenu, menuStr);
    GetLocalizedString(menuStr, kSTRListFinderLabelMenu, kStrLabelProject2);
    AppendMenu(gLabelMenu, menuStr);

    /* Special Menu */
    GetLocalizedString(menuStr, kSTRListFinderMenuTitles, kStrMenuSpecial);
    gSpecialMenu = NewMenu(133, menuStr);
    GetLocalizedString(menuStr, kSTRListFinderSpecialMenu, kStrCleanUpDesktop);
    AppendMenu(gSpecialMenu, menuStr);
    GetLocalizedString(menuStr, kSTRListFinderSpecialMenu, kStrEmptyTrash);
    AppendMenu(gSpecialMenu, menuStr);
    AppendMenu(gSpecialMenu, "\002(-");
    GetLocalizedString(menuStr, kSTRListFinderSpecialMenu, kStrEject);
    AppendMenu(gSpecialMenu, menuStr);
    GetLocalizedString(menuStr, kSTRListFinderSpecialMenu, kStrEraseDisk);
    AppendMenu(gSpecialMenu, menuStr);
    AppendMenu(gSpecialMenu, "\002(-");
    GetLocalizedString(menuStr, kSTRListFinderSpecialMenu, kStrRestart);
    AppendMenu(gSpecialMenu, menuStr);
    GetLocalizedString(menuStr, kSTRListFinderSpecialMenu, kStrShutDown);
    AppendMenu(gSpecialMenu, menuStr);

    /* Insert menus into menu bar in correct order: Apple, File, Edit, View, Label, Special */
    /* InsertMenu with 0 adds to end, so insert in order */
    InsertMenu(gAppleMenu, 0);    /* Apple at position 0 */
    InsertMenu(gFileMenu, 0);      /* File at position 1 */
    InsertMenu(gEditMenu, 0);      /* Edit at position 2 */
    InsertMenu(gViewMenu, 0);      /* View at position 3 */
    InsertMenu(gLabelMenu, 0);     /* Label at position 4 */
    InsertMenu(gSpecialMenu, 0);   /* Special at position 5 */

    /* Application (top-right) menu - icon only */
    /* Use system-defined Application menu ID (negative). If not available,
       fall back to a high ID unlikely to collide. */
    const short appMenuID = (short)0xBF97; /* kApplicationMenuID */
    MenuHandle appMenu = NewMenu(appMenuID, (ConstStr255Param)"\000");
    InsertMenu(appMenu, 0);

    extern void serial_puts(const char* str);
    serial_puts("Finder: About to call DrawMenuBar\n");
    DrawMenuBar();
    serial_puts("Finder: DrawMenuBar returned\n");

    /* Temporary fallback: only run if the application menu didn't stick */
    extern void SetupDefaultMenus(void);
    MenuHandle existingAppMenu = GetMenuHandle(appMenuID);
    if (existingAppMenu == NULL) {
        serial_puts("Finder: App menu handle missing, invoking SetupDefaultMenus\n");
        SetupDefaultMenus();
        serial_puts("Finder: SetupDefaultMenus returned\n");
    } else {
        serial_puts("Finder: App menu handle present, skipping SetupDefaultMenus\n");
    }

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
 * (Currently unused in the integrated build.)
 */
#if 0
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
#endif

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
    platform_halt();

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

/*-----------------------------------------------------------------------*/
/* Finder System Utilities                                              */
/*-----------------------------------------------------------------------*/

/*
 * FindFolder - Locate system folders
 * Basic stub implementation - returns root directory
 */
OSErr FindFolder(SInt16 vRefNum, OSType folderType, Boolean createFolder, SInt16* foundVRefNum, SInt32* foundDirID) {
    if (foundVRefNum) *foundVRefNum = vRefNum;
    if (foundDirID) *foundDirID = 2;  /* Root directory */
    return noErr;
}

/*-----------------------------------------------------------------------*/
/* Finder Event Handling Functions                                     */
/*-----------------------------------------------------------------------*/

/*
 * HandleKeyDown - Handle keyboard input in Finder
 */
void HandleKeyDown(EventRecord* event) {
    if (!event) return;

    /* Extract character code and key code from message */
    char charCode = (char)(event->message & charCodeMask);

    /* Key codes for special keys */
    #define kDeleteKey      0x08  /* Backspace/Delete */
    #define kReturnKey      0x0D  /* Return */
    #define kEnterKey       0x03  /* Enter */

    /* Check for command key shortcuts */
    if (event->modifiers & cmdKey) {
        extern long MenuKey(short ch);

        /* Convert to uppercase for menu matching */
        char menuChar = charCode;
        if (menuChar >= 'a' && menuChar <= 'z') {
            menuChar = menuChar - 'a' + 'A';
        }

        /* Call MenuKey to find matching menu command */
        long menuChoice = MenuKey(menuChar);

        if (menuChoice != 0) {
            /* Found a menu command - extract menuID and item, then execute */
            short menuID = HiWord(menuChoice);
            short menuItem = LoWord(menuChoice);
            DoMenuCommand(menuID, menuItem);
            return;
        }
    }

    /* Handle special keys without command modifier */
    if (!(event->modifiers & cmdKey)) {
        /* Delete key - delete selected items */
        if (charCode == kDeleteKey) {
            extern void Finder_Clear(void);
            Finder_Clear();
            return;
        }

        /* Return/Enter - open selected items */
        if (charCode == kReturnKey || charCode == kEnterKey) {
            extern void OpenSelectedItems(void);
            OpenSelectedItems();
            return;
        }
    }

    /* If we get here, key was not handled */
}

/*
 * HandleContentClick - Handle mouse click in window content area
 */
OSErr HandleContentClick(WindowPtr window, EventRecord* event) {
    if (!window || !event) {
        return paramErr;
    }

    /* Check if this is a folder window */
    extern Boolean IsFolderWindow(WindowPtr w);
    extern Boolean HandleFolderWindowClick(WindowPtr w, EventRecord *ev, Boolean isDoubleClick);

    if (IsFolderWindow(window)) {
        /* Extract double-click flag from event message */
        UInt16 clickCount = (event->message >> 16) & 0xFFFF;
        Boolean doubleClick = (clickCount >= 2);

        /* Delegate to folder window handler */
        HandleFolderWindowClick(window, event, doubleClick);
        return noErr;
    }

    /* For now, other window types (applications, control panels) handle their own clicks
     * through their event loops or will be implemented as needed */
    return noErr;
}

/*
 * CloseFinderWindow - Close a Finder window
 */
OSErr CloseFinderWindow(WindowPtr window) {
    if (!window) {
        return paramErr;
    }

    /* Try to close special windows first */
    extern void AboutWindow_CloseIf(WindowPtr w);
    extern void GetInfo_CloseIf(WindowPtr w);
    extern void Find_CloseIf(WindowPtr w);
    extern void CleanupFolderWindow(WindowPtr w);
    extern Boolean IsFolderWindow(WindowPtr w);

    /* Check About window */
    AboutWindow_CloseIf(window);

    /* Check Get Info window */
    GetInfo_CloseIf(window);

    /* Check Find window */
    Find_CloseIf(window);

    /* Check folder window */
    if (IsFolderWindow(window)) {
        CleanupFolderWindow(window);
        DisposeWindow(window);
        return noErr;
    }

    /* Default: just dispose */
    DisposeWindow(window);
    return noErr;
}

/*
 * DoUpdate - Handle window update events
 */
void DoUpdate(WindowPtr window) {
    if (!window) return;

    /* Route to appropriate update handler based on window type */
    extern Boolean AboutWindow_HandleUpdate(WindowPtr w);
    extern Boolean GetInfo_HandleUpdate(WindowPtr w);
    extern Boolean Find_HandleUpdate(WindowPtr w);
    extern void FolderWindow_Draw(WindowPtr w);
    extern Boolean IsFolderWindow(WindowPtr w);

    /* Try About window */
    if (AboutWindow_HandleUpdate(window)) {
        return;
    }

    /* Try Get Info window */
    if (GetInfo_HandleUpdate(window)) {
        return;
    }

    /* Try Find window */
    if (Find_HandleUpdate(window)) {
        return;
    }

    /* Try folder window - FolderWindow_Draw handles its own port and update region */
    if (IsFolderWindow(window)) {
        BeginUpdate(window);
        FolderWindow_Draw(window);
        EndUpdate(window);
        return;
    }

    /* FIXED (UPDATE-001): Generic window update handler for unknown window types
     * Previously this was a no-op, causing content not to redraw after drag/resize.
     * Now we properly call BeginUpdate/EndUpdate to clear the update region. */
    GrafPtr savePort;
    GetPort(&savePort);
    SetPort((GrafPtr)window);

    BeginUpdate(window);

    /* Erase content area for unknown windows */
    Rect contentRect = window->port.portRect;
    EraseRect(&contentRect);

    EndUpdate(window);

    SetPort(savePort);
}

/*
 * CleanUpWindow - Clean up items in a window
 */
OSErr CleanUpWindow(WindowPtr window, SInt16 cleanupType) {
    if (!window) return paramErr;

    /* Check if it's a folder window */
    extern Boolean IsFolderWindow(WindowPtr w);
    extern void FolderWindow_CleanUp(WindowPtr w, Boolean selectedOnly);

    if (IsFolderWindow(window)) {
        /* cleanupType: 0 = all items, 1 = selected only */
        Boolean selectedOnly = (cleanupType == 1);
        FolderWindow_CleanUp(window, selectedOnly);
    }

    return noErr;
}
