/*
 * cdev_desktop.c - the Desktop Patterns control panel
 *
 * Laid out as Apple's Desktop Patterns control panel (System 7.5), the first
 * to carry colour patterns: one pattern at a time, tiled in a large preview,
 * a scroll bar beneath it to move through the collection, and a "Set Desktop
 * Pattern" button that puts the one shown on the desktop. Moving through the
 * collection changes only the preview; the close box simply closes.
 *
 * The collection is every colour pattern ('ppat') the system has, then the
 * black-and-white ones ('PAT ' 16 to 47). This used to be a grid of the
 * black-and-white patterns alone with OK and Cancel, so none of the colour
 * patterns could be chosen.
 */

#include "ControlPanels/DesktopPatterns.h"
#include "PatternMgr/pattern_manager.h"
#include "PatternMgr/pattern_resources.h"
#include "ControlManager/ControlManager.h"
#include "ControlManager/ControlTypes.h"
#include "WindowManager/WindowManager.h"
#include "QuickDraw/QuickDraw.h"
#include "QuickDraw/QuickDrawInternal.h"
#include "QuickDrawConstants.h"
#include "ResourceManager.h"
#include "System71StdLib.h"
#include "TimeManager/TimeBase.h"
#include <string.h>

extern QDGlobals qd;

#define pushButProc   0
#define scrollBarProc 16

/* Content layout, in the window's local coordinates */
enum {
    kContentW = 236, kContentH = 196,
    kPreviewTop = 12, kPreviewLeft = 12, kPreviewBottom = 132, kPreviewRight = 224,
    kBarTop = 140, kBarBottom = 156,
    kButtonTop = 166, kButtonBottom = 186, kButtonLeft = 38, kButtonRight = 198
};

/* The collection */
#define kMaxPatterns 64
typedef struct {
    Boolean color;      /* a 'ppat', else a 'PAT ' */
    int16_t id;
} PatternEntry;

static PatternEntry gPatterns[kMaxPatterns];
static SInt16 gPatternCount = 0;
static SInt16 gShown = 0;               /* index of the pattern in the preview */

static WindowPtr gDesktopCdevWin = NULL;
static ControlHandle gScrollBar = NULL;
static ControlHandle gSetButton = NULL;

/* The colour pattern last decoded for the preview, so redrawing it does not
 * load and decode the resource again. */
static int16_t gDecodedID = 0;
static uint32_t gDecoded[64];

static void BuildCollection(void) {
    gPatternCount = 0;

    SInt16 colours = CountResources(FOURCC('p','p','a','t'));
    for (SInt16 i = 1; i <= colours && gPatternCount < kMaxPatterns; i++) {
        Handle h = GetIndResource(FOURCC('p','p','a','t'), i);
        if (!h) continue;
        ResID id;
        ResType type;
        char name[256];
        GetResInfo(h, &id, &type, name);
        /* Each once - the same pattern can be in more than one open file */
        Boolean have = false;
        for (SInt16 k = 0; k < gPatternCount; k++) {
            if (gPatterns[k].color && gPatterns[k].id == id) have = true;
        }
        uint32_t probe[64];
        /* Only patterns the Pattern Manager can show are offered */
        if (!have && PM_LoadColorPattern(id, probe)) {
            gPatterns[gPatternCount].color = true;
            gPatterns[gPatternCount].id = id;
            gPatternCount++;
        }
    }

    for (int16_t id = 16; id <= 47 && gPatternCount < kMaxPatterns; id++) {
        Pattern pat;
        if (LoadPATResource(id, &pat)) {
            gPatterns[gPatternCount].color = false;
            gPatterns[gPatternCount].id = id;
            gPatternCount++;
        }
    }
}

/* Which entry the desktop is using now, or 0 */
static SInt16 CurrentDesktopIndex(void) {
    DesktopPref pref = PM_GetSavedDesktopPref();
    for (SInt16 i = 0; i < gPatternCount; i++) {
        if (gPatterns[i].color == pref.usePixPat &&
            gPatterns[i].id == (pref.usePixPat ? pref.ppatID : pref.patID)) {
            return i;
        }
    }
    return 0;
}

static void DrawPreview(void) {
    if (!gDesktopCdevWin || gPatternCount == 0) return;
    SetPort((GrafPtr)gDesktopCdevWin);

    Rect box = { kPreviewTop, kPreviewLeft, kPreviewBottom, kPreviewRight };
    PenNormal();
    FrameRect(&box);
    Rect interior = box;
    InsetRect(&interior, 1, 1);

    const PatternEntry* e = &gPatterns[gShown];
    if (e->color) {
        if (gDecodedID != e->id) {
            gDecodedID = PM_LoadColorPattern(e->id, gDecoded) ? e->id : 0;
        }
        if (gDecodedID == e->id) {
            QD_FillRectColorPattern(&interior, gDecoded);
            return;
        }
    } else {
        Pattern pat;
        if (LoadPATResource(e->id, &pat)) {
            ForeColor(blackColor);
            BackColor(whiteColor);
            FillRect(&interior, &pat);
            return;
        }
    }
    EraseRect(&interior);
}

static void DrawContents(void) {
    SetPort((GrafPtr)gDesktopCdevWin);
    EraseRect(&gDesktopCdevWin->port.portRect);
    DrawPreview();
    DrawControls(gDesktopCdevWin);
}

/* When the arrows last moved the preview on */
static UInt32 gLastStepTick = 0;
enum { kStepTicks = 20 };   /* one pattern per third of a second while held */

/*
 * The scroll bar moved: show the pattern it now points at. The arrows step one
 * pattern a click; held, they go on at a pace you can follow rather than at
 * the scroll bar's own repeat rate, which went past several patterns in an
 * ordinary click.
 */
static void ScrollAction(ControlHandle control, SInt16 part) {
    SInt16 value = GetControlValue(control);
    if (value == gShown || value < 0 || value >= gPatternCount) return;
    if (part == inUpButton || part == inDownButton) {
        UInt32 now = TickCount();
        if (now - gLastStepTick < kStepTicks) {
            SetControlValue(control, gShown);   /* too soon: undo the step */
            return;
        }
        gLastStepTick = now;
    }
    gShown = value;
    DrawPreview();
}

/* Put the pattern shown on the desktop, and remember it */
static void SetDesktopPattern(void) {
    const PatternEntry* e = &gPatterns[gShown];
    DesktopPref pref = PM_GetSavedDesktopPref();
    pref.usePixPat = e->color;
    if (e->color) {
        pref.ppatID = e->id;
    } else {
        pref.patID = e->id;
    }
    if (PM_ApplyDesktopPref(&pref)) {
        PM_SaveDesktopPref(&pref);
    }
}

void OpenDesktopCdev(void) {
    if (gDesktopCdevWin) {
        SelectWindow(gDesktopCdevWin);
        return;
    }

    BuildCollection();
    if (gPatternCount == 0) return;
    gShown = CurrentDesktopIndex();

    Rect bounds = { 60, 60, 60 + 21 + kContentH, 60 + 2 + kContentW };
    static Str255 title;
    c2pstrcpy(title, "Desktop Patterns");
    gDesktopCdevWin = NewWindow(NULL, &bounds, title, false, noGrowDocProc,
                                (WindowPtr)-1L, true, 0);
    if (!gDesktopCdevWin) return;
    SetPort((GrafPtr)gDesktopCdevWin);

    Rect barRect = { kBarTop, kPreviewLeft, kBarBottom, kPreviewRight };
    gScrollBar = NewControl(gDesktopCdevWin, &barRect, (ConstStr255Param)"", true,
                            gShown, 0, (SInt16)(gPatternCount - 1), scrollBarProc, 0);

    Rect buttonRect = { kButtonTop, kButtonLeft, kButtonBottom, kButtonRight };
    static Str255 setTitle;
    c2pstrcpy(setTitle, "Set Desktop Pattern");
    gSetButton = NewControl(gDesktopCdevWin, &buttonRect, setTitle, true, 0, 0, 1,
                            pushButProc, 0);

    ShowWindow(gDesktopCdevWin);
    SelectWindow(gDesktopCdevWin);
    DrawContents();
}

void CloseDesktopCdev(void) {
    if (!gDesktopCdevWin) return;
    DisposeWindow(gDesktopCdevWin);
    gDesktopCdevWin = NULL;
    gScrollBar = NULL;
    gSetButton = NULL;
}

Boolean DesktopPatterns_HandleEvent(EventRecord *event) {
    if (!event || !gDesktopCdevWin) {
        return false;
    }

    switch (event->what) {
        case updateEvt:
            if ((WindowPtr)(uintptr_t)event->message != gDesktopCdevWin) {
                return false;
            }
            BeginUpdate(gDesktopCdevWin);
            DrawContents();
            EndUpdate(gDesktopCdevWin);
            return true;

        case mouseDown: {
            WindowPtr which;
            SInt16 part = FindWindow(event->where, &which);
            if (which != gDesktopCdevWin) {
                return false;    /* someone else's window, or the desktop */
            }
            switch (part) {
                case inContent: {
                    if (FrontWindow() != gDesktopCdevWin) {
                        SelectWindow(gDesktopCdevWin);
                        break;
                    }
                    SetPort((GrafPtr)gDesktopCdevWin);
                    Point where = event->where;
                    GlobalToLocal(&where);
                    ControlHandle control;
                    SInt16 cpart = FindControl(where, gDesktopCdevWin, &control);
                    if (control == gScrollBar && cpart) {
                        SInt16 delta = 0;
                        gLastStepTick = 0;   /* a new click steps at once */
                        TrackScrollbarAction(control, where, cpart, ScrollAction, &delta);
                        ScrollAction(control, cpart);
                    } else if (control == gSetButton && cpart) {
                        if (TrackControl(control, where, NULL)) {
                            SetDesktopPattern();
                        }
                    }
                    break;
                }
                case inDrag:
                    DragWindow(gDesktopCdevWin, event->where, &qd.screenBits.bounds);
                    break;
                case inGoAway:
                    if (TrackGoAway(gDesktopCdevWin, event->where)) {
                        CloseDesktopCdev();
                    }
                    break;
            }
            return true;
        }

        case activateEvt:
            return (WindowPtr)(uintptr_t)event->message == gDesktopCdevWin;

        default:
            break;
    }
    return false;
}

Boolean DesktopPatterns_IsWindow(WindowPtr window) {
    return window != NULL && window == gDesktopCdevWin;
}

WindowPtr DesktopPatterns_GetWindow(void) {
    return gDesktopCdevWin;
}
