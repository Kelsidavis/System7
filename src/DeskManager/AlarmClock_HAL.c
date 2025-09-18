/*
 * Copyright (c) 2024 System7 Project
 * MIT License - See LICENSE file
 */

/*
 * AlarmClock_HAL.c - Hardware Abstraction Layer for Alarm Clock
 * Provides platform-specific implementations for time, display, and notifications
 */

#include "DeskManager/AlarmClock.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>

#ifdef __APPLE__
#include <CoreGraphics/CoreGraphics.h>
#include <ApplicationServices/ApplicationServices.h>
#include <AudioToolbox/AudioToolbox.h>
#else
#ifdef HAS_X11
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <cairo/cairo.h>
#include <cairo/cairo-xlib.h>
#endif
#endif

/* Platform-specific alarm window structure */
typedef struct {
    WindowPtr macWindow;
    AlarmClock *clock;
    pthread_t updateThread;
    Boolean threadRunning;

#ifdef __APPLE__
    CGContextRef context;
    CGRect bounds;
    SystemSoundID alarmSound;
#else
#ifdef HAS_X11
    Display *display;
    Window window;
    GC gc;
    cairo_surface_t *surface;
    cairo_t *cairo;
    XFontStruct *font;
    XFontStruct *largeFont;
#endif
#endif
} HAL_AlarmWindow;

/* Global HAL state */
static struct {
    HAL_AlarmWindow *currentWindow;
    Boolean initialized;
    pthread_mutex_t updateMutex;

#ifdef __APPLE__
    CGColorSpaceRef colorSpace;
    CGColorRef blackColor;
    CGColorRef whiteColor;
    CGColorRef redColor;
    CGColorRef greenColor;
#else
#ifdef HAS_X11
    Display *display;
    int screen;
    unsigned long blackPixel;
    unsigned long whitePixel;
    unsigned long redPixel;
    unsigned long greenPixel;
#endif
#endif
} gAlarmHAL = {0};

/* Forward declarations */
static void* AlarmClock_HAL_UpdateThread(void *data);
static void AlarmClock_HAL_DrawClock(HAL_AlarmWindow *halWindow);
static void AlarmClock_HAL_CheckAlarms(AlarmClock *clock);

/* Initialize Alarm Clock HAL */
void AlarmClock_HAL_Init(void) {
    if (gAlarmHAL.initialized) return;

    pthread_mutex_init(&gAlarmHAL.updateMutex, NULL);

#ifdef __APPLE__
    /* Initialize Core Graphics colors */
    gAlarmHAL.colorSpace = CGColorSpaceCreateDeviceRGB();

    CGFloat black[] = {0.0, 0.0, 0.0, 1.0};
    CGFloat white[] = {1.0, 1.0, 1.0, 1.0};
    CGFloat red[] = {1.0, 0.0, 0.0, 1.0};
    CGFloat green[] = {0.0, 0.7, 0.0, 1.0};

    gAlarmHAL.blackColor = CGColorCreate(gAlarmHAL.colorSpace, black);
    gAlarmHAL.whiteColor = CGColorCreate(gAlarmHAL.colorSpace, white);
    gAlarmHAL.redColor = CGColorCreate(gAlarmHAL.colorSpace, red);
    gAlarmHAL.greenColor = CGColorCreate(gAlarmHAL.colorSpace, green);

#else
#ifdef HAS_X11
    /* Initialize X11 */
    gAlarmHAL.display = XOpenDisplay(NULL);
    if (!gAlarmHAL.display) {
        fprintf(stderr, "Cannot open X display\n");
        return;
    }

    gAlarmHAL.screen = DefaultScreen(gAlarmHAL.display);
    gAlarmHAL.blackPixel = BlackPixel(gAlarmHAL.display, gAlarmHAL.screen);
    gAlarmHAL.whitePixel = WhitePixel(gAlarmHAL.display, gAlarmHAL.screen);

    /* Create red and green pixels */
    Colormap cmap = DefaultColormap(gAlarmHAL.display, gAlarmHAL.screen);
    XColor red, green;

    red.red = 65535; red.green = 0; red.blue = 0;
    red.flags = DoRed | DoGreen | DoBlue;
    XAllocColor(gAlarmHAL.display, cmap, &red);
    gAlarmHAL.redPixel = red.pixel;

    green.red = 0; green.green = 45000; green.blue = 0;
    green.flags = DoRed | DoGreen | DoBlue;
    XAllocColor(gAlarmHAL.display, cmap, &green);
    gAlarmHAL.greenPixel = green.pixel;
#endif
#endif

    gAlarmHAL.initialized = true;
}

/* Cleanup Alarm Clock HAL */
void AlarmClock_HAL_Cleanup(void) {
    if (!gAlarmHAL.initialized) return;

    pthread_mutex_destroy(&gAlarmHAL.updateMutex);

#ifdef __APPLE__
    if (gAlarmHAL.blackColor) CGColorRelease(gAlarmHAL.blackColor);
    if (gAlarmHAL.whiteColor) CGColorRelease(gAlarmHAL.whiteColor);
    if (gAlarmHAL.redColor) CGColorRelease(gAlarmHAL.redColor);
    if (gAlarmHAL.greenColor) CGColorRelease(gAlarmHAL.greenColor);
    if (gAlarmHAL.colorSpace) CGColorSpaceRelease(gAlarmHAL.colorSpace);

#else
#ifdef HAS_X11
    if (gAlarmHAL.display) {
        XCloseDisplay(gAlarmHAL.display);
    }
#endif
#endif

    gAlarmHAL.initialized = false;
}

/* Create alarm clock window */
WindowPtr AlarmClock_HAL_CreateWindow(AlarmClock *clock) {
    if (!clock) return NULL;

    HAL_AlarmWindow *halWindow = (HAL_AlarmWindow*)calloc(1, sizeof(HAL_AlarmWindow));
    if (!halWindow) return NULL;

    halWindow->clock = clock;

    /* Create Mac-style window structure */
    WindowPtr window = (WindowPtr)calloc(1, sizeof(WindowRecord));
    if (!window) {
        free(halWindow);
        return NULL;
    }

    /* Initialize window record */
    window->portRect = clock->windowBounds;
    window->portBits.bounds = clock->windowBounds;
    window->visible = true;
    window->windowKind = userKind;
    window->refCon = (long)halWindow;

#ifdef __APPLE__
    /* Create Core Graphics context */
    size_t width = clock->windowBounds.right - clock->windowBounds.left;
    size_t height = clock->windowBounds.bottom - clock->windowBounds.top;

    halWindow->bounds = CGRectMake(0, 0, width, height);

    CGContextRef context = CGBitmapContextCreate(
        NULL, width, height, 8,
        width * 4,
        gAlarmHAL.colorSpace,
        kCGImageAlphaPremultipliedLast
    );

    halWindow->context = context;
    halWindow->macWindow = window;

    /* Load system sound for alarm */
    AudioServicesCreateSystemSoundID(
        CFSTR("/System/Library/Sounds/Glass.aiff"),
        &halWindow->alarmSound
    );

#else
#ifdef HAS_X11
    /* Create X11 window */
    int width = clock->windowBounds.right - clock->windowBounds.left;
    int height = clock->windowBounds.bottom - clock->windowBounds.top;

    Window rootWindow = RootWindow(gAlarmHAL.display, gAlarmHAL.screen);

    halWindow->window = XCreateSimpleWindow(
        gAlarmHAL.display, rootWindow,
        clock->windowBounds.left, clock->windowBounds.top,
        width, height, 1,
        gAlarmHAL.blackPixel, gAlarmHAL.whitePixel
    );

    /* Set window properties */
    XStoreName(gAlarmHAL.display, halWindow->window, "Alarm Clock");

    /* Select input events */
    XSelectInput(gAlarmHAL.display, halWindow->window,
                 ExposureMask | KeyPressMask | ButtonPressMask |
                 ButtonReleaseMask | PointerMotionMask | StructureNotifyMask);

    /* Create graphics context */
    halWindow->gc = XCreateGC(gAlarmHAL.display, halWindow->window, 0, NULL);

    /* Load fonts */
    halWindow->font = XLoadQueryFont(gAlarmHAL.display,
                                     "-*-helvetica-medium-r-*-*-12-*-*-*-*-*-*-*");
    halWindow->largeFont = XLoadQueryFont(gAlarmHAL.display,
                                          "-*-helvetica-bold-r-*-*-24-*-*-*-*-*-*-*");

    if (halWindow->font) {
        XSetFont(gAlarmHAL.display, halWindow->gc, halWindow->font->fid);
    }

    /* Create Cairo surface for advanced drawing */
    halWindow->surface = cairo_xlib_surface_create(
        gAlarmHAL.display, halWindow->window,
        DefaultVisual(gAlarmHAL.display, gAlarmHAL.screen),
        width, height
    );
    halWindow->cairo = cairo_create(halWindow->surface);

    /* Show window */
    XMapWindow(gAlarmHAL.display, halWindow->window);
    XFlush(gAlarmHAL.display);

    halWindow->display = gAlarmHAL.display;
    halWindow->macWindow = window;
#endif
#endif

    /* Start update thread */
    halWindow->threadRunning = true;
    pthread_create(&halWindow->updateThread, NULL,
                   AlarmClock_HAL_UpdateThread, halWindow);

    gAlarmHAL.currentWindow = halWindow;
    return window;
}

/* Dispose alarm clock window */
void AlarmClock_HAL_DisposeWindow(WindowPtr window) {
    if (!window) return;

    HAL_AlarmWindow *halWindow = (HAL_AlarmWindow*)window->refCon;
    if (!halWindow) return;

    /* Stop update thread */
    halWindow->threadRunning = false;
    pthread_join(halWindow->updateThread, NULL);

#ifdef __APPLE__
    if (halWindow->context) {
        CGContextRelease(halWindow->context);
    }
    if (halWindow->alarmSound) {
        AudioServicesDisposeSystemSoundID(halWindow->alarmSound);
    }

#else
#ifdef HAS_X11
    if (halWindow->cairo) cairo_destroy(halWindow->cairo);
    if (halWindow->surface) cairo_surface_destroy(halWindow->surface);
    if (halWindow->font) XFreeFont(halWindow->display, halWindow->font);
    if (halWindow->largeFont) XFreeFont(halWindow->display, halWindow->largeFont);
    if (halWindow->gc) XFreeGC(halWindow->display, halWindow->gc);
    if (halWindow->window) {
        XDestroyWindow(halWindow->display, halWindow->window);
        XFlush(halWindow->display);
    }
#endif
#endif

    free(halWindow);
    free(window);

    if (gAlarmHAL.currentWindow == halWindow) {
        gAlarmHAL.currentWindow = NULL;
    }
}

/* Update thread function */
static void* AlarmClock_HAL_UpdateThread(void *data) {
    HAL_AlarmWindow *halWindow = (HAL_AlarmWindow*)data;

    while (halWindow->threadRunning) {
        pthread_mutex_lock(&gAlarmHAL.updateMutex);

        /* Update time */
        AlarmClock_UpdateTime(halWindow->clock);

        /* Check alarms */
        AlarmClock_HAL_CheckAlarms(halWindow->clock);

        /* Redraw clock */
        AlarmClock_HAL_DrawClock(halWindow);

        pthread_mutex_unlock(&gAlarmHAL.updateMutex);

        /* Sleep for update interval */
        usleep(halWindow->clock->updateInterval * 1000);
    }

    return NULL;
}

/* Draw clock display */
static void AlarmClock_HAL_DrawClock(HAL_AlarmWindow *halWindow) {
    if (!halWindow || !halWindow->clock) return;

    AlarmClock *clock = halWindow->clock;

#ifdef __APPLE__
    CGContextRef ctx = halWindow->context;

    /* Clear background */
    CGContextSetFillColorWithColor(ctx, gAlarmHAL.whiteColor);
    CGContextFillRect(ctx, halWindow->bounds);

    /* Draw border */
    CGContextSetStrokeColorWithColor(ctx, gAlarmHAL.blackColor);
    CGContextSetLineWidth(ctx, 2.0);
    CGContextStrokeRect(ctx, halWindow->bounds);

    /* Draw time */
    CGContextSetFillColorWithColor(ctx, gAlarmHAL.blackColor);
    CGContextSelectFont(ctx, "Helvetica-Bold", 24, kCGEncodingMacRoman);

    CGFloat x = clock->timeRect.left;
    CGFloat y = clock->timeRect.bottom;

    CGContextShowTextAtPoint(ctx, x, y,
                            clock->timeString, strlen(clock->timeString));

    /* Draw date if enabled */
    if (clock->showDate) {
        CGContextSelectFont(ctx, "Helvetica", 14, kCGEncodingMacRoman);
        y = clock->dateRect.bottom;
        CGContextShowTextAtPoint(ctx, x, y,
                                clock->dateString, strlen(clock->dateString));
    }

    /* Draw alarm indicator if any alarms are active */
    if (clock->numActiveAlarms > 0) {
        CGContextSetFillColorWithColor(ctx, gAlarmHAL.redColor);
        CGContextSelectFont(ctx, "Helvetica", 12, kCGEncodingMacRoman);

        char alarmText[32];
        snprintf(alarmText, sizeof(alarmText), "⏰ %d active alarm%s",
                clock->numActiveAlarms,
                clock->numActiveAlarms > 1 ? "s" : "");

        y = clock->alarmRect.bottom;
        CGContextShowTextAtPoint(ctx, x, y, alarmText, strlen(alarmText));
    }

#else
#ifdef HAS_X11
    HAL_AlarmWindow *hw = halWindow;

    /* Clear background */
    XSetForeground(hw->display, hw->gc, gAlarmHAL.whitePixel);
    XFillRectangle(hw->display, hw->window, hw->gc, 0, 0,
                   clock->windowBounds.right - clock->windowBounds.left,
                   clock->windowBounds.bottom - clock->windowBounds.top);

    /* Draw border */
    XSetForeground(hw->display, hw->gc, gAlarmHAL.blackPixel);
    XDrawRectangle(hw->display, hw->window, hw->gc, 0, 0,
                   clock->windowBounds.right - clock->windowBounds.left - 1,
                   clock->windowBounds.bottom - clock->windowBounds.top - 1);

    /* Draw time with large font */
    if (hw->largeFont) {
        XSetFont(hw->display, hw->gc, hw->largeFont->fid);
    }

    XSetForeground(hw->display, hw->gc, gAlarmHAL.blackPixel);
    XDrawString(hw->display, hw->window, hw->gc,
               clock->timeRect.left, clock->timeRect.bottom,
               clock->timeString, strlen(clock->timeString));

    /* Draw date with normal font */
    if (clock->showDate) {
        if (hw->font) {
            XSetFont(hw->display, hw->gc, hw->font->fid);
        }

        XDrawString(hw->display, hw->window, hw->gc,
                   clock->dateRect.left, clock->dateRect.bottom,
                   clock->dateString, strlen(clock->dateString));
    }

    /* Draw alarm indicator */
    if (clock->numActiveAlarms > 0) {
        XSetForeground(hw->display, hw->gc, gAlarmHAL.redPixel);

        char alarmText[32];
        snprintf(alarmText, sizeof(alarmText), "%d active alarm%s",
                clock->numActiveAlarms,
                clock->numActiveAlarms > 1 ? "s" : "");

        XDrawString(hw->display, hw->window, hw->gc,
                   clock->alarmRect.left, clock->alarmRect.bottom,
                   alarmText, strlen(alarmText));
    }

    XFlush(hw->display);
#endif
#endif
}

/* Check and trigger alarms */
static void AlarmClock_HAL_CheckAlarms(AlarmClock *clock) {
    if (!clock) return;

    time_t now = time(NULL);
    struct tm *tm = localtime(&now);

    DateTime currentTime;
    currentTime.year = tm->tm_year + 1900;
    currentTime.month = tm->tm_mon + 1;
    currentTime.day = tm->tm_mday;
    currentTime.hour = tm->tm_hour;
    currentTime.minute = tm->tm_min;
    currentTime.second = tm->tm_sec;
    currentTime.weekday = tm->tm_wday;

    /* Check each alarm */
    Alarm *alarm = clock->alarms;
    int activeCount = 0;

    while (alarm) {
        if (alarm->state == ALARM_STATE_ENABLED) {
            activeCount++;

            /* Check if alarm should trigger */
            if (AlarmClock_ShouldTrigger(alarm, &currentTime)) {
                if (alarm->state != ALARM_STATE_TRIGGERED) {
                    AlarmClock_HAL_TriggerAlarm(clock, alarm);
                    alarm->state = ALARM_STATE_TRIGGERED;
                }
            }
        }
        alarm = alarm->next;
    }

    clock->numActiveAlarms = activeCount;
}

/* Trigger alarm notification */
void AlarmClock_HAL_TriggerAlarm(AlarmClock *clock, Alarm *alarm) {
    if (!clock || !alarm) return;

    /* Play sound if enabled */
    if ((alarm->notifyType & NOTIFY_SOUND) && clock->soundEnabled) {
#ifdef __APPLE__
        HAL_AlarmWindow *halWindow = gAlarmHAL.currentWindow;
        if (halWindow && halWindow->alarmSound) {
            AudioServicesPlaySystemSound(halWindow->alarmSound);
        }
#else
        /* Use system beep on other platforms */
        printf("\a"); /* Bell character */
        fflush(stdout);
#endif
    }

    /* Show notification dialog if enabled */
    if (alarm->notifyType & NOTIFY_DIALOG) {
        char message[256];
        snprintf(message, sizeof(message),
                "Alarm: %s\nTime: %02d:%02d",
                alarm->name,
                alarm->triggerTime.hour,
                alarm->triggerTime.minute);

        AlarmClock_HAL_ShowNotification(message);
    }
}

/* Show notification dialog */
void AlarmClock_HAL_ShowNotification(const char *message) {
    if (!message) return;

#ifdef __APPLE__
    /* Use system notification */
    CFStringRef cfMessage = CFStringCreateWithCString(
        kCFAllocatorDefault, message, kCFStringEncodingUTF8);

    CFUserNotificationDisplayNotice(
        0,  /* timeout */
        kCFUserNotificationNoteAlertLevel,
        NULL,  /* icon URL */
        NULL,  /* sound URL */
        NULL,  /* localization URL */
        CFSTR("Alarm Clock"),  /* header */
        cfMessage,
        CFSTR("OK")  /* default button */
    );

    CFRelease(cfMessage);

#else
    /* Console notification for other platforms */
    printf("\n*** ALARM NOTIFICATION ***\n%s\n", message);
    fflush(stdout);
#endif
}

/* Get system time */
void AlarmClock_HAL_GetSystemTime(DateTime *dt) {
    if (!dt) return;

    time_t now = time(NULL);
    struct tm *tm = localtime(&now);

    dt->year = tm->tm_year + 1900;
    dt->month = tm->tm_mon + 1;
    dt->day = tm->tm_mday;
    dt->hour = tm->tm_hour;
    dt->minute = tm->tm_min;
    dt->second = tm->tm_sec;
    dt->weekday = tm->tm_wday;
}

/* Set alarm time (for dialog) */
Boolean AlarmClock_HAL_SetAlarmDialog(Alarm *alarm) {
    if (!alarm) return false;

    /* For now, just set a default time */
    /* In a full implementation, this would show a time picker dialog */

    time_t now = time(NULL);
    struct tm *tm = localtime(&now);

    /* Set alarm for next occurrence of specified time */
    alarm->triggerTime.year = tm->tm_year + 1900;
    alarm->triggerTime.month = tm->tm_mon + 1;
    alarm->triggerTime.day = tm->tm_mday;
    /* Keep hour/minute as set by user */
    alarm->triggerTime.second = 0;

    return true;
}

/* Handle window events */
void AlarmClock_HAL_HandleEvent(WindowPtr window, EventRecord *event) {
    if (!window || !event) return;

    HAL_AlarmWindow *halWindow = (HAL_AlarmWindow*)window->refCon;
    if (!halWindow) return;

    switch (event->what) {
        case updateEvt:
            AlarmClock_HAL_DrawClock(halWindow);
            break;

        case mouseDown:
            /* Handle click on alarm area to set new alarm */
            if (PtInRect(event->where, &halWindow->clock->alarmRect)) {
                /* Create simple alarm dialog */
                DateTime alarmTime;
                AlarmClock_HAL_GetSystemTime(&alarmTime);
                alarmTime.minute += 1;  /* Set for 1 minute from now */

                if (alarmTime.minute >= 60) {
                    alarmTime.minute -= 60;
                    alarmTime.hour++;
                    if (alarmTime.hour >= 24) {
                        alarmTime.hour = 0;
                    }
                }

                AlarmClock_CreateAlarm(halWindow->clock,
                                     "Test Alarm",
                                     &alarmTime,
                                     ALARM_TYPE_ONCE);
            }
            break;

        case keyDown:
            /* Handle keyboard shortcuts */
            switch (event->message & charCodeMask) {
                case 'q':
                case 'Q':
                    /* Quit alarm clock */
                    AlarmClock_HAL_DisposeWindow(window);
                    break;

                case 'a':
                case 'A':
                    /* Add new alarm */
                    {
                        DateTime alarmTime;
                        AlarmClock_HAL_GetSystemTime(&alarmTime);
                        alarmTime.minute += 5;  /* 5 minutes from now */

                        AlarmClock_CreateAlarm(halWindow->clock,
                                             "Quick Alarm",
                                             &alarmTime,
                                             ALARM_TYPE_ONCE);
                    }
                    break;
            }
            break;
    }
}