/*
 * MenuTrack.c - Basic dropdown rendering and tracking
 *
 * Draws menu item list under the title and lets user select with mouse.
 */

#include "SystemTypes.h"
#include "MenuManager/MenuManager.h"
#include "MenuManager/MenuLogging.h"
#include "MenuManager/MenuTypes.h"
#include "QuickDraw/QuickDraw.h"
#include "FontManager/FontManager.h"
#include "EventManager/EventTypes.h"  /* For mouse masks */

/* Function declarations */
extern SInt16 CountMenuItems(MenuHandle theMenu);

/* External functions */
extern void serial_puts(const char* str);
extern QDGlobals qd;
extern void DrawDesktop(void);
extern void DrawText(const void* textBuf, short firstByte, short byteCount);
extern void DrawVolumeIcon(void);
extern Boolean Button(void);          /* Check if mouse button is pressed */
extern void GetMouse(Point* mouseLoc);
extern void MoveTo(short h, short v);
extern void DrawMenuBar(void);        /* Redraw the menu bar */

/* Forward declarations for static functions */
static void DrawHighlightRect(short left, short top, short right, short bottom, Boolean highlight);
static void DrawInvertedText(const char* text, short x, short y, Boolean inverted);
static void DrawInvertedAppleIcon(short x, short y);
void DrawMenuBarWithHighlight(short highlightMenuID);

/* Forward declarations for menu tracking functions */
long BeginTrackMenu(short menuID, Point *startPt);
void UpdateMenuTrackingNew(Point mousePt);
long EndMenuTrackingNew(void);
Boolean IsMenuTrackingNew(void);
long TrackMenu(short menuID, Point *startPt);

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
    short lineHeight;          /* Height of each menu item */
    short titleLeft;           /* Left position of menu title in menu bar */
    short titleWidth;          /* Width of menu title in menu bar */
} g_menuTrackState = {0};

/* Global framebuffer from main.c */
extern void* framebuffer;
extern uint32_t fb_width;
extern uint32_t fb_height;
extern uint32_t fb_pitch;

/* Rect helpers */
extern void SetRect(Rect* rect, short left, short top, short right, short bottom);
extern void InvalRect(const Rect* rect);

/* Draw filled rectangle */
static void DrawMenuRect(short left, short top, short right, short bottom, uint32_t color) {
    if (!framebuffer) return;

    uint32_t *fb = (uint32_t*)framebuffer;
    int pitch = fb_pitch / 4;

    if (left < 0) left = 0;
    if (top < 0) top = 0;
    if (right > (int)fb_width) right = fb_width;
    if (bottom > (int)fb_height) bottom = fb_height;
    if (left >= right || top >= bottom) return;

    for (int y = top; y < bottom; y++) {
        for (int x = left; x < right; x++) {
            fb[y * pitch + x] = color;
        }
    }
}


/* Draw text */
static void DrawMenuItemText(const char* text, short x, short y) {
    MoveTo(x, y);
    short len = 0;
    while (text[len] != 0) len++;
    if (len > 0) DrawText(text, 0, len);

    MENU_LOG_TRACE("Drawing menu item: %s at (%d,%d)\n", text, x, y);
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

/* Draw dropdown menu */
static void DrawMenuOld(MenuHandle theMenu, short left, short top, short itemCount, short menuWidth, short lineHeight) {
    /* Save current port and ensure we're in screen port for menu drawing */
    GrafPtr savePort;
    GetPort(&savePort);
    if (qd.thePort) {
        SetPort(qd.thePort);  /* Use screen port for global coordinates */
    }

    /* Draw white background */
    DrawMenuRect(left, top, left + menuWidth, top + itemCount * lineHeight + 4, 0xFFFFFFFF);
    volatile int stack_align = 0;  /* Fix stack alignment issue */
    stack_align++;  /* Prevent compiler optimization */

    /* Draw border */
    DrawMenuRect(left, top, left + menuWidth, top + 1, 0xFF000000);
    DrawMenuRect(left, top + itemCount * lineHeight + 3, left + menuWidth, top + itemCount * lineHeight + 4, 0xFF000000);
    DrawMenuRect(left, top, left + 1, top + itemCount * lineHeight + 4, 0xFF000000);
    DrawMenuRect(left + menuWidth - 1, top, left + menuWidth, top + itemCount * lineHeight + 4, 0xFF000000);

    /* Items - clamp iteration to prevent runaway loops */
    short maxItems = itemCount > 64 ? 64 : itemCount;
    for (short i = 1; i <= maxItems; i++) {
        char itemText[64];
        GetItemText(theMenu, i, itemText);
        if (itemText[0] == 0) continue;

        short itemTop = top + 2 + (i - 1) * lineHeight;
        DrawMenuItemText(itemText, left + 4, itemTop + 12);
    }

    /* Restore original port */
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
        SetPort(qd.thePort);  /* Draw menus in screen port for global coords */
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
    /* Different menus need different widths */
    short menuWidth = 120;  /* Default width */
    if (menuID == 128) menuWidth = 150;  /* Apple menu - "About This Macintosh" */
    if (menuID == 131) menuWidth = 130;  /* View menu - "Clean Up Selection" */
    short lineHeight = 16;

    /* Save menu tracking state */
    g_menuTrackState.isTracking = true;
    g_menuTrackState.activeMenu = theMenu;
    g_menuTrackState.menuID = menuID;
    g_menuTrackState.menuLeft = left;
    g_menuTrackState.menuTop = top;
    g_menuTrackState.menuWidth = menuWidth;
    /* Use SInt32 to prevent overflow in height calculation */
    SInt32 calcHeight = (SInt32)itemCount * (SInt32)lineHeight + 4;
    if (calcHeight > 32767) calcHeight = 32767;  /* Clamp to max short */
    g_menuTrackState.menuHeight = (short)calcHeight;
    g_menuTrackState.itemCount = itemCount;
    g_menuTrackState.highlightedItem = 0;
    g_menuTrackState.lineHeight = lineHeight;
    MENU_LOG_TRACE("BeginTrackMenu: Initial highlightedItem = %d\n", g_menuTrackState.highlightedItem);

    /* Calculate and store menu title position */
    /* For now, estimate based on menu ID and typical widths */
    short titleX = 0;
    short titleW = 30;  /* Default width */
    if (menuID == 128) {  /* Apple menu */
        titleX = 0;
        titleW = 30;
    } else if (menuID == 129) {  /* File menu */
        titleX = 30;
        titleW = 32;
    } else if (menuID == 130) {  /* Edit menu */
        titleX = 62;
        titleW = 32;
    } else if (menuID == 131) {  /* View menu */
        titleX = 94;
        titleW = 36;
    } else if (menuID == 132) {  /* Label menu */
        titleX = 130;
        titleW = 40;
    } else if (menuID == 133) {  /* Special menu */
        titleX = 170;
        titleW = 50;
    }

    g_menuTrackState.titleLeft = titleX;
    g_menuTrackState.titleWidth = titleW;

    serial_puts("BeginTrackMenu: About to call DrawMenuBarWithHighlight\n");
    /* Redraw the menu bar with the active menu highlighted */
    DrawMenuBarWithHighlight(menuID);
    serial_puts("BeginTrackMenu: Returned from DrawMenuBarWithHighlight\n");

    serial_puts("BeginTrackMenu: About to call DrawMenuOld\n");
    /* Draw the menu dropdown */
    DrawMenuOld(theMenu, left, top, itemCount, menuWidth, lineHeight);
    serial_puts("BeginTrackMenu: Dropdown drawn, tracking started\n");

    /* Restore original port */
    if (savePort) SetPort(savePort);

    /* Return 0 - actual selection will come from event handling */
    return 0;
}

/* Draw inverted text directly to framebuffer */
static void DrawInvertedText(const char* text, short x, short y, Boolean inverted) {
    extern void* framebuffer;
    extern uint32_t fb_width;
    extern uint32_t fb_height;
    extern uint32_t fb_pitch;
    extern uint32_t pack_color(uint8_t r, uint8_t g, uint8_t b);

    if (!text || !framebuffer) return;

    #include "chicago_font.h"

    uint32_t* fb = (uint32_t*)framebuffer;
    uint32_t textColor = inverted ? 0xFFFFFFFF : 0xFF000000;  /* White if inverted, black if normal */

    int len = 0;
    int currentX = x;

    /* Direct rendering using Chicago font bitmap */
    while (text[len] && len < 255) {
        char ch = text[len];
        if (ch >= 32 && ch <= 126) {
            ChicagoCharInfo info = chicago_ascii[ch - 32];

            /* Draw character */
            for (int row = 0; row < CHICAGO_HEIGHT; row++) {
                if (y - 12 + row < 0 || y - 12 + row >= fb_height) continue;

                const uint8_t *strike_row = chicago_bitmap + (row * CHICAGO_ROW_BYTES);

                for (int col = 0; col < info.bit_width; col++) {
                    int px = currentX + info.left_offset + col;
                    if (px < 0 || px >= fb_width) continue;

                    int bit_position = info.bit_start + col;
                    uint8_t bit = (strike_row[bit_position >> 3] >> (7 - (bit_position & 7))) & 1;

                    if (bit) {
                        int fb_offset = (y - 12 + row) * (fb_pitch / 4) + px;
                        fb[fb_offset] = textColor;
                    }
                }
            }

            currentX += info.advance;
        }
        len++;
    }
}

/* Draw rectangle with specified color */
static void DrawHighlightRect(short left, short top, short right, short bottom, Boolean highlight) {
    extern void* framebuffer;
    extern uint32_t fb_width;
    extern uint32_t fb_height;
    extern uint32_t fb_pitch;

    if (!framebuffer) return;

    uint32_t* fb = (uint32_t*)framebuffer;
    uint32_t color = highlight ? 0xFF000000 : 0xFFFFFFFF; /* Black for highlight, white for clear */

    /* Fill rectangle */
    for (int y = top; y < bottom && y < fb_height; y++) {
        if (y < 0) continue;
        for (int x = left; x < right && x < fb_width; x++) {
            if (x < 0) continue;
            fb[y * (fb_pitch / 4) + x] = color;
        }
    }
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
    short lineHeight = g_menuTrackState.lineHeight;
    short itemCount = g_menuTrackState.itemCount;
    MenuHandle theMenu = g_menuTrackState.activeMenu;

    /* Check if mouse is over a menu item - account for 2px top padding */
    short newHighlight = 0;
    short itemsTop = top + 2;  /* Menu items start 2 pixels below menu top */

    /* First check if mouse is horizontally within menu */
    if (mousePt.h >= left && mousePt.h < left + menuWidth) {
        /* Check each item's position to find which one the mouse is over */
        for (short i = 1; i <= itemCount; i++) {
            short itemTop = itemsTop + (i - 1) * lineHeight;
            short itemBottom = itemTop + lineHeight;

            /* Check if mouse is vertically within this item */
            if (mousePt.v >= itemTop && mousePt.v < itemBottom) {
                /* Check if this item has text (not a separator/empty) */
                char itemText[64];
                GetItemText(theMenu, i, itemText);
                if (itemText[0] != 0) {
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
            short oldTop = top + 2 + (g_menuTrackState.highlightedItem - 1) * lineHeight;
            MENU_LOG_TRACE("UpdateMenu: Clearing old highlight at y=%d\n", oldTop);

            /* Draw white background to clear the highlight */
            DrawHighlightRect(left + 2, oldTop, left + menuWidth - 2, oldTop + lineHeight - 1, false);

            /* Redraw text in normal black on white */
            char oldText[64];
            GetItemText(theMenu, g_menuTrackState.highlightedItem, oldText);
            if (oldText[0] != 0) {
                /* Use DrawMenuItemText for consistent normal rendering */
                DrawMenuItemText(oldText, left + 4, oldTop + 12);
            }
        }

        /* Draw new highlight and text */
        if (newHighlight > 0) {
            short itemTop = top + 2 + (newHighlight - 1) * lineHeight;
            MENU_LOG_TRACE("UpdateMenu: Drawing new highlight at y=%d for item %d\n", itemTop, newHighlight);

            /* Draw black background for highlight */
            DrawHighlightRect(left + 2, itemTop, left + menuWidth - 2, itemTop + lineHeight - 1, true);

            /* Redraw text in white on black using inverted text */
            char newText[64];
            GetItemText(theMenu, newHighlight, newText);
            if (newText[0] != 0) {
                DrawInvertedText(newText, left + 4, itemTop + 12, true);  /* true = white inverted text */
            }
        }

        g_menuTrackState.highlightedItem = newHighlight;
    }
}

/* End menu tracking and return selection */
long EndMenuTrackingNew(void) {
    extern void serial_printf(const char* fmt, ...);
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
        SetPort(qd.thePort);  /* Ensure we redraw in screen port */
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

/* TrackMenu - Full implementation with mouse tracking loop */
__attribute__((optimize("O0")))
long TrackMenu(short menuID, Point *startPt) {
    /* NULL check to prevent crash */
    if (!startPt) {
        return 0;
    }

    /* Declare all variables at function start */
    GrafPtr savePort;
    Rect menuRect;
    Handle savedBits;
    Point mousePt;
    long result = 0;

    /* External functions for event pumping */
    extern void SystemTask(void);
    extern void EventPumpYield(void);

    /* Save current port */
    GetPort(&savePort);
    if (qd.thePort) {
        SetPort(qd.thePort);
        serial_puts("TrackMenu: SetPort done\n");
    }

    /* Get the menu */
    MenuHandle theMenu = GetMenuHandle(menuID);
    serial_puts("TrackMenu: GetMenuHandle returned\n");
    if (!theMenu) {
        if (savePort) SetPort(savePort);
        // s_inTrackMenu cleared
        return 0;
    }

    /* Test if theMenu pointer is safe to dereference */
    unsigned long menuPtr = (unsigned long)theMenu;
    /* ARM64 heap addresses are typically in the 0x40000000+ range
     * x86 addresses are typically lower. Accept a wide range. */
    if (menuPtr < 0x1000 || menuPtr > 0x80000000) {
        serial_puts("TrackMenu: Menu handle looks invalid (bad address range)\n");
        if (savePort) SetPort(savePort);
        // s_inTrackMenu cleared
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


    short menuWidth = 120;
    if (menuID == 128) menuWidth = 150;
    if (menuID == 131) menuWidth = 130;


    if (menuWidth <= 0) {
        serial_puts("TrackMenu: Invalid menuWidth, using default\n");
        menuWidth = 120;
    }


    short lineHeight = 16;
    if (lineHeight <= 0) {
        serial_puts("TrackMenu: Invalid lineHeight, using default\n");
        lineHeight = 16;
    }

    /* Use SInt32 to prevent overflow in height calculation */
    SInt32 calcHeight = (SInt32)itemCount * (SInt32)lineHeight + 4;
    if (calcHeight > 32767) calcHeight = 32767;  /* Clamp to max short */
    short menuHeight = (short)calcHeight;

    /* Get coordinates from startPt (already validated non-NULL earlier) */
    short left = startPt->h;
    short top = 20;

    /* Calculate menu rectangle */
    menuRect.left = left;
    menuRect.top = top;
    menuRect.right = left + menuWidth;
    menuRect.bottom = top + menuHeight;

    /* Clip to screen bounds to prevent out-of-bounds save/restore */
    #define SCREEN_WIDTH 640
    #define SCREEN_HEIGHT 480
    if (menuRect.left < 0) menuRect.left = 0;
    if (menuRect.top < 0) menuRect.top = 0;
    if (menuRect.right > SCREEN_WIDTH) menuRect.right = SCREEN_WIDTH;
    if (menuRect.bottom > SCREEN_HEIGHT) menuRect.bottom = SCREEN_HEIGHT;

    /* Validate rect is non-empty after clipping */
    if (menuRect.right <= menuRect.left || menuRect.bottom <= menuRect.top) {
        serial_puts("TrackMenu: Invalid rect after clipping, aborting\n");
        if (savePort) SetPort(savePort);
        // s_inTrackMenu cleared
        return 0;
    }

    extern Handle SaveMenuBits(const Rect *menuRect);
    extern OSErr RestoreMenuBits(Handle bitsHandle);
    extern OSErr DiscardMenuBits(Handle bitsHandle);

    savedBits = SaveMenuBits(&menuRect);
    serial_puts("TrackMenu: SaveMenuBits returned\n");
    if (savedBits) {
    }

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
    g_menuTrackState.lineHeight = lineHeight;

    /* Draw the menu bar with the active menu highlighted */
    DrawMenuBarWithHighlight(menuID);
    serial_puts("TrackMenu: Menu bar highlight drawn\n");

    /* Draw the menu dropdown */
    DrawMenuOld(theMenu, left, top, itemCount, menuWidth, lineHeight);
    serial_puts("TrackMenu: DrawMenuOld returned\n");
    serial_puts("TrackMenu: Menu drawn, entering tracking loop\n");

    /* Persistent menu tracking - menu stays open until user makes a selection or clicks outside */
    /* ADD SAFETY TIMEOUT: Prevent infinite tracking loop */
    Boolean tracking = true;
    Boolean buttonWasReleased = false;
    int updateCount = 0;
    int buttonCheckCount = 0;
    const int MAX_TRACKING_UPDATES = 1000000;  /* Increased: menu can stay open longer */

    serial_puts("TrackMenu: Starting persistent menu tracking\n");

    /* Track menu - menu stays open even after button is released */
    while (tracking && updateCount < MAX_TRACKING_UPDATES) {
        /* Pump events for responsive UI */
        SystemTask();          /* House-keeping tasks */
        EventPumpYield();      /* Platform's input pump */

        /* Increment update counter */
        updateCount++;

        /* Draw cursor (menu tracking has its own event loop that bypasses main loop) */
        extern void UpdateCursorDisplay(void);
        UpdateCursorDisplay();

        /* Get current mouse position */
        GetMouse(&mousePt);

        /* Update menu highlighting based on mouse position */
        UpdateMenuTrackingNew(mousePt);

        /* Check button state */
        buttonCheckCount++;
        Boolean buttonState = Button();

        /* Debug: Log first few button states to diagnose tracking freeze */
        if (buttonCheckCount <= 3) {
            serial_puts(buttonState ? "[TRACK] Button=DOWN\n" : "[TRACK] Button=UP\n");
        }

        /* Track when button is first released */
        if (!buttonState && !buttonWasReleased) {
            buttonWasReleased = true;
            serial_puts("TrackMenu: Button released, menu stays open for selection\n");
        }

        /* After button is released, wait for next click to make selection */
        /* Detect when button goes from released (false) to pressed (true) again */
        if (buttonWasReleased && buttonState) {
            /* User clicked again while menu is open - check what they clicked on */
            serial_puts("TrackMenu: Second click detected\n");

            Point clickPt = mousePt;  /* Capture click position */

            /* Check if click is within the menu bounds */
            if (clickPt.h >= left && clickPt.h < left + menuWidth) {
                short itemsTop = top + 2;
                if (clickPt.v >= itemsTop && clickPt.v < itemsTop + itemCount * lineHeight) {
                    /* Click was within menu - find which item was clicked */
                    /* Re-scan items to find which one the click was on */
                    short clickedItem = 0;
                    for (short i = 1; i <= itemCount; i++) {
                        short itemTop = itemsTop + (i - 1) * lineHeight;
                        short itemBottom = itemTop + lineHeight;
                        if (clickPt.v >= itemTop && clickPt.v < itemBottom) {
                            char itemText[64];
                            GetItemText(theMenu, i, itemText);
                            if (itemText[0] != 0) {  /* Skip empty items */
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
                            submenuPt.v = top + 2 + (clickedItem - 1) * lineHeight;  /* Align with item */

                            /* Track the submenu - it will return the final selection */
                            result = TrackMenu(submenuID, &submenuPt);
                        } else {
                            /* No submenu - this is the final selection */
                            result = ((long)menuID << 16) | clickedItem;
                            MENU_LOG_TRACE("TrackMenu: Item %d selected by click\n", clickedItem);

                            /* Show visual feedback - keep selected item highlighted briefly */
                            volatile int feedbackDelay = 0;
                            for (feedbackDelay = 0; feedbackDelay < 200000; feedbackDelay++) {
                                SystemTask();
                                EventPumpYield();
                            }
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

    if (updateCount >= MAX_TRACKING_UPDATES) {
        MENU_LOG_WARN("TrackMenu: Tracking timeout! Escaped loop after %d updates\n", updateCount);
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
    extern void InvalidateCursor(void);
    InvalidateCursor();

    /* Clear reentrancy flag before returning */
        // s_inTrackMenu cleared

    return result;
}

/* Draw inverted Apple icon for highlighted Apple menu */
static void DrawInvertedAppleIcon(short x, short y) {
    extern void* framebuffer;
    extern uint32_t fb_width;
    extern uint32_t fb_height;
    extern uint32_t fb_pitch;

    if (!framebuffer) return;

    uint32_t* fb = (uint32_t*)framebuffer;

    /* Simple Apple logo pattern - inverted colors (white on black) */
    static const uint8_t apple[13][11] = {
        {0,0,0,0,0,1,1,0,0,0,0},
        {0,0,0,0,1,1,1,0,0,0,0},
        {0,0,0,0,0,1,0,0,0,0,0},
        {0,0,1,1,1,1,1,1,1,0,0},
        {0,1,1,1,1,1,1,1,1,1,0},
        {1,1,1,1,1,1,1,1,1,1,1},
        {1,1,1,1,1,1,1,1,1,1,1},
        {1,1,1,1,1,1,1,1,1,1,1},
        {1,1,1,1,1,1,1,1,1,1,1},
        {1,1,1,1,1,1,1,1,1,1,1},
        {0,1,1,1,1,1,1,1,1,1,0},
        {0,0,1,1,0,0,0,1,1,0,0},
        {0,0,0,0,0,0,0,0,0,0,0}
    };

    /* Draw the inverted Apple logo (white pixels) */
    for (int row = 0; row < 13; row++) {
        for (int col = 0; col < 11; col++) {
            if (apple[row][col]) {
                int px = x + col;
                int py = y + row;
                if (px >= 0 && px < fb_width && py >= 0 && py < fb_height) {
                    fb[py * (fb_pitch / 4) + px] = 0xFFFFFFFF;  /* White */
                }
            }
        }
    }
}

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

    /* Calculate actual menu positions based on string widths */
    /* These match the calculations in DrawMenuBar */
    short x = 0;
    short titleX = 0;
    short titleW = 0;

    /* Apple menu - always first at x=0 */
    if (highlightMenuID == 128) {
        titleX = 0;
        titleW = 30;  /* Apple icon width + padding */
    } else {
        /* Skip past Apple menu */
        x = 30;

        /* File menu */
        if (highlightMenuID == 129) {
            titleX = x;
            titleW = TextWidth("File", 0, 4) + 20;
        } else {
            x += TextWidth("File", 0, 4) + 20;

            /* Edit menu */
            if (highlightMenuID == 130) {
                titleX = x;
                titleW = TextWidth("Edit", 0, 4) + 20;
            } else {
                x += TextWidth("Edit", 0, 4) + 20;

                /* View menu */
                if (highlightMenuID == 131) {
                    titleX = x;
                    titleW = TextWidth("View", 0, 4) + 20;
                } else {
                    x += TextWidth("View", 0, 4) + 20;

                    /* Label menu */
                    if (highlightMenuID == 132) {
                        titleX = x;
                        titleW = TextWidth("Label", 0, 5) + 20;
                    } else {
                        x += TextWidth("Label", 0, 5) + 20;

                        /* Special menu */
                        if (highlightMenuID == 133) {
                            titleX = x;
                            titleW = TextWidth("Special", 0, 7) + 20;
                        }
                    }
                }
            }
        }
    }

    /* Draw black background for the title */
    DrawHighlightRect(titleX, 0, titleX + titleW, 19, true);

    /* Redraw the title text in white */
    const char* titleText = "";
    if (highlightMenuID == 129) { titleText = "File"; }
    else if (highlightMenuID == 130) { titleText = "Edit"; }
    else if (highlightMenuID == 131) { titleText = "View"; }
    else if (highlightMenuID == 132) { titleText = "Label"; }
    else if (highlightMenuID == 133) { titleText = "Special"; }

    if (highlightMenuID != 128) {  /* Not Apple menu - draw text */
        DrawInvertedText(titleText, titleX + 4, 14, true);
    } else {
        /* For Apple menu, draw inverted Apple icon */
        DrawInvertedAppleIcon(8, 2);
    }

    lastHighlightMenuID = highlightMenuID;
}

