/*
 * Simple Mouse control panel window.
 * Placeholder UI for tracking speed and button swap options.
 */

#include <stdio.h>
#include <string.h>

#define _FILE_DEFINED

#include "SystemTypes.h"
#include "System71StdLib.h"

#include "ControlPanels/Mouse.h"

#include "ControlManager/ControlManager.h"
#include "ControlManager/ControlTypes.h"
#include "QuickDraw/QuickDraw.h"
#include "WindowManager/WindowManager.h"
#include "FontManager/FontInternal.h"

extern QDGlobals qd;

typedef struct MousePanelState {
    Boolean isOpen;
    WindowPtr window;
    ControlHandle slowerButton;
    ControlHandle fasterButton;
    ControlHandle swapCheckbox;
    ControlHandle doubleClickCheckbox;
    SInt16 trackingSpeed; /* 1..10 */
    Boolean swapButtons;
    Boolean doubleClickAssist;
} MousePanelState;

static MousePanelState gMouseState = {0};

static void cstr_to_pstr(const char *src, Str255 dst)
{
    if (!src || !dst) {
        return;
    }
    size_t len = strlen(src);
    if (len > 255) len = 255;
    dst[0] = (UInt8)len;
    memcpy(&dst[1], src, len);
}

static void dispose_controls(void)
{
    if (gMouseState.slowerButton) {
        DisposeControl(gMouseState.slowerButton);
        gMouseState.slowerButton = NULL;
    }
    if (gMouseState.fasterButton) {
        DisposeControl(gMouseState.fasterButton);
        gMouseState.fasterButton = NULL;
    }
    if (gMouseState.swapCheckbox) {
        DisposeControl(gMouseState.swapCheckbox);
        gMouseState.swapCheckbox = NULL;
    }
    if (gMouseState.doubleClickCheckbox) {
        DisposeControl(gMouseState.doubleClickCheckbox);
        gMouseState.doubleClickCheckbox = NULL;
    }
}

static void ensure_controls(void)
{
    if (!gMouseState.window) return;

    Rect portRect = gMouseState.window->port.portRect;
    Rect buttonRect;

    if (!gMouseState.slowerButton) {
        buttonRect.top = portRect.bottom - 80;
        buttonRect.bottom = buttonRect.top + 20;
        buttonRect.left = 20;
        buttonRect.right = buttonRect.left + 120;
        gMouseState.slowerButton = NewControl(
            gMouseState.window,
            &buttonRect,
            "\017Slower Tracking",
            true,
            0,
            0,
            0,
            pushButProc,
            0
        );
    }

    if (!gMouseState.fasterButton) {
        buttonRect.left = 160;
        buttonRect.right = buttonRect.left + 120;
        gMouseState.fasterButton = NewControl(
            gMouseState.window,
            &buttonRect,
            "\017Faster Tracking",
            true,
            0,
            0,
            0,
            pushButProc,
            0
        );
    }

    if (!gMouseState.swapCheckbox) {
        Rect checkRect;
        checkRect.top = portRect.bottom - 50;
        checkRect.bottom = checkRect.top + 18;
        checkRect.left = 20;
        checkRect.right = portRect.right - 20;
        gMouseState.swapCheckbox = NewControl(
            gMouseState.window,
            &checkRect,
            "\021Swap Left/Right Buttons",
            true,
            gMouseState.swapButtons ? 1 : 0,
            0,
            1,
            checkBoxProc,
            0
        );
    } else {
        SetControlValue(gMouseState.swapCheckbox, gMouseState.swapButtons ? 1 : 0);
    }

    if (!gMouseState.doubleClickCheckbox) {
        Rect checkRect;
        checkRect.top = portRect.bottom - 30;
        checkRect.bottom = checkRect.top + 18;
        checkRect.left = 20;
        checkRect.right = portRect.right - 20;
        gMouseState.doubleClickCheckbox = NewControl(
            gMouseState.window,
            &checkRect,
            "\025Assist with Double-Clicking",
            true,
            gMouseState.doubleClickAssist ? 1 : 0,
            0,
            1,
            checkBoxProc,
            0
        );
    } else {
        SetControlValue(gMouseState.doubleClickCheckbox, gMouseState.doubleClickAssist ? 1 : 0);
    }
}

static void request_redraw(void)
{
    if (!gMouseState.window) return;
    GrafPtr savePort;
    GetPort(&savePort);
    SetPort((GrafPtr)gMouseState.window);
    Rect r = gMouseState.window->port.portRect;
    InvalRect(&r);
    SetPort(savePort);
}

static void draw_contents(void)
{
    if (!gMouseState.window) return;

    GrafPtr savePort;
    GetPort(&savePort);
    SetPort((GrafPtr)gMouseState.window);

    Rect r = gMouseState.window->port.portRect;
    EraseRect(&r);

    char buffer[64];
    snprintf(buffer, sizeof(buffer), "Tracking Speed: %d", gMouseState.trackingSpeed);
    Str255 str;
    cstr_to_pstr(buffer, str);
    MoveTo(20, 40);
    DrawString(str);

    const char *tips = "Adjust how quickly the pointer moves.";
    cstr_to_pstr(tips, str);
    MoveTo(20, 60);
    DrawString(str);

    if (gMouseState.swapButtons) {
        cstr_to_pstr("Left-handed mode: ON", str);
    } else {
        cstr_to_pstr("Left-handed mode: OFF", str);
    }
    MoveTo(20, 100);
    DrawString(str);

    if (gMouseState.doubleClickAssist) {
        cstr_to_pstr("Double-click assistance enabled.", str);
    } else {
        cstr_to_pstr("Double-click assistance disabled.", str);
    }
    MoveTo(20, 120);
    DrawString(str);

    SetPort(savePort);
}

void MousePanel_Open(void)
{
    if (gMouseState.isOpen && gMouseState.window) {
        SelectWindow(gMouseState.window);
        return;
    }

    Rect bounds;
    bounds.top = 120;
    bounds.left = 180;
    bounds.bottom = bounds.top + 180;
    bounds.right = bounds.left + 320;

    static unsigned char title[] = {5, 'M','o','u','s','e'};
    gMouseState.window = NewWindow(NULL, &bounds, title, true, documentProc, (WindowPtr)-1, true, 0);
    if (!gMouseState.window) {
        return;
    }

    if (gMouseState.trackingSpeed == 0) {
        gMouseState.trackingSpeed = 5;
    }

    gMouseState.isOpen = true;
    ensure_controls();
    draw_contents();
    DrawControls(gMouseState.window);
    ShowWindow(gMouseState.window);
}

void MousePanel_Close(void)
{
    if (!gMouseState.isOpen) return;

    dispose_controls();
    if (gMouseState.window) {
        DisposeWindow(gMouseState.window);
        gMouseState.window = NULL;
    }

    gMouseState.isOpen = false;
}

Boolean MousePanel_IsOpen(void)
{
    return gMouseState.isOpen;
}

WindowPtr MousePanel_GetWindow(void)
{
    return gMouseState.window;
}

static void adjust_tracking(SInt16 delta)
{
    SInt16 newValue = gMouseState.trackingSpeed + delta;
    if (newValue < 1) newValue = 1;
    if (newValue > 10) newValue = 10;
    if (newValue != gMouseState.trackingSpeed) {
        gMouseState.trackingSpeed = newValue;
        request_redraw();
    }
}

Boolean MousePanel_HandleEvent(EventRecord *event)
{
    if (!event || !gMouseState.isOpen || !gMouseState.window) {
        return false;
    }

    WindowPtr whichWindow = NULL;
    switch (event->what) {
        case updateEvt:
            if ((WindowPtr)(uintptr_t)event->message != gMouseState.window) {
                return false;
            }
            BeginUpdate(gMouseState.window);
            ensure_controls();
            draw_contents();
            DrawControls(gMouseState.window);
            EndUpdate(gMouseState.window);
            return true;

        case activateEvt:
            if ((WindowPtr)(uintptr_t)event->message == gMouseState.window) {
                Boolean active = (event->modifiers & activeFlag) != 0;
                if (gMouseState.slowerButton) {
                    HiliteControl(gMouseState.slowerButton, active ? noHilite : inactiveHilite);
                }
                if (gMouseState.fasterButton) {
                    HiliteControl(gMouseState.fasterButton, active ? noHilite : inactiveHilite);
                }
                if (gMouseState.swapCheckbox) {
                    HiliteControl(gMouseState.swapCheckbox, active ? noHilite : inactiveHilite);
                }
                if (gMouseState.doubleClickCheckbox) {
                    HiliteControl(gMouseState.doubleClickCheckbox, active ? noHilite : inactiveHilite);
                }
                return true;
            }
            return false;

        case mouseDown: {
            short part = FindWindow(event->where, &whichWindow);
            if (whichWindow != gMouseState.window) {
                return false;
            }

            switch (part) {
                case inGoAway:
                    if (TrackGoAway(gMouseState.window, event->where)) {
                        MousePanel_Close();
                    }
                    return true;

                case inDrag: {
                    Rect limit = qd.screenBits.bounds;
                    DragWindow(gMouseState.window, event->where, &limit);
                    return true;
                }

                case inContent: {
                    SelectWindow(gMouseState.window);
                    SetPort((GrafPtr)gMouseState.window);
                    Point localPt = event->where;
                    GlobalToLocal(&localPt);
                    ControlHandle ctl;
                    short ctlPart = FindControl(localPt, gMouseState.window, &ctl);
                    if (ctlPart != 0 && ctl != NULL) {
                        TrackControl(ctl, localPt, NULL);
                        if (ctl == gMouseState.slowerButton) {
                            adjust_tracking(-1);
                        } else if (ctl == gMouseState.fasterButton) {
                            adjust_tracking(1);
                        } else if (ctl == gMouseState.swapCheckbox) {
                            gMouseState.swapButtons = !gMouseState.swapButtons;
                            SetControlValue(gMouseState.swapCheckbox, gMouseState.swapButtons ? 1 : 0);
                            request_redraw();
                        } else if (ctl == gMouseState.doubleClickCheckbox) {
                            gMouseState.doubleClickAssist = !gMouseState.doubleClickAssist;
                            SetControlValue(gMouseState.doubleClickCheckbox, gMouseState.doubleClickAssist ? 1 : 0);
                            request_redraw();
                        }
                        draw_contents();
                        DrawControls(gMouseState.window);
                        return true;
                    }
                    break;
                }

                default:
                    break;
            }
            return true;
        }

        default:
            break;
    }

    return false;
}
