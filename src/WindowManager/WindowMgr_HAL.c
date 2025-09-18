/*
 * Window Manager Hardware Abstraction Layer
 * Bridges classic Mac OS Window Manager to modern platforms
 * Integrates with Memory, Resource, and File Managers
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#ifdef __APPLE__
#include <CoreGraphics/CoreGraphics.h>
#include <ApplicationServices/ApplicationServices.h>
#endif

#ifdef __linux__
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#endif

#include "WindowManager/window_manager.h"
#include "WindowManager/WindowTypes.h"
#include "MemoryMgr/memory_manager.h"
#include "ResourceMgr/resource_manager.h"
#include "QuickDraw/QuickDraw.h"

/* Platform-specific window context */
typedef struct PlatformWindow {
#ifdef __APPLE__
    CGWindowID      windowID;
    CGContextRef    context;
#elif defined(__linux__)
    Display*        display;
    Window          window;
    GC              gc;
#else
    void*           nativeHandle;
#endif
    uint32_t        lastUpdateTime;
} PlatformWindow;

/* Window Manager globals */
static struct {
    WindowPeek      windowList;        /* Head of window list */
    WindowPeek      activeWindow;      /* Currently active window */
    WindowPeek      frontWindow;       /* Front-most window */
    GrafPtr         currentPort;       /* Current graphics port */
    RgnHandle       grayRgn;           /* Desktop region */
    RgnHandle       menuBarRgn;        /* Menu bar region */
    pthread_mutex_t wmLock;            /* Thread safety */
    bool            initialized;

    /* Platform-specific */
#ifdef __linux__
    Display*        display;
    int             screen;
    Window          rootWindow;
#endif
} gWindowMgr = {0};

/* Forward declarations */
static OSErr WindowMgr_HAL_CreatePlatformWindow(WindowPeek window);
static void WindowMgr_HAL_DestroyPlatformWindow(WindowPeek window);
static void WindowMgr_HAL_UpdateWindowRegion(WindowPeek window);

/* Initialize Window Manager HAL */
OSErr WindowMgr_HAL_Initialize(void)
{
    if (gWindowMgr.initialized) {
        return noErr;
    }

    /* Initialize mutex */
    pthread_mutex_init(&gWindowMgr.wmLock, NULL);

    /* Create desktop regions using Memory Manager */
    gWindowMgr.grayRgn = NewRgn();
    gWindowMgr.menuBarRgn = NewRgn();

    if (!gWindowMgr.grayRgn || !gWindowMgr.menuBarRgn) {
        return memFullErr;
    }

    /* Set up desktop region */
    Rect screenBounds = {0, 0, 768, 1024};  /* Classic Mac screen size */
    RectRgn(gWindowMgr.grayRgn, &screenBounds);

    /* Set up menu bar region */
    Rect menuBarBounds = {0, 0, 20, 1024};
    RectRgn(gWindowMgr.menuBarRgn, &menuBarBounds);

#ifdef __linux__
    /* Initialize X11 */
    gWindowMgr.display = XOpenDisplay(NULL);
    if (!gWindowMgr.display) {
        return -1;
    }
    gWindowMgr.screen = DefaultScreen(gWindowMgr.display);
    gWindowMgr.rootWindow = RootWindow(gWindowMgr.display, gWindowMgr.screen);
#endif

    gWindowMgr.initialized = true;
    return noErr;
}

/* Terminate Window Manager HAL */
void WindowMgr_HAL_Terminate(void)
{
    if (!gWindowMgr.initialized) {
        return;
    }

    /* Close all windows */
    while (gWindowMgr.windowList) {
        CloseWindow((WindowPtr)gWindowMgr.windowList);
    }

    /* Dispose regions using Memory Manager */
    if (gWindowMgr.grayRgn) {
        DisposeRgn(gWindowMgr.grayRgn);
    }
    if (gWindowMgr.menuBarRgn) {
        DisposeRgn(gWindowMgr.menuBarRgn);
    }

#ifdef __linux__
    if (gWindowMgr.display) {
        XCloseDisplay(gWindowMgr.display);
    }
#endif

    pthread_mutex_destroy(&gWindowMgr.wmLock);
    gWindowMgr.initialized = false;
}

/* Create new window - integrates with Resource Manager */
WindowPtr WindowMgr_HAL_NewWindow(void* storage, const Rect* bounds,
                                  ConstStr255Param title, Boolean visible,
                                  int16_t procID, WindowPtr behind,
                                  Boolean goAwayFlag, int32_t refCon)
{
    WindowPeek window;

    pthread_mutex_lock(&gWindowMgr.wmLock);

    /* Allocate window record using Memory Manager */
    if (storage) {
        window = (WindowPeek)storage;
    } else {
        window = (WindowPeek)NewPtr(sizeof(WindowRecord));
        if (!window) {
            pthread_mutex_unlock(&gWindowMgr.wmLock);
            return NULL;
        }
    }

    /* Initialize window record */
    memset(window, 0, sizeof(WindowRecord));

    /* Set up graphics port */
    OpenPort(&window->port);

    /* Copy window bounds */
    window->port.portRect = *bounds;
    window->strucRgn = NewRgn();
    window->contRgn = NewRgn();
    window->updateRgn = NewRgn();
    window->clipRgn = NewRgn();
    window->visRgn = NewRgn();

    if (!window->strucRgn || !window->contRgn || !window->updateRgn ||
        !window->clipRgn || !window->visRgn) {
        pthread_mutex_unlock(&gWindowMgr.wmLock);
        return NULL;
    }

    /* Set window regions */
    RectRgn(window->strucRgn, bounds);
    RectRgn(window->contRgn, bounds);
    SetEmptyRgn(window->updateRgn);

    /* Set window attributes */
    window->windowKind = userKind;
    window->visible = visible;
    window->hilited = false;
    window->goAwayFlag = goAwayFlag;
    window->spareFlag = false;
    window->refCon = refCon;
    window->windowDefProc = (Handle)procID;  /* Will load from resources */

    /* Copy title using Memory Manager */
    if (title && title[0] > 0) {
        window->titleHandle = NewHandle(title[0] + 1);
        if (window->titleHandle) {
            BlockMoveData(title, *window->titleHandle, title[0] + 1);
        }
    }

    /* Create platform window */
    WindowMgr_HAL_CreatePlatformWindow(window);

    /* Add to window list */
    if (behind == (WindowPtr)-1L || !behind) {
        /* Add at front */
        window->nextWindow = gWindowMgr.windowList;
        gWindowMgr.windowList = window;
        if (!gWindowMgr.frontWindow) {
            gWindowMgr.frontWindow = window;
        }
    } else {
        /* Insert after specified window */
        WindowPeek w = (WindowPeek)behind;
        window->nextWindow = w->nextWindow;
        w->nextWindow = window;
    }

    /* Make visible if requested */
    if (visible) {
        ShowWindow((WindowPtr)window);
    }

    pthread_mutex_unlock(&gWindowMgr.wmLock);
    return (WindowPtr)window;
}

/* Get window from resource - integrates with Resource Manager */
WindowPtr WindowMgr_HAL_GetNewWindow(int16_t resourceID, void* storage,
                                     WindowPtr behind)
{
    Handle windResource;
    WindowPtr window = NULL;

    /* Load WIND resource using Resource Manager */
    windResource = GetResource('WIND', resourceID);
    if (!windResource || !*windResource) {
        return NULL;
    }

    /* Parse WIND resource structure */
    struct WINDResource {
        Rect    boundsRect;
        int16_t procID;
        int16_t visible;
        int16_t goAwayFlag;
        int32_t refCon;
        Str255  title;
    } *windData;

    HLock(windResource);
    windData = (struct WINDResource*)*windResource;

    /* Create window from resource data */
    window = WindowMgr_HAL_NewWindow(storage,
                                     &windData->boundsRect,
                                     windData->title,
                                     windData->visible != 0,
                                     windData->procID,
                                     behind,
                                     windData->goAwayFlag != 0,
                                     windData->refCon);

    HUnlock(windResource);
    ReleaseResource(windResource);

    return window;
}

/* Close window */
void WindowMgr_HAL_CloseWindow(WindowPtr window)
{
    WindowPeek w = (WindowPeek)window;
    WindowPeek prev;

    if (!w) return;

    pthread_mutex_lock(&gWindowMgr.wmLock);

    /* Hide window first */
    if (w->visible) {
        HideWindow(window);
    }

    /* Remove from window list */
    if (gWindowMgr.windowList == w) {
        gWindowMgr.windowList = w->nextWindow;
    } else {
        for (prev = gWindowMgr.windowList; prev; prev = prev->nextWindow) {
            if (prev->nextWindow == w) {
                prev->nextWindow = w->nextWindow;
                break;
            }
        }
    }

    /* Update active/front window pointers */
    if (gWindowMgr.activeWindow == w) {
        gWindowMgr.activeWindow = NULL;
    }
    if (gWindowMgr.frontWindow == w) {
        gWindowMgr.frontWindow = gWindowMgr.windowList;
    }

    /* Destroy platform window */
    WindowMgr_HAL_DestroyPlatformWindow(w);

    /* Dispose window regions using Memory Manager */
    if (w->strucRgn) DisposeRgn(w->strucRgn);
    if (w->contRgn) DisposeRgn(w->contRgn);
    if (w->updateRgn) DisposeRgn(w->updateRgn);
    if (w->clipRgn) DisposeRgn(w->clipRgn);
    if (w->visRgn) DisposeRgn(w->visRgn);

    /* Dispose title handle */
    if (w->titleHandle) {
        DisposeHandle(w->titleHandle);
    }

    /* Close graphics port */
    ClosePort(&w->port);

    /* Free window memory if we allocated it */
    if (w->windowKind == userKind) {
        DisposePtr((Ptr)w);
    }

    pthread_mutex_unlock(&gWindowMgr.wmLock);
}

/* Dispose window */
void WindowMgr_HAL_DisposeWindow(WindowPtr window)
{
    /* Same as CloseWindow for user windows */
    WindowMgr_HAL_CloseWindow(window);
}

/* Window visibility */
void WindowMgr_HAL_ShowWindow(WindowPtr window)
{
    WindowPeek w = (WindowPeek)window;

    if (!w || w->visible) return;

    pthread_mutex_lock(&gWindowMgr.wmLock);

    w->visible = true;
    w->hilited = (w == gWindowMgr.frontWindow);

    /* Calculate visible region */
    WindowMgr_HAL_UpdateWindowRegion(w);

    /* Invalidate window for redraw */
    InvalRgn(w->contRgn);

#ifdef __linux__
    if (w->platformWindow) {
        PlatformWindow* pw = (PlatformWindow*)w->platformWindow;
        XMapWindow(gWindowMgr.display, pw->window);
        XFlush(gWindowMgr.display);
    }
#endif

    pthread_mutex_unlock(&gWindowMgr.wmLock);
}

void WindowMgr_HAL_HideWindow(WindowPtr window)
{
    WindowPeek w = (WindowPeek)window;

    if (!w || !w->visible) return;

    pthread_mutex_lock(&gWindowMgr.wmLock);

    w->visible = false;
    w->hilited = false;

#ifdef __linux__
    if (w->platformWindow) {
        PlatformWindow* pw = (PlatformWindow*)w->platformWindow;
        XUnmapWindow(gWindowMgr.display, pw->window);
        XFlush(gWindowMgr.display);
    }
#endif

    pthread_mutex_unlock(&gWindowMgr.wmLock);
}

/* Window ordering */
void WindowMgr_HAL_SelectWindow(WindowPtr window)
{
    WindowPeek w = (WindowPeek)window;
    WindowPeek prev;

    if (!w || w == gWindowMgr.frontWindow) return;

    pthread_mutex_lock(&gWindowMgr.wmLock);

    /* Remove from current position */
    if (gWindowMgr.windowList == w) {
        gWindowMgr.windowList = w->nextWindow;
    } else {
        for (prev = gWindowMgr.windowList; prev; prev = prev->nextWindow) {
            if (prev->nextWindow == w) {
                prev->nextWindow = w->nextWindow;
                break;
            }
        }
    }

    /* Add at front */
    w->nextWindow = gWindowMgr.windowList;
    gWindowMgr.windowList = w;

    /* Update front window */
    if (gWindowMgr.frontWindow) {
        gWindowMgr.frontWindow->hilited = false;
    }
    gWindowMgr.frontWindow = w;
    w->hilited = true;

#ifdef __linux__
    if (w->platformWindow) {
        PlatformWindow* pw = (PlatformWindow*)w->platformWindow;
        XRaiseWindow(gWindowMgr.display, pw->window);
        XFlush(gWindowMgr.display);
    }
#endif

    pthread_mutex_unlock(&gWindowMgr.wmLock);
}

void WindowMgr_HAL_BringToFront(WindowPtr window)
{
    WindowMgr_HAL_SelectWindow(window);
}

void WindowMgr_HAL_SendBehind(WindowPtr window, WindowPtr behindWindow)
{
    WindowPeek w = (WindowPeek)window;
    WindowPeek behind = (WindowPeek)behindWindow;
    WindowPeek prev;

    if (!w) return;

    pthread_mutex_lock(&gWindowMgr.wmLock);

    /* Remove from current position */
    if (gWindowMgr.windowList == w) {
        gWindowMgr.windowList = w->nextWindow;
    } else {
        for (prev = gWindowMgr.windowList; prev; prev = prev->nextWindow) {
            if (prev->nextWindow == w) {
                prev->nextWindow = w->nextWindow;
                break;
            }
        }
    }

    /* Insert after target window */
    if (behind) {
        w->nextWindow = behind->nextWindow;
        behind->nextWindow = w;
    } else {
        /* Send to back */
        WindowPeek last = gWindowMgr.windowList;
        while (last && last->nextWindow) {
            last = last->nextWindow;
        }
        if (last) {
            last->nextWindow = w;
        } else {
            gWindowMgr.windowList = w;
        }
        w->nextWindow = NULL;
    }

    pthread_mutex_unlock(&gWindowMgr.wmLock);
}

/* Window movement and sizing */
void WindowMgr_HAL_MoveWindow(WindowPtr window, int16_t h, int16_t v,
                              Boolean front)
{
    WindowPeek w = (WindowPeek)window;
    int16_t dh, dv;

    if (!w) return;

    pthread_mutex_lock(&gWindowMgr.wmLock);

    /* Calculate offset */
    dh = h - w->port.portRect.left;
    dv = v - w->port.portRect.top;

    /* Move window bounds */
    OffsetRect(&w->port.portRect, dh, dv);

    /* Move window regions */
    OffsetRgn(w->strucRgn, dh, dv);
    OffsetRgn(w->contRgn, dh, dv);

#ifdef __linux__
    if (w->platformWindow) {
        PlatformWindow* pw = (PlatformWindow*)w->platformWindow;
        XMoveWindow(gWindowMgr.display, pw->window, h, v);
        XFlush(gWindowMgr.display);
    }
#endif

    if (front) {
        WindowMgr_HAL_SelectWindow(window);
    }

    pthread_mutex_unlock(&gWindowMgr.wmLock);
}

void WindowMgr_HAL_SizeWindow(WindowPtr window, int16_t w, int16_t h,
                              Boolean update)
{
    WindowPeek win = (WindowPeek)window;

    if (!win) return;

    pthread_mutex_lock(&gWindowMgr.wmLock);

    /* Update window bounds */
    win->port.portRect.right = win->port.portRect.left + w;
    win->port.portRect.bottom = win->port.portRect.top + h;

    /* Update window regions */
    RectRgn(win->strucRgn, &win->port.portRect);
    RectRgn(win->contRgn, &win->port.portRect);

#ifdef __linux__
    if (win->platformWindow) {
        PlatformWindow* pw = (PlatformWindow*)win->platformWindow;
        XResizeWindow(gWindowMgr.display, pw->window, w, h);
        XFlush(gWindowMgr.display);
    }
#endif

    if (update) {
        InvalRect(&win->port.portRect);
    }

    pthread_mutex_unlock(&gWindowMgr.wmLock);
}

/* Window updating */
void WindowMgr_HAL_BeginUpdate(WindowPtr window)
{
    WindowPeek w = (WindowPeek)window;

    if (!w) return;

    /* Save current port */
    GetPort(&gWindowMgr.currentPort);
    SetPort(&w->port);

    /* Set clip region to update region */
    CopyRgn(w->updateRgn, w->port.clipRgn);
}

void WindowMgr_HAL_EndUpdate(WindowPtr window)
{
    WindowPeek w = (WindowPeek)window;

    if (!w) return;

    /* Clear update region */
    SetEmptyRgn(w->updateRgn);

    /* Restore previous port */
    if (gWindowMgr.currentPort) {
        SetPort(gWindowMgr.currentPort);
    }
}

void WindowMgr_HAL_InvalRect(const Rect* r)
{
    GrafPtr port;
    WindowPeek window;

    GetPort(&port);
    window = (WindowPeek)port;

    if (window && window->updateRgn) {
        RgnHandle tempRgn = NewRgn();
        if (tempRgn) {
            RectRgn(tempRgn, r);
            UnionRgn(window->updateRgn, tempRgn, window->updateRgn);
            DisposeRgn(tempRgn);
        }
    }
}

void WindowMgr_HAL_InvalRgn(RgnHandle rgn)
{
    GrafPtr port;
    WindowPeek window;

    GetPort(&port);
    window = (WindowPeek)port;

    if (window && window->updateRgn) {
        UnionRgn(window->updateRgn, rgn, window->updateRgn);
    }
}

void WindowMgr_HAL_ValidRect(const Rect* r)
{
    GrafPtr port;
    WindowPeek window;

    GetPort(&port);
    window = (WindowPeek)port;

    if (window && window->updateRgn) {
        RgnHandle tempRgn = NewRgn();
        if (tempRgn) {
            RectRgn(tempRgn, r);
            DiffRgn(window->updateRgn, tempRgn, window->updateRgn);
            DisposeRgn(tempRgn);
        }
    }
}

void WindowMgr_HAL_ValidRgn(RgnHandle rgn)
{
    GrafPtr port;
    WindowPeek window;

    GetPort(&port);
    window = (WindowPeek)port;

    if (window && window->updateRgn) {
        DiffRgn(window->updateRgn, rgn, window->updateRgn);
    }
}

/* Window Manager queries */
WindowPtr WindowMgr_HAL_FrontWindow(void)
{
    return (WindowPtr)gWindowMgr.frontWindow;
}

WindowPtr WindowMgr_HAL_FindWindow(Point pt, int16_t* partCode)
{
    WindowPeek w;

    pthread_mutex_lock(&gWindowMgr.wmLock);

    /* Search from front to back */
    for (w = gWindowMgr.windowList; w; w = w->nextWindow) {
        if (!w->visible) continue;

        /* Check if point is in window */
        if (PtInRgn(pt, w->strucRgn)) {
            if (partCode) {
                /* Determine which part of window */
                if (PtInRgn(pt, w->contRgn)) {
                    *partCode = inContent;
                } else {
                    /* Check for title bar, close box, etc. */
                    Rect titleBar = w->port.portRect;
                    titleBar.bottom = titleBar.top;
                    titleBar.top -= 20;  /* Standard title bar height */

                    if (PtInRect(pt, &titleBar)) {
                        if (w->goAwayFlag && pt.h < titleBar.left + 20) {
                            *partCode = inGoAway;
                        } else {
                            *partCode = inDrag;
                        }
                    } else {
                        *partCode = inGrow;  /* Assume grow box for now */
                    }
                }
            }
            pthread_mutex_unlock(&gWindowMgr.wmLock);
            return (WindowPtr)w;
        }
    }

    if (partCode) {
        *partCode = inDesk;
    }

    pthread_mutex_unlock(&gWindowMgr.wmLock);
    return NULL;
}

/* Platform-specific window creation */
static OSErr WindowMgr_HAL_CreatePlatformWindow(WindowPeek window)
{
    PlatformWindow* pw = (PlatformWindow*)NewPtr(sizeof(PlatformWindow));
    if (!pw) {
        return memFullErr;
    }

    window->platformWindow = pw;

#ifdef __linux__
    if (gWindowMgr.display) {
        XSetWindowAttributes attrs;
        attrs.background_pixel = WhitePixel(gWindowMgr.display, gWindowMgr.screen);
        attrs.border_pixel = BlackPixel(gWindowMgr.display, gWindowMgr.screen);
        attrs.event_mask = ExposureMask | KeyPressMask | ButtonPressMask |
                           StructureNotifyMask;

        pw->display = gWindowMgr.display;
        pw->window = XCreateWindow(gWindowMgr.display, gWindowMgr.rootWindow,
                                  window->port.portRect.left,
                                  window->port.portRect.top,
                                  window->port.portRect.right - window->port.portRect.left,
                                  window->port.portRect.bottom - window->port.portRect.top,
                                  1, CopyFromParent, InputOutput, CopyFromParent,
                                  CWBackPixel | CWBorderPixel | CWEventMask, &attrs);

        pw->gc = XCreateGC(gWindowMgr.display, pw->window, 0, NULL);

        /* Set window title if present */
        if (window->titleHandle && *window->titleHandle) {
            char title[256];
            uint8_t* pascalStr = (uint8_t*)*window->titleHandle;
            memcpy(title, pascalStr + 1, pascalStr[0]);
            title[pascalStr[0]] = '\0';
            XStoreName(gWindowMgr.display, pw->window, title);
        }
    }
#endif

    return noErr;
}

/* Platform-specific window destruction */
static void WindowMgr_HAL_DestroyPlatformWindow(WindowPeek window)
{
    if (!window->platformWindow) return;

    PlatformWindow* pw = (PlatformWindow*)window->platformWindow;

#ifdef __linux__
    if (pw->display) {
        if (pw->gc) {
            XFreeGC(pw->display, pw->gc);
        }
        if (pw->window) {
            XDestroyWindow(pw->display, pw->window);
        }
        XFlush(pw->display);
    }
#endif

    DisposePtr((Ptr)pw);
    window->platformWindow = NULL;
}

/* Update window visible region */
static void WindowMgr_HAL_UpdateWindowRegion(WindowPeek window)
{
    WindowPeek w;

    /* Start with content region */
    CopyRgn(window->contRgn, window->visRgn);

    /* Subtract regions of windows in front */
    for (w = gWindowMgr.windowList; w && w != window; w = w->nextWindow) {
        if (w->visible) {
            DiffRgn(window->visRgn, w->strucRgn, window->visRgn);
        }
    }

    /* Intersect with screen bounds */
    SectRgn(window->visRgn, gWindowMgr.grayRgn, window->visRgn);
}

/* Window Manager event handling */
Boolean WindowMgr_HAL_IsDialogEvent(const EventRecord* event)
{
    /* Check if event belongs to a dialog */
    WindowPtr window;
    int16_t partCode;

    if (event->what == mouseDown) {
        window = FindWindow(event->where, &partCode);
        if (window) {
            WindowPeek w = (WindowPeek)window;
            return (w->windowKind < 0);  /* Dialog windows have negative kinds */
        }
    }

    return false;
}

Boolean WindowMgr_HAL_DialogSelect(const EventRecord* event, DialogPtr* dialog,
                                   int16_t* itemHit)
{
    /* Handle dialog events */
    /* This would integrate with Dialog Manager */
    return false;
}

/* Desktop management */
void WindowMgr_HAL_PaintOne(WindowPeek window, RgnHandle clobberedRgn)
{
    if (!window || !window->visible) return;

    /* Add clobbered region to update region */
    if (clobberedRgn) {
        UnionRgn(window->updateRgn, clobberedRgn, window->updateRgn);
    }

    /* Generate update event for window */
    /* This would post an event to the event queue */
}

void WindowMgr_HAL_PaintBehind(WindowPeek startWindow, RgnHandle clobberedRgn)
{
    WindowPeek w;

    for (w = startWindow; w; w = w->nextWindow) {
        WindowMgr_HAL_PaintOne(w, clobberedRgn);
    }
}

void WindowMgr_HAL_CalcVis(WindowPeek window)
{
    WindowMgr_HAL_UpdateWindowRegion(window);
}

void WindowMgr_HAL_CalcVisBehind(WindowPeek startWindow, RgnHandle clobberedRgn)
{
    WindowPeek w;

    for (w = startWindow; w; w = w->nextWindow) {
        WindowMgr_HAL_CalcVis(w);
    }
}

/* Initialize on first use */
__attribute__((constructor))
static void WindowMgr_HAL_Init(void)
{
    WindowMgr_HAL_Initialize();
}

/* Cleanup on exit */
__attribute__((destructor))
static void WindowMgr_HAL_Cleanup(void)
{
    WindowMgr_HAL_Terminate();
}