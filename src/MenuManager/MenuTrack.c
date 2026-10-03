/*
 * MenuTrack.c - Basic dropdown rendering and tracking
 *
 * Draws menu item list under the title and lets user select with mouse.
 */

#include "SystemTypes.h"
#include "System71StdLib.h"
#include "MenuManager/MenuManager.h"
#include "WindowManager/WindowManager.h"
#include "SystemInternal.h"
#include "MenuManager/MenuLogging.h"
#include "MenuManager/MenuTypes.h"
#include "MenuManager/MenuInternalTypes.h"
#include "MenuManager/menu_private.h"
#include "DeskManager/DeskManager.h"
#include "QuickDraw.h"
#include "QuickDrawConstants.h"
#include "FontManager/FontManager.h"
#include "EventManager/EventManager.h"
#include "EventManager/EventTypes.h"  /* For mouse masks */
#include "TimeManager/TimeBase.h"
#include "Platform/Framebuffer.h"

/* Menus draw anywhere on the screen, so the screen port is opened to all of
 * it: a clip someone else left there hid item text wherever it did not reach. */
static void Menu_ClipToScreen(void) {
    ClipRect(&qd.screenBits.bounds);
}
extern void DrawDesktop(void);
extern void DrawVolumeIcon(void);

/* Forward declarations for static functions */
static void DrawHighlightRect(short left, short top, short right, short bottom, Boolean highlight);
void DrawMenuBarWithHighlight(short highlightMenuID);

short TrackMenu_TakeSwitch(void);

/* Global menu tracking state for event-based menu handling */
static struct {
    Boolean isTracking;        /* Are we currently tracking a menu? */
    MenuHandle activeMenu;      /* The menu being tracked */
    short menuID;              /* ID of active menu */
    short menuLeft;            /* Left edge of dropdown */
    short menuTop;             /* Top edge of dropdown */
    short menuWidth;           /* Width of dropdown */
    short menuHeight;          /* Height of dropdown */
    short itemCount;           /* Number of items */
    short highlightedItem;     /* Currently highlighted item (0=none) */
    short titleLeft;           /* Left position of menu title in menu bar */
    short titleWidth;          /* Width of menu title in menu bar */
} g_menuTrackState = {0};

/* Global framebuffer from main.c */
/* Rect helpers */

static void FillFramebufferRect(short left, short top, short right, short bottom,
                                uint32_t color, Boolean shieldPointer)
{
    if (!framebuffer) return;
    if (shieldPointer) {
        Pointer_Shield(left, top, right, bottom);
    }

    uint32_t *fb = (uint32_t*)framebuffer;
    int pitch = fb_pitch / 4;

    if (left < 0) left = 0;
    if (top < 0) top = 0;
    if (left > (int)fb_width) left = fb_width;
    if (top > (int)fb_height) top = fb_height;
    if (right < 0) right = 0;
    if (bottom < 0) bottom = 0;
    if (right > (int)fb_width) right = fb_width;
    if (bottom > (int)fb_height) bottom = fb_height;
    if (left >= right || top >= bottom) return;

    for (int y = top; y < bottom; y++) {
        for (int x = left; x < right; x++) {
            fb[y * pitch + x] = color;
        }
    }
}

/*
 * Grey out a drawn row by removing every other pixel, which is what the System
 * 7 MDEF gets by drawing the item and then applying the 50% grey pattern in
 * patBic. Nothing here consulted the enable flags at all, so Print and
 * Sharing... - disabled on every pass through Finder_AdjustMenus - were drawn
 * in the same solid black as the items you can actually pick.
 */
/*
 * The command ("cloverleaf") symbol shown beside command-key equivalents.
 *
 * Chicago carries this at character 0x11, but the strike extracted into this
 * tree only covers ASCII 32-126 and FM_DrawChicagoCharInternal rejects
 * ch < 32, so the character drew as nothing and menus showed a bare "N" where
 * System 7 shows the symbol followed by N. Rather than fabricate font data,
 * the glyph is drawn geometrically - the standard looped square (U+2318): a
 * 5x5 centre square with a loop wrapped around each corner.
 */
/*
 * The check mark shown against a chosen item - the View menu's current view,
 * for instance. Chicago carries it at character 18, which is outside the ASCII
 * strike extracted into this tree, so it is drawn geometrically for the same
 * reason as the command symbol above.
 */
/* System 7 reserves a column on the left of every menu for the item mark, so
 * text starts clear of it and a check does not collide with the name. */
/* Menu bar titles still use the shared text renderer via a Pascal adapter. */
static void DrawTrackedMenuText(const char* text, short x, short baseline,
                                Style textStyle) {
    Str255 pascalText;
    short length = 0;
    while (text[length] != '\0' && length < 255) {
        pascalText[length + 1] = (unsigned char)text[length];
        length++;
    }
    if (length == 0) return;
    pascalText[0] = (unsigned char)length;

    Rect textRect = {baseline - 11, x - 4, baseline + 5, x + 251};
    DrawMenuItemText(&textRect, pascalText, textStyle, true, false);
}

/* --- Get actual menu items from menu handle --- */
static void GetItemText(MenuHandle theMenu, short index, char* text) {
    if (!theMenu || !text) {
        text[0] = 0;
        return;
    }

    /* Get item text from actual menu structure */
    Str255 itemString;
    GetMenuItemText(theMenu, index, itemString);

    /* Convert Pascal string to C string */
    short len = itemString[0];
    if (len > 63) len = 63;  /* Limit to buffer size */
    for (short i = 0; i < len; i++) {
        text[i] = itemString[i + 1];
    }
    text[len] = 0;
}

/*
 * CalcMenuWidth - size a menu to its widest item, as System 7 does.
 *
 * The widths used to be hardcoded per menu ID (120, or 150 for the Apple menu),
 * which left no room for the command-key column: once command keys were drawn,
 * "Close Window" ran straight into its own glyph. Measuring the items also means
 * a translated menu sizes itself instead of being clipped to an English width.
 */
static short CalcMenuWidth(MenuHandle theMenu, short itemCount) {
    short widest = 0;

    for (short i = 1; i <= itemCount; i++) {
        char itemText[64];
        short w = 0;
        short cmdChar = 0;
        short subID = 0;

        if (CheckMenuItemSeparator(theMenu, i)) continue;

        GetItemText(theMenu, i, itemText);
        for (short c = 0; itemText[c]; c++) {
            w += CharWidth((short)(unsigned char)itemText[c]);
        }

        GetItemSubmenu(theMenu, i, &subID);
        GetItemCmd(theMenu, i, &cmdChar);
        if (subID != 0) {
            w += 20;                                     /* triangle column */
        } else if (cmdChar != 0) {
            w += kMenuCommandGlyphWidth + 4 + CharWidth('W') + 8;
        }

        if (w > widest) widest = w;
    }

    widest += kMenuItemContentInset + 12;   /* content column, plus right margin */
    if (widest < 100) widest = 100;
    return widest;
}

/*
 * DrawMenuItemRow - draw one menu item, normal or highlighted.
 *
 * Shared by the initial menu draw and by highlight tracking. Tracking used to
 * redraw only the item's text, so moving the mouse across "New Folder <cmd>N"
 * erased its command key and left the row half drawn; going through one routine
 * means whatever an item is made of gets restored.
 */
/* Draw a tracked row through MenuDisplay's shared item renderer. */
static void GetTrackedMenuItemRect(MenuHandle theMenu, short item, short left,
                                   short top, short menuWidth, Rect* itemRect) {
    Rect menuRect = {top, left, top + g_menuTrackState.menuHeight,
                     left + menuWidth};
    CalcMenuItemRect(theMenu, item, &menuRect, itemRect);
}

static void DrawMenuItemRow(MenuHandle theMenu, short i, short left, short top,
                            short menuWidth, Boolean highlighted) {
    Rect itemRect;
    GetTrackedMenuItemRect(theMenu, i, left, top, menuWidth, &itemRect);
    DrawMenuItemAtRect(theMenu, i, &itemRect, highlighted);
}

/*
 * Blink the chosen item as many times as SetMenuFlash asks, about 3 ticks
 * each way, ending highlighted (Inside Macintosh: Toolbox Essentials,
 * 3-116). This held the highlight for 200000 turns of an untimed loop.
 */
static void FlashChosenItem(MenuHandle theMenu, short item, short left, short top,
                            short menuWidth) {
    short flashes = GetMenuFlashCount();
    for (short n = 0; n < flashes; n++) {
        for (int on = 0; on <= 1; on++) {
            DrawMenuItemRow(theMenu, item, left, top, menuWidth, on);
            UInt32 until = TickCount() + 3;
            while (TickCount() < until) {
                SystemTask();
                EventPumpYield();
            }
        }
    }
}

/* Draw dropdown menu */
static void DrawTrackedMenu(MenuHandle theMenu, short left, short top,
                            short menuWidth, short menuHeight) {
    Rect menuRect = {top, left, top + menuHeight, left + menuWidth};
    GrafPtr savePort;
    GetPort(&savePort);
    if (qd.thePort) {
        QD_SetScreenPort();
        Menu_ClipToScreen();
    }
    DrawMenu(theMenu, &menuRect, 0);
    if (savePort) SetPort(savePort);
}

/* Begin tracking a menu - draws it and sets up state */
long BeginTrackMenu(short menuID, Point *startPt) {
    serial_puts("BeginTrackMenu: ENTER\n");

    /* Prevent re-entry */
    if (g_menuTrackState.isTracking) {
        serial_puts("BeginTrackMenu: Already tracking, aborting to prevent re-entry\n");
        return 0;
    }

    if (!framebuffer) {
        serial_puts("BeginTrackMenu: ERROR - No framebuffer!\n");
        return 0;
    }

    /* Save current port and ensure we draw in screen port */
    GrafPtr savePort;
    GetPort(&savePort);
    if (qd.thePort) {
        QD_SetScreenPort();  /* menus use global coordinates */
        Menu_ClipToScreen();
    }

    /* Get the actual menu handle for this menu ID */
    MenuHandle theMenu = GetMenuHandle(menuID);
    if (!theMenu) {
        MENU_LOG_TRACE("BeginTrackMenu: Menu %d not found\n", menuID);
        if (savePort) SetPort(savePort);
        return 0;
    }

    /* Get actual item count from menu */
    short itemCount = CountMenuItems(theMenu);
    if (itemCount == 0) itemCount = 5;  /* Fallback */

    short left = startPt->h;
    short top = 20;       /* below menubar */
    short menuWidth = CalcMenuWidth(theMenu, itemCount);
    /* Save menu tracking state */
    g_menuTrackState.isTracking = true;
    g_menuTrackState.activeMenu = theMenu;
    g_menuTrackState.menuID = menuID;
    g_menuTrackState.menuLeft = left;
    g_menuTrackState.menuTop = top;
    g_menuTrackState.menuWidth = menuWidth;
    /* Use SInt32 to prevent overflow in height calculation */
    SInt32 calcHeight = 8;
    for (short i = 1; i <= itemCount; i++) calcHeight += GetMenuItemHeight(theMenu, i);
    if (calcHeight > 32767) calcHeight = 32767;  /* Clamp to max short */
    g_menuTrackState.menuHeight = (short)calcHeight;
    g_menuTrackState.itemCount = itemCount;
    g_menuTrackState.highlightedItem = 0;
    MENU_LOG_TRACE("BeginTrackMenu: Initial highlightedItem = %d\n", g_menuTrackState.highlightedItem);

    /* Store the menu title position, taken from what the menu bar actually
     * measured rather than a table of guesses. The estimates that used to live
     * here - Apple 0/30, File 30/32, Edit 62/32, View 94/36, Label 130/40,
     * Special 170/50 - did not match the real layout at all: Special sits at
     * 247..305. They were also fixed English widths. */
    short titleX = 0;
    short titleW = 30;
    {
        Rect tr;
        if (GetMenuTitleRectByID(menuID, &tr)) {
            titleX = tr.left;
            titleW = tr.right - tr.left;
        }
    }

    g_menuTrackState.titleLeft = titleX;
    g_menuTrackState.titleWidth = titleW;

    serial_puts("BeginTrackMenu: About to call DrawMenuBarWithHighlight\n");
    /* Redraw the menu bar with the active menu highlighted */
    DrawMenuBarWithHighlight(menuID);
    serial_puts("BeginTrackMenu: Returned from DrawMenuBarWithHighlight\n");

    serial_puts("BeginTrackMenu: About to call DrawTrackedMenu\n");
    /* Draw the menu dropdown */
    DrawTrackedMenu(theMenu, left, top, menuWidth, (short)calcHeight);
    serial_puts("BeginTrackMenu: Dropdown drawn, tracking started\n");

    /* Restore original port */
    if (savePort) SetPort(savePort);

    /* Return 0 - actual selection will come from event handling */
    return 0;
}

/* Draw rectangle with specified color */
static void DrawHighlightRect(short left, short top, short right, short bottom, Boolean highlight) {
    uint32_t color = highlight ? 0xFF000000 : 0xFFFFFFFF;
    FillFramebufferRect(left, top, right, bottom, color, false);
}

/* Handle mouse movement while tracking menu */
void UpdateMenuTrackingNew(Point mousePt) {
    static int updateCount = 0;
    updateCount++;

    /* Only print debug every 10 calls to avoid overflow */
    if (updateCount % 10 == 0) {
        MENU_LOG_TRACE("UpdateMenu: call #%d, mouse at (%d,%d)\n",
                      updateCount, mousePt.h, mousePt.v);
    }

    if (!g_menuTrackState.isTracking) return;

    /* Validate tracking state to prevent crashes */
    if (!g_menuTrackState.activeMenu) {
        serial_puts("UpdateMenuTracking: activeMenu is NULL, aborting\n");
        return;
    }
    if (g_menuTrackState.itemCount <= 0) {
        serial_puts("UpdateMenuTracking: itemCount is 0, aborting\n");
        return;
    }

    short left = g_menuTrackState.menuLeft;
    short top = g_menuTrackState.menuTop;
    short menuWidth = g_menuTrackState.menuWidth;
    short itemCount = g_menuTrackState.itemCount;
    MenuHandle theMenu = g_menuTrackState.activeMenu;

    /* Check if mouse is over a menu item - account for 2px top padding */
    short newHighlight = 0;

    /* First check if mouse is horizontally within menu */
    if (mousePt.h >= left && mousePt.h < left + menuWidth) {
        /* Check each item's position to find which one the mouse is over */
        for (short i = 1; i <= itemCount; i++) {
            Rect itemRect;
            GetTrackedMenuItemRect(theMenu, i, left, top, menuWidth, &itemRect);

            /* Check if mouse is vertically within this item */
            if (mousePt.v >= itemRect.top && mousePt.v < itemRect.bottom) {
                /* Dividers and disabled items never highlight in System 7.
                 * Testing only for non-empty text let dividers highlight,
                 * since a divider's text is "-". */
                char itemText[64];
                GetItemText(theMenu, i, itemText);
                if (itemText[0] != 0 &&
                    !CheckMenuItemSeparator(theMenu, i) &&
                    CheckMenuItemEnabled(theMenu, i)) {
                    newHighlight = i;
                    MENU_LOG_TRACE("UpdateMenu: Mouse at (%d,%d) is over item %d\n",
                                 mousePt.h, mousePt.v, i);
                }
                break;  /* Found the item, stop searching */
            }
        }
    }

    /* Update highlight if changed */
    if (newHighlight != g_menuTrackState.highlightedItem) {
        MENU_LOG_TRACE("UpdateMenu: Highlight change from %d to %d\n",
                      g_menuTrackState.highlightedItem, newHighlight);

        /* Clear old highlight and redraw text */
        if (g_menuTrackState.highlightedItem > 0) {
            MENU_LOG_TRACE("UpdateMenu: Clearing old highlight at y=%d\n", top);

            /* Restore the row through the shared item renderer. */
            DrawMenuItemRow(theMenu, g_menuTrackState.highlightedItem,
                            left, top, menuWidth, false);
        }

        /* Draw new highlight and text */
        if (newHighlight > 0) {
            MENU_LOG_TRACE("UpdateMenu: Drawing new highlight at item %d\n", newHighlight);

            /* Draw the selected row through the shared item renderer. */
            DrawMenuItemRow(theMenu, newHighlight, left, top, menuWidth, true);
        }

        g_menuTrackState.highlightedItem = newHighlight;
    }
}

/* End menu tracking and return selection */
long EndMenuTrackingNew(void) {
    serial_printf("*** EndMenuTrackingNew: CALLED\n");
    serial_printf("***   isTracking=%d\n", g_menuTrackState.isTracking);
    serial_printf("***   menuID=%d\n", g_menuTrackState.menuID);
    serial_printf("***   highlightedItem=%d\n", g_menuTrackState.highlightedItem);

    if (!g_menuTrackState.isTracking) {
        serial_printf("***   Returning 0 (not tracking)\n");
        return 0;
    }

    long result = 0;
    if (g_menuTrackState.highlightedItem > 0) {
        /* Pack menuID in high word, item in low word */
        result = ((long)g_menuTrackState.menuID << 16) | g_menuTrackState.highlightedItem;
        serial_printf("***   Returning menuChoice=0x%lx (menu=%d, item=%d)\n",
                     result, g_menuTrackState.menuID, g_menuTrackState.highlightedItem);
        MENU_LOG_TRACE("EndMenuTracking: Selected item %d from menu %d\n",
                     g_menuTrackState.highlightedItem, g_menuTrackState.menuID);
    } else {
        serial_printf("***   Returning 0 (no item highlighted)\n");
    }

    /* Clear tracking state */
    g_menuTrackState.isTracking = false;
    g_menuTrackState.activeMenu = NULL;
    g_menuTrackState.highlightedItem = 0;
    g_menuTrackState.menuID = 0;

    /* Save current port and set screen port for redrawing */
    GrafPtr savePort;
    GetPort(&savePort);
    if (qd.thePort) {
        QD_SetScreenPort();
        Menu_ClipToScreen();
    }

    /* Redraw everything cleanly */
    DrawMenuBar();      /* This redraws menu bar without highlight */
    DrawDesktop();
    DrawVolumeIcon();

    /* Restore original port */
    if (savePort) SetPort(savePort);

    return result;
}

/* Check if we're currently tracking a menu */
Boolean IsMenuTrackingNew(void) {
    return g_menuTrackState.isTracking;
}

/* How deep the current TrackMenu is: 1 for a menu from the bar, more for a
 * submenu. And the title the pointer moved onto, for MenuSelect to open next. */
static int gTrackDepth = 0;
static short gMenuSwitchTo = 0;

static long TrackMenu_Body(short menuID, Point *startPt);

long TrackMenu(short menuID, Point *startPt) {
    gTrackDepth++;
    long r = TrackMenu_Body(menuID, startPt);
    gTrackDepth--;
    return r;
}

/* The title the pointer was dragged onto, if the last TrackMenu ended that way. */
short TrackMenu_TakeSwitch(void) {
    short id = gMenuSwitchTo;
    gMenuSwitchTo = 0;
    return id;
}

/* TrackMenu - Full implementation with mouse tracking loop */
__attribute__((optimize("O0")))
static long TrackMenu_Body(short menuID, Point *startPt) {
    /* NULL check to prevent crash */
    if (!startPt) {
        return 0;
    }

    GrafPtr savePort;
    Rect menuRect;
    Handle savedBits;
    Point mousePt;
    long result = 0;

    /* Save current port */
    GetPort(&savePort);
    QD_SetScreenPort();
    Menu_ClipToScreen();
    serial_puts("TrackMenu: SetPort done\n");

    /* Get the menu */
    MenuHandle theMenu = GetMenuHandle(menuID);
    serial_puts("TrackMenu: GetMenuHandle returned\n");
    if (!theMenu) {
        if (savePort) SetPort(savePort);
        return 0;
    }

    /* Test if theMenu pointer is safe to dereference */
    unsigned long menuPtr = (unsigned long)theMenu;
    /* ARM64 heap addresses are typically in the 0x40000000+ range
     * x86 addresses are typically lower. Accept a wide range. */
    if (menuPtr < 0x1000 || menuPtr > 0x80000000) {
        serial_puts("TrackMenu: Menu handle looks invalid (bad address range)\n");
        if (savePort) SetPort(savePort);
        return 0;
    }
    serial_puts("TrackMenu: Menu handle address looks reasonable\n");

    /* Calculate menu geometry */
    short itemCount = CountMenuItems(theMenu);
    serial_puts("TrackMenu: CountMenuItems returned\n");
    if (itemCount == 0) {
        itemCount = 5;
    } else {
    }


    /* Validate geometry to prevent zero/negative sizes */
    if (itemCount <= 0) {
        serial_puts("TrackMenu: Invalid itemCount, using default\n");
        itemCount = 5;
    }


    short menuWidth = CalcMenuWidth(theMenu, itemCount);

    if (menuWidth <= 0) {
        serial_puts("TrackMenu: Invalid menuWidth, using default\n");
        menuWidth = 120;
    }


    /* Use SInt32 to prevent overflow in height calculation */
    SInt32 calcHeight = 8;
    for (short i = 1; i <= itemCount; i++) calcHeight += GetMenuItemHeight(theMenu, i);
    if (calcHeight > 32767) calcHeight = 32767;  /* Clamp to max short */
    short menuHeight = (short)calcHeight;

    /* Get coordinates from startPt (already validated non-NULL earlier) */
    short left = startPt->h;
    /* A submenu opens beside its item; it was pinned under the menu bar. */
    short top = (gTrackDepth > 1) ? (short)(startPt->v - 2) : 20;

    /* A menu that would run off the right of the screen is moved left to
     * fit (Inside Macintosh: Toolbox Essentials, 3-10). This clipped to a
     * fixed 640x480, so on a wider screen a menu whose title sat past 640 -
     * the Application menu's, at the right end - never opened, and one
     * reaching past 640 was drawn whole but saved and restored only in
     * part, leaving the rest on screen. */
    short screenRight = qd.screenBits.bounds.right;
    short screenBottom = qd.screenBits.bounds.bottom;
    if (left + menuWidth > screenRight) left = screenRight - menuWidth;
    if (left < 0) left = 0;

    /* Calculate menu rectangle */
    menuRect.left = left;
    menuRect.top = top;
    menuRect.right = left + menuWidth;
    menuRect.bottom = top + menuHeight;

    /* A submenu that would run off the bottom moves up to fit */
    if (gTrackDepth > 1 && top + menuHeight > screenBottom) {
        top = screenBottom - menuHeight;
        if (top < 20) top = 20;
        menuRect.top = top;
        menuRect.bottom = top + menuHeight;
    }

    if (menuRect.right > screenRight) menuRect.right = screenRight;
    if (menuRect.bottom > screenBottom) menuRect.bottom = screenBottom;

    /* Validate rect is non-empty after clipping */
    if (menuRect.right <= menuRect.left || menuRect.bottom <= menuRect.top) {
        serial_puts("TrackMenu: Invalid rect after clipping, aborting\n");
        if (savePort) SetPort(savePort);
        return 0;
    }

    extern Handle SaveMenuBits(const Rect *menuRect);
    extern OSErr RestoreMenuBits(Handle bitsHandle);
    extern OSErr DiscardMenuBits(Handle bitsHandle);

    /* The saved pixels must not include the pointer, or restoring them
     * would put a copy of it back where it was. */
    Pointer_TakeOffScreen();
    savedBits = SaveMenuBits(&menuRect);
    serial_puts("TrackMenu: SaveMenuBits returned\n");

    /* Set up tracking state */
    g_menuTrackState.isTracking = true;
    g_menuTrackState.activeMenu = theMenu;
    g_menuTrackState.menuID = menuID;
    g_menuTrackState.menuLeft = left;
    g_menuTrackState.menuTop = top;
    g_menuTrackState.menuWidth = menuWidth;
    g_menuTrackState.menuHeight = menuHeight;
    g_menuTrackState.itemCount = itemCount;
    g_menuTrackState.highlightedItem = 0;

    /* Draw the menu bar with the active menu highlighted */
    DrawMenuBarWithHighlight(menuID);
    serial_puts("TrackMenu: Menu bar highlight drawn\n");

    /* Draw the menu dropdown */
    DrawTrackedMenu(theMenu, left, top, menuWidth, menuHeight);
    serial_puts("TrackMenu: DrawTrackedMenu returned\n");
    serial_puts("TrackMenu: Menu drawn, entering tracking loop\n");

    /* Persistent menu tracking - menu stays open until user makes a selection or clicks outside */
    /* ADD SAFETY TIMEOUT: Prevent infinite tracking loop */
    Boolean tracking = true;
    Boolean buttonWasReleased = false;
    int updateCount = 0;

    /* Safety stop measured in ticks (1/60 s), not iterations.
     *
     * This loop is not paced: its body is SystemTask() plus an input pump that
     * returns immediately once the controller buffer is drained, so it runs
     * hundreds of thousands of times a second. A 1,000,000-iteration cap that
     * was meant to let "the menu stay open longer" actually expired after a
     * couple of seconds of real time - and on a faster machine, sooner - which
     * closed open menus out from under the user. Two minutes of wall time is a
     * genuine runaway; a fast loop is not. */
    const UInt32 MAX_TRACKING_TICKS = 60 * 120;  /* 2 minutes */
    const UInt32 trackStartTick = TickCount();
    UInt32 releaseStartTick = 0;  /* 0 = button not currently released */

    serial_puts("TrackMenu: Starting persistent menu tracking\n");

    /* Track menu - menu stays open even after button is released */
    while (tracking && (TickCount() - trackStartTick) < MAX_TRACKING_TICKS) {
        /* Pump events for responsive UI */
        SystemTask();          /* House-keeping tasks */
        EventPumpYield();      /* Platform's input pump */

        /* Increment update counter */
        updateCount++;

        /* Draw cursor (menu tracking has its own event loop that bypasses main loop) */
        UpdateCursorDisplay();

        /* Get current mouse position */
        GetMouse(&mousePt);

        /* Update menu highlighting based on mouse position */
        UpdateMenuTrackingNew(mousePt);

        /* Dragged onto another title in the bar: close this one and let
         * MenuSelect open that (Inside Macintosh: Toolbox Essentials, 3-11).
         * Only a click on another title used to change menus, and it took
         * two - this one cancelled, then a fresh click. */
        if (gTrackDepth == 1 && mousePt.v >= 0 && mousePt.v < 20) {
            short over = FindMenuAtPoint_Internal(mousePt);
            if (over != 0 && over != menuID) {
                gMenuSwitchTo = over;
                result = 0;
                break;
            }
        }

        /* Resting on an item with a submenu opens it, as a click does */
        static short hoverItem = 0;
        static UInt32 hoverSince = 0;
        Boolean openSubmenuNow = false;
        {
            short hi = g_menuTrackState.highlightedItem;
            if (hi != hoverItem) {
                hoverItem = hi;
                hoverSince = TickCount();
            } else if (hi > 0 && TickCount() - hoverSince >= 12) {
                short sub = 0;
                GetItemSubmenu(theMenu, hi, &sub);
                if (sub != 0) openSubmenuNow = true;
            }
        }

        /* Check button state.
         *
         * This used to declare `extern volatile uint8_t g_mouseState` and test
         * bit 0 of it. g_mouseState is not a byte - it is a struct in
         * Platform/x86/ps2.c whose first member is `int16_t x`, the cursor's
         * horizontal position. The mismatched extern is invisible to the
         * compiler (separate translation units) and to the linker, so this read
         * was fetching the low byte of the cursor X coordinate and testing its
         * least-significant bit. "Is the button down" was really "is the cursor
         * on an odd X pixel": menu selection either never fired or fired at
         * random depending on where the pointer happened to sit.
         *
         * GetMouseButtons() is the accessor ps2.c exports for exactly this. */
        extern uint8_t GetMouseButtons(void);
        Boolean buttonState = (GetMouseButtons() & 0x01) != 0;

        /* Arm the menu for selection only once the button has been steadily up
         * for a short window AND the menu has been open a moment.
         *
         * The menu is opened by a click, so the very release that ends that
         * click arrives immediately afterwards. Arming on the first sample that
         * reads "up" meant the opening click's own release armed the menu, and
         * any bounce in the same physical gesture then read as the selecting
         * click - the menu appeared and vanished in one motion. That is the
         * normal case for a laptop touchpad, where a tap is a press and release
         * a few milliseconds apart, and where tap emulation is prone to
         * chattering around the transition.
         *
         * Both gates are in ticks, so they describe real time rather than however
         * fast this loop happens to spin. */
        const UInt32 MENU_ARM_TICKS     = 12; /* ~200ms open before selectable */
        const UInt32 RELEASE_DEBOUNCE   = 2;  /* ~33ms of steady release */

        /* Is the pointer over one of this menu's items right now? */
        Boolean overItem = false;
        {
            if (mousePt.h >= left && mousePt.h < left + menuWidth) {
                for (short i = 1; i <= itemCount; i++) {
                    Rect itemRect;
                    GetTrackedMenuItemRect(theMenu, i, left, top, menuWidth, &itemRect);
                    if (mousePt.v >= itemRect.top && mousePt.v < itemRect.bottom) {
                        overItem = true;
                        break;
                    }
                }
            }
        }

        Boolean commitSelection = false;

        if (!buttonState) {
            if (releaseStartTick == 0) {
                releaseStartTick = TickCount();
                if (releaseStartTick == 0) releaseStartTick = 1;  /* 0 means "not released" */
            }
            if (!buttonWasReleased &&
                (TickCount() - releaseStartTick) >= RELEASE_DEBOUNCE &&
                (TickCount() - trackStartTick) >= MENU_ARM_TICKS) {

                /* System 7 accepts either gesture. Pressing the title,
                 * dragging down and releasing over an item chooses it; this
                 * only handled the other one - click to open, click again to
                 * choose - so a press-drag-release selected nothing and left
                 * the menu standing open. A release anywhere but over an item
                 * still arms the menu for that second click. */
                if (overItem) {
                    commitSelection = true;
                    serial_puts("TrackMenu: Released over an item\n");
                } else {
                    buttonWasReleased = true;
                    serial_puts("TrackMenu: Button released, menu armed for selection\n");
                }
            }
        } else {
            releaseStartTick = 0;  /* button down again - restart release timing */
        }

        if (openSubmenuNow) {
            commitSelection = true;
        }

        /* After the menu is armed, the next press makes the selection. */
        if (buttonWasReleased && buttonState) {
            serial_puts("TrackMenu: Second click detected\n");
            commitSelection = true;
        }

        if (commitSelection) {
            Point clickPt = mousePt;  /* Capture the indicated position */

            /* Check if click is within the menu bounds */
            if (clickPt.h >= left && clickPt.h < left + menuWidth) {
                if (clickPt.v >= top + 4 && clickPt.v < top + menuHeight - 4) {
                    /* Click was within menu - find which item was clicked */
                    /* Re-scan items to find which one the click was on */
                    short clickedItem = 0;
                    for (short i = 1; i <= itemCount; i++) {
                        Rect itemRect;
                        GetTrackedMenuItemRect(theMenu, i, left, top, menuWidth, &itemRect);
                        if (clickPt.v >= itemRect.top && clickPt.v < itemRect.bottom) {
                            char itemText[64];
                            GetItemText(theMenu, i, itemText);
                            /* A disabled item or a divider chooses nothing
                             * (Inside Macintosh: Toolbox Essentials, 3-111);
                             * only empty text used to be refused, so a
                             * greyed command ran when released on. */
                            if (itemText[0] != 0 &&
                                !CheckMenuItemSeparator(theMenu, i) &&
                                CheckMenuItemEnabled(theMenu, i)) {
                                clickedItem = i;
                            }
                            break;
                        }
                    }

                    if (clickedItem > 0) {
                        /* Check if this item has a submenu */
                        short submenuID = 0;
                        GetItemSubmenu(theMenu, clickedItem, &submenuID);

                        char itemText[64];
                        GetItemText(theMenu, clickedItem, itemText);

                        char debugBuf[128];
                        snprintf(debugBuf, sizeof(debugBuf),
                                "[TM] Item %d (%s) submenuID=%d\n",
                                clickedItem, itemText, submenuID);
                        serial_puts(debugBuf);

                        if (submenuID != 0) {
                            /* This item has a submenu - open it instead of returning */
                            MENU_LOG_TRACE("TrackMenu: Item %d has submenu %d, opening it\n", clickedItem, submenuID);

                            /* Calculate submenu position to the right of current menu */
                            Point submenuPt;
                            submenuPt.h = left + menuWidth;  /* Open to the right */
                            Rect clickedRect;
                            GetTrackedMenuItemRect(theMenu, clickedItem, left, top,
                                                   menuWidth, &clickedRect);
                            submenuPt.v = clickedRect.top;

                            /* Track the submenu - it will return the final selection */
                            result = TrackMenu(submenuID, &submenuPt);
                        } else {
                            /* No submenu - this is the final selection */
                            result = ((long)menuID << 16) | clickedItem;
                            MENU_LOG_TRACE("TrackMenu: Item %d selected by click\n", clickedItem);

                            FlashChosenItem(theMenu, clickedItem, left, top,
                                            menuWidth);
                        }
                    }
                    tracking = false;
                    break;
                }
            }

            /* Click was outside menu - cancel and close */
            MENU_LOG_TRACE("TrackMenu: Click outside menu at (%d,%d), cancelling\n", clickPt.h, clickPt.v);
            result = 0;
            tracking = false;
            break;
        }

        /* Small delay to prevent CPU hogging */
        {
            volatile int i;
            for (i = 0; i < 100; i++);
        }

        /* Debug output every 100 updates to avoid spam */
        if (updateCount % 100 == 0) {
            MENU_LOG_TRACE("TrackMenu: Still tracking, update %d, menu open=%d\n", updateCount, buttonWasReleased);
        }
    }

    if ((TickCount() - trackStartTick) >= MAX_TRACKING_TICKS) {
        MENU_LOG_WARN("TrackMenu: Tracking timeout after %u ticks (%d updates)\n",
                      (unsigned)(TickCount() - trackStartTick), updateCount);
    }

    serial_puts("TrackMenu: Menu tracking complete\n");

    /* Restore background */
    if (savedBits) {
        RestoreMenuBits(savedBits);
        DiscardMenuBits(savedBits);
        serial_puts("TrackMenu: Background restored\n");
    }

    /* Clear tracking state */
    g_menuTrackState.isTracking = false;
    g_menuTrackState.activeMenu = NULL;
    g_menuTrackState.highlightedItem = 0;

    /* Restore port */
    if (savePort) SetPort(savePort);

    /* Invalidate cursor so it gets redrawn (menu operations corrupt cursor background) */
    InvalidateCursor();

    return result;
}

/* Draw inverted Apple icon for highlighted Apple menu */
/* Draw menu bar with a specific menu title highlighted */
void DrawMenuBarWithHighlight(short highlightMenuID) {
    static short lastHighlightMenuID = 0;
    static Boolean menuBarDrawn = false;

    /* Draw menu bar first time, or when clearing highlight */
    if (!menuBarDrawn || (highlightMenuID == 0 && lastHighlightMenuID != 0)) {
        DrawMenuBar();
        menuBarDrawn = true;
        if (highlightMenuID == 0) {
            lastHighlightMenuID = 0;
            return;
        }
    }

    /* If no menu to highlight and nothing was highlighted, do nothing */
    if (highlightMenuID == 0) {
        return;
    }

    /*
     * Use the title rectangle the menu bar actually measured when it drew
     * itself. DrawMenuBar records each one through AddMenuTitle, so it is the
     * only layout that matches what is on screen.
     *
     * This used to recompute the layout here from hardcoded English strings -
     * TextWidth("File",0,4) + 20, TextWidth("Label",0,5) + 20 and so on - which
     * was a third independent copy of the menu bar layout and disagreed with
     * the real one: selecting Special highlighted x=225..290 while the title
     * actually sat at 247..305, so the black box landed across "Label". It also
     * meant a translated build highlighted a region computed from English
     * widths, and drew English text into it.
     */
    Rect titleRect;
    if (!GetMenuTitleRectByID(highlightMenuID, &titleRect)) {
        return;
    }
    short titleX = titleRect.left;
    short titleW = titleRect.right - titleRect.left;

    /* Draw black background for the title */
    DrawHighlightRect(titleX, 0, titleX + titleW, 19, true);

    /* The Apple and Application menus have icons for titles, drawn as
     * DrawMenuTitle draws them; this drew a hand-made 11x13 apple four
     * pixels right of the real one, and the Application menu as a box. */
    extern short MenuAppleIcon_Draw(GrafPtr port, short x, short y, Boolean inverted);
    extern short MenuAppIcon_Draw(GrafPtr port, short x, short y, Boolean inverted);
    if (MenuIsAppleMenu(highlightMenuID) || MenuIsApplicationMenu(highlightMenuID)) {
        GrafPtr screen = NULL;
        GetWMgrPort(&screen);
        if (MenuIsAppleMenu(highlightMenuID)) {
            MenuAppleIcon_Draw(screen, titleRect.left, titleRect.top, true);
        } else {
            MenuAppIcon_Draw(screen, titleRect.left, titleRect.top, true);
        }
    } else {
        /* Title text comes from the menu itself, not a hardcoded table */
        MenuHandle theMenu = GetMenuHandle(highlightMenuID);
        if (theMenu) {
            char titleText[64];
            short len = (*(MenuInfo**)theMenu)->menuData[0];
            if (len > 63) len = 63;
            for (short i = 0; i < len; i++) {
                titleText[i] = (char)(*(MenuInfo**)theMenu)->menuData[1 + i];
            }
            titleText[len] = '\0';
            GrafPtr savePort;
            GetPort(&savePort);
            QD_SetScreenPort();
            Menu_ClipToScreen();
            ForeColor(whiteColor);
            DrawTrackedMenuText(titleText, titleX + 4, 14, normal);
            ForeColor(blackColor);
            SetPort(savePort);
        }
    }

    lastHighlightMenuID = highlightMenuID;
}
