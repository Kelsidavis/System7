/**
 * @file datetime_cdev.c
 * @brief Minimal Date & Time control panel window.
 */

#include "SystemTypes.h"
#include "System71StdLib.h"
#include "OSUtils/OSUtils.h"

#include "Datetime/datetime_cdev.h"

#include "ControlManager/ControlManager.h"
#include "../ControlPanels/ControlPanelInternal.h"
#include "ControlManager/ControlTypes.h"
#include "MenuManager/MenuManager.h"
#include "QuickDraw/QuickDraw.h"
#include "WindowManager/WindowManager.h"
#include "FontManager/FontInternal.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>


/* ----------------------------------------------------------------------------
 * Helpers
 * ---------------------------------------------------------------------------- */

#define PANEL_WIDTH   320
#define PANEL_HEIGHT  180
typedef struct DateTimePanelState {
    Boolean    isOpen;
    Boolean    showSeconds;
    Boolean    use24Hour;
    WindowPtr  window;
    ControlHandle showSecondsCheck;
    ControlHandle use24HourCheck;
    UInt32     lastDisplayKey;
    Rect       textRect;
} DateTimePanelState;

static DateTimePanelState gPanel = {
    .isOpen = false,
    .showSeconds = false,
    .use24Hour = false,
    .window = NULL,
    .showSecondsCheck = NULL,
    .use24HourCheck = NULL,
    .lastDisplayKey = 0,
    .textRect = { 40, 20, 100, PANEL_WIDTH - 20 }
};

static const char *kWeekdayNames[7] = {
    "Sunday",
    "Monday",
    "Tuesday",
    "Wednesday",
    "Thursday",
    "Friday",
    "Saturday"
};

static const char *kMonthNames[12] = {
    "January",
    "February",
    "March",
    "April",
    "May",
    "June",
    "July",
    "August",
    "September",
    "October",
    "November",
    "December"
};

static UInt32 current_mac_time(void)
{
    /* Ask the system clock directly. Going out to Unix time and back only
     * added a conversion either side of the same answer. */
    UInt32 secs = 0;
    GetDateTime(&secs);
    return secs;
}

static UInt32 compute_display_key(UInt32 macTime)
{
    return gPanel.showSeconds ? macTime : (macTime / 60U);
}

static void format_date_string(const DateTimeRec* parts, char* buffer, size_t bufferSize)
{
    if (!buffer || bufferSize == 0) {
        return;
    }

    const char *weekday = (parts->dayOfWeek >= 1 && parts->dayOfWeek <= 7)
        ? kWeekdayNames[parts->dayOfWeek - 1] : "";
    const char *month = (parts->month >= 1 && parts->month <= 12)
        ? kMonthNames[parts->month - 1] : "";

    snprintf(buffer, bufferSize, "%s, %s %d, %d",
             weekday, month, parts->day, parts->year);
}

static void format_time_string(const DateTimeRec* parts, char* buffer, size_t bufferSize)
{
    if (!buffer || bufferSize == 0) {
        return;
    }

    int hour = parts->hour;
    const char *suffix = "";

    if (!gPanel.use24Hour) {
        suffix = (hour >= 12) ? " PM" : " AM";
        hour = hour % 12;
        if (hour == 0) {
            hour = 12;
        }
    }

    if (gPanel.showSeconds) {
        if (gPanel.use24Hour) {
            snprintf(buffer, bufferSize, "%02d:%02d:%02d",
                     parts->hour, parts->minute, parts->second);
        } else {
            snprintf(buffer, bufferSize, "%02d:%02d:%02d%s",
                     hour, parts->minute, parts->second, suffix);
        }
    } else {
        if (gPanel.use24Hour) {
            snprintf(buffer, bufferSize, "%02d:%02d",
                     parts->hour, parts->minute);
        } else {
            snprintf(buffer, bufferSize, "%02d:%02d%s",
                     hour, parts->minute, suffix);
        }
    }
}

static void draw_panel_contents(void)
{
    if (!gPanel.window) {
        return;
    }

    GrafPtr previousPort;
    GetPort(&previousPort);
    SetPort((GrafPtr)gPanel.window);

    /* Clear the text area */
    Rect drawRect = gPanel.textRect;
    EraseRect(&drawRect);

    UInt32 macTime = current_mac_time();
    gPanel.lastDisplayKey = compute_display_key(macTime);

    DateTimeRec parts = {0};
    Secs2Date(macTime, &parts);

    char dateBuf[64];
    char timeBuf[32];
    format_date_string(&parts, dateBuf, sizeof(dateBuf));
    format_time_string(&parts, timeBuf, sizeof(timeBuf));

    Str255 pascalStr;
    MoveTo(gPanel.textRect.left, gPanel.textRect.top + 20);
    c2pstrcpy(pascalStr, dateBuf);
    DrawString(pascalStr);

    MoveTo(gPanel.textRect.left, gPanel.textRect.top + 44);
    c2pstrcpy(pascalStr, timeBuf);
    DrawString(pascalStr);

    SetPort(previousPort);
}

static void update_checkbox(ControlHandle ctrl, Boolean checked)
{
    if (!ctrl) {
        return;
    }
    SetControlValue(ctrl, checked ? 1 : 0);
    Draw1Control(ctrl);
}

static void ensure_controls(void)
{
    if (!gPanel.window) {
        return;
    }

    Rect checkboxRect;
    Rect portRect = gPanel.window->port.portRect;

    if (!gPanel.use24HourCheck) {
        checkboxRect.top = portRect.bottom - 60;
        checkboxRect.left = 20;
        checkboxRect.bottom = checkboxRect.top + 18;
        checkboxRect.right = portRect.right - 20;
        gPanel.use24HourCheck = NewControl(
            gPanel.window,
            &checkboxRect,
            PSTR("24-Hour Time"),
            true,
            gPanel.use24Hour ? 1 : 0,
            0,
            1,
            checkBoxProc,
            0
        );
    } else {
        update_checkbox(gPanel.use24HourCheck, gPanel.use24Hour);
    }

    if (!gPanel.showSecondsCheck) {
        checkboxRect.top = portRect.bottom - 36;
        checkboxRect.left = 20;
        checkboxRect.bottom = checkboxRect.top + 18;
        checkboxRect.right = portRect.right - 20;
        gPanel.showSecondsCheck = NewControl(
            gPanel.window,
            &checkboxRect,
            PSTR("Show Seconds"),
            true,
            gPanel.showSeconds ? 1 : 0,
            0,
            1,
            checkBoxProc,
            0
        );
    } else {
        update_checkbox(gPanel.showSecondsCheck, gPanel.showSeconds);
    }
}

static void destroy_controls(void)
{
    ControlHandle* controls[] = {
        &gPanel.use24HourCheck,
        &gPanel.showSecondsCheck
    };
    ControlPanel_DisposeControls(controls, sizeof(controls) / sizeof(controls[0]));
}

static void request_redraw(void)
{
    if (!gPanel.window) {
        return;
    }
    GrafPtr previousPort;
    GetPort(&previousPort);
    SetPort((GrafPtr)gPanel.window);
    Rect r = gPanel.textRect;
    InvalRect(&r);
    SetPort(previousPort);
}

/* ----------------------------------------------------------------------------
 * Public API
 * ---------------------------------------------------------------------------- */

void DateTimePanel_Open(void)
{
    if (gPanel.isOpen && gPanel.window) {
        SelectWindow(gPanel.window);
        return;
    }

    Rect bounds;
    bounds.top = 80;
    bounds.left = 120;
    bounds.bottom = bounds.top + PANEL_HEIGHT;
    bounds.right = bounds.left + PANEL_WIDTH;

    static Str255 title;
    c2pstrcpy(title, "Date & Time");
    gPanel.window = NewWindow(NULL, &bounds, title, true, documentProc, (WindowPtr)-1, true, 0);
    if (!gPanel.window) {
        return;
    }

    gPanel.isOpen = true;
    gPanel.lastDisplayKey = 0;

    Rect portRect = gPanel.window->port.portRect;
    gPanel.textRect.left = 20;
    gPanel.textRect.top = 40;
    gPanel.textRect.right = portRect.right - 20;
    gPanel.textRect.bottom = gPanel.textRect.top + 80;

    ensure_controls();
    draw_panel_contents();
    ShowWindow(gPanel.window);
}

void DateTimePanel_Close(void)
{
    if (!gPanel.isOpen) {
        return;
    }

    destroy_controls();

    if (gPanel.window) {
        DisposeWindow(gPanel.window);
        gPanel.window = NULL;
    }

    gPanel.isOpen = false;
}

Boolean DateTimePanel_IsOpen(void)
{
    return gPanel.isOpen;
}

WindowPtr DateTimePanel_GetWindow(void)
{
    return gPanel.window;
}

static void toggle_checkbox_state(ControlHandle ctrl, Boolean *flag)
{
    if (!ctrl || !flag) {
        return;
    }

    Boolean newValue = !(*flag);
    *flag = newValue;
    SetControlValue(ctrl, newValue ? 1 : 0);
    Draw1Control(ctrl);

    gPanel.lastDisplayKey = UINT32_MAX;
    request_redraw();
}

Boolean DateTimePanel_HandleEvent(EventRecord *event)
{
    if (!event || !gPanel.isOpen || !gPanel.window) {
        return false;
    }

    WindowPtr whichWindow = NULL;
    Point localPt;
    ControlHandle ctl;
    SInt16 partCode;

    switch (event->what) {
        case updateEvt:
            if ((WindowPtr)(uintptr_t)event->message != gPanel.window) {
                return false;
            }
            BeginUpdate(gPanel.window);
            ensure_controls();
            draw_panel_contents();
            DrawControls(gPanel.window);
            EndUpdate(gPanel.window);
            return true;

        case mouseDown:
            partCode = FindWindow(event->where, &whichWindow);
            if (whichWindow != gPanel.window) {
                return false;
            }

            switch (partCode) {
                case inGoAway:
                    if (TrackGoAway(gPanel.window, event->where)) {
                        DateTimePanel_Close();
                    }
                    return true;

                case inContent:
                    SelectWindow(gPanel.window);
                    SetPort((GrafPtr)gPanel.window);
                    localPt = event->where;
                    GlobalToLocal(&localPt);

                    partCode = FindControl(localPt, gPanel.window, &ctl);
                    if (partCode != 0 && ctl != NULL) {
                        SInt16 trackResult = TrackControl(ctl, localPt, NULL);
                        if (trackResult != 0) {
                            if (ctl == gPanel.use24HourCheck) {
                                toggle_checkbox_state(gPanel.use24HourCheck, &gPanel.use24Hour);
                            } else if (ctl == gPanel.showSecondsCheck) {
                                toggle_checkbox_state(gPanel.showSecondsCheck, &gPanel.showSeconds);
                            }
                        }
                        return true;
                    }
                    return false;

                case inDrag: {
                    Rect limit = qd.screenBits.bounds;
                    DragWindow(gPanel.window, event->where, &limit);
                    return true;
                }

                default:
                    return false;
            }

        case activateEvt:
            if ((WindowPtr)(uintptr_t)event->message == gPanel.window) {
                Boolean becomingActive = (event->modifiers & activeFlag) != 0;
                if (gPanel.use24HourCheck) {
                    HiliteControl(gPanel.use24HourCheck, becomingActive ? noHilite : inactiveHilite);
                }
                if (gPanel.showSecondsCheck) {
                    HiliteControl(gPanel.showSecondsCheck, becomingActive ? noHilite : inactiveHilite);
                }
                return true;
            }
            return false;

        default:
            return false;
    }
}

void DateTimePanel_Tick(void)
{
    if (!gPanel.isOpen || !gPanel.window) {
        return;
    }

    UInt32 now = current_mac_time();
    UInt32 key = compute_display_key(now);
    if (key != gPanel.lastDisplayKey) {
        gPanel.lastDisplayKey = key;
        request_redraw();
    }
}
