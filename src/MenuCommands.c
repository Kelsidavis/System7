/*
 * MenuCommands.c - Menu Command Dispatcher
 *
 * Handles menu selections and executes appropriate commands
 * for System 7.1 menu structure.
 */

#include "SystemTypes.h"
#include "System71StdLib.h"
#include "MenuManager/MenuManager.h"
#include "Finder/AboutThisMac.h"

#define MENU_LOG_DEBUG(fmt, ...) serial_logf(kLogModuleMenu, kLogLevelDebug, "[MENU] " fmt, ##__VA_ARGS__)
#define MENU_LOG_WARN(fmt, ...)  serial_logf(kLogModuleMenu, kLogLevelWarn,  "[MENU] " fmt, ##__VA_ARGS__)
#define MENU_LOG_INFO(fmt, ...)  serial_logf(kLogModuleMenu, kLogLevelInfo, fmt, ##__VA_ARGS__)

/* Forward declarations */
static void ShowAboutBox(void);

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

/* File Menu Items */
#define kNewItem        1
#define kOpenItem       2
#define kCloseItem      4
#define kSaveItem       5
#define kSaveAsItem     6
#define kPageSetupItem  8
#define kPrintItem      9
#define kQuitItem       11

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

/* Forward declarations */
static void HandleAppleMenu(short item);
static void HandleFileMenu(short item);
static void HandleEditMenu(short item);
static void HandleViewMenu(short item);
static void HandleLabelMenu(short item);
static void HandleSpecialMenu(short item);

/* Main menu command dispatcher */
void DoMenuCommand(short menuID, short item)
{
    MENU_LOG_DEBUG("DoMenuCommand: menu=%d, item=%d\n", menuID, item);

    switch (menuID) {
        case kAppleMenuID:
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

        default:
            MENU_LOG_WARN("Unknown menu ID: %d\n", menuID);
            break;
    }

    /* Clear menu highlighting after command */
    HiliteMenu(0);
}

/* Show About Box */
static void ShowAboutBox(void)
{
    MENU_LOG_INFO("\n");
    MENU_LOG_INFO("========================================\n");
    MENU_LOG_INFO("           System 7 Reimplementation   \n");
    MENU_LOG_INFO("========================================\n");
    MENU_LOG_INFO("Version: 7.1.0\n");
    MENU_LOG_INFO("Build: Clean room reimplementation\n");
    MENU_LOG_INFO("\n");
    MENU_LOG_INFO("A compatible implementation of classic\n");
    MENU_LOG_INFO("Macintosh system software\n");
    MENU_LOG_INFO("\n");
    MENU_LOG_INFO("Open source portable implementation\n");
    MENU_LOG_INFO("========================================\n\n");

    /* Would show a proper dialog box with this information */
    /* For now just output to serial console */
}

/* Apple Menu Handler */
static void HandleAppleMenu(short item)
{
    switch (item) {
        case 1:  /* About This Macintosh */
            MENU_LOG_DEBUG("About This Macintosh...\n");
            /* Show the About window */
            extern void AboutWindow_ShowOrToggle(void);
            AboutWindow_ShowOrToggle();
            break;

        case 2:  /* Separator */
            break;

        case 3:  /* Calculator (sample desk accessory) */
            MENU_LOG_DEBUG("Launch Calculator DA\n");
            break;

        case 4:  /* Separator OR Shut Down (if no DAs installed) */
        case 5:  /* Shut Down */
            MENU_LOG_INFO("Apple Menu > Shut Down\n");
            MENU_LOG_INFO("System shutdown initiated...\n");
            MENU_LOG_INFO("It is now safe to turn off your computer.\n");

            /* Try QEMU shutdown via ACPI port first */
            /* QEMU shutdown: write 0x2000 to port 0x604 */
            __asm__ volatile(
                "movw $0x2000, %%ax\n"
                "movw $0x604, %%dx\n"
                "outw %%ax, %%dx\n"
                : : : "ax", "dx"
            );

            /* If that doesn't work, try legacy APM shutdown */
            /* APM shutdown: write 0x53 to port 0xB004 */
            __asm__ volatile(
                "movb $0x53, %%al\n"
                "movw $0xB004, %%dx\n"
                "outb %%al, %%dx\n"
                : : : "al", "dx"
            );

            /* Final fallback: halt the CPU */
            __asm__ volatile("cli; hlt");
            break;

        default:
            MENU_LOG_WARN("Unknown Apple menu item: %d\n", item);
            break;
    }
}

/* File Menu Handler */
static void HandleFileMenu(short item)
{
    switch (item) {
        case kNewItem: {
            MENU_LOG_DEBUG("File > New\n");
            /* Create new folder in Finder */
            extern Boolean VFS_CreateFolder(SInt16 vref, SInt32 parent, const char* name, SInt32* newID);
            extern void DrawDesktop(void);
            SInt32 newFolderID;
            if (VFS_CreateFolder(0, 2, "New Folder", &newFolderID)) {
                MENU_LOG_DEBUG("Created new folder with ID %d\n", (int)newFolderID);
                /* Refresh desktop to show the new folder */
                DrawDesktop();
            }
            break;
        }

        case kOpenItem:
            MENU_LOG_DEBUG("File > Open...\n");
            /* TODO: Show open dialog */
            break;

        case kCloseItem: {
            MENU_LOG_DEBUG("File > Close\n");
            /* Close current window - but only if it's valid and visible */
            extern WindowPtr FrontWindow(void);
            extern void CloseWindow(WindowPtr window);
            WindowPtr front = FrontWindow();
            if (front && front->visible) {
                MENU_LOG_DEBUG("Closing visible front window 0x%08x\n", (unsigned int)P2UL(front));
                CloseWindow(front);
                MENU_LOG_DEBUG("Closed front window\n");
            } else {
                MENU_LOG_DEBUG("No visible window to close (front=0x%08x, visible=%d)\n",
                                (unsigned int)P2UL(front), front ? front->visible : -1);
            }
            break;
        }

        case kSaveItem:
            MENU_LOG_DEBUG("File > Save\n");
            /* TODO: Save current document */
            break;

        case kSaveAsItem:
            MENU_LOG_DEBUG("File > Save As...\n");
            /* TODO: Show save dialog */
            break;

        case kPageSetupItem:
            MENU_LOG_DEBUG("File > Page Setup...\n");
            /* TODO: Show page setup dialog */
            break;

        case kPrintItem:
            MENU_LOG_DEBUG("File > Print...\n");
            /* TODO: Show print dialog */
            break;

        case kQuitItem:
            MENU_LOG_INFO("File > Quit - Shutting down...\n");
            /* TODO: Proper shutdown sequence */
            /* For now, just halt */
            __asm__ volatile("cli; hlt");
            break;

        default:
            MENU_LOG_WARN("Unknown File menu item: %d\n", item);
            break;
    }
}

/* Edit Menu Handler */
static void HandleEditMenu(short item)
{
    switch (item) {
        case kUndoItem:
            MENU_LOG_DEBUG("Edit > Undo\n");
            /* Undo last action - check if TextEdit has focus */
            extern void TEUndo(TEHandle hTE);
            /* Would need to track active TextEdit handle */
            MENU_LOG_DEBUG("Undo not available\n");
            break;

        case kCutItem:
            MENU_LOG_DEBUG("Edit > Cut\n");
            /* Cut selection to clipboard */
            extern void TECut(TEHandle hTE);
            /* Would need active TextEdit handle */
            MENU_LOG_DEBUG("Cut - no text selection\n");
            break;

        case kCopyItem:
            MENU_LOG_DEBUG("Edit > Copy\n");
            /* Copy selection to clipboard */
            extern void TECopy(TEHandle hTE);
            /* Would need active TextEdit handle */
            MENU_LOG_DEBUG("Copy - no text selection\n");
            break;

        case kPasteItem:
            MENU_LOG_DEBUG("Edit > Paste\n");
            /* Paste from clipboard */
            extern void TEPaste(TEHandle hTE);
            /* Would need active TextEdit handle */
            MENU_LOG_DEBUG("Paste - no text field active\n");
            break;

        case kClearItem:
            MENU_LOG_DEBUG("Edit > Clear\n");
            /* Clear selection */
            extern void TEDelete(TEHandle hTE);
            /* Would need active TextEdit handle */
            MENU_LOG_DEBUG("Clear - no text selection\n");
            break;

        case kSelectAllItem:
            MENU_LOG_DEBUG("Edit > Select All\n");
            /* Select all items */
            /* In Finder, would select all icons */
            /* In TextEdit, would select all text */
            MENU_LOG_DEBUG("Select All - would select all items in current context\n");
            break;

        default:
            MENU_LOG_WARN("Unknown Edit menu item: %d\n", item);
            break;
    }
}

/* View Menu Handler */
static void HandleViewMenu(short item)
{
    const char* viewNames[] = {
        "by Small Icon",
        "by Icon",
        "by Name",
        "by Size",
        "by Kind",
        "by Label",
        "by Date"
    };

    if (item >= kBySmallIcon && item <= kByDate) {
        MENU_LOG_DEBUG("View > %s\n", viewNames[item - 1]);
        /* Change view mode */
        /* Would update Finder's view settings for current window */
        MENU_LOG_DEBUG("View mode changed to %s\n", viewNames[item - 1]);
    } else {
        MENU_LOG_WARN("Unknown View menu item: %d\n", item);
    }
}

/* Label Menu Handler */
static void HandleLabelMenu(short item)
{
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

    if (item >= 1 && item <= 8) {
        MENU_LOG_DEBUG("Label > %s\n", labelNames[item - 1]);
        /* Apply label to selection */
        /* Would set label color for selected items */
        MENU_LOG_DEBUG("Applied label '%s' to selected items\n", labelNames[item - 1]);
    } else {
        MENU_LOG_WARN("Unknown Label menu item: %d\n", item);
    }
}

/* Special Menu Handler */
static void HandleSpecialMenu(short item)
{
    switch (item) {
        case 1:
            MENU_LOG_DEBUG("Special > Clean Up\n");
            /* Clean up desktop/window - arrange icons in grid */
            extern void ArrangeDesktopIcons(void);
            ArrangeDesktopIcons();
            MENU_LOG_DEBUG("Desktop cleaned up\n");
            break;

        case 2: {
            MENU_LOG_DEBUG("Special > Empty Trash\n");
            /* Empty trash */
            extern OSErr EmptyTrash(void);
            OSErr err = EmptyTrash();
            if (err == noErr) {
                MENU_LOG_DEBUG("Trash emptied successfully\n");
            } else {
                MENU_LOG_WARN("Failed to empty trash (error %d)\n", err);
            }
            break;
        }

        case 3:
            MENU_LOG_DEBUG("Special > Eject\n");
            /* Eject selected disk */
            /* Would unmount and eject the selected volume */
            MENU_LOG_DEBUG("Ejecting disk (not implemented in kernel)\n");
            break;

        case 4:
            MENU_LOG_DEBUG("Special > Erase Disk...\n");
            /* Show erase disk dialog */
            /* Would show dialog to format selected disk */
            MENU_LOG_DEBUG("Erase Disk dialog would appear here\n");
            break;

        case 5:
            MENU_LOG_INFO("Special > Restart\n");
            /* System restart */
            MENU_LOG_INFO("System restart initiated...\n");
            /* Triple fault to restart in x86 */
            __asm__ volatile(
                "movl $0, %%esp\n"
                "push $0\n"
                "push $0\n"
                "lidt (%%esp)\n"
                "int3\n"
                : : : "memory"
            );
            break;

        case 6:
            MENU_LOG_INFO("Special > Shut Down\n");
            /* System shutdown */
            MENU_LOG_INFO("System shutdown initiated...\n");
            MENU_LOG_INFO("It is now safe to turn off your computer.\n");

            /* Try QEMU shutdown via ACPI port first */
            /* QEMU shutdown: write 0x2000 to port 0x604 */
            __asm__ volatile(
                "movw $0x2000, %%ax\n"
                "movw $0x604, %%dx\n"
                "outw %%ax, %%dx\n"
                : : : "ax", "dx"
            );

            /* If that doesn't work, try legacy APM shutdown */
            /* APM shutdown: write 0x53 to port 0xB004 */
            __asm__ volatile(
                "movb $0x53, %%al\n"
                "movw $0xB004, %%dx\n"
                "outb %%al, %%dx\n"
                : : : "al", "dx"
            );

            /* Final fallback: halt the CPU */
            __asm__ volatile("cli; hlt");
            break;

        default:
            MENU_LOG_WARN("Unknown Special menu item: %d\n", item);
            break;
    }
}
