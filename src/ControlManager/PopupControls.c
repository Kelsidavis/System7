/* #include "SystemTypes.h" */
/**
 * @file PopupControls.c
 * @brief Popup menu control implementation
 *
 * This file implements popup menu controls which provide selection from a list
 * of options. Popup menus are essential for user interfaces that need to present
 * choices in a compact format.
 *
 * Copyright (c) 2024 - System 7.1 Portable Toolbox Project
 * Licensed under MIT License
 */

// #include "CompatibilityFix.h" // Removed
#include "SystemTypes.h"
/* PopupControls.h local */
#include "ControlManager/ControlManager.h"
/* ControlDrawing.h not needed */
#include "MenuManager/MenuManager.h"
#include "QuickDraw/QuickDraw.h"
#include "FontManager/FontManager.h"
#include "EventManager/EventManager.h"
#include "ResourceMgr/resource_types.h"
#include "SystemTypes.h"


/* Popup control constants */
#define POPUP_ARROW_WIDTH       16      /* Width of popup arrow */
#define POPUP_MARGIN            4       /* Text margin */
#define POPUP_FRAME_WIDTH       1       /* Frame width */
#define POPUP_MIN_WIDTH         32      /* Minimum popup width */

/* Popup triangle resource ID */
#define POPUP_TRIANGLE_PICT_ID  -8224

/* Popup data structure */
typedef struct PopupData {
    MenuHandle popupMenu;           /* Associated menu */
    SInt16 menuID;                 /* Menu ID */
    SInt16 selectedItem;           /* Currently selected item */
    SInt16 variation;              /* Popup variation flags */

    /* Layout */
    Rect labelRect;                 /* Label area rectangle */
    Rect menuRect;                  /* Menu area rectangle */
    Rect triangleRect;              /* Triangle area rectangle */
    SInt16 titleWidth;             /* Width of title text */

    /* Style flags */
    Boolean fixedWidth;                /* Fixed width popup */
    Boolean useWFont;                  /* Use window font */
    Boolean useAddResMenu;             /* Use AddResMenu */
    Boolean hasTitle;                  /* Has title label */

    /* Title style */
    SInt16 titleStyle;             /* Title text style */
    SInt16 titleJust;              /* Title justification */
    RGBColor titleColor;            /* Title text color */

    /* Visual state */
    Boolean menuDown;                  /* Menu is currently down */
    Boolean tracking;                  /* Currently tracking */

    /* Platform support */
    Boolean useNativePopup;            /* Use native popup control */
    Handle nativeControl;           /* Native control handle */

} PopupData;

/* Internal function prototypes */
static void InitializePopupData(ControlHandle popup, SInt16 varCode);
static void CalculatePopupRects(ControlHandle popup);
static void DrawPopupFrame(ControlHandle popup);
static void DrawPopupLabel(ControlHandle popup);
static void DrawPopupMenuContent(ControlHandle popup);
static void DrawPopupTriangle(ControlHandle popup);
static SInt16 CalcPopupPart(ControlHandle popup, Point pt);
static void HandlePopupTracking(ControlHandle popup, Point startPt);
static void ShowPopupMenu(ControlHandle popup, Point where);
static void UpdatePopupSelection(ControlHandle popup, SInt16 item);
static OSErr LoadPopupMenuItems(ControlHandle popup);
static void GetPopupItemText(ControlHandle popup, SInt16 item, Str255 text);

/**
 * Register popup control type
 */
void RegisterPopupControlType(void) {
    RegisterControlType(popupMenuProc, PopupMenuCDEF);
}

/**
 * Popup menu control definition procedure
 */
SInt32 PopupMenuCDEF(SInt16 varCode, ControlHandle theControl,
                      SInt16 message, SInt32 param) {
    PopupData *popupData;
    Point pt;

    if (!theControl) {
        return 0;
    }

    switch (message) {
    case initCntl:
        /* Initialize popup control */
        InitializePopupData(theControl, varCode);
        CalculatePopupRects(theControl);
        break;

    case dispCntl:
        /* Dispose popup control */
        if ((*theControl)->contrlData) {
            popupData = (PopupData *)*(*theControl)->contrlData;

            /* Dispose menu */
            if (popupData->popupMenu) {
                DisposeMenu(popupData->popupMenu);
            }

            /* Dispose native control if any */
            if (popupData->nativeControl) {
                DisposeHandle(popupData->nativeControl);
            }

            DisposeHandle((*theControl)->contrlData);
            (*theControl)->contrlData = NULL;
        }
        break;

    case drawCntl:
        /* Draw popup control */
        DrawPopupMenu(theControl);
        break;

    case testCntl:
        /* Test which part of popup was hit */
        pt = *(Point *)&param;
        return CalcPopupPart(theControl, pt);

    case calcCRgns:
    case calcCntlRgn:
        /* Recalculate popup regions */
        CalculatePopupRects(theControl);
        break;

    case posCntl:
        /* Handle value changes */
        if ((*theControl)->contrlData) {
            popupData = (PopupData *)*(*theControl)->contrlData;
            UpdatePopupSelection(theControl, (*theControl)->contrlValue);
        }
        break;

    case autoTrack:
        /* Handle popup tracking */
        if ((*theControl)->contrlData) {
            pt = *(Point *)&param;
            HandlePopupTracking(theControl, pt);
        }
        break;
    }

    return 0;
}

/**
 * Create new popup control
 */
ControlHandle NewPopupControl(WindowPtr window, const Rect *bounds,
                             ConstStr255Param title, Boolean visible,
                             SInt16 menuID, SInt16 variation,
                             SInt32 refCon) {
    ControlHandle control;
    PopupData *popupData;

    control = NewControl(window, bounds, title, visible, 1, 1, 100,
                        popupMenuProc | variation, refCon);
    if (!control) {
        return NULL;
    }

    /* Set menu ID */
    if ((*control)->contrlData) {
        popupData = (PopupData *)*(*control)->contrlData;
        popupData->menuID = menuID;

        /* Load menu if specified */
        if (menuID > 0) {
            popupData->popupMenu = GetMenu(menuID);
            if (!popupData->popupMenu) {
                /* Create empty menu */
                popupData->popupMenu = NewMenu(menuID, title);
            }
            LoadPopupMenuItems(control);
        }
    }

    return control;
}

/**
 * Set popup menu
 */
void SetPopupMenu(ControlHandle popup, MenuHandle menu) {
    PopupData *popupData;

    if (!popup || !IsPopupMenuControl(popup) || !(*popup)->contrlData) {
        return;
    }

    popupData = (PopupData *)*(*popup)->contrlData;

    /* Dispose old menu */
    if (popupData->popupMenu) {
        DisposeMenu(popupData->popupMenu);
    }

    /* Set new menu */
    popupData->popupMenu = menu;
    if (menu) {
        /* CRITICAL: Lock handle before dereferencing to prevent heap compaction issues */
        HLock((Handle)menu);
        popupData->menuID = (**menu).menuID;
        HUnlock((Handle)menu);
    }

    /* Update selection */
    if (menu && (*popup)->contrlValue > 0 && (*popup)->contrlValue <= CountMenuItems(menu)) {
        popupData->selectedItem = (*popup)->contrlValue;
    } else {
        popupData->selectedItem = 1;
        SetControlValue(popup, 1);
    }

    /* Redraw if visible */
    if ((*popup)->contrlVis) {
        Draw1Control(popup);
    }
}

/**
 * Get popup menu
 */
MenuHandle GetPopupMenu(ControlHandle popup) {
    PopupData *popupData;

    if (!popup || !IsPopupMenuControl(popup) || !(*popup)->contrlData) {
        return NULL;
    }

    popupData = (PopupData *)*(*popup)->contrlData;
    return popupData->popupMenu;
}

/**
 * Add popup menu item
 */
void AppendPopupMenuItem(ControlHandle popup, ConstStr255Param itemText) {
    PopupData *popupData;

    if (!popup || !IsPopupMenuControl(popup) || !(*popup)->contrlData || !itemText) {
        return;
    }

    popupData = (PopupData *)*(*popup)->contrlData;

    /* Create menu if needed */
    if (!popupData->popupMenu) {
        popupData->popupMenu = NewMenu(popupData->menuID, (*popup)->contrlTitle);
    }

    /* Add menu item */
    if (popupData->popupMenu) {
        AppendMenu(popupData->popupMenu, itemText);

        /* Update control max value */
        SetControlMaximum(popup, CountMenuItems(popupData->popupMenu));

        /* Set first item as selected if none selected */
        if (popupData->selectedItem == 0) {
            popupData->selectedItem = 1;
            SetControlValue(popup, 1);
        }

        /* Redraw if visible */
        if ((*popup)->contrlVis) {
            Draw1Control(popup);
        }
    }
}

/**
 * Insert popup menu item
 */
void InsertPopupMenuItem(ControlHandle popup, ConstStr255Param itemText, SInt16 afterItem) {
    PopupData *popupData;

    if (!popup || !IsPopupMenuControl(popup) || !(*popup)->contrlData || !itemText) {
        return;
    }

    popupData = (PopupData *)*(*popup)->contrlData;

    if (popupData->popupMenu) {
        InsertMenuItem(popupData->popupMenu, itemText, afterItem);

        /* Update control max value */
        SetControlMaximum(popup, CountMenuItems(popupData->popupMenu));

        /* Adjust selected item if necessary */
        if (popupData->selectedItem > afterItem) {
            popupData->selectedItem++;
            SetControlValue(popup, popupData->selectedItem);
        }

        /* Redraw if visible */
        if ((*popup)->contrlVis) {
            Draw1Control(popup);
        }
    }
}

/**
 * Delete popup menu item
 */
void DeletePopupMenuItem(ControlHandle popup, SInt16 item) {
    PopupData *popupData;
    SInt16 itemCount;

    if (!popup || !IsPopupMenuControl(popup) || !(*popup)->contrlData) {
        return;
    }

    popupData = (PopupData *)*(*popup)->contrlData;

    if (popupData->popupMenu && item > 0) {
        itemCount = CountMenuItems(popupData->popupMenu);

        if (item <= itemCount) {
            DeleteMenuItem(popupData->popupMenu, item);

            /* Update control max value */
            SetControlMaximum(popup, CountMenuItems(popupData->popupMenu));

            /* Adjust selected item */
            if (popupData->selectedItem == item) {
                /* Select previous item or first item */
                if (item > 1) {
                    popupData->selectedItem = item - 1;
                } else {
                    popupData->selectedItem = 1;
                }
                SetControlValue(popup, popupData->selectedItem);
            } else if (popupData->selectedItem > item) {
                popupData->selectedItem--;
                SetControlValue(popup, popupData->selectedItem);
            }

            /* Redraw if visible */
            if ((*popup)->contrlVis) {
                Draw1Control(popup);
            }
        }
    }
}

/**
 * Set popup menu item text
 */
void SetPopupMenuItemText(ControlHandle popup, SInt16 item, ConstStr255Param text) {
    PopupData *popupData;

    if (!popup || !IsPopupMenuControl(popup) || !(*popup)->contrlData || !text) {
        return;
    }

    popupData = (PopupData *)*(*popup)->contrlData;

    if (popupData->popupMenu && item > 0 && item <= CountMenuItems(popupData->popupMenu)) {
        SetMenuItemText(popupData->popupMenu, item, text);

        /* Redraw if visible and this is the selected item */
        if ((*popup)->contrlVis && popupData->selectedItem == item) {
            Draw1Control(popup);
        }
    }
}

/**
 * Get popup menu item text
 */
void GetPopupMenuItemText(ControlHandle popup, SInt16 item, Str255 text) {
    PopupData *popupData;

    if (!popup || !IsPopupMenuControl(popup) || !(*popup)->contrlData || !text) {
        text[0] = 0;
        return;
    }

    popupData = (PopupData *)*(*popup)->contrlData;

    if (popupData->popupMenu && item > 0 && item <= CountMenuItems(popupData->popupMenu)) {
        GetMenuItemText(popupData->popupMenu, item, text);
    } else {
        text[0] = 0;
    }
}

/* Internal Implementation */

/**
 * Initialize popup data
 */
static void InitializePopupData(ControlHandle popup, SInt16 varCode) {
    PopupData *popupData;

    (*popup)->contrlData = NewHandleClear(sizeof(PopupData));
    if (!(*popup)->contrlData) {
        return;
    }

    popupData = (PopupData *)*(*popup)->contrlData;

    /* Parse variation flags */
    popupData->variation = varCode;
    popupData->fixedWidth = (varCode & popupFixedWidth) != 0;
    popupData->useWFont = (varCode & popupUseWFont) != 0;
    popupData->useAddResMenu = (varCode & popupUseAddResMenu) != 0;

    /* Initialize defaults */
    popupData->selectedItem = 1;
    popupData->menuID = 0;
    popupData->titleWidth = 0;
    popupData->hasTitle = ((*popup)->contrlTitle[0] > 0);
    popupData->titleStyle = normal;
    popupData->titleJust = popupTitleLeftJust;
    popupData->menuDown = false;
    popupData->tracking = false;
    popupData->useNativePopup = false;

    /* Set title color */
    popupData->titleColor.red = 0;
    popupData->titleColor.green = 0;
    popupData->titleColor.blue = 0;

    /* Calculate title width if present */
    if (popupData->hasTitle) {
        popupData->titleWidth = StringWidth((*popup)->contrlTitle) + POPUP_MARGIN;
    }
}

/**
 * Calculate popup rectangles
 */
static void CalculatePopupRects(ControlHandle popup) {
    PopupData *popupData;
    Rect bounds;

    if (!popup || !(*popup)->contrlData) {
        return;
    }

    popupData = (PopupData *)*(*popup)->contrlData;
    bounds = (*popup)->contrlRect;

    if (popupData->hasTitle && popupData->titleWidth > 0) {
        /* Popup with title label */
        popupData->labelRect = bounds;
        popupData->labelRect.right = popupData->labelRect.left + popupData->titleWidth;

        popupData->menuRect = bounds;
        popupData->menuRect.left = popupData->labelRect.right;
        popupData->menuRect.right = bounds.right - POPUP_ARROW_WIDTH;

        popupData->triangleRect = bounds;
        popupData->triangleRect.left = bounds.right - POPUP_ARROW_WIDTH;
    } else {
        /* Popup without title - label rect is empty */
        popupData->labelRect.left = bounds.left;
        popupData->labelRect.right = bounds.left;
        popupData->labelRect.top = bounds.top;
        popupData->labelRect.bottom = bounds.bottom;

        popupData->menuRect = bounds;
        popupData->menuRect.right = bounds.right - POPUP_ARROW_WIDTH;

        popupData->triangleRect = bounds;
        popupData->triangleRect.left = bounds.right - POPUP_ARROW_WIDTH;
    }
}

/**
 * Draw complete popup menu
 */
void DrawPopupMenu(ControlHandle popup) {
    if (!popup) {
        return;
    }

    /* Draw popup frame */
    DrawPopupFrame(popup);

    /* Draw title label if present */
    DrawPopupLabel(popup);

    /* Draw menu area */
    DrawPopupMenuContent(popup);

    /* Draw triangle */
    DrawPopupTriangle(popup);

    /* Gray out if inactive */
    if ((*popup)->contrlHilite == inactiveHilite) {
        Rect bounds = (*popup)->contrlRect;
        PenPat(&gray);
        PenMode(patBic);
        PaintRect(&bounds);
        PenMode(patCopy);
        PenPat(&black);
    }
}

/**
 * Draw popup frame
 */
static void DrawPopupFrame(ControlHandle popup) {
    PopupData *popupData;
    Rect frameRect;

    if (!popup || !(*popup)->contrlData) {
        return;
    }

    popupData = (PopupData *)*(*popup)->contrlData;

    /* Draw menu area frame */
    frameRect = popupData->menuRect;
    PenPat(&black);
    FrameRect(&frameRect);

    /* Fill menu background */
    InsetRect(&frameRect, 1, 1);
    PenPat(&white);
    PaintRect(&frameRect);

    /* Draw triangle area frame */
    frameRect = popupData->triangleRect;
    PenPat(&black);
    FrameRect(&frameRect);

    /* Fill triangle background */
    InsetRect(&frameRect, 1, 1);
    PenPat(&ltGray);
    PaintRect(&frameRect);

    PenPat(&black);
}

/**
 * Draw popup label
 */
static void DrawPopupLabel(ControlHandle popup) {
    PopupData *popupData;
    FontInfo fontInfo;
    SInt16 h, v;

    if (!popup || !(*popup)->contrlData) {
        return;
    }

    popupData = (PopupData *)*(*popup)->contrlData;

    if (!popupData->hasTitle || (*popup)->contrlTitle[0] == 0) {
        return;
    }

    /* Set text color */
    RGBForeColor(&popupData->titleColor);

    /* Get font metrics */
    GetFontInfo(&fontInfo);

    /* Calculate text position */
    switch (popupData->titleJust) {
    case popupTitleLeftJust:
        h = (popupData)->left + POPUP_MARGIN;
        break;
    case popupTitleCenterJust:
        h = (popupData)->left +
            ((popupData)->right - (popupData)->left -
             StringWidth((*popup)->contrlTitle)) / 2;
        break;
    case popupTitleRightJust:
        h = (popupData)->right - StringWidth((*popup)->contrlTitle) - POPUP_MARGIN;
        break;
    default:
        h = (popupData)->left + POPUP_MARGIN;
        break;
    }

    v = (popupData)->top +
        ((popupData)->bottom - (popupData)->top + fontInfo.ascent) / 2;

    /* Draw title text */
    MoveTo(h, v);
    DrawString((*popup)->contrlTitle);

    ForeColor(blackColor);
}

/**
 * Draw popup menu content (selected item text in the menu area)
 */
static void DrawPopupMenuContent(ControlHandle popup) {
    PopupData *popupData;
    Str255 itemText;
    FontInfo fontInfo;
    SInt16 h, v;

    if (!popup || !(*popup)->contrlData) {
        return;
    }

    popupData = (PopupData *)*(*popup)->contrlData;

    /* Get selected item text */
    GetPopupItemText(popup, popupData->selectedItem, itemText);

    if (itemText[0] == 0) {
        return;
    }

    /* Get font metrics */
    GetFontInfo(&fontInfo);

    /* Calculate text position (left-aligned in menu area) */
    h = (popupData)->left + POPUP_MARGIN;
    v = (popupData)->top +
        ((popupData)->bottom - (popupData)->top + fontInfo.ascent) / 2;

    /* Draw selected item text */
    MoveTo(h, v);
    DrawString(itemText);
}

/**
 * Draw popup triangle
 */
static void DrawPopupTriangle(ControlHandle popup) {
    PopupData *popupData;
    Point trianglePts[3];
    SInt16 centerH, centerV;
    PolyHandle trianglePoly;

    if (!popup || !(*popup)->contrlData) {
        return;
    }

    popupData = (PopupData *)*(*popup)->contrlData;

    /* Calculate triangle center */
    centerH = ((popupData)->left + (popupData)->right) / 2;
    centerV = ((popupData)->top + (popupData)->bottom) / 2;

    /* Define triangle points (pointing down) */
    trianglePts[0].h = centerH;
    trianglePts[0].v = centerV + 3;
    trianglePts[1].h = centerH - 4;
    trianglePts[1].v = centerV - 3;
    trianglePts[2].h = centerH + 4;
    trianglePts[2].v = centerV - 3;

    /* Draw triangle */
    PenPat(&black);
    OpenPoly();
    MoveTo(trianglePts[0].h, trianglePts[0].v);
    LineTo(trianglePts[1].h, trianglePts[1].v);
    LineTo(trianglePts[2].h, trianglePts[2].v);
    LineTo(trianglePts[0].h, trianglePts[0].v);
    trianglePoly = ClosePoly();

    if (trianglePoly) {
        PaintPoly(trianglePoly);
        KillPoly(trianglePoly);
    }
}

/**
 * Calculate popup part at point
 */
static SInt16 CalcPopupPart(ControlHandle popup, Point pt) {
    PopupData *popupData;

    if (!popup || !(*popup)->contrlData) {
        return 0;
    }

    popupData = (PopupData *)*(*popup)->contrlData;

    if (PtInRect(pt, &popupData->labelRect)) {
        return inLabel;
    }
    if (PtInRect(pt, &popupData->triangleRect)) {
        return inTriangle;
    }
    if (PtInRect(pt, &popupData->menuRect)) {
        return inMenu;
    }

    return 0;
}

/**
 * Handle popup tracking
 */
static void HandlePopupTracking(ControlHandle popup, Point startPt) {
    PopupData *popupData;
    Point globalPt;
    SInt16 selectedItem;

    if (!popup || !(*popup)->contrlData) {
        return;
    }

    popupData = (PopupData *)*(*popup)->contrlData;

    if (!popupData->popupMenu) {
        return;
    }

    /* Convert to global coordinates */
    globalPt = startPt;
    LocalToGlobal(&globalPt);

    /* Show popup menu */
    popupData->tracking = true;
    selectedItem = PopUpMenuSelect(popupData->popupMenu, globalPt.v, globalPt.h,
                                  popupData->selectedItem);
    popupData->tracking = false;

    /* Update selection if changed */
    if (selectedItem > 0 && selectedItem != popupData->selectedItem) {
        UpdatePopupSelection(popup, selectedItem);

        /* Call action procedure if set */
        if ((*popup)->contrlAction) {
            (*(*popup)->contrlAction)(popup, inMenu);
        }
    }
}

/**
 * Update popup selection
 */
static void UpdatePopupSelection(ControlHandle popup, SInt16 item) {
    PopupData *popupData;

    if (!popup || !(*popup)->contrlData) {
        return;
    }

    popupData = (PopupData *)*(*popup)->contrlData;

    if (popupData->popupMenu && item > 0 && item <= CountMenuItems(popupData->popupMenu)) {
        popupData->selectedItem = item;
        SetControlValue(popup, item);

        /* Redraw if visible */
        if ((*popup)->contrlVis) {
            Draw1Control(popup);
        }
    }
}

/**
 * Get popup item text
 */
static void GetPopupItemText(ControlHandle popup, SInt16 item, Str255 text) {
    PopupData *popupData;

    text[0] = 0;

    if (!popup || !(*popup)->contrlData) {
        return;
    }

    popupData = (PopupData *)*(*popup)->contrlData;

    if (popupData->popupMenu && item > 0 && item <= CountMenuItems(popupData->popupMenu)) {
        GetMenuItemText(popupData->popupMenu, item, text);
    }
}

/**
 * Check if control is popup menu
 */
Boolean IsPopupMenuControl(ControlHandle control) {
    if (!control) {
        return false;
    }

    SInt16 procID = GetControlVariant(control) & 0xFFF0;
    return (procID == popupMenuProc);
}
