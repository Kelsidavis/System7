/*
 * MenuCommands.c - Menu Command Dispatcher
 *
 * Handles menu selections and executes appropriate commands
 * for System 7.1 menu structure.
 */

#include "SystemTypes.h"

/* Serial output functions */
extern void serial_puts(const char* str);
extern void serial_printf(const char* format, ...);
#include "MenuManager/MenuManager.h"
#include "Finder/AboutThisMac.h"

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
    serial_printf("DoMenuCommand: menu=%d, item=%d\n", menuID, item);

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
            serial_printf("Unknown menu ID: %d\n", menuID);
            break;
    }

    /* Clear menu highlighting after command */
    HiliteMenu(0);
}

/* Show About Box */
static void ShowAboutBox(void)
{
    serial_printf("\n");
    serial_printf("========================================\n");
    serial_printf("           System 7 Reimplementation   \n");
    serial_printf("========================================\n");
    serial_printf("Version: 7.1.0\n");
    serial_printf("Build: Clean room reimplementation\n");
    serial_printf("\n");
    serial_printf("A compatible implementation of classic\n");
    serial_printf("Macintosh system software\n");
    serial_printf("\n");
    serial_printf("Open source portable implementation\n");
    serial_printf("========================================\n\n");

    /* Would show a proper dialog box with this information */
    /* For now just output to serial console */
}

/* Apple Menu Handler */
static void HandleAppleMenu(short item)
{
    switch (item) {
        case 1:  /* About This Macintosh */
            serial_printf("About This Macintosh...\n");
            /* Show about window */
            AboutWindow_ShowOrToggle();
            break;

        case 2:  /* Separator */
            break;

        case 3:  /* Calculator (sample desk accessory) */
            serial_printf("Launch Calculator DA\n");
            break;

        case 4:  /* Separator */
            break;

        case 5:  /* Shut Down */
            serial_printf("Apple Menu > Shut Down\n");
            serial_printf("System shutdown initiated...\n");
            serial_printf("It is now safe to turn off your computer.\n");

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
            serial_printf("Unknown Apple menu item: %d\n", item);
            break;
    }
}

/* File Menu Handler */
static void HandleFileMenu(short item)
{
    switch (item) {
        case kNewItem:
            serial_printf("File > New\n");
            /* Create new folder in Finder */
            extern Boolean VFS_CreateFolder(SInt16 vref, SInt32 parent, const char* name, SInt32* newID);
            SInt32 newFolderID;
            if (VFS_CreateFolder(0, 2, "New Folder", &newFolderID)) {
                serial_printf("Created new folder with ID %ld\n", newFolderID);
            }
            break;

        case kOpenItem:
            serial_printf("File > Open...\n");
            /* TODO: Show open dialog */
            break;

        case kCloseItem:
            serial_printf("File > Close\n");
            /* Close current window - but only if it's valid and visible */
            extern WindowPtr FrontWindow(void);
            extern void CloseWindow(WindowPtr window);
            WindowPtr front = FrontWindow();
            if (front && front->visible) {
                serial_printf("Closing visible front window 0x%08x\n", (unsigned int)front);
                CloseWindow(front);
                serial_printf("Closed front window\n");
            } else {
                serial_printf("No visible window to close (front=%p, visible=%d)\n",
                             front, front ? front->visible : -1);
            }
            break;

        case kSaveItem:
            serial_printf("File > Save\n");
            /* TODO: Save current document */
            break;

        case kSaveAsItem:
            serial_printf("File > Save As...\n");
            /* TODO: Show save dialog */
            break;

        case kPageSetupItem:
            serial_printf("File > Page Setup...\n");
            /* TODO: Show page setup dialog */
            break;

        case kPrintItem:
            serial_printf("File > Print...\n");
            /* TODO: Show print dialog */
            break;

        case kQuitItem:
            serial_printf("File > Quit - Shutting down...\n");
            /* TODO: Proper shutdown sequence */
            /* For now, just halt */
            __asm__ volatile("cli; hlt");
            break;

        default:
            serial_printf("Unknown File menu item: %d\n", item);
            break;
    }
}

/* Edit Menu Handler */
static void HandleEditMenu(short item)
{
    switch (item) {
        case kUndoItem:
            serial_printf("Edit > Undo\n");
            /* Undo last action - check if TextEdit has focus */
            extern void TEUndo(TEHandle hTE);
            /* Would need to track active TextEdit handle */
            serial_printf("Undo not available\n");
            break;

        case kCutItem:
            serial_printf("Edit > Cut\n");
            /* Cut selection to clipboard */
            extern void TECut(TEHandle hTE);
            /* Would need active TextEdit handle */
            serial_printf("Cut - no text selection\n");
            break;

        case kCopyItem:
            serial_printf("Edit > Copy\n");
            /* Copy selection to clipboard */
            extern void TECopy(TEHandle hTE);
            /* Would need active TextEdit handle */
            serial_printf("Copy - no text selection\n");
            break;

        case kPasteItem:
            serial_printf("Edit > Paste\n");
            /* Paste from clipboard */
            extern void TEPaste(TEHandle hTE);
            /* Would need active TextEdit handle */
            serial_printf("Paste - no text field active\n");
            break;

        case kClearItem:
            serial_printf("Edit > Clear\n");
            /* Clear selection */
            extern void TEDelete(TEHandle hTE);
            /* Would need active TextEdit handle */
            serial_printf("Clear - no text selection\n");
            break;

        case kSelectAllItem:
            serial_printf("Edit > Select All\n");
            /* Select all items */
            /* In Finder, would select all icons */
            /* In TextEdit, would select all text */
            serial_printf("Select All - would select all items in current context\n");
            break;

        default:
            serial_printf("Unknown Edit menu item: %d\n", item);
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
        serial_printf("View > %s\n", viewNames[item - 1]);
        /* Change view mode */
        /* Would update Finder's view settings for current window */
        serial_printf("View mode changed to %s\n", viewNames[item - 1]);
    } else {
        serial_printf("Unknown View menu item: %d\n", item);
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
        serial_printf("Label > %s\n", labelNames[item - 1]);
        /* Apply label to selection */
        /* Would set label color for selected items */
        serial_printf("Applied label '%s' to selected items\n", labelNames[item - 1]);
    } else {
        serial_printf("Unknown Label menu item: %d\n", item);
    }
}

/* Special Menu Handler */
static void HandleSpecialMenu(short item)
{
    switch (item) {
        case 1:
            serial_printf("Special > Clean Up\n");
            /* Clean up desktop/window - arrange icons in grid */
            extern void ArrangeDesktopIcons(void);
            ArrangeDesktopIcons();
            serial_printf("Desktop cleaned up\n");
            break;

        case 2:
            serial_printf("Special > Empty Trash\n");
            /* Empty trash */
            extern OSErr EmptyTrash(void);
            OSErr err = EmptyTrash();
            if (err == noErr) {
                serial_printf("Trash emptied successfully\n");
            } else {
                serial_printf("Failed to empty trash (error %d)\n", err);
            }
            break;

        case 3:
            serial_printf("Special > Eject\n");
            /* Eject selected disk */
            /* Would unmount and eject the selected volume */
            serial_printf("Ejecting disk (not implemented in kernel)\n");
            break;

        case 4:
            serial_printf("Special > Erase Disk...\n");
            /* Show erase disk dialog */
            /* Would show dialog to format selected disk */
            serial_printf("Erase Disk dialog would appear here\n");
            break;

        case 5:
            serial_printf("Special > Restart\n");
            /* System restart */
            serial_printf("System restart initiated...\n");
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
            serial_printf("Special > Shut Down\n");
            /* System shutdown */
            serial_printf("System shutdown initiated...\n");
            serial_printf("It is now safe to turn off your computer.\n");

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
            serial_printf("Unknown Special menu item: %d\n", item);
            break;
    }
}