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

/* --- Safe strcpy (avoids missing symbol) --- */
static void simple_strcpy(char* dst, const char* src) {
    if (!dst || !src) return;
    while (*src) {
        *dst++ = *src++;
    }
    *dst = 0;
}

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

    /* Items */
    for (short i = 1; i <= itemCount; i++) {
        char itemText[64];
        GetItemText(theMenu, i, itemText);
        if (itemText[0] == 0) continue;

        short itemTop = top + 2 + (i - 1) * lineHeight;
        DrawMenuItemText(itemText, left + 4, itemTop + 12);
    }

    /* Restore original port */
    if (savePort) SetPort(savePort);
}

/* Simple cursor drawing - arrow pointer */
static void DrawCursor(short x, short y) {
    if (!framebuffer) return;

    uint32_t *fb = (uint32_t*)framebuffer;
    int pitch = fb_pitch / 4;

    /* Simple arrow cursor pattern (11x16) */
    static const char arrow[16][11] = {
        {1,0,0,0,0,0,0,0,0,0,0},
        {1,1,0,0,0,0,0,0,0,0,0},
        {1,2,1,0,0,0,0,0,0,0,0},
        {1,2,2,1,0,0,0,0,0,0,0},
        {1,2,2,2,1,0,0,0,0,0,0},
        {1,2,2,2,2,1,0,0,0,0,0},
        {1,2,2,2,2,2,1,0,0,0,0},
        {1,2,2,2,2,2,2,1,0,0,0},
        {1,2,2,2,2,2,2,2,1,0,0},
        {1,2,2,2,2,1,1,1,1,1,0},
        {1,2,2,1,2,1,0,0,0,0,0},
        {1,2,1,0,1,2,1,0,0,0,0},
        {1,1,0,0,1,2,1,0,0,0,0},
        {1,0,0,0,0,1,2,1,0,0,0},
        {0,0,0,0,0,1,2,1,0,0,0},
        {0,0,0,0,0,0,1,1,0,0,0}
    };

    /* Draw arrow */
    for (int row = 0; row < 16; row++) {
        for (int col = 0; col < 11; col++) {
            int px = x + col;
            int py = y + row;
            if (px >= 0 && px < (int)fb_width && py >= 0 && py < (int)fb_height) {
                if (arrow[row][col] == 1) {
                    fb[py * pitch + px] = 0xFF000000; /* Black outline */
                } else if (arrow[row][col] == 2) {
                    fb[py * pitch + px] = 0xFFFFFFFF; /* White fill */
                }
            }
        }
    }
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
    g_menuTrackState.menuHeight = itemCount * lineHeight + 4;
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
    if (mousePt.h >= left && mousePt.h < left + menuWidth &&
        mousePt.v >= itemsTop && mousePt.v < itemsTop + itemCount * lineHeight) {
        newHighlight = (mousePt.v - itemsTop) / lineHeight + 1;
        if (newHighlight < 1 || newHighlight > itemCount) {
            newHighlight = 0;
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
            char itemText[64];
            GetItemText(theMenu, g_menuTrackState.highlightedItem, itemText);
            if (itemText[0] != 0) {
                /* Use DrawMenuItemText for consistent normal rendering */
                DrawMenuItemText(itemText, left + 4, oldTop + 12);
            }
        }

        /* Draw new highlight and text */
        if (newHighlight > 0) {
            short itemTop = top + 2 + (newHighlight - 1) * lineHeight;
            MENU_LOG_TRACE("UpdateMenu: Drawing new highlight at y=%d for item %d\n", itemTop, newHighlight);

            /* Draw black background for highlight */
            DrawHighlightRect(left + 2, itemTop, left + menuWidth - 2, itemTop + lineHeight - 1, true);

            /* Redraw text in white on black using inverted text */
            char itemText[64];
            GetItemText(theMenu, newHighlight, itemText);
            if (itemText[0] != 0) {
                DrawInvertedText(itemText, left + 4, itemTop + 12, true);  /* true = white inverted text */
            }
        }

        g_menuTrackState.highlightedItem = newHighlight;
    }
}

/* End menu tracking and return selection */
long EndMenuTrackingNew(void) {
    if (!g_menuTrackState.isTracking) return 0;

    long result = 0;
    if (g_menuTrackState.highlightedItem > 0) {
        /* Pack menuID in high word, item in low word */
        result = ((long)g_menuTrackState.menuID << 16) | g_menuTrackState.highlightedItem;
        MENU_LOG_TRACE("EndMenuTracking: Selected item %d from menu %d\n",
                     g_menuTrackState.highlightedItem, g_menuTrackState.menuID);
    }

    /* Clear tracking state */
    short wasMenuID = g_menuTrackState.menuID;
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
    /* Static reentrancy guard */
    static Boolean s_inTrackMenu = false;

    /* Prevent reentrancy */
    if (s_inTrackMenu) {
        serial_puts("TrackMenu: Already tracking, preventing reentrancy\n");
        return 0;
    }

    /* NULL check to prevent crash */
    if (!startPt) {
        return 0;
    }

    /* Set reentrancy flag */
    s_inTrackMenu = true;

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
        s_inTrackMenu = false;  /* Clear reentrancy flag */
        return 0;
    }

    /* Test if theMenu pointer is safe to dereference */
    unsigned long menuPtr = (unsigned long)theMenu;
    if (menuPtr < 0x1000 || menuPtr > 0x40000000) {
        serial_puts("TrackMenu: Menu handle looks invalid (bad address range)\n");
        if (savePort) SetPort(savePort);
        s_inTrackMenu = false;  /* Clear reentrancy flag */
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

    serial_puts("TrackMenu: After itemCount check\n");

    /* Validate geometry to prevent zero/negative sizes */
    if (itemCount <= 0) {
        serial_puts("TrackMenu: Invalid itemCount, using default\n");
        itemCount = 5;
    }

    serial_puts("TrackMenu: After itemCount validation\n");

    short menuWidth = 120;
    if (menuID == 128) menuWidth = 150;
    if (menuID == 131) menuWidth = 130;

    serial_puts("TrackMenu: After menuWidth calculation\n");

    if (menuWidth <= 0) {
        serial_puts("TrackMenu: Invalid menuWidth, using default\n");
        menuWidth = 120;
    }

    serial_puts("TrackMenu: After menuWidth validation\n");

    short lineHeight = 16;
    if (lineHeight <= 0) {
        serial_puts("TrackMenu: Invalid lineHeight, using default\n");
        lineHeight = 16;
    }

    serial_puts("TrackMenu: After lineHeight setup\n");

    short menuHeight = itemCount * lineHeight + 4;
    serial_puts("TrackMenu: After menuHeight calculation\n");

    /* Get coordinates from startPt (already validated non-NULL earlier) */
    short left = startPt->h;
    short top = 20;

    serial_puts("TrackMenu: After reading startPt coordinates\n");

    /* Calculate menu rectangle */
    menuRect.left = left;
    menuRect.top = top;
    menuRect.right = left + menuWidth;
    menuRect.bottom = top + menuHeight;

    serial_puts("TrackMenu: After menuRect calculation\n");

    /* Clip to screen bounds to prevent out-of-bounds save/restore */
    #define SCREEN_WIDTH 640
    #define SCREEN_HEIGHT 480
    serial_puts("TrackMenu: Before screen bounds clipping\n");
    if (menuRect.left < 0) menuRect.left = 0;
    if (menuRect.top < 0) menuRect.top = 0;
    if (menuRect.right > SCREEN_WIDTH) menuRect.right = SCREEN_WIDTH;
    if (menuRect.bottom > SCREEN_HEIGHT) menuRect.bottom = SCREEN_HEIGHT;

    serial_puts("TrackMenu: After screen bounds clipping\n");

    /* Validate rect is non-empty after clipping */
    if (menuRect.right <= menuRect.left || menuRect.bottom <= menuRect.top) {
        serial_puts("TrackMenu: Invalid rect after clipping, aborting\n");
        if (savePort) SetPort(savePort);
        s_inTrackMenu = false;  /* Clear reentrancy flag */
        return 0;
    }

    serial_puts("TrackMenu: menuRect validation passed\n");

    extern Handle SaveMenuBits(const Rect *menuRect);
    extern OSErr RestoreMenuBits(Handle bitsHandle);
    extern OSErr DiscardMenuBits(Handle bitsHandle);

    serial_puts("TrackMenu: About to call SaveMenuBits\n");
    serial_puts("TrackMenu: menuRect validation passed\n");

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

    /* Classic Mac-style tracking loop with event pumping */
    /* Track while button is held down - this is the System 7 behavior */
    /* Initially, wait for button release from the initial click */
    int releaseWaitCount = 0;
    while (Button()) {
        SystemTask();          /* House-keeping */
        EventPumpYield();      /* Platform's input pump */
        releaseWaitCount++;
        if (releaseWaitCount % 1000 == 0) {
            MENU_LOG_TRACE("TrackMenu: Waiting for initial release, count=%d\n", releaseWaitCount);
        }
    }
    MENU_LOG_TRACE("TrackMenu: Initial button release detected after %d iterations\n", releaseWaitCount);

    /* Add a small debounce delay after release */
    {
        volatile int i;
        for (i = 0; i < 10000; i++);  /* Debounce delay */
    }
    serial_puts("TrackMenu: Debounce complete, starting tracking\n");

    /* Now track until button is pressed again (user selects or cancels) */
    Boolean tracking = true;
    int updateCount = 0;
    int buttonCheckCount = 0;

    while (tracking) {
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

        /* Check if button pressed to end tracking */
        buttonCheckCount++;
        Boolean buttonState = Button();
        if (buttonCheckCount <= 5) {
            MENU_LOG_TRACE("TrackMenu: Button check #%d = %d\n", buttonCheckCount, buttonState);
        }

        if (buttonState) {
            MENU_LOG_TRACE("TrackMenu: Button pressed at check #%d, ending tracking\n", buttonCheckCount);
            tracking = false;
        }

        /* Small delay to prevent CPU hogging (optional) */
        /* In real Classic Mac, WaitNextEvent would provide this */
        {
            volatile int i;
            for (i = 0; i < 100; i++);  /* Much smaller delay with event pumping */
        }

        /* Debug output every 100 updates to avoid spam */
        if (updateCount % 100 == 0) {
            MENU_LOG_TRACE("TrackMenu: Still tracking, update %d\n", updateCount);
        }
    }

    serial_puts("TrackMenu: Mouse released\n");

    /* Get final selection */
    if (g_menuTrackState.highlightedItem > 0) {
        result = ((long)menuID << 16) | g_menuTrackState.highlightedItem;
        serial_puts("TrackMenu: Item selected\n");
    } else {
        serial_puts("TrackMenu: No selection\n");
    }

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
    s_inTrackMenu = false;

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
    short titleLen = 0;
    if (highlightMenuID == 129) { titleText = "File"; titleLen = 4; }
    else if (highlightMenuID == 130) { titleText = "Edit"; titleLen = 4; }
    else if (highlightMenuID == 131) { titleText = "View"; titleLen = 4; }
    else if (highlightMenuID == 132) { titleText = "Label"; titleLen = 5; }
    else if (highlightMenuID == 133) { titleText = "Special"; titleLen = 7; }

    if (highlightMenuID != 128) {  /* Not Apple menu - draw text */
        DrawInvertedText(titleText, titleX + 4, 14, true);
    } else {
        /* For Apple menu, draw inverted Apple icon */
        DrawInvertedAppleIcon(8, 2);
    }

    lastHighlightMenuID = highlightMenuID;
}

