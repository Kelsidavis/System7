/*
 * MenuTrack.c - Basic dropdown rendering and tracking
 *
 * Draws menu item list under the title and lets user select with mouse.
 */

#include "SystemTypes.h"
#include "MenuManager/MenuManager.h"
#include "MenuManager/MenuTypes.h"
#include "QuickDraw/QuickDraw.h"
#include "EventManager/EventTypes.h"  /* For mouse masks */

/* External functions */
extern int serial_printf(const char* format, ...);
extern void serial_puts(const char* str);
extern QDGlobals qd;
extern void DrawDesktop(void);
extern void DrawText(const void* textBuf, short firstByte, short byteCount);
extern void DrawVolumeIcon(void);
extern Boolean Button(void);          /* Check if mouse button is pressed */
extern void PollPS2Input(void);       /* Poll PS/2 devices for input */
extern void GetMouse(Point* mouseLoc);
extern void MoveTo(short h, short v);
extern void DrawMenuBar(void);        /* Redraw the menu bar */

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

    serial_printf("Drawing menu item: %s at (%d,%d)\n", text, x, y);
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
static void DrawMenu(MenuHandle theMenu, short left, short top, short itemCount, short menuWidth, short lineHeight) {
    DrawMenuRect(left, top, left + menuWidth, top + itemCount * lineHeight + 4, 0xFFFFFFFF);

    /* Border */
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
}

/* Track menu selection */
long TrackMenu(short menuID, Point startPt) {
    serial_puts("TrackMenu: ENTER\n");

    if (!framebuffer) {
        serial_puts("TrackMenu: ERROR - No framebuffer!\n");
        return 0;
    }

    /* Get the actual menu handle for this menu ID */
    MenuHandle theMenu = GetMenuHandle(menuID);
    if (!theMenu) {
        serial_printf("TrackMenu: Menu %d not found\n", menuID);
        return 0;
    }

    /* Get actual item count from menu */
    short itemCount = CountMenuItems(theMenu);
    if (itemCount == 0) itemCount = 5;  /* Fallback */

    short left = startPt.h;
    short top = 20;       /* below menubar */
    short menuWidth = 120;
    short lineHeight = 16;

    DrawMenu(theMenu, left, top, itemCount, menuWidth, lineHeight);
    serial_puts("TrackMenu: Dropdown drawn\n");

    /* Track mouse while button is held */
    Point currentPt;
    short lastHighlight = -1;

    while (Button()) {
        PollPS2Input();
        GetMouse(&currentPt);

        /* Check if mouse is over a menu item */
        if (currentPt.h >= left && currentPt.h < left + menuWidth &&
            currentPt.v >= top && currentPt.v < top + itemCount * lineHeight) {

            short highlightIndex = (currentPt.v - top) / lineHeight + 1;

            /* Highlight item under mouse if changed */
            if (highlightIndex != lastHighlight && highlightIndex >= 1 && highlightIndex <= itemCount) {
                /* Clear previous highlight */
                if (lastHighlight > 0) {
                    short oldTop = top + 2 + (lastHighlight - 1) * lineHeight;
                    DrawMenuRect(left + 2, oldTop, left + menuWidth - 2, oldTop + lineHeight - 1, 0xFFFFFFFF);

                    char itemText[64];
                    GetItemText(theMenu, lastHighlight, itemText);
                    DrawMenuItemText(itemText, left + 4, oldTop + 12);
                }

                /* Draw new highlight */
                short itemTop = top + 2 + (highlightIndex - 1) * lineHeight;
                DrawMenuRect(left + 2, itemTop, left + menuWidth - 2, itemTop + lineHeight - 1, 0xFF000080);  /* Dark blue */

                char itemText[64];
                GetItemText(theMenu, highlightIndex, itemText);
                /* Draw highlighted text in white */
                DrawMenuItemText(itemText, left + 4, itemTop + 12);

                lastHighlight = highlightIndex;
            }
        } else {
            /* Mouse outside menu - clear highlight */
            if (lastHighlight > 0) {
                short oldTop = top + 2 + (lastHighlight - 1) * lineHeight;
                DrawMenuRect(left + 2, oldTop, left + menuWidth - 2, oldTop + lineHeight - 1, 0xFFFFFFFF);

                char itemText[64];
                GetItemText(theMenu, lastHighlight, itemText);
                DrawMenuItemText(itemText, left + 4, oldTop + 12);

                lastHighlight = -1;
            }
        }

        /* Small delay to reduce CPU usage */
        for (volatile int i = 0; i < 1000; i++) {}
    }

    /* Release point */
    Point releasePt;
    GetMouse(&releasePt);
    serial_printf("TrackMenu: Release at (h=%d,v=%d)\n", releasePt.h, releasePt.v);

    /* Hit-test */
    short index = (releasePt.v - top) / lineHeight + 1;
    short itemID = 0;
    if (index >= 1 && index <= itemCount &&
        releasePt.h >= left && releasePt.h < left + menuWidth &&
        releasePt.v >= top && releasePt.v < top + itemCount * lineHeight) {
        itemID = index;
        serial_printf("TrackMenu: Selected item %d\n", itemID);
    } else {
        serial_puts("TrackMenu: Click outside menu\n");
    }

    /* Redraw entire desktop with proper ppat pattern */
    DrawDesktop();

    /* Redraw the menu bar since dropdown might have overlapped it */
    DrawMenuBar();

    /* Always redraw the volume icon since menus might overlap it */
    DrawVolumeIcon();

    serial_puts("TrackMenu: Menu cleared\n");

    if (itemID) {
        /* Return in classic Mac format: high word = menuID, low word = item */
        long result = ((long)menuID << 16) | (itemID & 0xFFFF);
        serial_printf("TrackMenu: Returning 0x%08lX\n", result);
        return result;
    }
    return 0;
}