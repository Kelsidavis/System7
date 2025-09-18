/*
 * HelpBalloons.c - Help Balloon Display and Positioning Implementation
 *
 * This file implements the balloon display system for the Mac OS Help Manager.
 * It handles balloon creation, positioning, drawing, and animation.
 */

#include "HelpBalloons.h"
#include "HelpContent.h"
#include "UserPreferences.h"

#include "MacTypes.h"
#include "Quickdraw.h"
#include "Windows.h"
#include "Memory.h"
#include "Errors.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

/* Balloon Manager globals */
typedef struct HMBalloonManager {
    Boolean initialized;             /* Manager is initialized */
    HMBalloonInfo *currentBalloon;   /* Currently displayed balloon */
    HMModernBalloonConfig config;    /* Modern balloon configuration */
    RgnHandle tempRgn1;              /* Temporary region handle */
    RgnHandle tempRgn2;              /* Temporary region handle */
    long animationStartTime;         /* Animation start time */
    Boolean animationActive;         /* Animation is active */
} HMBalloonManager;

static HMBalloonManager gBalloonMgr = {0};

/* Forward declarations */
static OSErr HMBalloonCreateWindow(const HMBalloonContent *content,
                                  const Rect *balloonRect, WindowPtr *window);
static OSErr HMBalloonDrawFrame(const Rect *balloonRect, HMBalloonTailPosition tailPos,
                               const HMBalloonContent *content);
static OSErr HMBalloonDrawTailInternal(const Rect *balloonRect, Point tipPoint,
                                      HMBalloonTailPosition tailPos);
static HMBalloonTailPosition HMBalloonDetermineTailPosition(const Rect *balloonRect,
                                                           Point tipPoint,
                                                           const Rect *screenBounds);
static OSErr HMBalloonSetupRegions(const Rect *balloonRect, Point tipPoint,
                                  HMBalloonTailPosition tailPos, RgnHandle balloonRgn,
                                  RgnHandle structRgn);

/*
 * HMBalloonManagerInit - Initialize the balloon manager
 */
OSErr HMBalloonManagerInit(void)
{
    if (gBalloonMgr.initialized) {
        return noErr;
    }

    /* Initialize global state */
    memset(&gBalloonMgr, 0, sizeof(gBalloonMgr));

    /* Set default modern configuration */
    gBalloonMgr.config.style = kHMBalloonStyleClassic;
    gBalloonMgr.config.animation = kHMBalloonAnimFade;
    gBalloonMgr.config.useDropShadow = true;
    gBalloonMgr.config.useGradient = false;
    gBalloonMgr.config.useTransparency = false;
    gBalloonMgr.config.opacity = 1.0f;
    gBalloonMgr.config.cornerRadius = kHMBalloonCornerRadius;
    gBalloonMgr.config.respectAccessibility = true;

    /* Create temporary regions */
    gBalloonMgr.tempRgn1 = NewRgn();
    gBalloonMgr.tempRgn2 = NewRgn();

    if (!gBalloonMgr.tempRgn1 || !gBalloonMgr.tempRgn2) {
        HMBalloonManagerShutdown();
        return memFullErr;
    }

    gBalloonMgr.initialized = true;
    return noErr;
}

/*
 * HMBalloonManagerShutdown - Shutdown the balloon manager
 */
void HMBalloonManagerShutdown(void)
{
    if (!gBalloonMgr.initialized) {
        return;
    }

    /* Clean up current balloon */
    if (gBalloonMgr.currentBalloon) {
        HMBalloonHide(gBalloonMgr.currentBalloon);
        free(gBalloonMgr.currentBalloon);
        gBalloonMgr.currentBalloon = NULL;
    }

    /* Dispose regions */
    if (gBalloonMgr.tempRgn1) {
        DisposeRgn(gBalloonMgr.tempRgn1);
        gBalloonMgr.tempRgn1 = NULL;
    }
    if (gBalloonMgr.tempRgn2) {
        DisposeRgn(gBalloonMgr.tempRgn2);
        gBalloonMgr.tempRgn2 = NULL;
    }

    memset(&gBalloonMgr, 0, sizeof(gBalloonMgr));
}

/*
 * HMBalloonCreate - Create a new balloon
 */
OSErr HMBalloonCreate(const HMBalloonContent *content,
                     const HMBalloonPlacement *placement,
                     HMBalloonInfo *balloonInfo)
{
    OSErr err = noErr;
    Rect balloonRect;
    Point tipPoint;
    HMBalloonTailPosition tailPos;

    if (!gBalloonMgr.initialized) {
        return hmHelpManagerNotInited;
    }

    if (!content || !placement || !balloonInfo) {
        return paramErr;
    }

    /* Initialize balloon info */
    memset(balloonInfo, 0, sizeof(HMBalloonInfo));

    /* Calculate balloon rectangle and position */
    err = HMBalloonCalculateRect(content, placement, &balloonRect,
                                &tipPoint, &tailPos);
    if (err != noErr) {
        return err;
    }

    /* Set up balloon info */
    balloonInfo->balloonRect = balloonRect;
    balloonInfo->tipPoint = tipPoint;
    balloonInfo->tailPos = tailPos;
    balloonInfo->style = gBalloonMgr.config.style;
    balloonInfo->variant = 0;
    balloonInfo->showTime = TickCount();
    balloonInfo->autoHide = true;
    balloonInfo->hideDelay = 300;  /* 5 seconds */

    /* Calculate content rectangle */
    balloonInfo->contentRect = balloonRect;
    InsetRect(&balloonInfo->contentRect, kHMBalloonMargin, kHMBalloonMargin);

    /* Adjust for tail */
    switch (tailPos) {
        case kHMBalloonTailLeft:
            balloonInfo->contentRect.left += kHMBalloonTailSize;
            break;
        case kHMBalloonTailRight:
            balloonInfo->contentRect.right -= kHMBalloonTailSize;
            break;
        case kHMBalloonTailTop:
            balloonInfo->contentRect.top += kHMBalloonTailSize;
            break;
        case kHMBalloonTailBottom:
            balloonInfo->contentRect.bottom -= kHMBalloonTailSize;
            break;
    }

    balloonInfo->isVisible = false;
    return noErr;
}

/*
 * HMBalloonShow - Display a balloon
 */
OSErr HMBalloonShow(HMBalloonInfo *balloonInfo, short method)
{
    OSErr err = noErr;

    if (!gBalloonMgr.initialized) {
        return hmHelpManagerNotInited;
    }

    if (!balloonInfo) {
        return paramErr;
    }

    if (balloonInfo->isVisible) {
        return noErr;  /* Already visible */
    }

    /* Create balloon window based on method */
    switch (method) {
        case kHMRegularWindow:
        case kHMSaveBitsWindow:
            /* Create a floating window for the balloon */
            err = HMBalloonCreateWindow(NULL, &balloonInfo->balloonRect,
                                       &balloonInfo->balloonWindow);
            if (err != noErr) {
                return err;
            }
            break;

        case kHMSaveBitsNoWindow:
            /* Draw directly to screen without window */
            balloonInfo->balloonWindow = NULL;
            break;

        default:
            return hmOperationUnsupported;
    }

    /* Set up balloon regions */
    RgnHandle balloonRgn = NewRgn();
    RgnHandle structRgn = NewRgn();

    if (!balloonRgn || !structRgn) {
        if (balloonRgn) DisposeRgn(balloonRgn);
        if (structRgn) DisposeRgn(structRgn);
        return memFullErr;
    }

    err = HMBalloonSetupRegions(&balloonInfo->balloonRect, balloonInfo->tipPoint,
                               balloonInfo->tailPos, balloonRgn, structRgn);
    if (err != noErr) {
        DisposeRgn(balloonRgn);
        DisposeRgn(structRgn);
        return err;
    }

    /* Show the balloon window */
    if (balloonInfo->balloonWindow) {
        /* Set window shape to balloon region */
        SetWindowShape(balloonInfo->balloonWindow, structRgn);
        ShowWindow(balloonInfo->balloonWindow);
    }

    /* Draw balloon content */
    GrafPtr savedPort;
    GetPort(&savedPort);

    if (balloonInfo->balloonWindow) {
        SetPortWindowPort(balloonInfo->balloonWindow);
    }

    /* Draw balloon background and border */
    HMBalloonDrawBackground(&balloonInfo->balloonRect, balloonInfo->tailPos, NULL);
    HMBalloonDrawBorder(&balloonInfo->balloonRect, balloonInfo->tailPos, NULL);
    HMBalloonDrawTail(&balloonInfo->balloonRect, balloonInfo->tipPoint,
                     balloonInfo->tailPos, NULL);

    /* Draw content in content rectangle */
    HMBalloonDrawContent(&balloonInfo->contentRect, NULL);

    SetPort(savedPort);

    /* Clean up regions */
    DisposeRgn(balloonRgn);
    DisposeRgn(structRgn);

    /* Apply animation if configured */
    if (gBalloonMgr.config.animation != kHMBalloonAnimNone) {
        HMBalloonAnimate(balloonInfo, gBalloonMgr.config.animation, true, 15);
    }

    balloonInfo->isVisible = true;
    gBalloonMgr.currentBalloon = balloonInfo;

    return noErr;
}

/*
 * HMBalloonHide - Hide a balloon
 */
OSErr HMBalloonHide(HMBalloonInfo *balloonInfo)
{
    if (!gBalloonMgr.initialized) {
        return hmHelpManagerNotInited;
    }

    if (!balloonInfo || !balloonInfo->isVisible) {
        return hmNoBalloonUp;
    }

    /* Apply hide animation if configured */
    if (gBalloonMgr.config.animation != kHMBalloonAnimNone) {
        HMBalloonAnimate(balloonInfo, gBalloonMgr.config.animation, false, 10);
    }

    /* Hide balloon window */
    if (balloonInfo->balloonWindow) {
        HideWindow(balloonInfo->balloonWindow);
        DisposeWindow(balloonInfo->balloonWindow);
        balloonInfo->balloonWindow = NULL;
    }

    balloonInfo->isVisible = false;

    if (gBalloonMgr.currentBalloon == balloonInfo) {
        gBalloonMgr.currentBalloon = NULL;
    }

    return noErr;
}

/*
 * HMBalloonCalculateRect - Calculate balloon rectangle and position
 */
OSErr HMBalloonCalculateRect(const HMBalloonContent *content,
                           const HMBalloonPlacement *placement,
                           Rect *balloonRect, Point *tipPoint,
                           HMBalloonTailPosition *tailPos)
{
    OSErr err = noErr;
    Size contentSize;

    if (!content || !placement || !balloonRect || !tipPoint || !tailPos) {
        return paramErr;
    }

    /* Measure content size */
    err = HMBalloonMeasureContent(content, &contentSize);
    if (err != noErr) {
        return err;
    }

    /* Add margins and tail space */
    Size balloonSize;
    balloonSize.h = contentSize.h + 2 * kHMBalloonMargin + kHMBalloonTailSize;
    balloonSize.v = contentSize.v + 2 * kHMBalloonMargin + kHMBalloonTailSize;

    /* Constrain size */
    if (balloonSize.h > kHMBalloonMaxWidth) {
        balloonSize.h = kHMBalloonMaxWidth;
    }
    if (balloonSize.v > kHMBalloonMaxHeight) {
        balloonSize.v = kHMBalloonMaxHeight;
    }
    if (balloonSize.h < kHMBalloonMinWidth) {
        balloonSize.h = kHMBalloonMinWidth;
    }
    if (balloonSize.v < kHMBalloonMinHeight) {
        balloonSize.v = kHMBalloonMinHeight;
    }

    /* Find best position */
    return HMBalloonFindBestPosition(&placement->hotRect, &placement->screenBounds,
                                   &balloonSize, tipPoint, balloonRect, tailPos);
}

/*
 * HMBalloonFindBestPosition - Find the best balloon position
 */
OSErr HMBalloonFindBestPosition(const Rect *hotRect, const Rect *screenBounds,
                              const Size *balloonSize, Point *tipPoint,
                              Rect *balloonRect, HMBalloonTailPosition *tailPos)
{
    if (!hotRect || !screenBounds || !balloonSize || !tipPoint ||
        !balloonRect || !tailPos) {
        return paramErr;
    }

    /* Try positions in order of preference: below, above, right, left */
    Rect testRect;
    Point testTip;

    /* Try below hot rect */
    testTip.h = (hotRect->left + hotRect->right) / 2;
    testTip.v = hotRect->bottom;
    SetRect(&testRect, testTip.h - balloonSize->h / 2, testTip.v,
            testTip.h + balloonSize->h / 2, testTip.v + balloonSize->v);

    if (HMBalloonRectFitsOnScreen(&testRect, screenBounds)) {
        *balloonRect = testRect;
        *tipPoint = testTip;
        *tailPos = kHMBalloonTailTop;
        return noErr;
    }

    /* Try above hot rect */
    testTip.h = (hotRect->left + hotRect->right) / 2;
    testTip.v = hotRect->top;
    SetRect(&testRect, testTip.h - balloonSize->h / 2, testTip.v - balloonSize->v,
            testTip.h + balloonSize->h / 2, testTip.v);

    if (HMBalloonRectFitsOnScreen(&testRect, screenBounds)) {
        *balloonRect = testRect;
        *tipPoint = testTip;
        *tailPos = kHMBalloonTailBottom;
        return noErr;
    }

    /* Try to the right */
    testTip.h = hotRect->right;
    testTip.v = (hotRect->top + hotRect->bottom) / 2;
    SetRect(&testRect, testTip.h, testTip.v - balloonSize->v / 2,
            testTip.h + balloonSize->h, testTip.v + balloonSize->v / 2);

    if (HMBalloonRectFitsOnScreen(&testRect, screenBounds)) {
        *balloonRect = testRect;
        *tipPoint = testTip;
        *tailPos = kHMBalloonTailLeft;
        return noErr;
    }

    /* Try to the left */
    testTip.h = hotRect->left;
    testTip.v = (hotRect->top + hotRect->bottom) / 2;
    SetRect(&testRect, testTip.h - balloonSize->h, testTip.v - balloonSize->v / 2,
            testTip.h, testTip.v + balloonSize->v / 2);

    if (HMBalloonRectFitsOnScreen(&testRect, screenBounds)) {
        *balloonRect = testRect;
        *tipPoint = testTip;
        *tailPos = kHMBalloonTailRight;
        return noErr;
    }

    /* If nothing fits, use below and adjust to fit */
    testTip.h = (hotRect->left + hotRect->right) / 2;
    testTip.v = hotRect->bottom;
    SetRect(&testRect, testTip.h - balloonSize->h / 2, testTip.v,
            testTip.h + balloonSize->h / 2, testTip.v + balloonSize->v);

    HMBalloonAdjustForScreen(&testRect, screenBounds);

    *balloonRect = testRect;
    *tipPoint = testTip;
    *tailPos = kHMBalloonTailTop;

    return noErr;
}

/*
 * HMBalloonRectFitsOnScreen - Check if balloon fits on screen
 */
Boolean HMBalloonRectFitsOnScreen(const Rect *balloonRect, const Rect *screenBounds)
{
    if (!balloonRect || !screenBounds) {
        return false;
    }

    return (balloonRect->left >= screenBounds->left &&
            balloonRect->right <= screenBounds->right &&
            balloonRect->top >= screenBounds->top &&
            balloonRect->bottom <= screenBounds->bottom);
}

/*
 * HMBalloonAdjustForScreen - Adjust balloon to fit on screen
 */
OSErr HMBalloonAdjustForScreen(Rect *balloonRect, const Rect *screenBounds)
{
    if (!balloonRect || !screenBounds) {
        return paramErr;
    }

    /* Adjust horizontally */
    if (balloonRect->right > screenBounds->right) {
        OffsetRect(balloonRect, screenBounds->right - balloonRect->right, 0);
    }
    if (balloonRect->left < screenBounds->left) {
        OffsetRect(balloonRect, screenBounds->left - balloonRect->left, 0);
    }

    /* Adjust vertically */
    if (balloonRect->bottom > screenBounds->bottom) {
        OffsetRect(balloonRect, 0, screenBounds->bottom - balloonRect->bottom);
    }
    if (balloonRect->top < screenBounds->top) {
        OffsetRect(balloonRect, 0, screenBounds->top - balloonRect->top);
    }

    return noErr;
}

/*
 * HMBalloonDrawBackground - Draw balloon background
 */
OSErr HMBalloonDrawBackground(const Rect *balloonRect,
                            HMBalloonTailPosition tailPos,
                            const HMBalloonContent *content)
{
    if (!balloonRect) {
        return paramErr;
    }

    /* Set background color */
    RGBColor bgColor = {0xFFFF, 0xFFFF, 0xE000};  /* Light yellow */
    if (content && content->backgroundColor.red != 0) {
        bgColor = content->backgroundColor;
    }

    RGBForeColor(&bgColor);
    PaintRect(balloonRect);

    return noErr;
}

/*
 * HMBalloonDrawBorder - Draw balloon border
 */
OSErr HMBalloonDrawBorder(const Rect *balloonRect,
                        HMBalloonTailPosition tailPos,
                        const HMBalloonContent *content)
{
    if (!balloonRect) {
        return paramErr;
    }

    /* Set border color */
    RGBColor borderColor = {0x0000, 0x0000, 0x0000};  /* Black */
    if (content && content->borderColor.red != 0) {
        borderColor = content->borderColor;
    }

    RGBForeColor(&borderColor);
    FrameRect(balloonRect);

    return noErr;
}

/*
 * HMBalloonDrawTail - Draw balloon tail
 */
OSErr HMBalloonDrawTail(const Rect *balloonRect, Point tipPoint,
                      HMBalloonTailPosition tailPos,
                      const HMBalloonContent *content)
{
    return HMBalloonDrawTailInternal(balloonRect, tipPoint, tailPos);
}

/*
 * HMBalloonDrawContent - Draw balloon content
 */
OSErr HMBalloonDrawContent(const Rect *contentRect,
                         const HMBalloonContent *content)
{
    if (!contentRect) {
        return paramErr;
    }

    if (!content || !content->message) {
        /* Draw placeholder text */
        MoveTo(contentRect->left + 4, contentRect->top + 12);
        DrawString("\pHelp content");
        return noErr;
    }

    /* Set text color */
    RGBColor textColor = {0x0000, 0x0000, 0x0000};  /* Black */
    if (content->textColor.red != 0) {
        textColor = content->textColor;
    }
    RGBForeColor(&textColor);

    /* Set font */
    TextFont(content->fontID);
    TextSize(content->fontSize);
    TextFace(content->fontStyle);

    /* Draw content based on message type */
    switch (content->message->hmmHelpType) {
        case khmmString:
            /* Draw Pascal string */
            MoveTo(contentRect->left + 4, contentRect->top + content->fontSize + 2);
            DrawString((ConstStr255Param)content->message->u.hmmString);
            break;

        case khmmPict:
            /* Draw picture */
            /* Implementation would load and draw PICT resource */
            break;

        case khmmSTRRes:
            /* Draw STR resource */
            /* Implementation would load and draw STR resource */
            break;

        default:
            /* Draw error message */
            MoveTo(contentRect->left + 4, contentRect->top + 12);
            DrawString("\pUnsupported help type");
            break;
    }

    return noErr;
}

/*
 * HMBalloonMeasureContent - Measure content size
 */
OSErr HMBalloonMeasureContent(const HMBalloonContent *content, Size *contentSize)
{
    if (!content || !contentSize) {
        return paramErr;
    }

    /* Default size */
    contentSize->h = 120;
    contentSize->v = 30;

    if (!content->message) {
        return noErr;
    }

    /* Set font for measurement */
    GrafPtr savedPort;
    GetPort(&savedPort);

    TextFont(content->fontID);
    TextSize(content->fontSize);
    TextFace(content->fontStyle);

    /* Measure based on message type */
    switch (content->message->hmmHelpType) {
        case khmmString: {
            /* Measure Pascal string */
            unsigned char *str = (unsigned char *)content->message->u.hmmString;
            if (str[0] > 0) {
                contentSize->h = TextWidth(str, 1, str[0]) + 8;
                FontInfo fontInfo;
                GetFontInfo(&fontInfo);
                contentSize->v = fontInfo.ascent + fontInfo.descent + fontInfo.leading + 4;
            }
            break;
        }

        case khmmPict:
            /* Picture size would be determined from PICT resource */
            contentSize->h = 100;
            contentSize->v = 80;
            break;

        default:
            /* Use default size */
            break;
    }

    SetPort(savedPort);
    return noErr;
}

/*
 * Animation functions
 */
OSErr HMBalloonAnimate(HMBalloonInfo *balloonInfo, HMBalloonAnimation animation,
                     Boolean showing, long duration)
{
    if (!balloonInfo) {
        return paramErr;
    }

    /* Simple animation implementation */
    gBalloonMgr.animationStartTime = TickCount();
    gBalloonMgr.animationActive = true;

    switch (animation) {
        case kHMBalloonAnimFade:
            if (showing) {
                return HMBalloonFadeIn(balloonInfo, duration);
            } else {
                return HMBalloonFadeOut(balloonInfo, duration);
            }

        case kHMBalloonAnimSlide:
            if (showing) {
                return HMBalloonSlideIn(balloonInfo, balloonInfo->tailPos, duration);
            } else {
                return HMBalloonSlideOut(balloonInfo, balloonInfo->tailPos, duration);
            }

        case kHMBalloonAnimPop:
            /* Pop animation would scale from 0 to full size */
            break;

        default:
            break;
    }

    gBalloonMgr.animationActive = false;
    return noErr;
}

OSErr HMBalloonFadeIn(HMBalloonInfo *balloonInfo, long duration)
{
    /* Implementation would gradually increase opacity */
    return noErr;
}

OSErr HMBalloonFadeOut(HMBalloonInfo *balloonInfo, long duration)
{
    /* Implementation would gradually decrease opacity */
    return noErr;
}

OSErr HMBalloonSlideIn(HMBalloonInfo *balloonInfo, HMBalloonTailPosition fromSide,
                     long duration)
{
    /* Implementation would slide balloon in from specified side */
    return noErr;
}

OSErr HMBalloonSlideOut(HMBalloonInfo *balloonInfo, HMBalloonTailPosition toSide,
                      long duration)
{
    /* Implementation would slide balloon out to specified side */
    return noErr;
}

/*
 * Modern balloon configuration
 */
OSErr HMBalloonConfigureModern(const HMModernBalloonConfig *config)
{
    if (!gBalloonMgr.initialized) {
        return hmHelpManagerNotInited;
    }

    if (!config) {
        return paramErr;
    }

    gBalloonMgr.config = *config;
    return noErr;
}

/*
 * Utility functions
 */
OSErr HMBalloonGetCurrent(HMBalloonInfo **currentBalloon)
{
    if (!currentBalloon) {
        return paramErr;
    }

    *currentBalloon = gBalloonMgr.currentBalloon;
    return noErr;
}

OSErr HMBalloonSetCurrent(HMBalloonInfo *balloonInfo)
{
    gBalloonMgr.currentBalloon = balloonInfo;
    return noErr;
}

void HMBalloonCleanupAll(void)
{
    if (gBalloonMgr.currentBalloon) {
        HMBalloonHide(gBalloonMgr.currentBalloon);
        free(gBalloonMgr.currentBalloon);
        gBalloonMgr.currentBalloon = NULL;
    }
}

/*
 * Private helper functions
 */

static OSErr HMBalloonCreateWindow(const HMBalloonContent *content,
                                  const Rect *balloonRect, WindowPtr *window)
{
    if (!balloonRect || !window) {
        return paramErr;
    }

    /* Create a floating window for the balloon */
    *window = NewWindow(NULL, balloonRect, "\p", false, plainDBox,
                       (WindowPtr)-1L, false, 0);

    if (!*window) {
        return memFullErr;
    }

    return noErr;
}

static OSErr HMBalloonDrawTailInternal(const Rect *balloonRect, Point tipPoint,
                                      HMBalloonTailPosition tailPos)
{
    if (!balloonRect) {
        return paramErr;
    }

    /* Draw a simple triangular tail */
    PolyHandle poly = OpenPoly();

    switch (tailPos) {
        case kHMBalloonTailTop:
            MoveTo(tipPoint.h - 6, balloonRect->top);
            LineTo(tipPoint.h, tipPoint.v);
            LineTo(tipPoint.h + 6, balloonRect->top);
            LineTo(tipPoint.h - 6, balloonRect->top);
            break;

        case kHMBalloonTailBottom:
            MoveTo(tipPoint.h - 6, balloonRect->bottom);
            LineTo(tipPoint.h, tipPoint.v);
            LineTo(tipPoint.h + 6, balloonRect->bottom);
            LineTo(tipPoint.h - 6, balloonRect->bottom);
            break;

        case kHMBalloonTailLeft:
            MoveTo(balloonRect->left, tipPoint.v - 6);
            LineTo(tipPoint.h, tipPoint.v);
            LineTo(balloonRect->left, tipPoint.v + 6);
            LineTo(balloonRect->left, tipPoint.v - 6);
            break;

        case kHMBalloonTailRight:
            MoveTo(balloonRect->right, tipPoint.v - 6);
            LineTo(tipPoint.h, tipPoint.v);
            LineTo(balloonRect->right, tipPoint.v + 6);
            LineTo(balloonRect->right, tipPoint.v - 6);
            break;
    }

    ClosePoly();

    if (poly) {
        PaintPoly(poly);
        KillPoly(poly);
    }

    return noErr;
}

static OSErr HMBalloonSetupRegions(const Rect *balloonRect, Point tipPoint,
                                  HMBalloonTailPosition tailPos, RgnHandle balloonRgn,
                                  RgnHandle structRgn)
{
    if (!balloonRect || !balloonRgn || !structRgn) {
        return paramErr;
    }

    /* Create balloon region */
    RectRgn(balloonRgn, balloonRect);

    /* Add tail to region */
    RgnHandle tailRgn = NewRgn();
    if (!tailRgn) {
        return memFullErr;
    }

    /* Create tail region based on position */
    Rect tailRect;
    switch (tailPos) {
        case kHMBalloonTailTop:
            SetRect(&tailRect, tipPoint.h - 6, tipPoint.v,
                    tipPoint.h + 6, balloonRect->top);
            break;
        case kHMBalloonTailBottom:
            SetRect(&tailRect, tipPoint.h - 6, balloonRect->bottom,
                    tipPoint.h + 6, tipPoint.v);
            break;
        case kHMBalloonTailLeft:
            SetRect(&tailRect, tipPoint.h, tipPoint.v - 6,
                    balloonRect->left, tipPoint.v + 6);
            break;
        case kHMBalloonTailRight:
            SetRect(&tailRect, balloonRect->right, tipPoint.v - 6,
                    tipPoint.h, tipPoint.v + 6);
            break;
        default:
            SetRect(&tailRect, 0, 0, 0, 0);
            break;
    }

    RectRgn(tailRgn, &tailRect);
    UnionRgn(balloonRgn, tailRgn, balloonRgn);
    DisposeRgn(tailRgn);

    /* Structure region is same as balloon region */
    CopyRgn(balloonRgn, structRgn);

    return noErr;
}