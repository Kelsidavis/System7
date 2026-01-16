#include "MemoryMgr/MemoryManager.h"
/*
 * MenuCommands.c - Menu Command Dispatcher
 *
 * Handles menu selections and executes appropriate commands
 * for System 7.1 menu structure.
 */

#include "SystemTypes.h"
#include "System71StdLib.h"
#include "MenuManager/MenuManager.h"
#include "EventManager/EventManager.h"
#include "Finder/AboutThisMac.h"
#include "Finder/GetInfo.h"
#include "Finder/finder.h"
#include "FS/hfs_types.h"
#include "ControlPanels/DesktopPatterns.h"
#include "ControlPanels/Sound.h"
#include "ControlPanels/Mouse.h"
#include "ControlPanels/Keyboard.h"
#include "ControlPanels/ControlStrip.h"
#include "Datetime/datetime_cdev.h"
#include "DeskManager/Notepad.h"
#include "Platform/Halt.h"
#include "Platform/include/io.h"

#include <string.h>

#define MENU_LOG_DEBUG(fmt, ...) serial_logf(kLogModuleMenu, kLogLevelDebug, "[MENU] " fmt, ##__VA_ARGS__)
#define MENU_LOG_WARN(fmt, ...)  serial_logf(kLogModuleMenu, kLogLevelWarn,  "[MENU] " fmt, ##__VA_ARGS__)
#define MENU_LOG_INFO(fmt, ...)  serial_logf(kLogModuleMenu, kLogLevelInfo, fmt, ##__VA_ARGS__)

/* Forward declarations */
static void perform_power_off(void);

/* Menu IDs - Standard System 7.1 */
#define kAppleMenuID    128  /* Apple menu is ID 128 in our system */
#define kFileMenuID     129
#define kEditMenuID     130
#define kViewMenuID     131
#define kLabelMenuID    132
#define kSpecialMenuID  133

/* Apple Menu Items */
#define kAboutItem      1
#define kDeskAccItem    2  /* Desk accessories start here */

/* File Menu Items - Finder specific (System 7.1) */
#define kNewFolderItem  1
#define kOpenItem       2
#define kPrintItem      3
#define kCloseItem      4
#define kGetInfoItem    6
#define kSharingItem    7
#define kDuplicateItem  8
#define kMakeAliasItem  9
#define kPutAwayItem    10
#define kFindItem       12
#define kFindAgainItem  13

/* Edit Menu Items */
#define kUndoItem       1
#define kCutItem        3
#define kCopyItem       4
#define kPasteItem      5
#define kClearItem      6
#define kSelectAllItem  8

/* View Menu Items */
#define kBySmallIcon    1
#define kByIcon         2
#define kByName         3
#define kBySize         4
#define kByKind         5
#define kByLabel        6
#define kByDate         7

static void perform_power_off(void) {
#if defined(__i386__) || defined(__x86_64__)
    /* ACPI power-off sequence for x86 hardware */
    hal_outw(0x604, 0x2000);
    hal_outb(0xB004, 0x53);
#endif
    platform_halt();
}

static void perform_restart(void) {
#if defined(__i386__) || defined(__x86_64__)
    __asm__ volatile(
        "movl $0, %%esp\n\t"
        "push $0\n\t"
        "push $0\n\t"
        "lidt (%%esp)\n\t"
        "int3\n\t"
        :
        :
        : "memory"
    );
#endif
    platform_halt();
}

/* Forward declarations */
static void HandleAppleMenu(short item);
static void HandleFileMenu(short item);
static void HandleEditMenu(short item);
static void HandleViewMenu(short item);
static void HandleLabelMenu(short item);
static void HandleSpecialMenu(short item);
static void HandleControlPanelsMenu(short item);

/* Main menu command dispatcher */
void DoMenuCommand(short menuID, short item)
{
    extern void serial_printf(const char* fmt, ...);
    serial_printf("*** DoMenuCommand: CALLED with menu=%d, item=%d\n", menuID, item);
    MENU_LOG_DEBUG("DoMenuCommand: menu=%d, item=%d\n", menuID, item);

    switch (menuID) {
        case kAppleMenuID:
            serial_printf("*** DoMenuCommand: Calling HandleAppleMenu(%d)\n", item);
            HandleAppleMenu(item);
            break;

        case kFileMenuID:
            HandleFileMenu(item);
            break;

        case kEditMenuID:
            HandleEditMenu(item);
            break;

        case kViewMenuID:
            HandleViewMenu(item);
            break;

        case kLabelMenuID:
            HandleLabelMenu(item);
            break;

        case kSpecialMenuID:
            HandleSpecialMenu(item);
            break;

        case 134:  /* Control Panels submenu */
            HandleControlPanelsMenu(item);
            break;

        default:
            MENU_LOG_WARN("Unknown menu ID: %d\n", menuID);
            break;
    }

    /* Clear menu highlighting after command */
    HiliteMenu(0);
}

/* Apple Menu Handler */
static Boolean GetMenuItemCString(short menuID, short item, char *out, size_t outSize)
{
    if (!out || outSize == 0) {
        return false;
    }

    MenuHandle menu = GetMenuHandle(menuID);
    if (!menu) {
        return false;
    }

    Str255 itemText;
    GetMenuItemText(menu, item, itemText);

    size_t len = itemText[0];
    if (len >= outSize) {
        len = outSize - 1;
    }

    memcpy(out, &itemText[1], len);
    out[len] = '\0';
    return true;
}

static void HandleAppleMenu(short item)
{
    char itemName[256];
    if (!GetMenuItemCString(kAppleMenuID, item, itemName, sizeof(itemName))) {
        MENU_LOG_WARN("Apple Menu: unable to resolve item %d\n", item);
        return;
    }

    if (strcmp(itemName, "About This Macintosh") == 0) {
        MENU_LOG_DEBUG("About This Macintosh...\n");
        extern void AboutWindow_ShowOrToggle(void);
        AboutWindow_ShowOrToggle();
        return;
    }

    if (strcmp(itemName, "Desktop Patterns...") == 0) {
        MENU_LOG_DEBUG("Apple Menu > Desktop Patterns...\n");
        OpenDesktopCdev();
        return;
    }

    if (strcmp(itemName, "Date & Time...") == 0) {
        MENU_LOG_DEBUG("Apple Menu > Date & Time...\n");
        DateTimePanel_Open();
        return;
    }

    if (strcmp(itemName, "Sound...") == 0) {
        MENU_LOG_DEBUG("Apple Menu > Sound...\n");
        SoundPanel_Open();
        return;
    }

    if (strcmp(itemName, "Mouse...") == 0) {
        MENU_LOG_DEBUG("Apple Menu > Mouse...\n");
        MousePanel_Open();
        return;
    }

    if (strcmp(itemName, "Keyboard...") == 0) {
        MENU_LOG_DEBUG("Apple Menu > Keyboard...\n");
        KeyboardPanel_Open();
        return;
    }

    if (strcmp(itemName, "Control Strip...") == 0) {
        MENU_LOG_DEBUG("Apple Menu > Control Strip...\n");
        ControlStrip_Toggle();
        return;
    }

    if (strcmp(itemName, "Notepad") == 0) {
        MENU_LOG_DEBUG("Apple Menu > Notepad\n");
        serial_puts("[MENU] Opening Notepad...\n");
        /* Open Notepad desk accessory */
        WindowPtr notepadWin;
        OSErr err = Notepad_Open(&notepadWin);
        char buf[80];
        snprintf(buf, sizeof(buf), "[MENU] Notepad_Open returned %d, window=%p\n", err, (void*)notepadWin);
        serial_puts(buf);
        return;
    }

    if (strcmp(itemName, "-") == 0) {
        /* Separator */
        return;
    }

    MENU_LOG_WARN("Unknown Apple menu item: '%s' (index %d)\n", itemName, item);
}

/* Control Panels Menu Handler */
static void HandleControlPanelsMenu(short item)
{
    char itemName[256];
    if (!GetMenuItemCString(134, item, itemName, sizeof(itemName))) {
        MENU_LOG_WARN("Control Panels Menu: unable to resolve item %d\n", item);
        return;
    }

    if (strcmp(itemName, "Desktop Patterns...") == 0) {
        MENU_LOG_DEBUG("Control Panels > Desktop Patterns...\n");
        OpenDesktopCdev();
        return;
    }

    if (strcmp(itemName, "Date & Time...") == 0) {
        MENU_LOG_DEBUG("Control Panels > Date & Time...\n");
        DateTimePanel_Open();
        return;
    }

    if (strcmp(itemName, "Sound...") == 0) {
        MENU_LOG_DEBUG("Control Panels > Sound...\n");
        SoundPanel_Open();
        return;
    }

    if (strcmp(itemName, "Mouse...") == 0) {
        MENU_LOG_DEBUG("Control Panels > Mouse...\n");
        MousePanel_Open();
        return;
    }

    if (strcmp(itemName, "Keyboard...") == 0) {
        MENU_LOG_DEBUG("Control Panels > Keyboard...\n");
        KeyboardPanel_Open();
        return;
    }

    if (strcmp(itemName, "Control Strip...") == 0) {
        MENU_LOG_DEBUG("Control Panels > Control Strip...\n");
        ControlStrip_Toggle();
        return;
    }

    MENU_LOG_WARN("Unknown Control Panels menu item: '%s' (index %d)\n", itemName, item);
}

/* File Menu Handler - Finder specific */
static void HandleFileMenu(short item)
{
    extern WindowPtr FrontWindow(void);
    extern void DrawDesktop(void);

    switch (item) {
        case kNewFolderItem: {
            MENU_LOG_INFO("File > New Folder\n");
            /* Create new folder in current window or desktop */
            extern bool VFS_CreateFolder(VRefNum vref, DirID parent, const char* name, DirID* newID);
            extern bool VFS_GenerateUniqueName(VRefNum vref, DirID dir, const char* base, char* out);
            extern Boolean IsFolderWindow(WindowPtr w);
            extern VRefNum FolderWindow_GetVRef(WindowPtr w);
            extern DirID FolderWindow_GetCurrentDir(WindowPtr w);

            WindowPtr front = FrontWindow();
            VRefNum targetVRef = 0;
            DirID targetDir = 2;  /* Default to desktop */

            /* If front window is a folder window, create in that folder */
            if (front && IsFolderWindow(front)) {
                targetVRef = FolderWindow_GetVRef(front);
                targetDir = FolderWindow_GetCurrentDir(front);
                MENU_LOG_DEBUG("Creating folder in folder window (vref=%d, dir=%d)\n",
                             targetVRef, targetDir);
            } else {
                MENU_LOG_DEBUG("Creating folder on desktop (dir=2)\n");
            }

            /* Generate unique name */
            char folderName[256];
            if (!VFS_GenerateUniqueName(targetVRef, targetDir, "untitled folder", folderName)) {
                MENU_LOG_DEBUG("Failed to generate unique folder name\n");
                break;
            }

            /* Create the folder */
            DirID newFolderID;
            if (VFS_CreateFolder(targetVRef, targetDir, folderName, &newFolderID)) {
                MENU_LOG_DEBUG("Created new folder '%s' with ID %d\n", folderName, (int)newFolderID);

                /* Refresh the window or desktop */
                if (front && IsFolderWindow(front)) {
                    /* Reload folder contents to show the new folder */
                    extern void InitializeFolderContentsEx(WindowPtr w, Boolean isTrash, VRefNum vref, DirID dirID);
                    InitializeFolderContentsEx(front, false, targetVRef, targetDir);

                    /* Trigger redraw */
                    PostEvent(updateEvt, (UInt32)(uintptr_t)front);
                } else {
                    DrawDesktop();
                }
            } else {
                MENU_LOG_DEBUG("Failed to create folder '%s'\n", folderName);
            }
            break;
        }

        case kOpenItem: {
            MENU_LOG_INFO("File > Open\n");
            /* Open selected item in Finder window - or open front window's selected item */
            extern void OpenSelectedItems(void);
            OpenSelectedItems();
            break;
        }

        case kPrintItem: {
            MENU_LOG_INFO("File > Print\n");
            /* Print selected item - used for printing documents from Finder */
            MENU_LOG_DEBUG("Print not implemented for Finder\n");
            break;
        }

        case kCloseItem: {
            MENU_LOG_INFO("File > Close\n");
            /* Close current window - standard close operation */
            extern void CloseWindow(WindowPtr window);
            WindowPtr front = FrontWindow();
            if (front && front->visible) {
                MENU_LOG_DEBUG("Closing front window 0x%08x\n", (unsigned int)P2UL(front));
                CloseWindow(front);
            }
            break;
        }

        case kGetInfoItem: {
            MENU_LOG_INFO("File > Get Info\n");
            /* Show Get Info dialog for selected item(s) */
            extern void ShowGetInfoDialog(WindowPtr w);
            WindowPtr front = FrontWindow();
            if (front) {
                ShowGetInfoDialog(front);
            }
            break;
        }

        case kSharingItem: {
            MENU_LOG_INFO("File > Sharing...\n");
            /* Show Sharing settings for selected item */
            MENU_LOG_DEBUG("Sharing settings not implemented\n");
            break;
        }

        case kDuplicateItem: {
            MENU_LOG_INFO("File > Duplicate\n");
            /* Duplicate selected items in Finder */
            extern void DuplicateSelectedItems(WindowPtr w);
            WindowPtr front = FrontWindow();
            if (front) {
                DuplicateSelectedItems(front);
            }
            break;
        }

        case kMakeAliasItem: {
            MENU_LOG_INFO("File > Make Alias\n");
            /* Create alias of selected items */
            extern void MakeAliasOfSelectedItems(WindowPtr w);
            WindowPtr front = FrontWindow();
            if (front) {
                MakeAliasOfSelectedItems(front);
            }
            break;
        }

        case kPutAwayItem: {
            MENU_LOG_INFO("File > Put Away\n");
            /* Put away selected item - move to trash or eject volume */
            extern void PutAwaySelectedItems(WindowPtr w);
            WindowPtr front = FrontWindow();
            if (front) {
                PutAwaySelectedItems(front);
            }
            break;
        }

        case kFindItem: {
            MENU_LOG_INFO("File > Find...\n");
            /* Show Find dialog to search for files */
            extern OSErr ShowFind(void);
            OSErr err = ShowFind();
            if (err != noErr) {
                MENU_LOG_DEBUG("ShowFind failed with error %d\n", err);
            }
            break;
        }

        case kFindAgainItem: {
            MENU_LOG_INFO("File > Find Again\n");
            /* Repeat the last find operation */
            extern OSErr FindAgain(void);
            OSErr err = FindAgain();
            if (err != noErr) {
                MENU_LOG_DEBUG("FindAgain failed with error %d\n", err);
            }
            break;
        }

        default:
            MENU_LOG_WARN("Unknown File menu item: %d\n", item);
            break;
    }
}

/* Edit Menu Handler - System 7.1 standard */
static void HandleEditMenu(short item)
{
    extern WindowPtr FrontWindow(void);

    switch (item) {
        case kUndoItem: {
            MENU_LOG_INFO("Edit > Undo\n");
            /* Undo last action - handled by active document/window */
            extern void Finder_Undo(void);
            Finder_Undo();
            break;
        }

        case kCutItem: {
            MENU_LOG_INFO("Edit > Cut\n");
            /* Cut selection to clipboard - context dependent */
            extern void Finder_Cut(void);
            Finder_Cut();
            break;
        }

        case kCopyItem: {
            MENU_LOG_INFO("Edit > Copy\n");
            /* Copy selection to clipboard */
            extern void Finder_Copy(void);
            Finder_Copy();
            break;
        }

        case kPasteItem: {
            MENU_LOG_INFO("Edit > Paste\n");
            /* Paste from clipboard */
            extern void Finder_Paste(void);
            Finder_Paste();
            break;
        }

        case kClearItem: {
            MENU_LOG_INFO("Edit > Clear\n");
            /* Clear/delete selection */
            extern void Finder_Clear(void);
            Finder_Clear();
            break;
        }

        case kSelectAllItem: {
            MENU_LOG_INFO("Edit > Select All\n");
            /* Select all items in current context */
            extern void Finder_SelectAll(void);
            Finder_SelectAll();
            break;
        }

        default:
            MENU_LOG_WARN("Unknown Edit menu item: %d\n", item);
            break;
    }
}

/* View Menu Handler - System 7.1 Finder views */
static void HandleViewMenu(short item)
{
    extern WindowPtr FrontWindow(void);
    extern void SetWindowViewMode(WindowPtr w, short viewMode);
    extern OSErr CleanUpWindow(WindowPtr w, short sortMode);

    WindowPtr front = FrontWindow();

    switch (item) {
        case 1:  /* by Icon */
            MENU_LOG_INFO("View > by Icon\n");
            if (front) SetWindowViewMode(front, 1);
            break;

        case 2:  /* by Name */
            MENU_LOG_INFO("View > by Name\n");
            if (front) SetWindowViewMode(front, 2);
            break;

        case 3:  /* by Size */
            MENU_LOG_INFO("View > by Size\n");
            if (front) SetWindowViewMode(front, 3);
            break;

        case 4:  /* by Kind */
            MENU_LOG_INFO("View > by Kind\n");
            if (front) SetWindowViewMode(front, 4);
            break;

        case 5:  /* by Label */
            MENU_LOG_INFO("View > by Label\n");
            if (front) SetWindowViewMode(front, 5);
            break;

        case 6:  /* by Date */
            MENU_LOG_INFO("View > by Date\n");
            if (front) SetWindowViewMode(front, 6);
            break;

        case 7:  /* separator */
            return;

        case 8:  /* Clean Up Window */
            MENU_LOG_INFO("View > Clean Up Window\n");
            if (front) CleanUpWindow(front, 0);
            break;

        case 9:  /* Clean Up Selection */
            MENU_LOG_INFO("View > Clean Up Selection\n");
            if (front) CleanUpWindow(front, 1);
            break;

        default:
            MENU_LOG_WARN("Unknown View menu item: %d\n", item);
            break;
    }
}

/* Label Menu Handler - System 7.1 label colors */
static void HandleLabelMenu(short item)
{
    extern WindowPtr FrontWindow(void);
    extern void ApplyLabelToSelection(WindowPtr w, short labelIndex);

    const char* labelNames[] = {
        "None",
        "Essential",
        "Hot",
        "In Progress",
        "Cool",
        "Personal",
        "Project 1",
        "Project 2"
    };

    WindowPtr front = FrontWindow();

    if (item >= 1 && item <= 8) {
        MENU_LOG_INFO("Label > %s\n", labelNames[item - 1]);
        /* Apply label to selection */
        if (front) {
            ApplyLabelToSelection(front, item - 1);
        }
    } else {
        MENU_LOG_WARN("Unknown Label menu item: %d\n", item);
    }
}

/* Special Menu Handler - System 7.1 Special menu */
static void HandleSpecialMenu(short item)
{
    extern void ArrangeDesktopIcons(void);
    extern OSErr EmptyTrash(Boolean force);

    switch (item) {
        case 1: {  /* Clean Up Desktop */
            MENU_LOG_INFO("Special > Clean Up Desktop\n");
            ArrangeDesktopIcons();
            break;
        }

        case 2: {  /* Empty Trash */
            MENU_LOG_INFO("Special > Empty Trash\n");
            OSErr err = EmptyTrash(false);
            if (err == noErr) {
                MENU_LOG_DEBUG("Trash emptied successfully\n");
            } else {
                MENU_LOG_WARN("Failed to empty trash (error %d)\n", err);
            }
            break;
        }

        case 3: {  /* separator */
            return;
        }

        case 4: {  /* Eject */
            MENU_LOG_INFO("Special > Eject\n");
            /* Eject removable media - platform specific */
            MENU_LOG_DEBUG("Eject: Ejecting removable media\n");
            break;
        }

        case 5: {  /* Erase Disk */
            MENU_LOG_INFO("Special > Erase Disk\n");
            /* Show erase disk confirmation dialog */
            MENU_LOG_DEBUG("Erase Disk: Confirmation dialog would appear\n");
            break;
        }

        case 6: {  /* separator */
            return;
        }

        case 7: {  /* Restart */
            MENU_LOG_INFO("Special > Restart\n");
            MENU_LOG_INFO("System restart initiated...\n");
            perform_restart();
            break;
        }

        case 8: {  /* Shut Down */
            MENU_LOG_INFO("Special > Shut Down\n");
            MENU_LOG_INFO("System shutdown initiated...\n");
            MENU_LOG_INFO("It is now safe to turn off your computer.\n");
            perform_power_off();
            break;
        }

        default:
            MENU_LOG_WARN("Unknown Special menu item: %d\n", item);
            break;
    }
}

/* ========================================================================
 * FINDER OPERATION STUBS - implementations provided by Finder module
 * ======================================================================== */

/* File Operations */
void OpenSelectedItems(void) {
    MENU_LOG_DEBUG("OpenSelectedItems called\n");

    /* Get front window */
    extern WindowPtr FrontWindow(void);
    WindowPtr frontWin = FrontWindow();
    if (!frontWin) {
        MENU_LOG_DEBUG("OpenSelectedItems: No front window\n");
        return;
    }

    /* Check if it's a folder window */
    extern Boolean IsFolderWindow(WindowPtr w);
    if (!IsFolderWindow(frontWin)) {
        MENU_LOG_DEBUG("OpenSelectedItems: Front window is not a folder window\n");
        return;
    }

    /* Call folder window open helper - simulates double-click on selected item */
    extern void FolderWindow_OpenSelected(WindowPtr w);
    FolderWindow_OpenSelected(frontWin);
    MENU_LOG_DEBUG("OpenSelectedItems: Opened selected item\n");
}

void ShowGetInfoDialog(WindowPtr w) {
    MENU_LOG_DEBUG("ShowGetInfoDialog called\n");

    if (!w) {
        MENU_LOG_DEBUG("ShowGetInfoDialog: No window provided\n");
        return;
    }

    /* Check if it's a folder window */
    extern Boolean IsFolderWindow(WindowPtr w);
    if (!IsFolderWindow(w)) {
        MENU_LOG_DEBUG("ShowGetInfoDialog: Window is not a folder window\n");
        return;
    }

    /* Get selected item */
    extern Boolean FolderWindow_GetSelectedItem(WindowPtr w, VRefNum* outVref, FileID* outFileID);
    extern void GetInfo_Show(VRefNum vref, FileID fileID);

    VRefNum vref;
    FileID fileID;

    if (!FolderWindow_GetSelectedItem(w, &vref, &fileID)) {
        MENU_LOG_DEBUG("ShowGetInfoDialog: No item selected\n");
        return;
    }

    /* Show Get Info window */
    GetInfo_Show(vref, fileID);
    MENU_LOG_DEBUG("ShowGetInfoDialog: Displayed Get Info for fileID=%d\n", (int)fileID);
}

void DuplicateSelectedItems(WindowPtr w) {
    MENU_LOG_DEBUG("DuplicateSelectedItems called\n");

    if (!w) {
        /* Get front window if not specified */
        extern WindowPtr FrontWindow(void);
        w = FrontWindow();
    }

    if (!w) {
        MENU_LOG_DEBUG("DuplicateSelectedItems: No window\n");
        return;
    }

    /* Check if it's a folder window */
    extern Boolean IsFolderWindow(WindowPtr w);
    if (!IsFolderWindow(w)) {
        MENU_LOG_DEBUG("DuplicateSelectedItems: Window is not a folder window\n");
        return;
    }

    /* Call folder window duplicate helper */
    extern void FolderWindow_DuplicateSelected(WindowPtr w);
    FolderWindow_DuplicateSelected(w);
    MENU_LOG_DEBUG("DuplicateSelectedItems: Duplicated selected items\n");
}

void MakeAliasOfSelectedItems(WindowPtr w) {
    MENU_LOG_DEBUG("MakeAliasOfSelectedItems called\n");

    if (!w) {
        /* Get front window if not specified */
        extern WindowPtr FrontWindow(void);
        w = FrontWindow();
    }

    if (!w) {
        MENU_LOG_DEBUG("MakeAliasOfSelectedItems: No window\n");
        return;
    }

    /* Check if it's a folder window */
    extern Boolean IsFolderWindow(WindowPtr w);
    if (!IsFolderWindow(w)) {
        MENU_LOG_DEBUG("MakeAliasOfSelectedItems: Window is not a folder window\n");
        return;
    }

    /* Get selected items as FSSpec array */
    extern short FolderWindow_GetSelectedAsSpecs(WindowPtr w, FSSpec** outSpecs);
    FSSpec* specs = NULL;
    short count = FolderWindow_GetSelectedAsSpecs(w, &specs);

    if (count == 0 || !specs) {
        MENU_LOG_DEBUG("MakeAliasOfSelectedItems: No items selected\n");
        return;
    }

    MENU_LOG_DEBUG("MakeAliasOfSelectedItems: Creating aliases for %d items\n", count);

    /* Get current folder location */
    extern VRefNum FolderWindow_GetVRef(WindowPtr w);
    extern DirID FolderWindow_GetCurrentDir(WindowPtr w);
    VRefNum vref = FolderWindow_GetVRef(w);
    DirID dirID = FolderWindow_GetCurrentDir(w);

    /* Create alias for each selected item */
    for (short i = 0; i < count; i++) {
        /* Build alias file name: "<original name> alias" */
        char aliasName[256];
        snprintf(aliasName, sizeof(aliasName), "%s alias", specs[i].name);

        /* Create FSSpec for the alias file in the same folder */
        FSSpec aliasSpec;

        /* Convert C string to Pascal string for FSMakeFSSpec */
        unsigned char pascalName[256];
        pascalName[0] = (unsigned char)strlen(aliasName);
        strncpy((char*)&pascalName[1], aliasName, sizeof(pascalName) - 2);
        pascalName[sizeof(pascalName) - 1] = '\0';

        OSErr err = FSMakeFSSpec(vref, dirID, pascalName, &aliasSpec);
        if (err != noErr && err != fnfErr) {
            /* FSMakeFSSpec returns fnfErr if file doesn't exist, which is expected */
            MENU_LOG_DEBUG("MakeAliasOfSelectedItems: FSMakeFSSpec failed with error %d for item %d\n", err, i);
            continue;
        }

        /* Create the alias */
        extern OSErr CreateAlias(FSSpec *target, FSSpec *aliasFile);
        err = CreateAlias(&specs[i], &aliasSpec);

        if (err == noErr) {
            MENU_LOG_DEBUG("MakeAliasOfSelectedItems: Created alias '%s' for '%s'\n",
                         aliasName, specs[i].name);
        } else {
            MENU_LOG_DEBUG("MakeAliasOfSelectedItems: CreateAlias failed with error %d for item %d\n", err, i);
        }
    }

    /* Free the specs array */
    DisposePtr((Ptr)specs);

    /* Reload the folder to show the new aliases */
    extern void InitializeFolderContentsEx(WindowPtr w, Boolean isTrash, VRefNum vref, DirID dirID);
    InitializeFolderContentsEx(w, false, vref, dirID);

    /* Trigger redraw */
    PostEvent(updateEvt, (UInt32)(uintptr_t)w);

    MENU_LOG_DEBUG("MakeAliasOfSelectedItems: Complete\n");
}

void PutAwaySelectedItems(WindowPtr w) {
    MENU_LOG_DEBUG("PutAwaySelectedItems called\n");

    if (!w) {
        /* Get front window if not specified */
        extern WindowPtr FrontWindow(void);
        w = FrontWindow();
    }

    if (!w) {
        MENU_LOG_DEBUG("PutAwaySelectedItems: No window\n");
        return;
    }

    /* Check if it's a folder window */
    extern Boolean IsFolderWindow(WindowPtr w);
    if (!IsFolderWindow(w)) {
        MENU_LOG_DEBUG("PutAwaySelectedItems: Window is not a folder window\n");
        return;
    }

    /* Get selected items as FSSpec array */
    extern short FolderWindow_GetSelectedAsSpecs(WindowPtr w, FSSpec** outSpecs);
    FSSpec* specs = NULL;
    short count = FolderWindow_GetSelectedAsSpecs(w, &specs);

    if (count == 0 || !specs) {
        MENU_LOG_DEBUG("PutAwaySelectedItems: No items selected\n");
        return;
    }

    MENU_LOG_DEBUG("PutAwaySelectedItems: Moving %d items to trash\n", count);

    /* Move to trash */
    extern OSErr MoveToTrash(FSSpec *items, short count);
    OSErr err = MoveToTrash(specs, count);

    if (err == noErr) {
        MENU_LOG_DEBUG("PutAwaySelectedItems: Successfully moved %d items to trash\n", count);

        /* Reload the folder to reflect the changes */
        extern void InitializeFolderContentsEx(WindowPtr w, Boolean isTrash, VRefNum vref, DirID dirID);
        extern VRefNum FolderWindow_GetVRef(WindowPtr w);
        extern DirID FolderWindow_GetCurrentDir(WindowPtr w);

        VRefNum vref = FolderWindow_GetVRef(w);
        DirID dirID = FolderWindow_GetCurrentDir(w);

        InitializeFolderContentsEx(w, false, vref, dirID);

        /* Trigger redraw */
        PostEvent(updateEvt, (UInt32)(uintptr_t)w);
    } else {
        MENU_LOG_DEBUG("PutAwaySelectedItems: MoveToTrash failed with error %d\n", err);
    }

    /* Free the specs array */
    DisposePtr((Ptr)specs);
}

/* Edit Operations */
void Finder_Undo(void) {
    MENU_LOG_DEBUG("[STUB] Finder_Undo called\n");
}

void Finder_Cut(void) {
    MENU_LOG_DEBUG("Finder_Cut called\n");

    /* Get front window */
    extern WindowPtr FrontWindow(void);
    WindowPtr frontWin = FrontWindow();
    if (!frontWin) {
        MENU_LOG_DEBUG("Finder_Cut: No front window\n");
        return;
    }

    /* Check if it's a folder window */
    extern Boolean IsFolderWindow(WindowPtr w);
    if (!IsFolderWindow(frontWin)) {
        MENU_LOG_DEBUG("Finder_Cut: Front window is not a folder window\n");
        return;
    }

    /* Get selected items as FSSpec array */
    extern short FolderWindow_GetSelectedAsSpecs(WindowPtr w, FSSpec** outSpecs);
    FSSpec* specs = NULL;
    short count = FolderWindow_GetSelectedAsSpecs(frontWin, &specs);

    if (count == 0 || !specs) {
        MENU_LOG_DEBUG("Finder_Cut: No items selected\n");
        return;
    }

    MENU_LOG_DEBUG("Finder_Cut: Cutting %d items to clipboard\n", count);

    /* Copy to clipboard with CUT flag set */
    /* Format: [count:2 bytes][cutMode:1 byte][FSSpec array] */
    extern void ZeroScrap(void);
    extern void PutScrap(long byteCount, OSType theType, const void* sourcePtr);

    ZeroScrap();  /* Clear clipboard */

    /* Calculate total size: count + cutMode + FSSpec array */
    /* Check for integer overflow: count and sizeof multiplication */
    if (count > SIZE_MAX / sizeof(FSSpec)) {
        MENU_LOG_DEBUG("Finder_Cut: Integer overflow in clipboard data size calculation\n");
        DisposePtr((Ptr)specs);
        return;
    }
    long dataSize = sizeof(SInt16) + sizeof(UInt8) + (sizeof(FSSpec) * count);
    void* data = NewPtr(dataSize);
    if (!data) {
        MENU_LOG_DEBUG("Finder_Cut: malloc failed\n");
        DisposePtr((Ptr)specs);
        return;
    }

    /* Pack data: count, cutMode=1, FSSpec array */
    *(SInt16*)data = count;
    *((UInt8*)data + sizeof(SInt16)) = 1;  /* cutMode = 1 for Cut */
    memcpy((char*)data + sizeof(SInt16) + sizeof(UInt8), specs, sizeof(FSSpec) * count);

    /* Put to scrap with 'fSSp' type */
    PutScrap(dataSize, 'fSSp', data);

    MENU_LOG_DEBUG("Finder_Cut: Successfully cut %d items to clipboard\n", count);

    /* Free buffers */
    DisposePtr((Ptr)data);
    DisposePtr((Ptr)specs);
}

void Finder_Copy(void) {
    MENU_LOG_DEBUG("Finder_Copy called\n");

    /* Get front window */
    extern WindowPtr FrontWindow(void);
    WindowPtr frontWin = FrontWindow();
    if (!frontWin) {
        MENU_LOG_DEBUG("Finder_Copy: No front window\n");
        return;
    }

    /* Check if it's a folder window */
    extern Boolean IsFolderWindow(WindowPtr w);
    if (!IsFolderWindow(frontWin)) {
        MENU_LOG_DEBUG("Finder_Copy: Front window is not a folder window\n");
        return;
    }

    /* Get selected items as FSSpec array */
    extern short FolderWindow_GetSelectedAsSpecs(WindowPtr w, FSSpec** outSpecs);
    FSSpec* specs = NULL;
    short count = FolderWindow_GetSelectedAsSpecs(frontWin, &specs);

    if (count == 0 || !specs) {
        MENU_LOG_DEBUG("Finder_Copy: No items selected\n");
        return;
    }

    MENU_LOG_DEBUG("Finder_Copy: Copying %d items to clipboard\n", count);

    /* Copy to clipboard using basic Scrap Manager */
    /* Format: [count:2 bytes][cutMode:1 byte][FSSpec array] */
    extern void ZeroScrap(void);
    extern void PutScrap(long byteCount, OSType theType, const void* sourcePtr);

    ZeroScrap();  /* Clear clipboard */

    /* Calculate total size: count + cutMode + FSSpec array */
    /* Check for integer overflow: count and sizeof multiplication */
    if (count > SIZE_MAX / sizeof(FSSpec)) {
        MENU_LOG_DEBUG("Finder_Copy: Integer overflow in clipboard data size calculation\n");
        DisposePtr((Ptr)specs);
        return;
    }
    long dataSize = sizeof(SInt16) + sizeof(UInt8) + (sizeof(FSSpec) * count);
    void* data = NewPtr(dataSize);
    if (!data) {
        MENU_LOG_DEBUG("Finder_Copy: malloc failed\n");
        DisposePtr((Ptr)specs);
        return;
    }

    /* Pack data: count, cutMode=0, FSSpec array */
    *(SInt16*)data = count;
    *((UInt8*)data + sizeof(SInt16)) = 0;  /* cutMode = 0 for Copy */
    memcpy((char*)data + sizeof(SInt16) + sizeof(UInt8), specs, sizeof(FSSpec) * count);

    /* Put to scrap with 'fSSp' type (file FSSpec) */
    PutScrap(dataSize, 'fSSp', data);

    MENU_LOG_DEBUG("Finder_Copy: Successfully copied %d items to clipboard\n", count);

    /* Free buffers */
    DisposePtr((Ptr)data);
    DisposePtr((Ptr)specs);
}

void Finder_Paste(void) {
    MENU_LOG_DEBUG("Finder_Paste called\n");

    /* Get front window */
    extern WindowPtr FrontWindow(void);
    WindowPtr frontWin = FrontWindow();
    if (!frontWin) {
        MENU_LOG_DEBUG("Finder_Paste: No front window\n");
        return;
    }

    /* Check if it's a folder window */
    extern Boolean IsFolderWindow(WindowPtr w);
    if (!IsFolderWindow(frontWin)) {
        MENU_LOG_DEBUG("Finder_Paste: Front window is not a folder window\n");
        return;
    }

    /* Get files from clipboard using basic Scrap Manager */
    extern long GetScrap(Handle hDest, OSType theType, long* offset);
    /* NewHandle, DisposeHandle now provided by MemoryManager.h */

    Handle scrapHandle = NewHandle(0);
    if (!scrapHandle) {
        MENU_LOG_DEBUG("Finder_Paste: Failed to allocate handle\n");
        return;
    }

    long scrapSize = GetScrap(scrapHandle, 'fSSp', NULL);
    if (scrapSize <= (sizeof(SInt16) + sizeof(UInt8))) {
        MENU_LOG_DEBUG("Finder_Paste: No files on clipboard (size=%ld)\n", scrapSize);
        DisposeHandle(scrapHandle);
        return;
    }

    /* Unpack data: count, cutMode, FSSpec array */
    void* scrapData = *scrapHandle;
    SInt16 sourceCount = *(SInt16*)scrapData;
    UInt8 cutMode = *((UInt8*)scrapData + sizeof(SInt16));
    FSSpec* sourceSpecs = (FSSpec*)((char*)scrapData + sizeof(SInt16) + sizeof(UInt8));

    if (sourceCount == 0) {
        MENU_LOG_DEBUG("Finder_Paste: Empty file list on clipboard\n");
        DisposeHandle(scrapHandle);
        return;
    }

    MENU_LOG_DEBUG("Finder_Paste: Pasting %d items from clipboard (cutMode=%d)\n", sourceCount, cutMode);

    /* Get destination folder info */
    extern VRefNum FolderWindow_GetVRef(WindowPtr w);
    extern DirID FolderWindow_GetCurrentDir(WindowPtr w);
    VRefNum destVRef = FolderWindow_GetVRef(frontWin);
    DirID destDir = FolderWindow_GetCurrentDir(frontWin);

    if (destVRef == 0 || destDir == 0) {
        MENU_LOG_DEBUG("Finder_Paste: Failed to get destination folder info\n");
        DisposePtr((Ptr)sourceSpecs);
        return;
    }

    /* Copy each file to the destination folder */
    extern bool VFS_GenerateUniqueName(VRefNum vref, DirID dir, const char* base, char* out);
    extern bool VFS_Copy(VRefNum vref, DirID fromDir, FileID id, DirID toDir, const char* newName, FileID* newID);
    extern bool VFS_Lookup(VRefNum vref, DirID dir, const char* name, CatEntry* entry);

    for (SInt16 i = 0; i < sourceCount; i++) {
        FSSpec* src = &sourceSpecs[i];

        /* Convert Pascal string name to C string */
        char sourceName[256];
        int len = src->name[0];
        if (len > 255) len = 255;
        memcpy(sourceName, &src->name[1], len);
        sourceName[len] = '\0';

        MENU_LOG_DEBUG("Finder_Paste: Processing '%s' from vref=%d parID=%d\n",
                     sourceName, src->vRefNum, src->parID);

        /* Get source file's catalog entry to get its FileID */
        CatEntry sourceEntry;
        if (!VFS_Lookup(src->vRefNum, src->parID, sourceName, &sourceEntry)) {
            MENU_LOG_DEBUG("Finder_Paste: Failed to get catalog entry for '%s'\n", sourceName);
            continue;
        }

        /* Generate unique name in destination */
        char destName[256];
        if (!VFS_GenerateUniqueName(destVRef, destDir, sourceName, destName)) {
            MENU_LOG_DEBUG("Finder_Paste: Failed to generate unique name for '%s'\n", sourceName);
            continue;
        }

        MENU_LOG_DEBUG("Finder_Paste: Copying '%s' to '%s' in dir=%d\n",
                     sourceName, destName, destDir);

        /* Copy the file */
        FileID newID = 0;
        bool copied = VFS_Copy(src->vRefNum, src->parID, sourceEntry.id,
                              destDir, destName, &newID);

        if (copied) {
            MENU_LOG_DEBUG("Finder_Paste: Successfully pasted '%s' as '%s' (newID=%d)\n",
                         sourceName, destName, newID);
        } else {
            MENU_LOG_DEBUG("Finder_Paste: Failed to copy '%s'\n", sourceName);
        }
    }

    /* If this was a CUT operation, delete the source files */
    if (cutMode == 1) {
        MENU_LOG_DEBUG("Finder_Paste: Cut mode - deleting source files\n");

        extern bool VFS_Delete(VRefNum vref, FileID id);

        for (SInt16 i = 0; i < sourceCount; i++) {
            FSSpec* src = &sourceSpecs[i];

            /* Convert Pascal string name to C string */
            char sourceName[256];
            int len = src->name[0];
            if (len > 255) len = 255;
            memcpy(sourceName, &src->name[1], len);
            sourceName[len] = '\0';

            /* Get source file's catalog entry to get its FileID */
            CatEntry sourceEntry;
            if (!VFS_Lookup(src->vRefNum, src->parID, sourceName, &sourceEntry)) {
                MENU_LOG_DEBUG("Finder_Paste: Failed to lookup '%s' for deletion\n", sourceName);
                continue;
            }

            /* Delete the source file */
            bool deleted = VFS_Delete(src->vRefNum, sourceEntry.id);
            if (deleted) {
                MENU_LOG_DEBUG("Finder_Paste: Deleted source file '%s'\n", sourceName);
            } else {
                MENU_LOG_DEBUG("Finder_Paste: Failed to delete source file '%s'\n", sourceName);
            }
        }

        /* Clear the clipboard after cut/paste */
        extern void ZeroScrap(void);
        ZeroScrap();
    }

    /* Free the scrap handle */
    DisposeHandle(scrapHandle);

    /* Refresh the folder window */
    PostEvent(updateEvt, (UInt32)(uintptr_t)frontWin);

    MENU_LOG_DEBUG("Finder_Paste: Paste operation complete\n");
}

void Finder_Clear(void) {
    MENU_LOG_DEBUG("Finder_Clear called\n");

    /* Get front window */
    extern WindowPtr FrontWindow(void);
    WindowPtr frontWin = FrontWindow();
    if (!frontWin) {
        MENU_LOG_DEBUG("Finder_Clear: No front window\n");
        return;
    }

    /* Check if it's a folder window */
    extern Boolean IsFolderWindow(WindowPtr w);
    if (!IsFolderWindow(frontWin)) {
        MENU_LOG_DEBUG("Finder_Clear: Front window is not a folder window\n");
        return;
    }

    /* Call folder window delete helper */
    extern void FolderWindow_DeleteSelected(WindowPtr w);
    FolderWindow_DeleteSelected(frontWin);
    MENU_LOG_DEBUG("Finder_Clear: Deleted selected items\n");
}

void Finder_SelectAll(void) {
    MENU_LOG_DEBUG("Finder_SelectAll called\n");

    /* Get front window */
    extern WindowPtr FrontWindow(void);
    WindowPtr frontWin = FrontWindow();
    if (!frontWin) {
        MENU_LOG_DEBUG("Finder_SelectAll: No front window\n");
        return;
    }

    /* Check if it's a folder window */
    extern Boolean IsFolderWindow(WindowPtr w);
    if (!IsFolderWindow(frontWin)) {
        MENU_LOG_DEBUG("Finder_SelectAll: Front window is not a folder window\n");
        return;
    }

    /* Select all items in the folder window */
    extern void FolderWindow_SelectAll(WindowPtr w);
    FolderWindow_SelectAll(frontWin);
    MENU_LOG_DEBUG("Finder_SelectAll: Selected all items in folder window\n");
}

/* View Operations */
void SetWindowViewMode(WindowPtr w, short viewMode) {
    if (!w) return;

    MENU_LOG_DEBUG("SetWindowViewMode called with mode=%d\n", viewMode);

    /* Check if it's a folder window */
    extern Boolean IsFolderWindow(WindowPtr w);
    extern void FolderWindow_SortAndArrange(WindowPtr w, short sortType);

    if (IsFolderWindow(w)) {
        /* viewMode: 1=by Icon, 2=by Name, 3=by Size, 4=by Kind, 5=by Label, 6=by Date */
        FolderWindow_SortAndArrange(w, viewMode);
        MENU_LOG_DEBUG("SetWindowViewMode: Sorted folder window by mode %d\n", viewMode);
    } else {
        MENU_LOG_DEBUG("SetWindowViewMode: Window is not a folder window\n");
    }
}

/* Label Operations */
void ApplyLabelToSelection(WindowPtr w, short labelIndex) {
    if (!w) return;

    MENU_LOG_DEBUG("ApplyLabelToSelection called with label=%d\n", labelIndex);

    /* Check if it's a folder window */
    extern Boolean IsFolderWindow(WindowPtr w);
    if (!IsFolderWindow(w)) {
        MENU_LOG_DEBUG("ApplyLabelToSelection: Window is not a folder window\n");
        return;
    }

    /* Apply label to selected items */
    extern void FolderWindow_SetLabelOnSelected(WindowPtr w, short labelIndex);
    FolderWindow_SetLabelOnSelected(w, labelIndex);

    MENU_LOG_DEBUG("ApplyLabelToSelection: Applied label %d to selected items\n", labelIndex);
}
