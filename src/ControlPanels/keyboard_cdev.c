/*
 * Simple Keyboard control panel window.
 * Placeholder UI for key repeat and delay settings.
 */

#include <stdio.h>
#include <string.h>

#define _FILE_DEFINED

#include "SystemTypes.h"
#include "System71StdLib.h"

#include "ControlPanels/Keyboard.h"

#include "ControlManager/ControlManager.h"
#include "ControlManager/ControlTypes.h"
#include "QuickDraw/QuickDraw.h"
#include "WindowManager/WindowManager.h"
#include "FontManager/FontInternal.h"

extern QDGlobals qd;

typedef struct KeyboardPanelState {
    Boolean isOpen;
    WindowPtr window;
    ControlHandle repeatSlower;
    ControlHandle repeatFaster;
    ControlHandle delayShorter;
    ControlHandle delayLonger;
    ControlHandle beepCheckbox;
    SInt16 repeatRate; /* 1..10 */
    SInt16 delayTicks; /* 1..10 */
    Boolean keyClick;
} KeyboardPanelState;

static KeyboardPanelState gKeyboardState = {0};

static void cstr_to_pstr(const char *src, Str255 dst)
{
    if (!src || !dst) return;
    size_t len = strlen(src);
    if (len > 255) len = 255;
    dst[0] = (UInt8)len;
    memcpy(&dst[1], src, len);
}

static void dispose_controls(void)
{
    if (gKeyboardState.repeatSlower) {
        DisposeControl(gKeyboardState.repeatSlower);
        gKeyboardState.repeatSlower = NULL;
    }
    if (gKeyboardState.repeatFaster) {
        DisposeControl(gKeyboardState.repeatFaster);
        gKeyboardState.repeatFaster = NULL;
    }
    if (gKeyboardState.delayShorter) {
        DisposeControl(gKeyboardState.delayShorter);
        gKeyboardState.delayShorter = NULL;
    }
    if (gKeyboardState.delayLonger) {
        DisposeControl(gKeyboardState.delayLonger);
        gKeyboardState.delayLonger = NULL;
    }
    if (gKeyboardState.beepCheckbox) {
        DisposeControl(gKeyboardState.beepCheckbox);
        gKeyboardState.beepCheckbox = NULL;
    }
}

static void ensure_controls(void)
{
    if (!gKeyboardState.window) return;

    Rect portRect = gKeyboardState.window->port.portRect;
    Rect buttonRect;

    if (!gKeyboardState.repeatSlower) {
        buttonRect.top = portRect.bottom - 90;
        buttonRect.bottom = buttonRect.top + 20;
        buttonRect.left = 20;
        buttonRect.right = buttonRect.left + 120;
        gKeyboardState.repeatSlower = NewControl(
            gKeyboardState.window,
            &buttonRect,
            "\017Slower Repeat",
            true,
            0,
            0,
            0,
            pushButProc,
            0
        );
    }

    if (!gKeyboardState.repeatFaster) {
        buttonRect.left = 160;
        buttonRect.right = buttonRect.left + 120;
        gKeyboardState.repeatFaster = NewControl(
            gKeyboardState.window,
            &buttonRect,
            "\017Faster Repeat",
            true,
            0,
            0,
            0,
            pushButProc,
            0
        );
    }

    if (!gKeyboardState.delayShorter) {
        buttonRect.top = portRect.bottom - 60;
        buttonRect.bottom = buttonRect.top + 20;
        buttonRect.left = 20;
        buttonRect.right = buttonRect.left + 120;
        gKeyboardState.delayShorter = NewControl(
            gKeyboardState.window,
            &buttonRect,
            "\021Shorter Delay",
            true,
            0,
            0,
            0,
            pushButProc,
            0
        );
    }

    if (!gKeyboardState.delayLonger) {
        buttonRect.left = 160;
        buttonRect.right = buttonRect.left + 120;
        gKeyboardState.delayLonger = NewControl(
            gKeyboardState.window,
            &buttonRect,
            "\017Longer Delay",
            true,
            0,
            0,
            0,
            pushButProc,
            0
        );
    }

    if (!gKeyboardState.beepCheckbox) {
        Rect checkRect;
        checkRect.top = portRect.bottom - 30;
        checkRect.bottom = checkRect.top + 18;
        checkRect.left = 20;
        checkRect.right = portRect.right - 20;
        gKeyboardState.beepCheckbox = NewControl(
            gKeyboardState.window,
            &checkRect,
            "\020Key Click Sound",
            true,
            gKeyboardState.keyClick ? 1 : 0,
            0,
            1,
            checkBoxProc,
            0
        );
    } else {
        SetControlValue(gKeyboardState.beepCheckbox, gKeyboardState.keyClick ? 1 : 0);
    }
}

static void request_redraw(void)
{
    if (!gKeyboardState.window) return;
    GrafPtr savePort;
    GetPort(&savePort);
    SetPort((GrafPtr)gKeyboardState.window);
    Rect r = gKeyboardState.window->port.portRect;
    InvalRect(&r);
    SetPort(savePort);
}

static void draw_contents(void)
{
    if (!gKeyboardState.window) return;

    GrafPtr savePort;
    GetPort(&savePort);
    SetPort((GrafPtr)gKeyboardState.window);

    Rect r = gKeyboardState.window->port.portRect;
    EraseRect(&r);

    char buffer[64];
    snprintf(buffer, sizeof(buffer), "Key Repeat Rate: %d", gKeyboardState.repeatRate);
    Str255 str;
    cstr_to_pstr(buffer, str);
    MoveTo(20, 40);
    DrawString(str);

    snprintf(buffer, sizeof(buffer), "Delay Until Repeat: %d", gKeyboardState.delayTicks);
    cstr_to_pstr(buffer, str);
    MoveTo(20, 60);
    DrawString(str);

    if (gKeyboardState.keyClick) {
        cstr_to_pstr("Key click sound: Enabled", str);
    } else {
        cstr_to_pstr("Key click sound: Disabled", str);
    }
    MoveTo(20, 100);
    DrawString(str);

    const char *tip = "Adjust keyboard responsiveness to your preference.";
    cstr_to_pstr(tip, str);
    MoveTo(20, 120);
    DrawString(str);

    SetPort(savePort);
}

static void adjust_repeat(SInt16 delta)
{
    SInt16 newValue = gKeyboardState.repeatRate + delta;
    if (newValue < 1) newValue = 1;
    if (newValue > 10) newValue = 10;
    if (newValue != gKeyboardState.repeatRate) {
        gKeyboardState.repeatRate = newValue;
        request_redraw();
    }
}

static void adjust_delay(SInt16 delta)
{
    SInt16 newValue = gKeyboardState.delayTicks + delta;
    if (newValue < 1) newValue = 1;
    if (newValue > 10) newValue = 10;
    if (newValue != gKeyboardState.delayTicks) {
        gKeyboardState.delayTicks = newValue;
        request_redraw();
    }
}

void KeyboardPanel_Open(void)
{
    if (gKeyboardState.isOpen && gKeyboardState.window) {
        SelectWindow(gKeyboardState.window);
        return;
    }

    Rect bounds;
    bounds.top = 140;
    bounds.left = 220;
    bounds.bottom = bounds.top + 190;
    bounds.right = bounds.left + 320;

    static unsigned char title[] = {8, 'K','e','y','b','o','a','r','d'};
    gKeyboardState.window = NewWindow(NULL, &bounds, title, true, documentProc, (WindowPtr)-1, true, 0);
    if (!gKeyboardState.window) {
        return;
    }

    if (gKeyboardState.repeatRate == 0) gKeyboardState.repeatRate = 5;
    if (gKeyboardState.delayTicks == 0) gKeyboardState.delayTicks = 5;

    gKeyboardState.isOpen = true;
    ensure_controls();
    draw_contents();
    DrawControls(gKeyboardState.window);
    ShowWindow(gKeyboardState.window);
}

void KeyboardPanel_Close(void)
{
    if (!gKeyboardState.isOpen) return;

    dispose_controls();
    if (gKeyboardState.window) {
        DisposeWindow(gKeyboardState.window);
        gKeyboardState.window = NULL;
    }

    gKeyboardState.isOpen = false;
}

Boolean KeyboardPanel_IsOpen(void)
{
    return gKeyboardState.isOpen;
}

WindowPtr KeyboardPanel_GetWindow(void)
{
    return gKeyboardState.window;
}

Boolean KeyboardPanel_HandleEvent(EventRecord *event)
{
    if (!event || !gKeyboardState.isOpen || !gKeyboardState.window) {
        return false;
    }

    WindowPtr whichWindow = NULL;
    switch (event->what) {
        case updateEvt:
            if ((WindowPtr)(uintptr_t)event->message != gKeyboardState.window) {
                return false;
            }
            BeginUpdate(gKeyboardState.window);
            ensure_controls();
            draw_contents();
            DrawControls(gKeyboardState.window);
            EndUpdate(gKeyboardState.window);
            return true;

        case activateEvt:
            if ((WindowPtr)(uintptr_t)event->message == gKeyboardState.window) {
                Boolean active = (event->modifiers & activeFlag) != 0;
                if (gKeyboardState.repeatSlower) {
                    HiliteControl(gKeyboardState.repeatSlower, active ? noHilite : inactiveHilite);
                }
                if (gKeyboardState.repeatFaster) {
                    HiliteControl(gKeyboardState.repeatFaster, active ? noHilite : inactiveHilite);
                }
                if (gKeyboardState.delayShorter) {
                    HiliteControl(gKeyboardState.delayShorter, active ? noHilite : inactiveHilite);
                }
                if (gKeyboardState.delayLonger) {
                    HiliteControl(gKeyboardState.delayLonger, active ? noHilite : inactiveHilite);
                }
                if (gKeyboardState.beepCheckbox) {
                    HiliteControl(gKeyboardState.beepCheckbox, active ? noHilite : inactiveHilite);
                }
                return true;
            }
            return false;

        case mouseDown: {
            short part = FindWindow(event->where, &whichWindow);
            if (whichWindow != gKeyboardState.window) {
                return false;
            }

            switch (part) {
                case inGoAway:
                    if (TrackGoAway(gKeyboardState.window, event->where)) {
                        KeyboardPanel_Close();
                    }
                    return true;

                case inDrag: {
                    Rect limit = qd.screenBits.bounds;
                    DragWindow(gKeyboardState.window, event->where, &limit);
                    return true;
                }

                case inContent: {
                    SelectWindow(gKeyboardState.window);
                    SetPort((GrafPtr)gKeyboardState.window);
                    Point localPt = event->where;
                    GlobalToLocal(&localPt);
                    ControlHandle ctl;
                    short ctlPart = FindControl(localPt, gKeyboardState.window, &ctl);
                    if (ctlPart != 0 && ctl != NULL) {
                        TrackControl(ctl, localPt, NULL);
                        if (ctl == gKeyboardState.repeatSlower) {
                            adjust_repeat(-1);
                        } else if (ctl == gKeyboardState.repeatFaster) {
                            adjust_repeat(1);
                        } else if (ctl == gKeyboardState.delayShorter) {
                            adjust_delay(-1);
                        } else if (ctl == gKeyboardState.delayLonger) {
                            adjust_delay(1);
                        } else if (ctl == gKeyboardState.beepCheckbox) {
                            gKeyboardState.keyClick = !gKeyboardState.keyClick;
                            SetControlValue(gKeyboardState.beepCheckbox, gKeyboardState.keyClick ? 1 : 0);
                        }
                        draw_contents();
                        DrawControls(gKeyboardState.window);
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
