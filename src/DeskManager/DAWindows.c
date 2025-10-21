#include "MemoryMgr/MemoryManager.h"
// #include "CompatibilityFix.h" // Removed
#include <stdlib.h>
#include <string.h>
/*
 * DAWindows.c - Desk Accessory Window Management
 *
 * Provides window management functionality for desk accessories including
 * window creation, event handling, drawing, and interaction with the
 * Mac OS window system.
 *
 * Derived from ROM desk accessory integration
 */

#include "SystemTypes.h"
#include "System71StdLib.h"

#include "DeskManager/DeskAccessory.h"
#include "DeskManager/DeskManager.h"


/* Window Types */
#define WINDOW_TYPE_DOCUMENT    0   /* Document window */
#define WINDOW_TYPE_MODAL       1   /* Modal dialog */
#define WINDOW_TYPE_MODELESS    2   /* Modeless dialog */
#define WINDOW_TYPE_ALERT       3   /* Alert dialog */

/* Window Attributes */
#define WINDOW_ATTR_CLOSEBOX    0x0001  /* Has close box */
#define WINDOW_ATTR_TITLE       0x0002  /* Has title bar */
#define WINDOW_ATTR_RESIZE      0x0004  /* Resizable */
#define WINDOW_ATTR_ZOOM        0x0008  /* Has zoom box */
#define WINDOW_ATTR_COLLAPSE    0x0010  /* Can collapse */
#define WINDOW_ATTR_FLOAT       0x0020  /* Floating window */

/* Window State */
typedef struct DAWindow {
    WindowRecord    *platformWindow;    /* Platform window handle */
    DeskAccessory   *owner;            /* Owning desk accessory */
    Rect            bounds;            /* Window bounds */
    Rect            contentRect;       /* Content rectangle */
    char            title[256];        /* Window title */
    SInt16         windowType;        /* Window type */
    UInt16        attributes;        /* Window attributes */
    Boolean            visible;           /* Window visibility */
    Boolean            active;            /* Window active state */
    Boolean            needsUpdate;       /* Needs redraw */
    void            *userData;         /* User data */

    /* Event handling */
    Boolean            trackingMouse;     /* Tracking mouse */
    Point           lastMousePos;      /* Last mouse position */

    struct DAWindow *next;             /* Next window in list */
} DAWindow;

/* Global window list */
static DAWindow *g_windowList = NULL;
static int g_windowCount = 0;

/* Internal Function Prototypes */
static DAWindow *DAWindow_Allocate(void);
static void DAWindow_Free(DAWindow *window);
static DAWindow *DAWindow_FindByPlatformWindow(WindowRecord *platformWindow);
static DAWindow *DAWindow_FindByDA(DeskAccessory *da);
static void DAWindow_AddToList(DAWindow *window);
static void DAWindow_RemoveFromList(DAWindow *window);
static int DAWindow_CreatePlatformWindow(DAWindow *window);
static void DAWindow_DestroyPlatformWindow(DAWindow *window);
static void DAWindow_UpdatePlatformWindow(DAWindow *window);
static Boolean DAWindow_PointInWindow(DAWindow *window, Point point);
static void DAWindow_InvalidateRect(DAWindow *window, const Rect *rect);

/*
 * Create DA window
 */
int DA_CreateWindow(DeskAccessory *da, const DAWindowAttr *attr)
{
    extern void serial_printf(const char *fmt, ...);
    serial_printf("[DA_CREATEWINDOW] Entry: da=%p, title='%s'\n", da, attr ? attr->title : "NULL");

    if (!da || !attr) {
        return DESK_ERR_INVALID_PARAM;
    }

    /* Check if DA already has a window */
    if (da->window) {
        serial_printf("[DA_CREATEWINDOW] Already open, returning DESK_ERR_ALREADY_OPEN\n");
        return DESK_ERR_ALREADY_OPEN;
    }

    /* Allocate window structure */
    serial_printf("[DA_CREATEWINDOW] Calling DAWindow_Allocate\n");
    DAWindow *window = DAWindow_Allocate();
    if (!window) {
        return DESK_ERR_NO_MEMORY;
    }

    /* Initialize window properties */
    window->owner = da;
    window->bounds = attr->bounds;
    window->contentRect = attr->bounds;
    strncpy(window->title, attr->title, sizeof(window->title) - 1);
    window->title[sizeof(window->title) - 1] = '\0';
    window->windowType = WINDOW_TYPE_DOCUMENT;
    window->visible = attr->visible;
    window->active = false;
    window->needsUpdate = true;

    /* Set window attributes */
    window->attributes = 0;
    if (attr->hasGoAway) {
        window->attributes |= WINDOW_ATTR_CLOSEBOX;
    }
    window->attributes |= WINDOW_ATTR_TITLE;

    /* Adjust content rectangle for title bar and borders */
    (window)->bounds.top += 20;  /* Title bar height */
    (window)->bounds.left += 1;  /* Left border */
    (window)->bounds.right -= 1; /* Right border */
    (window)->bounds.bottom -= 1; /* Bottom border */

    /* Create platform window */
    int result = DAWindow_CreatePlatformWindow(window);
    if (result != 0) {
        DAWindow_Free(window);
        return result;
    }

    /* Add to window list */
    DAWindow_AddToList(window);

    /* Link to DA */
    da->window = (WindowRecord *)window;

    /* Show window if visible */
    if (window->visible) {
        DAWindow_Show(da);
    }

    return DESK_ERR_NONE;
}

/*
 * Destroy DA window
 */
void DA_DestroyWindow(DeskAccessory *da)
{
    if (!da || !da->window) {
        return;
    }

    DAWindow *window = (DAWindow *)da->window;

    /* Hide window */
    DAWindow_Hide(da);

    /* Destroy platform window */
    DAWindow_DestroyPlatformWindow(window);

    /* Remove from window list */
    DAWindow_RemoveFromList(window);

    /* Unlink from DA */
    da->window = NULL;

    /* Free window structure */
    DAWindow_Free(window);
}

/*
 * Show DA window
 */
void DAWindow_Show(DeskAccessory *da)
{
    if (!da || !da->window) {
        return;
    }

    DAWindow *window = (DAWindow *)da->window;
    if (!window->visible) {
        window->visible = true;
        window->needsUpdate = true;
        DAWindow_UpdatePlatformWindow(window);
    }
}

/*
 * Hide DA window
 */
void DAWindow_Hide(DeskAccessory *da)
{
    if (!da || !da->window) {
        return;
    }

    DAWindow *window = (DAWindow *)da->window;
    if (window->visible) {
        window->visible = false;
        DAWindow_UpdatePlatformWindow(window);
    }
}

/*
 * Move DA window
 */
void DAWindow_Move(DeskAccessory *da, SInt16 h, SInt16 v)
{
    if (!da || !da->window) {
        return;
    }

    DAWindow *window = (DAWindow *)da->window;
    SInt16 deltaH = h - (window)->bounds.left;
    SInt16 deltaV = v - (window)->bounds.top;

    /* Update bounds */
    (window)->bounds.left += deltaH;
    (window)->bounds.top += deltaV;
    (window)->bounds.right += deltaH;
    (window)->bounds.bottom += deltaV;

    /* Update content rect */
    (window)->bounds.left += deltaH;
    (window)->bounds.top += deltaV;
    (window)->bounds.right += deltaH;
    (window)->bounds.bottom += deltaV;

    DAWindow_UpdatePlatformWindow(window);
}

/*
 * Resize DA window
 */
void DAWindow_Resize(DeskAccessory *da, SInt16 width, SInt16 height)
{
    if (!da || !da->window) {
        return;
    }

    DAWindow *window = (DAWindow *)da->window;

    /* Update bounds */
    (window)->bounds.right = (window)->bounds.left + width;
    (window)->bounds.bottom = (window)->bounds.top + height;

    /* Update content rect */
    window->contentRect = window->bounds;
    (window)->bounds.top += 20;  /* Title bar */
    (window)->bounds.left += 1;  /* Border */
    (window)->bounds.right -= 1;
    (window)->bounds.bottom -= 1;

    window->needsUpdate = true;
    DAWindow_UpdatePlatformWindow(window);

    /* Notify DA of resize */
    if (da->update) {
        da->update(da);
    }
}

/*
 * Set window title
 */
void DAWindow_SetTitle(DeskAccessory *da, const char *title)
{
    if (!da || !da->window || !title) {
        return;
    }

    DAWindow *window = (DAWindow *)da->window;
    strncpy(window->title, title, sizeof(window->title) - 1);
    window->title[sizeof(window->title) - 1] = '\0';

    DAWindow_UpdatePlatformWindow(window);
}

/*
 * Get window title
 */
const char *DAWindow_GetTitle(DeskAccessory *da)
{
    if (!da || !da->window) {
        return NULL;
    }

    DAWindow *window = (DAWindow *)da->window;
    return window->title;
}

/*
 * Activate DA window
 */
void DAWindow_Activate(DeskAccessory *da, Boolean active)
{
    if (!da || !da->window) {
        return;
    }

    DAWindow *window = (DAWindow *)da->window;
    if (window->active != active) {
        window->active = active;
        window->needsUpdate = true;
        DAWindow_UpdatePlatformWindow(window);
    }
}

/*
 * Check if window is active
 */
Boolean DAWindow_IsActive(DeskAccessory *da)
{
    if (!da || !da->window) {
        return false;
    }

    DAWindow *window = (DAWindow *)da->window;
    return window->active;
}

/*
 * Invalidate window rectangle
 */
void DAWindow_Invalidate(DeskAccessory *da, const Rect *rect)
{
    if (!da || !da->window) {
        return;
    }

    DAWindow *window = (DAWindow *)da->window;
    if (rect) {
        DAWindow_InvalidateRect(window, rect);
    } else {
        window->needsUpdate = true;
    }
}

/*
 * Update window
 */
void DAWindow_Update(DeskAccessory *da)
{
    if (!da || !da->window) {
        return;
    }

    DAWindow *window = (DAWindow *)da->window;
    if (window->needsUpdate && window->visible) {
        /* Call DA's update routine */
        if (da->update) {
            da->update(da);
        }
        window->needsUpdate = false;
    }
}

/*
 * Handle mouse down in window
 */
int DAWindow_HandleMouseDown(DeskAccessory *da, const EventRecord *event)
{
    if (!da || !da->window || !event) {
        return DESK_ERR_INVALID_PARAM;
    }

    DAWindow *window = (DAWindow *)da->window;
    Point localPoint = event->where;

    /* Convert to local coordinates */
    localPoint.h -= (window)->bounds.left;
    localPoint.v -= (window)->bounds.top;

    /* Check if click is in window */
    if (!DAWindow_PointInWindow(window, event->where)) {
        return DESK_ERR_INVALID_PARAM;
    }

    /* Activate window */
    DAWindow_Activate(da, true);

    /* Route event to DA */
    if (da->event) {
        return da->event(da, event);
    }

    return DESK_ERR_NONE;
}

/*
 * Handle mouse up in window
 */
int DAWindow_HandleMouseUp(DeskAccessory *da, const EventRecord *event)
{
    if (!da || !da->window || !event) {
        return DESK_ERR_INVALID_PARAM;
    }

    DAWindow *window = (DAWindow *)da->window;
    window->trackingMouse = false;

    /* Route event to DA */
    if (da->event) {
        return da->event(da, event);
    }

    return DESK_ERR_NONE;
}

/*
 * Handle key down in window
 */
int DAWindow_HandleKeyDown(DeskAccessory *da, const EventRecord *event)
{
    if (!da || !da->window || !event) {
        return DESK_ERR_INVALID_PARAM;
    }

    /* Route event to DA */
    if (da->event) {
        return da->event(da, event);
    }

    return DESK_ERR_NONE;
}

/*
 * Get window bounds
 */
void DAWindow_GetBounds(DeskAccessory *da, Rect *bounds)
{
    if (!da || !da->window || !bounds) {
        return;
    }

    DAWindow *window = (DAWindow *)da->window;
    *bounds = window->bounds;
}

/*
 * Get content rectangle
 */
void DAWindow_GetContentRect(DeskAccessory *da, Rect *contentRect)
{
    if (!da || !da->window || !contentRect) {
        return;
    }

    DAWindow *window = (DAWindow *)da->window;
    *contentRect = window->contentRect;
}

/*
 * Set window position
 */
void DAWindow_SetPosition(DeskAccessory *da, SInt16 h, SInt16 v)
{
    DAWindow_Move(da, h, v);
}

/*
 * Get window position
 */
void DAWindow_GetPosition(DeskAccessory *da, SInt16 *h, SInt16 *v)
{
    if (!da || !da->window || !h || !v) {
        return;
    }

    DAWindow *window = (DAWindow *)da->window;
    *h = (window)->bounds.left;
    *v = (window)->bounds.top;
}

/*
 * Set window size
 */
void DAWindow_SetSize(DeskAccessory *da, SInt16 width, SInt16 height)
{
    DAWindow_Resize(da, width, height);
}

/*
 * Get window size
 */
void DAWindow_GetSize(DeskAccessory *da, SInt16 *width, SInt16 *height)
{
    if (!da || !da->window || !width || !height) {
        return;
    }

    DAWindow *window = (DAWindow *)da->window;
    *width = (window)->bounds.right - (window)->bounds.left;
    *height = (window)->bounds.bottom - (window)->bounds.top;
}

/* Internal Functions */

/*
 * Allocate window structure
 */
static DAWindow *DAWindow_Allocate(void)
{
    return NewPtrClear(sizeof(DAWindow));
}

/*
 * Free window structure
 */
static void DAWindow_Free(DAWindow *window)
{
    if (window) {
        DisposePtr((Ptr)window);
    }
}

/*
 * Find window by platform window
 */
static DAWindow *DAWindow_FindByPlatformWindow(WindowRecord *platformWindow)
{
    DAWindow *window = g_windowList;
    while (window) {
        if (window->platformWindow == platformWindow) {
            return window;
        }
        window = window->next;
    }
    return NULL;
}

/*
 * Find window by DA
 */
static DAWindow *DAWindow_FindByDA(DeskAccessory *da)
{
    DAWindow *window = g_windowList;
    while (window) {
        if (window->owner == da) {
            return window;
        }
        window = window->next;
    }
    return NULL;
}

/*
 * Add window to list
 */
static void DAWindow_AddToList(DAWindow *window)
{
    if (!window) return;

    window->next = g_windowList;
    g_windowList = window;
    g_windowCount++;
}

/*
 * Remove window from list
 */
static void DAWindow_RemoveFromList(DAWindow *window)
{
    if (!window) return;

    DAWindow *prev = NULL;
    DAWindow *curr = g_windowList;

    while (curr && curr != window) {
        prev = curr;
        curr = curr->next;
    }

    if (curr) {
        if (prev) {
            prev->next = curr->next;
        } else {
            g_windowList = curr->next;
        }
        g_windowCount--;
    }
}

/*
 * Create platform window
 */
static int DAWindow_CreatePlatformWindow(DAWindow *window)
{
    if (!window) {
        return DESK_ERR_INVALID_PARAM;
    }

    /* Allocate WindowRecord properly via Window Manager */
    extern WindowPtr NewWindow(void* wStorage, const Rect* boundsRect,
                               ConstStr255Param title, Boolean visible,
                               short theProc, WindowPtr behind,
                               Boolean goAwayFlag, long refCon);

    /* Convert C string title to Pascal string */
    unsigned char pascalTitle[256];
    if (window->title && window->title[0] != '\0') {
        int len = strlen(window->title);
        if (len > 255) len = 255;
        pascalTitle[0] = (unsigned char)len;
        strncpy((char*)&pascalTitle[1], window->title, len);
    } else {
        pascalTitle[0] = 0;  /* Empty title */
    }

    /* Create window through Window Manager to get proper initialization */
    window->platformWindow = NewWindow(
        NULL,                       /* Allocate storage */
        &window->bounds,            /* Bounds rect */
        (ConstStr255Param)pascalTitle,  /* Title */
        window->visible,            /* Visible flag */
        documentProc,               /* Window proc */
        (WindowPtr)-1,              /* Behind (front) */
        true,                       /* GoAway flag */
        window->owner ? (long)window->owner : 0  /* RefCon */
    );

    if (!window->platformWindow) {
        return DESK_ERR_NO_MEMORY;
    }

    return DESK_ERR_NONE;
}

/*
 * Destroy platform window
 */
static void DAWindow_DestroyPlatformWindow(DAWindow *window)
{
    if (window && window->platformWindow) {
        /* In a real implementation, would destroy platform window */
        DisposePtr((Ptr)window->platformWindow);
        window->platformWindow = NULL;
    }
}

/*
 * Update platform window
 */
static void DAWindow_UpdatePlatformWindow(DAWindow *window)
{
    if (!window || !window->platformWindow) {
        return;
    }

    /* In a real implementation, this would update the platform window
     * with current properties (bounds, title, visibility, etc.) */
}

/*
 * Check if point is in window
 */
static Boolean DAWindow_PointInWindow(DAWindow *window, Point point)
{
    if (!window) {
        return false;
    }

    return (point.h >= (window)->bounds.left &&
            point.h < (window)->bounds.right &&
            point.v >= (window)->bounds.top &&
            point.v < (window)->bounds.bottom);
}

/*
 * Invalidate window rectangle
 */
static void DAWindow_InvalidateRect(DAWindow *window, const Rect *rect)
{
    if (!window) {
        return;
    }

    /* In a real implementation, would invalidate the specified rectangle */
    /* For now, just mark the whole window as needing update */
    window->needsUpdate = true;
}
