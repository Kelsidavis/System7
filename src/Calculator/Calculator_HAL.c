/*
 * Calculator HAL (Hardware Abstraction Layer)
 * Provides platform-specific implementations for calculator operations
 */

#include "calculator.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#ifdef __APPLE__
#include <CoreGraphics/CoreGraphics.h>
#include <ApplicationServices/ApplicationServices.h>
#else
#ifdef HAS_X11
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <cairo/cairo.h>
#include <cairo/cairo-xlib.h>
#endif
#endif

/* Platform-specific window structure */
typedef struct {
    WindowPtr macWindow;
#ifdef __APPLE__
    CGContextRef context;
    CGRect bounds;
#else
#ifdef HAS_X11
    Display *display;
    Window window;
    GC gc;
    cairo_surface_t *surface;
    cairo_t *cairo;
    XFontStruct *font;
#endif
#endif
} HAL_Window;

/* Global HAL state */
static struct {
    HAL_Window *currentWindow;
    Boolean initialized;

#ifdef __APPLE__
    CGColorSpaceRef colorSpace;
    CGColorRef blackColor;
    CGColorRef whiteColor;
    CGColorRef grayColor;
#else
#ifdef HAS_X11
    Display *display;
    int screen;
    unsigned long blackPixel;
    unsigned long whitePixel;
    unsigned long grayPixel;
#endif
#endif
} gCalcHAL = {0};

/* Initialize Calculator HAL */
void Calculator_HAL_Init(void) {
    if (gCalcHAL.initialized) return;

#ifdef __APPLE__
    /* Initialize Core Graphics colors */
    gCalcHAL.colorSpace = CGColorSpaceCreateDeviceRGB();

    CGFloat black[] = {0.0, 0.0, 0.0, 1.0};
    CGFloat white[] = {1.0, 1.0, 1.0, 1.0};
    CGFloat gray[] = {0.7, 0.7, 0.7, 1.0};

    gCalcHAL.blackColor = CGColorCreate(gCalcHAL.colorSpace, black);
    gCalcHAL.whiteColor = CGColorCreate(gCalcHAL.colorSpace, white);
    gCalcHAL.grayColor = CGColorCreate(gCalcHAL.colorSpace, gray);

#else
#ifdef HAS_X11
    /* Initialize X11 */
    gCalcHAL.display = XOpenDisplay(NULL);
    if (!gCalcHAL.display) {
        fprintf(stderr, "Cannot open X display\n");
        return;
    }

    gCalcHAL.screen = DefaultScreen(gCalcHAL.display);
    gCalcHAL.blackPixel = BlackPixel(gCalcHAL.display, gCalcHAL.screen);
    gCalcHAL.whitePixel = WhitePixel(gCalcHAL.display, gCalcHAL.screen);

    /* Create gray pixel */
    Colormap cmap = DefaultColormap(gCalcHAL.display, gCalcHAL.screen);
    XColor gray;
    gray.red = gray.green = gray.blue = 0xB000; /* Light gray */
    gray.flags = DoRed | DoGreen | DoBlue;
    XAllocColor(gCalcHAL.display, cmap, &gray);
    gCalcHAL.grayPixel = gray.pixel;
#endif
#endif

    gCalcHAL.initialized = true;
}

/* Cleanup Calculator HAL */
void Calculator_HAL_Cleanup(void) {
    if (!gCalcHAL.initialized) return;

#ifdef __APPLE__
    if (gCalcHAL.blackColor) CGColorRelease(gCalcHAL.blackColor);
    if (gCalcHAL.whiteColor) CGColorRelease(gCalcHAL.whiteColor);
    if (gCalcHAL.grayColor) CGColorRelease(gCalcHAL.grayColor);
    if (gCalcHAL.colorSpace) CGColorSpaceRelease(gCalcHAL.colorSpace);

#else
#ifdef HAS_X11
    if (gCalcHAL.display) {
        XCloseDisplay(gCalcHAL.display);
    }
#endif
#endif

    gCalcHAL.initialized = false;
}

/* Create calculator window */
WindowPtr Calculator_HAL_CreateWindow(const Rect *bounds, ConstStr255Param title) {
    HAL_Window *halWindow = (HAL_Window*)calloc(1, sizeof(HAL_Window));
    if (!halWindow) return NULL;

    /* Create Mac-style window structure */
    WindowPtr window = (WindowPtr)calloc(1, sizeof(WindowRecord));
    if (!window) {
        free(halWindow);
        return NULL;
    }

    /* Initialize window record */
    window->portRect = *bounds;
    window->portBits.bounds = *bounds;
    window->visible = true;
    window->windowKind = userKind;
    window->refCon = (long)halWindow;

#ifdef __APPLE__
    /* Create Core Graphics context */
    size_t width = bounds->right - bounds->left;
    size_t height = bounds->bottom - bounds->top;

    halWindow->bounds = CGRectMake(0, 0, width, height);

    /* For now, we'll create an offscreen context */
    /* In a real implementation, this would create an actual window */
    CGContextRef context = CGBitmapContextCreate(
        NULL, width, height, 8,
        width * 4,
        gCalcHAL.colorSpace,
        kCGImageAlphaPremultipliedLast
    );

    halWindow->context = context;
    halWindow->macWindow = window;

#else
#ifdef HAS_X11
    /* Create X11 window */
    int width = bounds->right - bounds->left;
    int height = bounds->bottom - bounds->top;

    Window rootWindow = RootWindow(gCalcHAL.display, gCalcHAL.screen);

    halWindow->window = XCreateSimpleWindow(
        gCalcHAL.display, rootWindow,
        bounds->left, bounds->top, width, height, 1,
        gCalcHAL.blackPixel, gCalcHAL.whitePixel
    );

    /* Set window properties */
    XStoreName(gCalcHAL.display, halWindow->window, "Calculator");

    /* Select input events */
    XSelectInput(gCalcHAL.display, halWindow->window,
                 ExposureMask | KeyPressMask | ButtonPressMask |
                 ButtonReleaseMask | PointerMotionMask);

    /* Create graphics context */
    halWindow->gc = XCreateGC(gCalcHAL.display, halWindow->window, 0, NULL);

    /* Load font */
    halWindow->font = XLoadQueryFont(gCalcHAL.display,
                                     "-*-helvetica-medium-r-*-*-12-*-*-*-*-*-*-*");
    if (halWindow->font) {
        XSetFont(gCalcHAL.display, halWindow->gc, halWindow->font->fid);
    }

    /* Create Cairo surface for advanced drawing */
    halWindow->surface = cairo_xlib_surface_create(
        gCalcHAL.display, halWindow->window,
        DefaultVisual(gCalcHAL.display, gCalcHAL.screen),
        width, height
    );
    halWindow->cairo = cairo_create(halWindow->surface);

    /* Show window */
    XMapWindow(gCalcHAL.display, halWindow->window);
    XFlush(gCalcHAL.display);

    halWindow->display = gCalcHAL.display;
    halWindow->macWindow = window;
#endif
#endif

    gCalcHAL.currentWindow = halWindow;
    return window;
}

/* Dispose calculator window */
void Calculator_HAL_DisposeWindow(WindowPtr window) {
    if (!window) return;

    HAL_Window *halWindow = (HAL_Window*)window->refCon;
    if (!halWindow) return;

#ifdef __APPLE__
    if (halWindow->context) {
        CGContextRelease(halWindow->context);
    }

#else
#ifdef HAS_X11
    if (halWindow->cairo) cairo_destroy(halWindow->cairo);
    if (halWindow->surface) cairo_surface_destroy(halWindow->surface);
    if (halWindow->font) XFreeFont(halWindow->display, halWindow->font);
    if (halWindow->gc) XFreeGC(halWindow->display, halWindow->gc);
    if (halWindow->window) {
        XDestroyWindow(halWindow->display, halWindow->window);
        XFlush(halWindow->display);
    }
#endif
#endif

    free(halWindow);
    free(window);

    if (gCalcHAL.currentWindow == halWindow) {
        gCalcHAL.currentWindow = NULL;
    }
}

/* Begin drawing operations */
void Calculator_HAL_BeginDrawing(WindowPtr window) {
    if (!window) return;

    HAL_Window *halWindow = (HAL_Window*)window->refCon;
    if (!halWindow) return;

    gCalcHAL.currentWindow = halWindow;

#ifdef __APPLE__
    CGContextSaveGState(halWindow->context);

#else
#ifdef HAS_X11
    /* Prepare for drawing */
    cairo_save(halWindow->cairo);
#endif
#endif
}

/* End drawing operations */
void Calculator_HAL_EndDrawing(void) {
    if (!gCalcHAL.currentWindow) return;

#ifdef __APPLE__
    CGContextRestoreGState(gCalcHAL.currentWindow->context);

#else
#ifdef HAS_X11
    cairo_restore(gCalcHAL.currentWindow->cairo);
    XFlush(gCalcHAL.currentWindow->display);
#endif
#endif
}

/* Clear window */
void Calculator_HAL_ClearWindow(void) {
    if (!gCalcHAL.currentWindow) return;

#ifdef __APPLE__
    CGContextRef ctx = gCalcHAL.currentWindow->context;
    CGContextSetFillColorWithColor(ctx, gCalcHAL.whiteColor);
    CGContextFillRect(ctx, gCalcHAL.currentWindow->bounds);

#else
#ifdef HAS_X11
    HAL_Window *hw = gCalcHAL.currentWindow;
    XSetForeground(hw->display, hw->gc, gCalcHAL.whitePixel);
    XFillRectangle(hw->display, hw->window, hw->gc, 0, 0,
                   CALC_WINDOW_WIDTH, CALC_WINDOW_HEIGHT);
#endif
#endif
}

/* Draw display background */
void Calculator_HAL_DrawDisplayBackground(const Rect *displayRect) {
    if (!gCalcHAL.currentWindow) return;

#ifdef __APPLE__
    CGContextRef ctx = gCalcHAL.currentWindow->context;

    /* Draw light gray background */
    CGRect rect = CGRectMake(displayRect->left, displayRect->top,
                             displayRect->right - displayRect->left,
                             displayRect->bottom - displayRect->top);

    CGContextSetFillColorWithColor(ctx, gCalcHAL.grayColor);
    CGContextFillRect(ctx, rect);

    /* Draw black frame */
    CGContextSetStrokeColorWithColor(ctx, gCalcHAL.blackColor);
    CGContextSetLineWidth(ctx, 1.0);
    CGContextStrokeRect(ctx, rect);

#else
#ifdef HAS_X11
    HAL_Window *hw = gCalcHAL.currentWindow;
    int width = displayRect->right - displayRect->left;
    int height = displayRect->bottom - displayRect->top;

    /* Draw gray background */
    XSetForeground(hw->display, hw->gc, gCalcHAL.grayPixel);
    XFillRectangle(hw->display, hw->window, hw->gc,
                   displayRect->left, displayRect->top, width, height);

    /* Draw black frame */
    XSetForeground(hw->display, hw->gc, gCalcHAL.blackPixel);
    XDrawRectangle(hw->display, hw->window, hw->gc,
                   displayRect->left, displayRect->top, width - 1, height - 1);
#endif
#endif
}

/* Draw display text */
void Calculator_HAL_DrawDisplayText(const char *text, const Rect *displayRect) {
    if (!gCalcHAL.currentWindow || !text) return;

#ifdef __APPLE__
    CGContextRef ctx = gCalcHAL.currentWindow->context;

    /* Set up text attributes */
    CGContextSetTextDrawingMode(ctx, kCGTextFill);
    CGContextSetFillColorWithColor(ctx, gCalcHAL.blackColor);
    CGContextSelectFont(ctx, "Helvetica", 14, kCGEncodingMacRoman);

    /* Calculate text position (right-aligned) */
    CGFloat textWidth = strlen(text) * 8; /* Approximate */
    CGFloat x = displayRect->right - textWidth - 10;
    CGFloat y = displayRect->top + 16;

    CGContextShowTextAtPoint(ctx, x, y, text, strlen(text));

#else
#ifdef HAS_X11
    HAL_Window *hw = gCalcHAL.currentWindow;

    /* Set text color */
    XSetForeground(hw->display, hw->gc, gCalcHAL.blackPixel);

    /* Calculate text width for right alignment */
    int textWidth = XTextWidth(hw->font ? hw->font :
                               XQueryFont(hw->display, XGContextFromGC(hw->gc)),
                               text, strlen(text));

    int x = displayRect->right - textWidth - 10;
    int y = displayRect->top + 16;

    XDrawString(hw->display, hw->window, hw->gc, x, y, text, strlen(text));
#endif
#endif
}

/* Draw button */
void Calculator_HAL_DrawButton(CalcButton *button) {
    if (!gCalcHAL.currentWindow || !button) return;

#ifdef __APPLE__
    CGContextRef ctx = gCalcHAL.currentWindow->context;

    CGRect rect = CGRectMake(button->bounds.left, button->bounds.top,
                            button->bounds.right - button->bounds.left,
                            button->bounds.bottom - button->bounds.top);

    if (button->hilited) {
        /* Inverted button */
        CGContextSetFillColorWithColor(ctx, gCalcHAL.blackColor);
        CGContextFillRect(ctx, rect);
        CGContextSetFillColorWithColor(ctx, gCalcHAL.whiteColor);
    } else {
        /* Normal button */
        CGContextSetFillColorWithColor(ctx, gCalcHAL.whiteColor);
        CGContextFillRect(ctx, rect);
        CGContextSetStrokeColorWithColor(ctx, gCalcHAL.blackColor);
        CGContextSetLineWidth(ctx, 1.0);
        CGContextStrokeRect(ctx, rect);
        CGContextSetFillColorWithColor(ctx, gCalcHAL.blackColor);
    }

    /* Draw label */
    CGFloat labelWidth = strlen(button->label) * 7; /* Approximate */
    CGFloat x = rect.origin.x + (rect.size.width - labelWidth) / 2;
    CGFloat y = rect.origin.y + 16;

    CGContextSelectFont(ctx, "Helvetica", 12, kCGEncodingMacRoman);
    CGContextShowTextAtPoint(ctx, x, y, button->label, strlen(button->label));

#else
#ifdef HAS_X11
    HAL_Window *hw = gCalcHAL.currentWindow;
    int width = button->bounds.right - button->bounds.left;
    int height = button->bounds.bottom - button->bounds.top;

    if (button->hilited) {
        /* Inverted button */
        XSetForeground(hw->display, hw->gc, gCalcHAL.blackPixel);
        XFillRectangle(hw->display, hw->window, hw->gc,
                      button->bounds.left, button->bounds.top, width, height);
        XSetForeground(hw->display, hw->gc, gCalcHAL.whitePixel);
    } else {
        /* Normal button */
        XSetForeground(hw->display, hw->gc, gCalcHAL.whitePixel);
        XFillRectangle(hw->display, hw->window, hw->gc,
                      button->bounds.left, button->bounds.top, width, height);
        XSetForeground(hw->display, hw->gc, gCalcHAL.blackPixel);
        XDrawRectangle(hw->display, hw->window, hw->gc,
                      button->bounds.left, button->bounds.top,
                      width - 1, height - 1);
    }

    /* Draw label centered */
    int textWidth = XTextWidth(hw->font ? hw->font :
                               XQueryFont(hw->display, XGContextFromGC(hw->gc)),
                               button->label, strlen(button->label));

    int x = button->bounds.left + (width - textWidth) / 2;
    int y = button->bounds.top + 16;

    XDrawString(hw->display, hw->window, hw->gc, x, y,
               button->label, strlen(button->label));
#endif
#endif
}

/* Drag window */
void Calculator_HAL_DragWindow(WindowPtr window, Point startPt) {
    /* Platform-specific window dragging */
    /* This would be implemented with platform-specific window manager calls */
}

/* Track go-away box */
Boolean Calculator_HAL_TrackGoAway(WindowPtr window, Point pt) {
    /* For simplicity, return true to close */
    return true;
}

/* Delay for specified milliseconds */
void Calculator_HAL_Delay(unsigned long milliseconds) {
    usleep(milliseconds * 1000);
}

/* Convert global point to local */
void Calculator_HAL_GlobalToLocal(Point *pt, WindowPtr window) {
    if (!window || !pt) return;

    /* Adjust point relative to window origin */
    pt->h -= window->portRect.left;
    pt->v -= window->portRect.top;
}

/* Convert local point to global */
void Calculator_HAL_LocalToGlobal(Point *pt, WindowPtr window) {
    if (!window || !pt) return;

    /* Adjust point relative to screen */
    pt->h += window->portRect.left;
    pt->v += window->portRect.top;
}

/* Check if point is in rectangle */
Boolean Calculator_HAL_PtInRect(Point pt, const Rect *r) {
    return (pt.h >= r->left && pt.h < r->right &&
            pt.v >= r->top && pt.v < r->bottom);
}

/* Process system events (for non-Mac platforms) */
void Calculator_HAL_ProcessEvents(void) {
#ifdef HAS_X11
    if (!gCalcHAL.currentWindow) return;

    XEvent event;
    HAL_Window *hw = gCalcHAL.currentWindow;

    while (XPending(hw->display) > 0) {
        XNextEvent(hw->display, &event);

        switch (event.type) {
            case Expose:
                /* Redraw calculator on expose */
                DrawCalculator();
                break;

            case ButtonPress:
                /* Handle mouse click */
                {
                    Point where;
                    where.h = event.xbutton.x;
                    where.v = event.xbutton.y;
                    HandleMouseDown(where);
                }
                break;

            case KeyPress:
                /* Handle keyboard input */
                {
                    char buffer[32];
                    KeySym keysym;
                    int len = XLookupString(&event.xkey, buffer, sizeof(buffer),
                                           &keysym, NULL);
                    if (len > 0) {
                        HandleKeyDown(buffer[0]);
                    }
                }
                break;
        }
    }
#endif
}