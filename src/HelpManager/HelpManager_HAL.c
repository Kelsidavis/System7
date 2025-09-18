/*
 * Copyright (c) 2024 System7 Project
 * MIT License - See LICENSE file
 */

/*
 * HelpManager_HAL.c - Hardware Abstraction Layer for Help Manager
 * Platform-specific help balloon and tooltip implementation
 */

#include "HelpManager/HelpManager.h"
#include "HelpManager/HelpManager_HAL.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef __APPLE__
#include <Cocoa/Cocoa.h>
#include <WebKit/WebKit.h>
#endif

#ifdef __linux__
#include <gtk/gtk.h>
#include <webkit2/webkit2.h>
#endif

#ifdef _WIN32
#include <windows.h>
#include <commctrl.h>
#include <htmlhelp.h>
#endif

/* Platform-specific help context */
typedef struct HelpContext {
#ifdef __APPLE__
    NSWindow *balloonWindow;
    NSTextView *textView;
    WKWebView *webView;
    NSHelpManager *helpManager;
#elif defined(__linux__)
    GtkWidget *balloonWindow;
    GtkWidget *textView;
    WebKitWebView *webView;
    GtkTooltip *currentTooltip;
#elif defined(_WIN32)
    HWND balloonWindow;
    HWND tooltipWindow;
    HHOOK mouseHook;
    HWND htmlHelpWindow;
#endif
    Boolean isInitialized;
    Boolean usesTooltips;       /* Use native tooltips instead of balloons */
    Boolean usesAccessibility; /* Enable screen reader support */
    char helpBasePath[256];    /* Base path for help files */
} HelpContext;

static HelpContext gHelpContext = {0};

/* Initialize HAL */
OSErr HelpManager_HAL_Init(void) {
    if (gHelpContext.isInitialized) {
        return noErr;
    }

    memset(&gHelpContext, 0, sizeof(HelpContext));

#ifdef __APPLE__
    @autoreleasepool {
        /* Initialize Cocoa help system */
        gHelpContext.helpManager = [NSHelpManager sharedHelpManager];

        /* Create balloon window (custom NSWindow) */
        NSRect frame = NSMakeRect(0, 0, 200, 100);
        gHelpContext.balloonWindow = [[NSWindow alloc] initWithContentRect:frame
            styleMask:NSWindowStyleMaskBorderless
            backing:NSBackingStoreBuffered
            defer:NO];

        [gHelpContext.balloonWindow setOpaque:NO];
        [gHelpContext.balloonWindow setBackgroundColor:[NSColor clearColor]];
        [gHelpContext.balloonWindow setLevel:NSStatusWindowLevel];
        [gHelpContext.balloonWindow setHasShadow:YES];

        /* Create text view for balloon content */
        gHelpContext.textView = [[NSTextView alloc] initWithFrame:frame];
        [gHelpContext.textView setEditable:NO];
        [gHelpContext.textView setSelectable:NO];
        [gHelpContext.textView setBackgroundColor:[NSColor colorWithWhite:1.0 alpha:0.95]];
        [gHelpContext.textView setTextContainerInset:NSMakeSize(8, 8)];

        [[gHelpContext.balloonWindow contentView] addSubview:gHelpContext.textView];

        /* Determine if we should use native tooltips */
        gHelpContext.usesTooltips = YES;  /* Default to native tooltips on macOS */
    }

#elif defined(__linux__)
    /* Initialize GTK if needed */
    if (!gtk_init_check(NULL, NULL)) {
        return memFullErr;
    }

    /* Create balloon window */
    gHelpContext.balloonWindow = gtk_window_new(GTK_WINDOW_POPUP);
    gtk_window_set_decorated(GTK_WINDOW(gHelpContext.balloonWindow), FALSE);
    gtk_window_set_type_hint(GTK_WINDOW(gHelpContext.balloonWindow),
                            GDK_WINDOW_TYPE_HINT_TOOLTIP);

    /* Create text view */
    gHelpContext.textView = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(gHelpContext.textView), FALSE);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(gHelpContext.textView), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(gHelpContext.textView), GTK_WRAP_WORD);

    GtkWidget *frame = gtk_frame_new(NULL);
    gtk_container_add(GTK_CONTAINER(frame), gHelpContext.textView);
    gtk_container_add(GTK_CONTAINER(gHelpContext.balloonWindow), frame);

    gtk_widget_set_margin_start(gHelpContext.textView, 8);
    gtk_widget_set_margin_end(gHelpContext.textView, 8);
    gtk_widget_set_margin_top(gHelpContext.textView, 8);
    gtk_widget_set_margin_bottom(gHelpContext.textView, 8);

    /* Use native tooltips by default on GTK */
    gHelpContext.usesTooltips = TRUE;

#elif defined(_WIN32)
    /* Initialize common controls for tooltips */
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_WIN95_CLASSES;
    InitCommonControlsEx(&icex);

    /* Create balloon window */
    gHelpContext.balloonWindow = CreateWindowEx(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        "STATIC",
        "",
        WS_POPUP | WS_BORDER,
        0, 0, 200, 100,
        NULL, NULL, GetModuleHandle(NULL), NULL);

    /* Create tooltip control */
    gHelpContext.tooltipWindow = CreateWindowEx(
        0, TOOLTIPS_CLASS, NULL,
        WS_POPUP | TTS_ALWAYSTIP | TTS_BALLOON,
        CW_USEDEFAULT, CW_USEDEFAULT,
        CW_USEDEFAULT, CW_USEDEFAULT,
        NULL, NULL, GetModuleHandle(NULL), NULL);

    SetWindowPos(gHelpContext.tooltipWindow, HWND_TOPMOST,
                0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

    /* Use native tooltips on Windows */
    gHelpContext.usesTooltips = TRUE;
#endif

    /* Set default help base path */
    strcpy(gHelpContext.helpBasePath, "./help/");

    gHelpContext.isInitialized = TRUE;
    return noErr;
}

/* Cleanup HAL */
void HelpManager_HAL_Cleanup(void) {
    if (!gHelpContext.isInitialized) {
        return;
    }

#ifdef __APPLE__
    @autoreleasepool {
        if (gHelpContext.balloonWindow) {
            [gHelpContext.balloonWindow close];
            [gHelpContext.balloonWindow release];
            gHelpContext.balloonWindow = nil;
        }
        if (gHelpContext.textView) {
            [gHelpContext.textView release];
            gHelpContext.textView = nil;
        }
        if (gHelpContext.webView) {
            [gHelpContext.webView release];
            gHelpContext.webView = nil;
        }
    }

#elif defined(__linux__)
    if (gHelpContext.balloonWindow) {
        gtk_widget_destroy(gHelpContext.balloonWindow);
        gHelpContext.balloonWindow = NULL;
    }
    if (gHelpContext.webView) {
        gtk_widget_destroy(GTK_WIDGET(gHelpContext.webView));
        gHelpContext.webView = NULL;
    }

#elif defined(_WIN32)
    if (gHelpContext.balloonWindow) {
        DestroyWindow(gHelpContext.balloonWindow);
        gHelpContext.balloonWindow = NULL;
    }
    if (gHelpContext.tooltipWindow) {
        DestroyWindow(gHelpContext.tooltipWindow);
        gHelpContext.tooltipWindow = NULL;
    }
    if (gHelpContext.htmlHelpWindow) {
        HtmlHelp(NULL, NULL, HH_CLOSE_ALL, 0);
        gHelpContext.htmlHelpWindow = NULL;
    }
#endif

    gHelpContext.isInitialized = FALSE;
}

/* Show balloon with platform-specific rendering */
OSErr HelpManager_HAL_ShowBalloon(const char *text, int x, int y,
                                 int width, int height, int variant) {
    if (!gHelpContext.isInitialized) {
        OSErr err = HelpManager_HAL_Init();
        if (err != noErr) return err;
    }

    if (!text || strlen(text) == 0) {
        return paramErr;
    }

#ifdef __APPLE__
    @autoreleasepool {
        if (gHelpContext.usesTooltips) {
            /* Use native tooltip */
            NSString *helpText = [NSString stringWithUTF8String:text];
            NSPoint point = NSMakePoint(x, y);

            /* Convert to screen coordinates */
            NSScreen *screen = [NSScreen mainScreen];
            point.y = screen.frame.size.height - point.y;

            [gHelpContext.helpManager setContextHelp:[[NSAttributedString alloc]
                initWithString:helpText] forObject:nil];
            [gHelpContext.helpManager showContextHelpForObject:nil locationHint:point];
        } else {
            /* Use custom balloon window */
            NSString *helpText = [NSString stringWithUTF8String:text];

            /* Set text */
            [gHelpContext.textView setString:helpText];

            /* Calculate size */
            NSSize textSize = [helpText sizeWithAttributes:@{
                NSFontAttributeName: [NSFont systemFontOfSize:11]
            }];

            if (width == 0) width = MIN(textSize.width + 16, 320);
            if (height == 0) height = MIN(textSize.height + 16, 200);

            /* Position window */
            NSRect frame = NSMakeRect(x, y, width, height);
            [gHelpContext.balloonWindow setFrame:frame display:NO];
            [gHelpContext.textView setFrame:NSMakeRect(0, 0, width, height)];

            /* Show window */
            [gHelpContext.balloonWindow orderFront:nil];
        }
    }

#elif defined(__linux__)
    if (gHelpContext.usesTooltips) {
        /* Use GTK tooltip */
        GtkWidget *widget = gtk_window_get_focus(GTK_WINDOW(
            gtk_application_get_active_window(gtk_application_get_default())));
        if (widget) {
            gtk_widget_set_tooltip_text(widget, text);
            gtk_widget_trigger_tooltip_query(widget);
        }
    } else {
        /* Use custom balloon window */
        GtkTextBuffer *buffer = gtk_text_view_get_buffer(
            GTK_TEXT_VIEW(gHelpContext.textView));
        gtk_text_buffer_set_text(buffer, text, -1);

        /* Calculate size if not specified */
        if (width == 0) width = 200;
        if (height == 0) height = 100;

        /* Position and show window */
        gtk_window_move(GTK_WINDOW(gHelpContext.balloonWindow), x, y);
        gtk_window_resize(GTK_WINDOW(gHelpContext.balloonWindow), width, height);
        gtk_widget_show_all(gHelpContext.balloonWindow);
    }

#elif defined(_WIN32)
    if (gHelpContext.usesTooltips) {
        /* Use Windows tooltip */
        TOOLINFO ti = {0};
        ti.cbSize = sizeof(TOOLINFO);
        ti.uFlags = TTF_ABSOLUTE | TTF_TRACK;
        ti.hwnd = NULL;
        ti.hinst = GetModuleHandle(NULL);
        ti.lpszText = (LPSTR)text;

        SendMessage(gHelpContext.tooltipWindow, TTM_ADDTOOL, 0, (LPARAM)&ti);
        SendMessage(gHelpContext.tooltipWindow, TTM_TRACKPOSITION, 0, MAKELPARAM(x, y));
        SendMessage(gHelpContext.tooltipWindow, TTM_TRACKACTIVATE, TRUE, (LPARAM)&ti);
    } else {
        /* Use custom balloon window */
        SetWindowText(gHelpContext.balloonWindow, text);

        /* Calculate size if not specified */
        if (width == 0) width = 200;
        if (height == 0) height = 100;

        /* Position and show window */
        SetWindowPos(gHelpContext.balloonWindow, HWND_TOPMOST,
                    x, y, width, height,
                    SWP_NOACTIVATE | SWP_SHOWWINDOW);
    }
#endif

    return noErr;
}

/* Hide balloon */
OSErr HelpManager_HAL_HideBalloon(void) {
    if (!gHelpContext.isInitialized) {
        return hmHelpManagerNotInited;
    }

#ifdef __APPLE__
    @autoreleasepool {
        if (gHelpContext.usesTooltips) {
            /* Hide native tooltip */
            [gHelpContext.helpManager removeContextHelpForObject:nil];
        } else {
            /* Hide balloon window */
            [gHelpContext.balloonWindow orderOut:nil];
        }
    }

#elif defined(__linux__)
    if (gHelpContext.usesTooltips) {
        /* Hide GTK tooltip */
        GtkWidget *widget = gtk_window_get_focus(GTK_WINDOW(
            gtk_application_get_active_window(gtk_application_get_default())));
        if (widget) {
            gtk_widget_set_has_tooltip(widget, FALSE);
        }
    } else {
        /* Hide balloon window */
        gtk_widget_hide(gHelpContext.balloonWindow);
    }

#elif defined(_WIN32)
    if (gHelpContext.usesTooltips) {
        /* Hide Windows tooltip */
        TOOLINFO ti = {0};
        ti.cbSize = sizeof(TOOLINFO);
        SendMessage(gHelpContext.tooltipWindow, TTM_TRACKACTIVATE, FALSE, (LPARAM)&ti);
        SendMessage(gHelpContext.tooltipWindow, TTM_DELTOOL, 0, (LPARAM)&ti);
    } else {
        /* Hide balloon window */
        ShowWindow(gHelpContext.balloonWindow, SW_HIDE);
    }
#endif

    return noErr;
}

/* Draw balloon frame with platform-specific style */
OSErr HelpManager_HAL_DrawBalloonFrame(void *context, const Rect *bounds,
                                       int variant, Boolean hasTip) {
    /* Platform-specific balloon frame drawing */

#ifdef __APPLE__
    @autoreleasepool {
        NSGraphicsContext *nsContext = [NSGraphicsContext currentContext];
        if (!nsContext) return paramErr;

        /* Draw rounded rectangle balloon */
        NSBezierPath *path = [NSBezierPath bezierPathWithRoundedRect:
            NSMakeRect(bounds->left, bounds->top,
                      bounds->right - bounds->left,
                      bounds->bottom - bounds->top)
            xRadius:8 yRadius:8];

        [[NSColor colorWithWhite:1.0 alpha:0.95] setFill];
        [path fill];

        [[NSColor colorWithWhite:0.5 alpha:1.0] setStroke];
        [path setLineWidth:1.0];
        [path stroke];

        /* Draw tip if needed */
        if (hasTip) {
            /* Draw triangular tip based on variant */
        }
    }

#elif defined(__linux__)
    /* GTK drawing would go here */

#elif defined(_WIN32)
    /* Windows GDI drawing would go here */
#endif

    return noErr;
}

/* Show HTML help */
OSErr HelpManager_HAL_ShowHTMLHelp(const char *topic, const char *anchor) {
    if (!topic) return paramErr;

#ifdef __APPLE__
    @autoreleasepool {
        NSString *helpTopic = [NSString stringWithUTF8String:topic];
        NSString *helpAnchor = anchor ? [NSString stringWithUTF8String:anchor] : nil;

        /* Use NSHelpManager to show help */
        if (helpAnchor) {
            [gHelpContext.helpManager openHelpAnchor:helpAnchor inBook:helpTopic];
        } else {
            /* Open help book */
            NSString *helpPath = [NSString stringWithFormat:@"%s%s.help",
                                 gHelpContext.helpBasePath, topic];
            [[NSWorkspace sharedWorkspace] openFile:helpPath];
        }
    }

#elif defined(__linux__)
    /* Use xdg-open or yelp for help */
    char command[512];
    snprintf(command, sizeof(command), "xdg-open %s%s.html#%s",
            gHelpContext.helpBasePath, topic, anchor ? anchor : "");
    system(command);

#elif defined(_WIN32)
    /* Use HTML Help API */
    char helpPath[512];
    snprintf(helpPath, sizeof(helpPath), "%s%s.chm",
            gHelpContext.helpBasePath, topic);

    if (anchor) {
        char fullPath[512];
        snprintf(fullPath, sizeof(fullPath), "%s::%s", helpPath, anchor);
        gHelpContext.htmlHelpWindow = HtmlHelp(NULL, fullPath,
                                              HH_DISPLAY_TOPIC, 0);
    } else {
        gHelpContext.htmlHelpWindow = HtmlHelp(NULL, helpPath,
                                              HH_DISPLAY_TOC, 0);
    }
#endif

    return noErr;
}

/* Search help content */
OSErr HelpManager_HAL_SearchHelp(const char *searchTerm, char *results,
                                int maxResults) {
    if (!searchTerm || !results) return paramErr;

#ifdef __APPLE__
    @autoreleasepool {
        /* Use NSHelpManager search */
        NSString *search = [NSString stringWithUTF8String:searchTerm];
        [gHelpContext.helpManager findString:search inBook:nil];
    }

#elif defined(__linux__)
    /* Search would be implemented here */

#elif defined(_WIN32)
    /* Use HTML Help search */
    HH_FTS_QUERY query;
    memset(&query, 0, sizeof(query));
    query.cbStruct = sizeof(query);
    query.pszSearchQuery = (char*)searchTerm;

    HtmlHelp(NULL, NULL, HH_DISPLAY_SEARCH, (DWORD_PTR)&query);
#endif

    /* For now, return empty results */
    results[0] = '\0';
    return noErr;
}

/* Enable accessibility support */
OSErr HelpManager_HAL_EnableAccessibility(Boolean enable) {
    gHelpContext.usesAccessibility = enable;

#ifdef __APPLE__
    @autoreleasepool {
        /* Enable VoiceOver support */
        if (enable) {
            /* Set accessibility attributes on balloon window */
            if (gHelpContext.balloonWindow) {
                [gHelpContext.balloonWindow setAccessibilityRole:NSAccessibilityHelpTagRole];
                [gHelpContext.balloonWindow setAccessibilityRoleDescription:
                    @"Help balloon"];
            }
        }
    }

#elif defined(__linux__)
    /* Enable ATK/AT-SPI support */
    if (gHelpContext.balloonWindow) {
        AtkObject *accessible = gtk_widget_get_accessible(gHelpContext.balloonWindow);
        if (accessible) {
            atk_object_set_role(accessible, ATK_ROLE_TOOL_TIP);
        }
    }

#elif defined(_WIN32)
    /* Enable MSAA/UIA support */
    if (enable) {
        /* Set accessibility properties on balloon window */
    }
#endif

    return noErr;
}

/* Set help base path */
OSErr HelpManager_HAL_SetHelpPath(const char *path) {
    if (!path) return paramErr;

    strncpy(gHelpContext.helpBasePath, path, sizeof(gHelpContext.helpBasePath) - 1);
    gHelpContext.helpBasePath[sizeof(gHelpContext.helpBasePath) - 1] = '\0';

    /* Ensure path ends with separator */
    size_t len = strlen(gHelpContext.helpBasePath);
    if (len > 0 && gHelpContext.helpBasePath[len - 1] != '/' &&
        gHelpContext.helpBasePath[len - 1] != '\\') {
        strcat(gHelpContext.helpBasePath, "/");
    }

    return noErr;
}

/* Get platform capabilities */
UInt32 HelpManager_HAL_GetCapabilities(void) {
    UInt32 caps = 0;

    caps |= kHMCapBasicBalloons;    /* Basic balloon help */
    caps |= kHMCapTooltips;         /* Native tooltips */

#ifdef __APPLE__
    caps |= kHMCapHTMLHelp;         /* HTML help viewer */
    caps |= kHMCapAccessibility;    /* VoiceOver support */
    caps |= kHMCapSearch;           /* Help search */
#endif

#ifdef __linux__
    caps |= kHMCapHTMLHelp;         /* Web browser help */
    caps |= kHMCapAccessibility;    /* ATK support */
#endif

#ifdef _WIN32
    caps |= kHMCapHTMLHelp;         /* HTML Help support */
    caps |= kHMCapAccessibility;    /* MSAA/UIA support */
    caps |= kHMCapSearch;           /* CHM search */
#endif

    return caps;
}

/* Set tooltip mode */
OSErr HelpManager_HAL_SetTooltipMode(Boolean useTooltips) {
    gHelpContext.usesTooltips = useTooltips;
    return noErr;
}

/* Get tooltip mode */
Boolean HelpManager_HAL_GetTooltipMode(void) {
    return gHelpContext.usesTooltips;
}