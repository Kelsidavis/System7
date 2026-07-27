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
#include "DeskManager/DeskAccessory.h"
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
static ConstStr255Param gFinderVersion = PSTR("Macintosh Finder Version 7.1");
static MenuHandle gAppleMenu, gFileMenu, gEditMenu, gViewMenu, gLabelMenu, gSpecialMenu, gControlPanelsMenu;
static MenuHandle gHelpMenu;

/*
 * Finder_InstallMenuBar - put the Finder's menus in the menu bar.
 *
 * The menu bar belongs to whichever application is active, so the Finder's
 * has to be re-installable, not just built once at startup: when a SimpleText
 * window closes, the Finder takes the bar back. This is the only description
 * of what the Finder's menu bar contains and in what order.
 */
void Finder_InstallMenuBar(void)
{
    InsertMenu(gAppleMenu, 0);     /* Apple at position 0 */
    InsertMenu(gFileMenu, 0);      /* File at position 1 */
    InsertMenu(gEditMenu, 0);      /* Edit at position 2 */
    InsertMenu(gViewMenu, 0);      /* View at position 3 */
    InsertMenu(gLabelMenu, 0);     /* Label at position 4 */
    InsertMenu(gSpecialMenu, 0);   /* Special at position 5 */
    if (gHelpMenu) {
        InsertMenu(gHelpMenu, 0);  /* Balloon help "?" - System 7 standard */
    }
    DrawMenuBar();
}

/* Forward Declarations */
OSErr InitializeFinder(void);  /* Made public for kernel integration */
static OSErr SetupMenus(void);
extern OSErr InitializeDesktopDB(void);  /* From desktop_manager.c */
extern OSErr InitializeTrashFolder(void);  /* From trash_manager.c */
static OSErr InitializeWindowManager(void);
/* HandleShutDown, HandleMenuChoice, HandleMouseDown, HandleKeyDown declared in #if 0 block below */
/* DoUpdate, DoActivate, DoBackgroundTasks declared in #if 0 block below */
/* MainEventLoop declared in #if 0 block below */

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

    /* Initialize Desk Manager so DAs are registered before menu setup */
    {
        extern int DeskManager_Initialize(void);
        DeskManager_Initialize();
    }

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

    /* Auto-open startup disk window — classic System 7 behavior.
     * The boot volume window opens automatically so users can see
     * their files immediately without double-clicking the disk icon. */
    {
        extern WindowPtr FolderWindow_OpenFolder(VRefNum vref, DirID dirID,
                                                  ConstStr255Param title);
        extern VRefNum VFS_GetBootVRef(void);

        extern bool VFS_GetVolumeInfo(VRefNum vref, VolumeControlBlock* vcb);
        VRefNum bootVref = VFS_GetBootVRef();

        /* Get actual volume name from VFS instead of hardcoding "Macintosh HD" */
        unsigned char hdTitle[256];
        VolumeControlBlock vcb;
        memset(&vcb, 0, sizeof(vcb));
        if (VFS_GetVolumeInfo(bootVref, &vcb) && vcb.name[0]) {
            int len = 0;
            while (vcb.name[len] && len < 255) len++;
            hdTitle[0] = (unsigned char)len;
            memcpy(&hdTitle[1], vcb.name, len);
        } else {
            /* Fallback to default name */
            static Str255 defaultTitle;
            c2pstrcpy(defaultTitle, "Macintosh HD");
            memcpy(hdTitle, defaultTitle, sizeof(defaultTitle));
        }

        WindowPtr diskWin = FolderWindow_OpenFolder(bootVref, 2, hdTitle);
        if (diskWin) {
            serial_puts("Finder: Opened startup disk window\n");
        }
    }

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
    AppendMenu(gAppleMenu, PSTR("(-"));
    AddResMenu(gAppleMenu, 'DRVR');

    /* Everything below the divider is the Apple Menu Items folder, which
     * System 7 lists alphabetically - Control Panels included, rather than
     * pinned above the desk accessories. There used to be a hardcoded "Notepad"
     * item here too, duplicating the "Note Pad" desk accessory registered in
     * BuiltinDAs.c, so the menu showed both. Dispatch in MenuCommands.c is by
     * item name rather than index, so the ordering is free to change and
     * "Note Pad" resolves through OpenDeskAcc. */
    {
        extern SInt16 CountMenuItems(MenuHandle theMenu);
        extern void SetItemSubmenu(MenuHandle theMenu, short item, short submenuID);

        /* static: 20x64 plus scratch is over 1.3K, too much for the kernel
         * stack this runs on - taking it as locals wiped the rest of the menu. */
        static char names[20][64];
        static char cpName[64];
        short count = 0;

        /* Control Panels: the localized strings carry a trailing '>' as a
         * stand-in for the hierarchical triangle, which showed up literally on
         * screen ("Control Panels>"). Strip it here rather than in 38
         * translation files; the triangle is drawn from the submenu link set
         * below. */
        GetLocalizedString(menuStr, kSTRListFinderAppleMenu, kStrControlPanelsSubmenu);
        {
            short len = menuStr[0];
            if (len > 0 && menuStr[len] == '>') len--;
            if (len > 63) len = 63;
            for (short c = 0; c < len; c++) names[count][c] = (char)menuStr[c + 1];
            names[count][len] = '\0';
            strncpy(cpName, names[count], 64);
            count++;
        }

        {
            DARegistryEntry* daEntries[16];
            int daCount = DA_GetRegisteredDAs(daEntries, 16);
            for (int d = 0; d < daCount && count < 20; d++) {
                if (daEntries[d] && daEntries[d]->name[0]) {
                    strncpy(names[count], daEntries[d]->name, 63);
                    names[count][63] = '\0';
                    count++;
                }
            }
        }

        /* Insertion sort by name */
        for (short a = 1; a < count; a++) {
            char key[64];
            short b = a - 1;
            strncpy(key, names[a], 64);
            while (b >= 0 && strcmp(names[b], key) > 0) {
                strncpy(names[b + 1], names[b], 64);
                b--;
            }
            strncpy(names[b + 1], key, 64);
        }

        for (short n = 0; n < count; n++) {
            unsigned char pName[64];
            int len = 0;
            while (names[n][len] && len < 63) { pName[len + 1] = names[n][len]; len++; }
            pName[0] = (unsigned char)len;
            AppendMenu(gAppleMenu, pName);

            /* Link the Control Panels item to its submenu wherever it sorted.
             * Matched against the localized string, not an English literal. */
            if (strcmp(names[n], cpName) == 0) {
                SetItemSubmenu(gAppleMenu, CountMenuItems(gAppleMenu), 134);
            }
        }
    }

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

    /* The Apple menu's link to this submenu is established above, at whatever
     * index Control Panels sorts to. A hardcoded SetItemSubmenu(gAppleMenu, 3,
     * 134) used to live here from when Control Panels was fixed at item 3;
     * once the items were sorted alphabetically it marked Alarm Clock as
     * hierarchical instead, giving it a stray triangle.
     * Note: Do NOT insert into menu bar - keep as submenu only */

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
    AppendMenu(gFileMenu, PSTR("(-"));
    GetLocalizedString(menuStr, kSTRListFinderFileMenu, kStrGetInfo);
    AppendMenu(gFileMenu, menuStr);
    SetItemCmd(gFileMenu, 6, 'I');  /* Cmd+I for Get Info */
    GetLocalizedString(menuStr, kSTRListFinderFileMenu, kStrSharing);
    AppendMenu(gFileMenu, menuStr);
    GetLocalizedString(menuStr, kSTRListFinderFileMenu, kStrDuplicate);
    AppendMenu(gFileMenu, menuStr);
    GetLocalizedString(menuStr, kSTRListFinderFileMenu, kStrMakeAlias);
    AppendMenu(gFileMenu, menuStr);
    GetLocalizedString(menuStr, kSTRListFinderFileMenu, kStrPutAway);
    AppendMenu(gFileMenu, menuStr);
    AppendMenu(gFileMenu, PSTR("(-"));
    GetLocalizedString(menuStr, kSTRListFinderFileMenu, kStrFindEllipsis);
    AppendMenu(gFileMenu, menuStr);
    GetLocalizedString(menuStr, kSTRListFinderFileMenu, kStrFindAgain);
    AppendMenu(gFileMenu, menuStr);

    /* Edit Menu */
    GetLocalizedString(menuStr, kSTRListFinderMenuTitles, kStrMenuEdit);
    gEditMenu = NewMenu(130, menuStr);
    GetLocalizedString(menuStr, kSTRListFinderEditMenu, kStrUndo);
    AppendMenu(gEditMenu, menuStr);
    AppendMenu(gEditMenu, PSTR("(-"));
    GetLocalizedString(menuStr, kSTRListFinderEditMenu, kStrCut);
    AppendMenu(gEditMenu, menuStr);
    GetLocalizedString(menuStr, kSTRListFinderEditMenu, kStrCopy);
    AppendMenu(gEditMenu, menuStr);
    GetLocalizedString(menuStr, kSTRListFinderEditMenu, kStrPaste);
    AppendMenu(gEditMenu, menuStr);
    GetLocalizedString(menuStr, kSTRListFinderEditMenu, kStrClear);
    AppendMenu(gEditMenu, menuStr);
    /* System 7 separates Select All from the cut/copy/paste group, which makes
     * it item 8 - the number MenuCommands.c has always dispatched on. Without
     * the divider it was item 7 and Select All fell through to the handler's
     * "unknown item" case, so the command did nothing at all. */
    AppendMenu(gEditMenu, PSTR("(-"));
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
    /* System 7's View menu is the view choices and nothing else. The two
     * Clean Up commands that used to hang off the bottom of it belong to
     * Special, which already had one of them - the menu carried a second,
     * differently-behaving copy of a command sitting two menus away. */

    /* System 7 shows a check against the current view. Folder windows open in
     * icon view, so item 1 starts checked; HandleViewMenu moves it from there.
     * CheckItem was already being called on selection - nothing had set the
     * initial state, so the menu opened with no view marked at all. */
    {
        extern void CheckItem(MenuHandle theMenu, short item, Boolean checked);
        CheckItem(gViewMenu, 1, true);
    }

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
    AppendMenu(gSpecialMenu, PSTR("(-"));
    GetLocalizedString(menuStr, kSTRListFinderSpecialMenu, kStrEject);
    AppendMenu(gSpecialMenu, menuStr);
    GetLocalizedString(menuStr, kSTRListFinderSpecialMenu, kStrEraseDisk);
    AppendMenu(gSpecialMenu, menuStr);
    AppendMenu(gSpecialMenu, PSTR("(-"));
    GetLocalizedString(menuStr, kSTRListFinderSpecialMenu, kStrRestart);
    AppendMenu(gSpecialMenu, menuStr);
    GetLocalizedString(menuStr, kSTRListFinderSpecialMenu, kStrShutDown);
    AppendMenu(gSpecialMenu, menuStr);

    /* The Help menu's contents never change, so it is built once here and
     * reinstalled with the rest of the bar. */
    {
        static Str255 helpTitle;
        c2pstrcpy(helpTitle, "?");
        gHelpMenu = NewMenu((short)0xBF96, (ConstStr255Param)helpTitle);
        if (gHelpMenu) {
            AppendMenu(gHelpMenu, "\020About Balloon Help\311");
            AppendMenu(gHelpMenu, PSTR("Show Balloons"));
        }
    }

    Finder_InstallMenuBar();

    /* Keep open folder windows honest about changes made outside the Finder. */
    FolderWindow_ListenForVolumeChanges();

    /* Application (top-right) menu — lists running applications */
    const short appMenuID = (short)0xBF97; /* kApplicationMenuID */
    MenuHandle appMenu = NewMenu(appMenuID, (ConstStr255Param)"\000");
    if (appMenu) {
        AppendMenu(appMenu, PSTR("Hide Others"));
        AppendMenu(appMenu, PSTR("Show All"));
        AppendMenu(appMenu, PSTR("(-"));
        AppendMenu(appMenu, PSTR("Finder"));
    }
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
        int tLen = 0;
        while (titleText[tLen]) tLen++;
        if (tLen > 254) tLen = 254;
        memcpy(&windowTitleBuf[1], titleText, tLen);
        windowTitleBuf[0] = (unsigned char)tLen;
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
                        StandardPutFile(PSTR("Save As:"), PSTR("Untitled"), &reply);
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
 * FindFolder - Locate system folders.
 *
 * The folder is looked up by name in the file system, because that is where
 * it actually is. This used to answer from a table of directory IDs written
 * into the code - System Folder is 3, Trash is 4, and so on - and the table
 * had drifted from the volume, which creates System Folder as 16, Documents
 * as 17 and Applications as 18. So FindFolder named directories that did not
 * exist: the Trash window enumerated directory 4 and found nothing, while the
 * Trash itself put files somewhere else entirely, and the desktop's trash can
 * showed full over a window that said it was empty.
 *
 * Nested folders are resolved through their parent, as in System 7, rather
 * than assumed to sit at the root.
 */
OSErr FindFolder(SInt16 vRefNum, OSType folderType, Boolean createFolder,
                 SInt16* foundVRefNum, SInt32* foundDirID) {
    extern VRefNum VFS_GetBootVRef(void);

    const char* inSystemFolder = NULL;   /* NULL means "at the root" */
    const char* name = NULL;

    switch (folderType) {
        case 'macs': name = "System Folder"; break;
        case 'trsh': name = "Trash"; break;
        case 'desk': name = NULL; break;              /* Desktop is the root */
        case 'temp': name = "Temporary Items"; break;
        case 'pref': name = "Preferences";        inSystemFolder = "System Folder"; break;
        case 'extn': name = "Extensions";         inSystemFolder = "System Folder"; break;
        case 'ctrl': name = "Control Panels";     inSystemFolder = "System Folder"; break;
        case 'font': name = "Fonts";              inSystemFolder = "System Folder"; break;
        case 'strt': name = "Startup Items";      inSystemFolder = "System Folder"; break;
        case 'amnu': name = "Apple Menu Items";   inSystemFolder = "System Folder"; break;
        default:     name = NULL; break;              /* Unknown: the root */
    }

    VRefNum vref = (vRefNum == kOnSystemDisk || vRefNum == 0)
                       ? VFS_GetBootVRef() : (VRefNum)vRefNum;
    DirID dir = 2;   /* the root */

    if (name) {
        CatEntry entry;
        DirID parent = 2;

        if (inSystemFolder) {
            CatEntry sysFolder;
            if (VFS_Lookup(vref, 2, inSystemFolder, &sysFolder) &&
                sysFolder.kind == kNodeDir) {
                parent = (DirID)sysFolder.id;
            }
        }

        if (VFS_Lookup(vref, parent, name, &entry) && entry.kind == kNodeDir) {
            dir = (DirID)entry.id;
        } else if (createFolder) {
            DirID created = 0;
            if (VFS_CreateFolder(vref, parent, name, &created)) {
                dir = created;
            } else {
                return fnfErr;
            }
        } else {
            return fnfErr;
        }
    }

    if (foundVRefNum) *foundVRefNum = (SInt16)vref;
    if (foundDirID) *foundDirID = (SInt32)dir;
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
        /* Cmd+Shift+3 = Screenshot (classic Mac shortcut, FKEY 3) */
        if (charCode == '3' && (event->modifiers & shiftKey)) {
            extern void SysBeep(short duration);
            extern void InvertRect(const Rect* r);
            extern QDGlobals qd;
            extern void hal_framebuffer_present(void);

            /* Flash the screen white (visual feedback for screenshot) */
            InvertRect(&qd.screenBits.bounds);
            hal_framebuffer_present();

            /* Brief pause for visual effect */
            extern OSErr MicrosecondDelay(UInt32 microseconds);
            MicrosecondDelay(100000);  /* 100ms flash */

            /* Restore screen */
            InvertRect(&qd.screenBits.bounds);
            hal_framebuffer_present();

            /* Camera shutter sound */
            SysBeep(1);
            return;
        }

        /* Cmd+Shift+Delete = Empty Trash (power-user shortcut) */
        /* Cmd+Delete = Move selected to Trash */
        if (charCode == kDeleteKey) {
            if (event->modifiers & shiftKey) {
                extern OSErr EmptyTrash(Boolean force);
                EmptyTrash(false);  /* false = show confirmation dialog */
            } else {
                extern void Finder_Clear(void);
                Finder_Clear();
            }
            return;
        }

        /* Cmd+Up Arrow = Navigate to parent folder */
        /* Cmd+Down Arrow = Open selected item */
        if (charCode == 0x1E || charCode == 0x1F) {
            extern WindowPtr FrontWindow(void);
            extern Boolean IsFolderWindow(WindowPtr w);

            WindowPtr front = FrontWindow();
            if (front && IsFolderWindow(front)) {
                if (charCode == 0x1F) {
                    /* Cmd+Down = Open selected (same as Return/Enter) */
                    extern void FolderWindow_OpenSelected(WindowPtr w);
                    FolderWindow_OpenSelected(front);
                } else {
                    /* Cmd+Up = Navigate to parent folder */
                    extern DirID FolderWindow_GetCurrentDir(WindowPtr w);
                    extern VRefNum FolderWindow_GetVRef(WindowPtr w);
                    extern WindowPtr FolderWindow_OpenFolder(VRefNum vref, DirID dirID,
                                                              ConstStr255Param title);
                    extern bool VFS_GetParentDir(VRefNum vref, DirID dirID, DirID* parentID);

                    VRefNum vref = FolderWindow_GetVRef(front);
                    DirID currentDir = FolderWindow_GetCurrentDir(front);

                    if (currentDir > 2) {  /* Not already at root */
                        DirID parentDir = 0;
                        if (VFS_GetParentDir(vref, currentDir, &parentDir) && parentDir >= 2) {
                            /* Build parent folder title */
                            extern const char* VFS_GetNameByID(VRefNum vref, DirID dir, FileID id);
                            const char* parentName = VFS_GetNameByID(vref, parentDir, parentDir);
                            unsigned char pTitle[256];
                            if (parentName) {
                                int len = 0;
                                while (parentName[len] && len < 255) len++;
                                pTitle[0] = (unsigned char)len;
                                memcpy(&pTitle[1], parentName, len);
                            } else {
                                pTitle[0] = 12;
                                memcpy(&pTitle[1], "Macintosh HD", 12);
                            }
                            FolderWindow_OpenFolder(vref, parentDir, pTitle);
                        }
                    } else {
                        extern void SysBeep(short duration);
                        SysBeep(1);  /* Already at root */
                    }
                }
                return;
            }
        }

        /* Cmd+Option+W = Close all windows (power-user shortcut) */
        if ((charCode == 'w' || charCode == 'W') && (event->modifiers & optionKey)) {
            extern WindowPtr FrontWindow(void);
            extern OSErr CloseFinderWindow(WindowPtr w);
            extern Boolean IsFolderWindow(WindowPtr w);

            /* Close all folder windows */
            WindowPtr w;
            while ((w = FrontWindow()) != NULL) {
                if (!IsFolderWindow(w)) break;  /* Stop at non-folder windows */
                CloseFinderWindow(w);
            }
            return;
        }

        /* Cmd+` = Cycle to next window (standard Mac OS shortcut) */
        if (charCode == '`' || charCode == '~') {
            extern WindowPtr FrontWindow(void);
            extern void SelectWindow(WindowPtr w);
            extern void SendBehind(WindowPtr window, WindowPtr behindWindow);

            WindowPtr front = FrontWindow();
            if (front && front->nextWindow) {
                /* Send the front window to the back, bringing the next one forward */
                SendBehind(front, NULL);  /* NULL = send to very back */
                WindowPtr newFront = FrontWindow();
                if (newFront) {
                    SelectWindow(newFront);
                }
            }
            return;
        }

        /* Adjust menu states before MenuKey so disabled items don't trigger */
        extern void Finder_AdjustMenus(void);
        Finder_AdjustMenus();

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

        /* Arrow keys - navigate selection in folder windows.
         * Shift+arrow extends selection (System 7 behavior). */
        if (charCode >= 0x1C && charCode <= 0x1F) {
            extern WindowPtr FrontWindow(void);
            extern Boolean IsFolderWindow(WindowPtr w);
            extern void FolderWindow_ArrowKey(WindowPtr w, Boolean isDown, Boolean extend);
            extern void FolderWindow_ArrowKeyLR(WindowPtr w, Boolean isRight);

            WindowPtr front = FrontWindow();
            Boolean shiftExtend = (event->modifiers & shiftKey) != 0;
            if (front && IsFolderWindow(front)) {
                if (charCode == 0x1E || charCode == 0x1F) {
                    FolderWindow_ArrowKey(front, charCode == 0x1F, shiftExtend);
                } else {
                    FolderWindow_ArrowKeyLR(front, charCode == 0x1D);
                }
                return;
            }
        }

        /* Tab/Shift+Tab - cycle selection to next/previous item */
        if (charCode == 0x09) {  /* Tab */
            extern WindowPtr FrontWindow(void);
            extern Boolean IsFolderWindow(WindowPtr w);
            extern void FolderWindow_TabKey(WindowPtr w, Boolean reverse);

            WindowPtr front = FrontWindow();
            if (front && IsFolderWindow(front)) {
                FolderWindow_TabKey(front, (event->modifiers & shiftKey) != 0);
                return;
            }
        }

        /* Type-ahead selection: typing letters jumps to matching file */
        if (charCode >= 0x20 && charCode <= 0x7E) {
            extern WindowPtr FrontWindow(void);
            extern Boolean IsFolderWindow(WindowPtr w);
            extern void FolderWindow_TypeAhead(WindowPtr w, char ch);

            WindowPtr front = FrontWindow();
            if (front && IsFolderWindow(front)) {
                FolderWindow_TypeAhead(front, charCode);
                return;
            }
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
    extern Boolean AboutWindow_CloseIf(WindowPtr w);
    extern Boolean GetInfo_CloseIf(WindowPtr w);
    extern Boolean Find_CloseIf(WindowPtr w);
    extern void CleanupFolderWindow(WindowPtr w);
    extern Boolean IsFolderWindow(WindowPtr w);

    /* Each of these disposes the window itself when it owns it, so the first
     * one that claims it ends the sequence - falling through to the dispose
     * below would free the window a second time. */
    if (AboutWindow_CloseIf(window)) return noErr;
    if (GetInfo_CloseIf(window))     return noErr;
    if (Find_CloseIf(window))        return noErr;

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
/*
 * Finder_DrawWindowContents - draw the content of whichever Finder window
 * this is. Returns false if it is not one the Finder knows.
 *
 * The caller owns BeginUpdate/EndUpdate; this only paints. That split is the
 * point: the live update path in EventDispatcher already brackets the draw,
 * and it only knew how to paint folder windows - so About This Macintosh,
 * Get Info and Find opened, got their content erased, and stayed blank. Each
 * of them has had a working draw handler the whole time and nothing called
 * it, because the dispatch that knew about them lived in DoUpdate, which no
 * longer has any callers.
 */
Boolean Finder_DrawWindowContents(WindowPtr window) {
    if (!window) return false;

    extern Boolean AboutWindow_HandleUpdate(WindowPtr w);
    extern Boolean GetInfo_HandleUpdate(WindowPtr w);
    extern Boolean Find_HandleUpdate(WindowPtr w);
    extern void FolderWindow_Draw(WindowPtr w);
    extern Boolean IsFolderWindow(WindowPtr w);

    if (AboutWindow_HandleUpdate(window)) return true;
    if (GetInfo_HandleUpdate(window))     return true;
    if (Find_HandleUpdate(window))        return true;

    if (IsFolderWindow(window)) {
        FolderWindow_Draw(window);
        return true;
    }

    return false;
}

void DoUpdate(WindowPtr window) {
    if (!window) return;

    BeginUpdate(window);

    GrafPtr savePort;
    GetPort(&savePort);
    SetPort((GrafPtr)window);

    if (!Finder_DrawWindowContents(window)) {
        /* Not a Finder window - clear the content so stale pixels do not
         * survive a drag or resize. */
        Rect contentRect = window->port.portRect;
        EraseRect(&contentRect);
    }

    SetPort(savePort);
    EndUpdate(window);
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
