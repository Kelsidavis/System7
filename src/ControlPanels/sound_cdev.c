/*
 * Simple Sound control panel window.
 * Provides placeholder UI for volume + mute settings.
 */

#include <stdio.h>
#include <string.h>

#define _FILE_DEFINED

#include "SystemTypes.h"
#include "System71StdLib.h"

#include "ControlPanels/Sound.h"

#include "ControlManager/ControlManager.h"
#include "ControlManager/ControlTypes.h"
#include "QuickDraw/QuickDraw.h"
#include "WindowManager/WindowManager.h"
#include "FontManager/FontInternal.h"

extern QDGlobals qd;

typedef struct SoundPanelState {
    Boolean isOpen;
    WindowPtr window;
    ControlHandle volumeDownButton;
    ControlHandle volumeUpButton;
    ControlHandle muteCheckbox;
    SInt16 volume;
    Boolean muted;
} SoundPanelState;

static SoundPanelState gSoundState = {0};

static void cstr_to_pstr(const char *src, Str255 dst)
{
    if (!src || !dst) {
        return;
    }
    size_t len = strlen(src);
    if (len > 255) {
        len = 255;
    }
    dst[0] = (UInt8)len;
    memcpy(&dst[1], src, len);
}

static void dispose_controls(void)
{
    if (gSoundState.volumeDownButton) {
        DisposeControl(gSoundState.volumeDownButton);
        gSoundState.volumeDownButton = NULL;
    }
    if (gSoundState.volumeUpButton) {
        DisposeControl(gSoundState.volumeUpButton);
        gSoundState.volumeUpButton = NULL;
    }
    if (gSoundState.muteCheckbox) {
        DisposeControl(gSoundState.muteCheckbox);
        gSoundState.muteCheckbox = NULL;
    }
}

static void ensure_controls(void)
{
    if (!gSoundState.window) {
        return;
    }

    Rect portRect = gSoundState.window->port.portRect;
    Rect buttonRect;

    if (!gSoundState.volumeDownButton) {
        buttonRect.top = portRect.bottom - 70;
        buttonRect.bottom = buttonRect.top + 20;
        buttonRect.left = 20;
        buttonRect.right = buttonRect.left + 100;
        gSoundState.volumeDownButton = NewControl(
            gSoundState.window,
            &buttonRect,
            "\013Volume Down",
            true,
            0,
            0,
            0,
            pushButProc,
            0
        );
    }

    if (!gSoundState.volumeUpButton) {
        buttonRect.left = 140;
        buttonRect.right = buttonRect.left + 100;
        gSoundState.volumeUpButton = NewControl(
            gSoundState.window,
            &buttonRect,
            "\011Volume Up",
            true,
            0,
            0,
            0,
            pushButProc,
            0
        );
    }

    if (!gSoundState.muteCheckbox) {
        Rect checkRect;
        checkRect.top = portRect.bottom - 40;
        checkRect.bottom = checkRect.top + 18;
        checkRect.left = 20;
        checkRect.right = portRect.right - 20;
        gSoundState.muteCheckbox = NewControl(
            gSoundState.window,
            &checkRect,
            "\010Mute Sound",
            true,
            gSoundState.muted ? 1 : 0,
            0,
            1,
            checkBoxProc,
            0
        );
    } else {
        SetControlValue(gSoundState.muteCheckbox, gSoundState.muted ? 1 : 0);
    }
}

static void request_redraw(void)
{
    if (!gSoundState.window) {
        return;
    }
    GrafPtr savePort;
    GetPort(&savePort);
    SetPort((GrafPtr)gSoundState.window);
    Rect r = gSoundState.window->port.portRect;
    InvalRect(&r);
    SetPort(savePort);
}

static void draw_contents(void)
{
    if (!gSoundState.window) {
        return;
    }

    GrafPtr savePort;
    GetPort(&savePort);
    SetPort((GrafPtr)gSoundState.window);

    Rect r = gSoundState.window->port.portRect;
    EraseRect(&r);

    char line[64];
    snprintf(line, sizeof(line), "Output Volume: %d%%", gSoundState.volume);
    Str255 volumeStr;
    cstr_to_pstr(line, volumeStr);
    MoveTo(20, 40);
    DrawString(volumeStr);

    const char *muteText = gSoundState.muted ? "Status: Muted" : "Status: Active";
    Str255 muteStr;
    cstr_to_pstr(muteText, muteStr);
    MoveTo(20, 60);
    DrawString(muteStr);

    const char *tipText = "Use the buttons below to adjust volume.";
    Str255 tipStr;
    cstr_to_pstr(tipText, tipStr);
    MoveTo(20, 90);
    DrawString(tipStr);

    SetPort(savePort);
}

static void adjust_volume(SInt16 delta)
{
    if (gSoundState.muted) {
        return;
    }
    SInt16 newVolume = gSoundState.volume + delta;
    if (newVolume < 0) newVolume = 0;
    if (newVolume > 100) newVolume = 100;
    if (newVolume != gSoundState.volume) {
        gSoundState.volume = newVolume;
        request_redraw();
    }
}

void SoundPanel_Open(void)
{
    if (gSoundState.isOpen && gSoundState.window) {
        SelectWindow(gSoundState.window);
        return;
    }

    Rect bounds;
    bounds.top = 100;
    bounds.left = 140;
    bounds.bottom = bounds.top + 160;
    bounds.right = bounds.left + 260;

    static unsigned char title[] = {5, 'S','o','u','n','d'};
    gSoundState.window = NewWindow(NULL, &bounds, title, true, documentProc, (WindowPtr)-1, true, 0);
    if (!gSoundState.window) {
        return;
    }

    gSoundState.isOpen = true;
    if (gSoundState.volume == 0) {
        gSoundState.volume = 70;
    }

    ensure_controls();
    draw_contents();
    DrawControls(gSoundState.window);
    ShowWindow(gSoundState.window);
}

void SoundPanel_Close(void)
{
    if (!gSoundState.isOpen) {
        return;
    }

    dispose_controls();

    if (gSoundState.window) {
        DisposeWindow(gSoundState.window);
        gSoundState.window = NULL;
    }

    gSoundState.isOpen = false;
}

Boolean SoundPanel_IsOpen(void)
{
    return gSoundState.isOpen;
}

WindowPtr SoundPanel_GetWindow(void)
{
    return gSoundState.window;
}

Boolean SoundPanel_HandleEvent(EventRecord *event)
{
    if (!event || !gSoundState.isOpen || !gSoundState.window) {
        return false;
    }

    WindowPtr whichWindow = NULL;
    switch (event->what) {
        case updateEvt:
            if ((WindowPtr)(uintptr_t)event->message != gSoundState.window) {
                return false;
            }
            BeginUpdate(gSoundState.window);
            ensure_controls();
            draw_contents();
            DrawControls(gSoundState.window);
            EndUpdate(gSoundState.window);
            return true;

        case activateEvt:
            if ((WindowPtr)(uintptr_t)event->message == gSoundState.window) {
                Boolean active = (event->modifiers & activeFlag) != 0;
                if (gSoundState.volumeDownButton) {
                    HiliteControl(gSoundState.volumeDownButton, active ? noHilite : inactiveHilite);
                }
                if (gSoundState.volumeUpButton) {
                    HiliteControl(gSoundState.volumeUpButton, active ? noHilite : inactiveHilite);
                }
                if (gSoundState.muteCheckbox) {
                    HiliteControl(gSoundState.muteCheckbox, active ? noHilite : inactiveHilite);
                }
                return true;
            }
            return false;

        case mouseDown: {
            short part = FindWindow(event->where, &whichWindow);
            if (whichWindow != gSoundState.window) {
                return false;
            }

            switch (part) {
                case inGoAway:
                    if (TrackGoAway(gSoundState.window, event->where)) {
                        SoundPanel_Close();
                    }
                    return true;

                case inDrag: {
                    Rect limit = qd.screenBits.bounds;
                    DragWindow(gSoundState.window, event->where, &limit);
                    return true;
                }

                case inContent: {
                    SelectWindow(gSoundState.window);
                    SetPort((GrafPtr)gSoundState.window);
                    Point localPt = event->where;
                    GlobalToLocal(&localPt);
                    ControlHandle ctl;
                    short ctlPart = FindControl(localPt, gSoundState.window, &ctl);
                    if (ctlPart != 0 && ctl != NULL) {
                        TrackControl(ctl, localPt, NULL);
                        if (ctl == gSoundState.volumeDownButton) {
                            adjust_volume(-10);
                        } else if (ctl == gSoundState.volumeUpButton) {
                            adjust_volume(10);
                        } else if (ctl == gSoundState.muteCheckbox) {
                            gSoundState.muted = !gSoundState.muted;
                            SetControlValue(gSoundState.muteCheckbox, gSoundState.muted ? 1 : 0);
                            request_redraw();
                        }
                        draw_contents();
                        DrawControls(gSoundState.window);
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
