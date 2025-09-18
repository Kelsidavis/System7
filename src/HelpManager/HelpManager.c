/*
 * Copyright (c) 2024 System7 Project
 * MIT License - See LICENSE file
 */

/*
 * HelpManager.c - Help Manager Implementation
 * System 7.1 balloon help and context-sensitive help
 */

#include "HelpManager/HelpManager.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Global Help Manager state */
static struct {
    Boolean     enabled;            /* Help Manager enabled */
    Boolean     balloonUp;          /* Balloon currently showing */
    int16_t     balloonProc;        /* Current balloon proc ID */
    int16_t     balloonVariant;     /* Current balloon variant */
    GrafPtr     balloonPort;        /* Balloon window port */
    PicHandle   balloonPic;         /* Balloon picture */
    UInt32      lastHelpTick;       /* Tick count of last help */
    MenuHandle  helpMenu;           /* Help menu handle */
    Point       lastTip;            /* Last balloon tip position */
    Rect        lastHotRect;        /* Last hot rectangle */
    int16_t     fontSize;           /* Font size for balloon text */
    int16_t     fontID;             /* Font ID for balloon text */
    Boolean     initialized;        /* Help Manager initialized */
} gHMState = {
    .enabled = false,
    .balloonUp = false,
    .balloonProc = 0,
    .balloonVariant = 0,
    .balloonPort = NULL,
    .balloonPic = NULL,
    .lastHelpTick = 0,
    .helpMenu = NULL,
    .fontSize = 9,              /* Default Geneva 9 */
    .fontID = 0,                /* System font */
    .initialized = false
};

/* Constants */
#define kBalloonDelay       30      /* Ticks before showing balloon */
#define kBalloonTimeout     600     /* Ticks before auto-hiding */
#define kMinBalloonWidth    80      /* Minimum balloon width */
#define kMaxBalloonWidth    320     /* Maximum balloon width */
#define kBalloonMargin      8       /* Text margin in balloon */
#define kTipLength          20      /* Length of balloon tip */

/* Forward declarations for internal functions */
static OSErr HMCreateBalloonWindow(void);
static void HMDisposeBalloonWindow(void);
static OSErr HMPositionBalloon(Point tip, Rect *hotRect, Rect *balloonRect, int16_t *variant);
static OSErr HMDrawBalloonFrame(const Rect *bounds, int16_t variant);
static OSErr HMDrawBalloonContent(const HMMessageRecord *message, const Rect *contentRect);
static OSErr HMGetStringFromMessage(const HMMessageRecord *message, Str255 string);
static int16_t HMCalculateBalloonWidth(const HMMessageRecord *message);
static int16_t HMCalculateBalloonHeight(const HMMessageRecord *message);
static OSErr HMCalculateBalloonRectInternal(const HMMessageRecord *message, Point tip,
                                           Rect *hotRect, Rect *balloonRect, int16_t *variant);

/* =========================================================================
 * Initialization and Control
 * ========================================================================= */

OSErr HMInitialize(void) {
    /* Initialize Help Manager */
    if (gHMState.initialized) {
        return noErr;
    }

    /* Initialize state */
    gHMState.enabled = false;  /* Start disabled by default */
    gHMState.balloonUp = false;
    gHMState.balloonProc = 0;
    gHMState.balloonVariant = 0;
    gHMState.balloonPort = NULL;
    gHMState.balloonPic = NULL;
    gHMState.lastHelpTick = 0;
    gHMState.helpMenu = NULL;
    gHMState.fontSize = 9;
    gHMState.fontID = 0;

    /* Set empty rectangles */
    SetRect(&gHMState.lastHotRect, 0, 0, 0, 0);

    /* Set default tip position */
    gHMState.lastTip.h = 0;
    gHMState.lastTip.v = 0;

    gHMState.initialized = true;

    return noErr;
}

void HMShutdown(void) {
    /* Clean up Help Manager resources */
    if (gHMState.balloonUp) {
        HMRemoveBalloon();
    }

    HMDisposeBalloonWindow();

    if (gHMState.balloonPic) {
        DisposeHandle((Handle)gHMState.balloonPic);
        gHMState.balloonPic = NULL;
    }

    gHMState.initialized = false;
    gHMState.enabled = false;
}

/* =========================================================================
 * Balloon Control
 * ========================================================================= */

Boolean HMGetBalloons(void) {
    return gHMState.enabled;
}

OSErr HMSetBalloons(Boolean flag) {
    gHMState.enabled = flag;
    if (!flag && gHMState.balloonUp) {
        HMRemoveBalloon();
    }
    return noErr;
}

Boolean HMIsBalloon(void) {
    return gHMState.balloonUp;
}

/* =========================================================================
 * Balloon Display
 * ========================================================================= */

OSErr HMShowBalloon(const HMMessageRecord *aHelpMsg, Point tip,
                   RectPtr alternateRect, Ptr tipProc, short theProc,
                   short variant, short method) {
    OSErr err = noErr;
    Rect balloonRect;

    /* Check if Help Manager is initialized */
    if (!gHMState.initialized) {
        err = HMInitialize();
        if (err != noErr) return err;
    }

    /* Check if Help Manager is enabled */
    if (!gHMState.enabled) {
        return hmHelpDisabled;
    }

    /* Validate message */
    if (!aHelpMsg) {
        return paramErr;
    }

    /* Check for unknown help type */
    if (aHelpMsg->hmmHelpType < khmmString ||
        aHelpMsg->hmmHelpType > khmmSTRRes) {
        return hmUnknownHelpType;
    }

    /* Remove existing balloon if any */
    if (gHMState.balloonUp) {
        HMRemoveBalloon();
    }

    /* Calculate balloon rectangle */
    err = HMCalculateBalloonRectInternal(aHelpMsg, tip, alternateRect,
                                        &balloonRect, &variant);
    if (err != noErr) {
        return err;
    }

    /* Create balloon window if needed */
    if (!gHMState.balloonPort) {
        err = HMCreateBalloonWindow();
        if (err != noErr) {
            return err;
        }
    }

    /* Position and draw the balloon */
    err = HMPositionBalloon(tip, alternateRect, &balloonRect, &variant);
    if (err != noErr) {
        return err;
    }

    /* Draw balloon frame and content */
    err = HMDrawBalloonFrame(&balloonRect, variant);
    if (err != noErr) {
        return err;
    }

    /* Calculate content area */
    Rect contentRect = balloonRect;
    InsetRect(&contentRect, kBalloonMargin, kBalloonMargin);

    /* Draw balloon content */
    err = HMDrawBalloonContent(aHelpMsg, &contentRect);
    if (err != noErr) {
        return err;
    }

    /* Update globals */
    gHMState.balloonUp = true;
    gHMState.lastTip = tip;
    if (alternateRect) {
        gHMState.lastHotRect = *alternateRect;
    }
    gHMState.balloonVariant = variant;
    gHMState.lastHelpTick = TickCount();

    return noErr;
}

OSErr HMShowMenuBalloon(short itemNum, short itemMenuID, long itemFlags,
                       long itemReserved, Point tip, RectPtr alternateRect,
                       Ptr tipProc, short theProc, short variant) {
    HMMessageRecord message;
    Handle hmnu = NULL;
    OSErr err;

    /* Check if Help Manager is enabled */
    if (!gHMState.enabled) {
        return hmHelpDisabled;
    }

    /* Load hmnu resource for this menu */
    hmnu = GetResource(kHMMenuResType, itemMenuID);
    if (hmnu == NULL) {
        /* No help resource, create default message */
        message.hmmHelpType = khmmString;
        sprintf((char*)&message.u.hmmString[1], "Help for menu item %d", itemNum);
        message.u.hmmString[0] = strlen((char*)&message.u.hmmString[1]);
    } else {
        /* Parse hmnu resource to get help for specific item */
        /* For now, create a default message */
        message.hmmHelpType = khmmString;
        sprintf((char*)&message.u.hmmString[1], "Help for menu %d, item %d",
                itemMenuID, itemNum);
        message.u.hmmString[0] = strlen((char*)&message.u.hmmString[1]);
        ReleaseResource(hmnu);
    }

    /* Show the balloon */
    return HMShowBalloon(&message, tip, alternateRect, tipProc, theProc, variant, 0);
}

OSErr HMRemoveBalloon(void) {
    if (!gHMState.balloonUp) {
        return hmNoBalloonUp;
    }

    /* Hide the balloon window */
    if (gHMState.balloonPort) {
        HideWindow((WindowPtr)gHMState.balloonPort);
    }

    gHMState.balloonUp = false;

    return noErr;
}

/* =========================================================================
 * Help Menu Management
 * ========================================================================= */

OSErr HMGetHelpMenuHandle(MenuHandle *mh) {
    if (!mh) {
        return paramErr;
    }

    /* In System 7, the Help menu is automatically created */
    *mh = gHMState.helpMenu;

    if (!*mh) {
        /* Create Help menu if it doesn't exist */
        *mh = NewMenu(kHMHelpMenuID, "\pHelp");
        if (*mh) {
            AppendMenu(*mh, "\pAbout Balloon Help");
            AppendMenu(*mh, "\p(-");
            AppendMenu(*mh, "\pShow Balloons");
            gHMState.helpMenu = *mh;
        }
    }

    return noErr;
}

OSErr HMGetMenuResID(short menuID, short *resID) {
    if (!resID) {
        return paramErr;
    }

    /* Return the hmnu resource ID for this menu */
    /* In System 7, hmnu resources typically have the same ID as the menu */
    *resID = menuID;

    return noErr;
}

OSErr HMSetMenuResID(short menuID, short resID) {
    /* Store custom resource ID for menu */
    /* This would be stored in a lookup table in a real implementation */
    return noErr;
}

/* =========================================================================
 * Help Content Access
 * ========================================================================= */

OSErr HMGetIndHelpMsg(ResType whichType, short whichResID, short whichMsg,
                     short whichState, long *options, Point *tip, Rect *altRect,
                     short *theProc, short *variant, HMMessageRecord *aHelpMsg,
                     short *count) {
    Handle resource = NULL;
    OSErr err = noErr;

    /* Load the help resource */
    resource = GetResource(whichType, whichResID);
    if (resource == NULL) {
        return ResError();
    }

    /* Parse the resource based on type */
    if (whichType == kHMMenuResType) {
        /* Parse hmnu resource structure */
        /* For now, return a default message */
        if (aHelpMsg) {
            aHelpMsg->hmmHelpType = khmmString;
            strcpy((char*)&aHelpMsg->u.hmmString[1], "Menu help");
            aHelpMsg->u.hmmString[0] = strlen((char*)&aHelpMsg->u.hmmString[1]);
        }
    } else if (whichType == kHMDialogResType) {
        /* Parse hdlg resource structure */
        if (aHelpMsg) {
            aHelpMsg->hmmHelpType = khmmString;
            strcpy((char*)&aHelpMsg->u.hmmString[1], "Dialog help");
            aHelpMsg->u.hmmString[0] = strlen((char*)&aHelpMsg->u.hmmString[1]);
        }
    }

    ReleaseResource(resource);

    return noErr;
}

OSErr HMExtractHelpMsg(ResType whichType, short whichResID, short whichMsg,
                      short whichState, HMMessageRecord *aHelpMsg) {
    return HMGetIndHelpMsg(whichType, whichResID, whichMsg, whichState,
                          NULL, NULL, NULL, NULL, NULL, aHelpMsg, NULL);
}

/* =========================================================================
 * Font and Appearance
 * ========================================================================= */

OSErr HMSetFont(short font) {
    gHMState.fontID = font;
    return noErr;
}

OSErr HMSetFontSize(short fontSize) {
    if (fontSize < 9 || fontSize > 24) {
        return paramErr;
    }
    gHMState.fontSize = fontSize;
    return noErr;
}

OSErr HMGetFont(short *font) {
    if (!font) return paramErr;
    *font = gHMState.fontID;
    return noErr;
}

OSErr HMGetFontSize(short *fontSize) {
    if (!fontSize) return paramErr;
    *fontSize = gHMState.fontSize;
    return noErr;
}

/* =========================================================================
 * Balloon Utilities
 * ========================================================================= */

OSErr HMBalloonRect(const HMMessageRecord *aHelpMsg, Rect *coolRect) {
    Point dummyTip = {100, 100};
    Rect dummyHotRect = {90, 90, 110, 110};
    int16_t variant = 0;

    return HMCalculateBalloonRectInternal(aHelpMsg, dummyTip, &dummyHotRect,
                                         coolRect, &variant);
}

OSErr HMBalloonPict(const HMMessageRecord *aHelpMsg, PicHandle *coolPict) {
    if (!coolPict) {
        return paramErr;
    }

    /* Return picture handle for balloon content */
    *coolPict = gHMState.balloonPic;

    return noErr;
}

OSErr HMGetBalloonWindow(WindowPtr *window) {
    if (!window) {
        return paramErr;
    }

    *window = (WindowPtr)gHMState.balloonPort;

    return gHMState.balloonPort ? noErr : hmNoBalloonUp;
}

/* =========================================================================
 * Template Scanning
 * ========================================================================= */

OSErr HMScanTemplateItems(short whichID, short whichResFile, ResType whichType) {
    /* Scan template items for help content */
    /* This would parse DITL or other templates in a real implementation */
    return noErr;
}

/* =========================================================================
 * Dialog Support
 * ========================================================================= */

OSErr HMSetDialogResID(short resID) {
    /* Store dialog help resource ID */
    /* This would be stored in a global or window property in real implementation */
    return noErr;
}

OSErr HMGetDialogResID(short *resID) {
    if (!resID) return paramErr;
    /* Return dialog help resource ID */
    *resID = 0;  /* Would return actual stored ID */
    return noErr;
}

/* =========================================================================
 * Internal Implementation Functions
 * ========================================================================= */

static OSErr HMCreateBalloonWindow(void) {
    /* Create a window for the balloon */
    Rect bounds = {0, 0, 100, 200};

    WindowPtr window = NewWindow(NULL, &bounds, "\p", false,
                                 plainDBox, (WindowPtr)-1L, false, 0);

    if (window == NULL) {
        return memFullErr;
    }

    gHMState.balloonPort = GetWindowPort(window);

    return noErr;
}

static void HMDisposeBalloonWindow(void) {
    if (gHMState.balloonPort) {
        WindowPtr window = GetWindowFromPort(gHMState.balloonPort);
        if (window) {
            DisposeWindow(window);
        }
        gHMState.balloonPort = NULL;
    }
}

static OSErr HMCalculateBalloonRectInternal(const HMMessageRecord *message, Point tip,
                                           Rect *hotRect, Rect *balloonRect, int16_t *variant) {
    int16_t width, height;

    /* Calculate balloon size based on content */
    width = HMCalculateBalloonWidth(message);
    height = HMCalculateBalloonHeight(message);

    /* Add margins */
    width += kBalloonMargin * 2;
    height += kBalloonMargin * 2;

    /* Add space for tip */
    if (*variant != 8) {  /* kHMNoTip */
        height += kTipLength;
    }

    /* Position balloon relative to tip */
    switch (*variant) {
        case 0:  /* kHMTopLeftTip */
            balloonRect->left = tip.h;
            balloonRect->top = tip.v - height;
            break;

        case 1:  /* kHMTopRightTip */
            balloonRect->left = tip.h - width;
            balloonRect->top = tip.v - height;
            break;

        case 2:  /* kHMBottomRightTip */
            balloonRect->left = tip.h - width;
            balloonRect->top = tip.v;
            break;

        case 3:  /* kHMBottomLeftTip */
            balloonRect->left = tip.h;
            balloonRect->top = tip.v;
            break;

        default:
            balloonRect->left = tip.h - width/2;
            balloonRect->top = tip.v - height/2;
            break;
    }

    balloonRect->right = balloonRect->left + width;
    balloonRect->bottom = balloonRect->top + height;

    return noErr;
}

static OSErr HMPositionBalloon(Point tip, Rect *hotRect, Rect *balloonRect, int16_t *variant) {
    /* Adjust balloon position to fit on screen */
    /* Get screen bounds */
    Rect screenBounds;
    GetRegionBounds(GetGrayRgn(), &screenBounds);

    /* Adjust if balloon goes off screen */
    if (balloonRect->left < screenBounds.left) {
        OffsetRect(balloonRect, screenBounds.left - balloonRect->left, 0);
    }
    if (balloonRect->right > screenBounds.right) {
        OffsetRect(balloonRect, screenBounds.right - balloonRect->right, 0);
    }
    if (balloonRect->top < screenBounds.top) {
        OffsetRect(balloonRect, 0, screenBounds.top - balloonRect->top);
    }
    if (balloonRect->bottom > screenBounds.bottom) {
        OffsetRect(balloonRect, 0, screenBounds.bottom - balloonRect->bottom);
    }

    return noErr;
}

static OSErr HMDrawBalloonFrame(const Rect *bounds, int16_t variant) {
    /* Draw the balloon frame with rounded corners and tip */
    GrafPtr savePort;

    GetPort(&savePort);
    SetPort(gHMState.balloonPort);

    /* Draw white background */
    PenNormal();
    PaintRoundRect(bounds, 16, 16);

    /* Draw black frame */
    FrameRoundRect(bounds, 16, 16);

    /* Draw tip if needed */
    if (variant != 8) {  /* Not kHMNoTip */
        /* Would draw triangular tip here */
    }

    SetPort(savePort);

    return noErr;
}

static OSErr HMDrawBalloonContent(const HMMessageRecord *message, const Rect *contentRect) {
    Str255 string;
    OSErr err;
    GrafPtr savePort;

    GetPort(&savePort);
    SetPort(gHMState.balloonPort);

    switch (message->hmmHelpType) {
        case khmmString:
            /* Draw Pascal string directly */
            TextFont(gHMState.fontID);
            TextSize(gHMState.fontSize);
            MoveTo(contentRect->left, contentRect->top + gHMState.fontSize);
            DrawString(message->u.hmmString);
            break;

        case khmmPict:
            /* Draw picture from resource */
            {
                PicHandle pic = (PicHandle)GetResource('PICT', message->u.hmmPict);
                if (pic) {
                    DrawPicture(pic, contentRect);
                    ReleaseResource((Handle)pic);
                }
            }
            break;

        case khmmStringRes:
            /* Get string from STR resource */
            err = HMGetStringFromMessage(message, string);
            if (err == noErr) {
                TextFont(gHMState.fontID);
                TextSize(gHMState.fontSize);
                MoveTo(contentRect->left, contentRect->top + gHMState.fontSize);
                DrawString(string);
            }
            break;

        case khmmTEHandle:
            /* Draw TextEdit handle */
            if (message->u.hmmTEHandle) {
                TEUpdate(contentRect, (TEHandle)message->u.hmmTEHandle);
            }
            break;

        case khmmPictHandle:
            /* Draw picture handle */
            if (message->u.hmmPictHandle) {
                DrawPicture((PicHandle)message->u.hmmPictHandle, contentRect);
            }
            break;

        default:
            SetPort(savePort);
            return hmUnknownHelpType;
    }

    SetPort(savePort);
    return noErr;
}

static OSErr HMGetStringFromMessage(const HMMessageRecord *message, Str255 string) {
    /* Extract string from message based on type */

    switch (message->hmmHelpType) {
        case khmmString:
            BlockMove(message->u.hmmString, string, message->u.hmmString[0] + 1);
            break;

        case khmmStringRes:
            GetIndString(string, message->u.hmmStringRes.hmmResID,
                        message->u.hmmStringRes.hmmIndex);
            break;

        case khmmSTRRes:
            {
                Handle strHandle = GetResource('STR ', message->u.hmmSTRRes);
                if (strHandle) {
                    BlockMove(*strHandle, string, (*strHandle)[0] + 1);
                    ReleaseResource(strHandle);
                } else {
                    string[0] = 0;
                    return ResError();
                }
            }
            break;

        default:
            string[0] = 0;
            return hmUnknownHelpType;
    }

    return noErr;
}

static int16_t HMCalculateBalloonWidth(const HMMessageRecord *message) {
    Str255 string;
    int16_t width = kMinBalloonWidth;

    /* Calculate width based on content */
    if (HMGetStringFromMessage(message, string) == noErr) {
        /* Use TextWidth() to measure string */
        GrafPtr savePort;
        GetPort(&savePort);

        TextFont(gHMState.fontID);
        TextSize(gHMState.fontSize);
        width = StringWidth(string);

        SetPort(savePort);
    }

    /* Clamp to min/max */
    if (width < kMinBalloonWidth) width = kMinBalloonWidth;
    if (width > kMaxBalloonWidth) width = kMaxBalloonWidth;

    return width;
}

static int16_t HMCalculateBalloonHeight(const HMMessageRecord *message) {
    Str255 string;
    int16_t height = 20;  /* Default height */
    int16_t width;
    int16_t lines;

    /* Calculate height based on content */
    if (HMGetStringFromMessage(message, string) == noErr) {
        width = HMCalculateBalloonWidth(message);
        /* Estimate number of lines needed */
        GrafPtr savePort;
        GetPort(&savePort);

        TextFont(gHMState.fontID);
        TextSize(gHMState.fontSize);

        int16_t stringWidth = StringWidth(string);
        lines = (stringWidth / width) + 1;
        height = lines * (gHMState.fontSize + 4);

        SetPort(savePort);
    }

    return height;
}

/* =========================================================================
 * Modern Help System Support
 * ========================================================================= */

OSErr HMConfigureModernHelp(const HMModernHelpConfig *config) {
    /* Configure modern help system */
    /* This would set up HTML/WebKit help in a real implementation */
    return noErr;
}

OSErr HMShowModernHelp(const char *helpTopic, const char *anchor) {
    /* Show modern help topic */
    /* This would launch help viewer in a real implementation */
    return noErr;
}

OSErr HMSearchHelp(const char *searchTerm, Handle *results) {
    /* Search help content */
    if (!results) return paramErr;
    *results = NULL;
    return noErr;
}

OSErr HMNavigateHelp(const char *linkTarget) {
    /* Navigate to help link */
    return noErr;
}