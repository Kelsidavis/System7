/*
 * Control strip style launcher palette.
 * Provides quick buttons to open the existing control panels.
 */

#define _FILE_DEFINED

#include <string.h>

#include "SystemTypes.h"
#include "System71StdLib.h"

#include "ControlPanels/ControlStrip.h"
#include "ControlPanels/DesktopPatterns.h"
#include "ControlPanels/Sound.h"
#include "ControlPanels/Mouse.h"
#include "ControlPanels/Keyboard.h"
#include "Datetime/datetime_cdev.h"

#include "ControlManager/ControlManager.h"
#include "ControlManager/ControlTypes.h"
#include "QuickDraw/QuickDraw.h"
#include "WindowManager/WindowManager.h"
#include "FontManager/FontInternal.h"
#include "EventManager/EventManager.h"

extern QDGlobals qd;

typedef struct ControlStripState {
    Boolean isVisible;
    WindowPtr window;
    ControlHandle buttons[5];
} ControlStripState;

static ControlStripState gControlStrip = {0};

static const char *kButtonLabels[5] = {
    "Desktop Patterns",
    "Date & Time",
    "Sound",
    "Mouse",
    "Keyboard"
};

static void create_buttons(void)
{
    if (!gControlStrip.window) {
        return;
    }

    Rect portRect = gControlStrip.window->port.portRect;
    Rect buttonRect;
    buttonRect.left = 12;
    buttonRect.right = portRect.right - 12;

    for (int i = 0; i < 5; i++) {
        if (gControlStrip.buttons[i]) {
            continue;
        }

        buttonRect.top = 30 + i * 28;
        buttonRect.bottom = buttonRect.top + 22;

        Str255 label;
        size_t len = strlen(kButtonLabels[i]);
        if (len > 255) len = 255;
        label[0] = (UInt8)len;
        memcpy(&label[1], kButtonLabels[i], len);

        gControlStrip.buttons[i] = NewControl(
            gControlStrip.window,
            &buttonRect,
            label,
            true,
            0,
            0,
            0,
            pushButProc,
            0
        );
    }
}

static void dispose_buttons(void)
{
    for (int i = 0; i < 5; i++) {
        if (gControlStrip.buttons[i]) {
            DisposeControl(gControlStrip.buttons[i]);
            gControlStrip.buttons[i] = NULL;
        }
    }
}

static void draw_contents(void)
{
    if (!gControlStrip.window) {
        return;
    }

    GrafPtr savePort;
    GetPort(&savePort);
    SetPort((GrafPtr)gControlStrip.window);

    Rect r = gControlStrip.window->port.portRect;
    EraseRect(&r);

    Str255 title;
    title[0] = 13;
    memcpy(&title[1], "Control Strip", 13);
    MoveTo(12, 20);
    DrawString(title);

    DrawControls(gControlStrip.window);

    SetPort(savePort);
}

void ControlStrip_Show(void)
{
    if (gControlStrip.isVisible && gControlStrip.window) {
        SelectWindow(gControlStrip.window);
        return;
    }

    short screenLeft = qd.screenBits.bounds.left;
    short screenRight = qd.screenBits.bounds.right;
    short screenTop = qd.screenBits.bounds.top;
    if (screenRight <= screenLeft) {
        screenRight = screenLeft + 640;
    }

    Rect bounds;
    bounds.left = screenRight - 220;
    bounds.right = screenRight - 20;
    bounds.top = screenTop + 60;
    bounds.bottom = bounds.top + 220;

    static unsigned char title[] = {13, 'C','o','n','t','r','o','l',' ','S','t','r','i','p'};
    gControlStrip.window = NewWindow(NULL, &bounds, title, true, documentProc, (WindowPtr)-1, true, 0);
    if (!gControlStrip.window) {
        return;
    }

    gControlStrip.isVisible = true;
    create_buttons();
    draw_contents();
    ShowWindow(gControlStrip.window);
    SelectWindow(gControlStrip.window);
    PostEvent(updateEvt, (UInt32)(uintptr_t)gControlStrip.window);
}

void ControlStrip_Hide(void)
{
    if (!gControlStrip.isVisible) {
        return;
    }

    dispose_buttons();
    if (gControlStrip.window) {
        DisposeWindow(gControlStrip.window);
        gControlStrip.window = NULL;
    }
    gControlStrip.isVisible = false;
}

void ControlStrip_Toggle(void)
{
    if (gControlStrip.isVisible) {
        ControlStrip_Hide();
    } else {
        ControlStrip_Show();
    }
}

static void launch_panel(int index)
{
    switch (index) {
        case 0: OpenDesktopCdev(); break;
        case 1: DateTimePanel_Open(); break;
        case 2: SoundPanel_Open(); break;
        case 3: MousePanel_Open(); break;
        case 4: KeyboardPanel_Open(); break;
        default: break;
    }
}

Boolean ControlStrip_HandleEvent(EventRecord *event)
{
    if (!event || !gControlStrip.isVisible || !gControlStrip.window) {
        return false;
    }

    WindowPtr target = NULL;
    Point localPt;
    switch (event->what) {
        case updateEvt:
            if ((WindowPtr)(uintptr_t)event->message != gControlStrip.window) {
                return false;
            }
            BeginUpdate(gControlStrip.window);
            draw_contents();
            EndUpdate(gControlStrip.window);
            return true;

        case activateEvt:
            if ((WindowPtr)(uintptr_t)event->message == gControlStrip.window) {
                Boolean active = (event->modifiers & activeFlag) != 0;
                for (int i = 0; i < 5; i++) {
                    if (gControlStrip.buttons[i]) {
                        HiliteControl(gControlStrip.buttons[i], active ? noHilite : inactiveHilite);
                    }
                }
                return true;
            }
            return false;

        case mouseDown: {
            short part = FindWindow(event->where, &target);
            if (target != gControlStrip.window) {
                return false;
            }

            switch (part) {
                case inGoAway:
                    if (TrackGoAway(gControlStrip.window, event->where)) {
                        ControlStrip_Hide();
                    }
                    return true;

                case inDrag: {
                    Rect limit = qd.screenBits.bounds;
                    DragWindow(gControlStrip.window, event->where, &limit);
                    return true;
                }

                case inContent: {
                    SelectWindow(gControlStrip.window);
                    SetPort((GrafPtr)gControlStrip.window);
                    localPt = event->where;
                    GlobalToLocal(&localPt);
                    ControlHandle ctl;
                    short ctlPart = FindControl(localPt, gControlStrip.window, &ctl);
                    if (ctlPart != 0 && ctl != NULL) {
                        TrackControl(ctl, localPt, NULL);
                        for (int i = 0; i < 5; i++) {
                            if (ctl == gControlStrip.buttons[i]) {
                                launch_panel(i);
                                break;
                            }
                        }
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
