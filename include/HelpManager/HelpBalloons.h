/*
 * HelpBalloons.h - Help Balloon Display and Positioning
 *
 * This file defines structures and functions for displaying and positioning
 * help balloons in the Mac OS Help Manager. Balloons are floating windows
 * that display context-sensitive help information.
 */

#ifndef HELPBALLOONS_H
#define HELPBALLOONS_H

#include "MacTypes.h"
#include "Quickdraw.h"
#include "Windows.h"
#include "HelpManager.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Balloon positioning constants */
enum {
    kHMBalloonMargin = 8,            /* Margin around balloon content */
    kHMBalloonTailSize = 12,         /* Size of balloon tail */
    kHMBalloonCornerRadius = 8,      /* Radius of balloon corners */
    kHMBalloonShadowOffset = 2,      /* Shadow offset in pixels */
    kHMBalloonMaxWidth = 300,        /* Maximum balloon width */
    kHMBalloonMaxHeight = 200,       /* Maximum balloon height */
    kHMBalloonMinWidth = 80,         /* Minimum balloon width */
    kHMBalloonMinHeight = 24         /* Minimum balloon height */
};

/* Balloon tail positions */
typedef enum {
    kHMBalloonTailNone = 0,          /* No tail */
    kHMBalloonTailLeft = 1,          /* Tail on left side */
    kHMBalloonTailRight = 2,         /* Tail on right side */
    kHMBalloonTailTop = 3,           /* Tail on top */
    kHMBalloonTailBottom = 4,        /* Tail on bottom */
    kHMBalloonTailTopLeft = 5,       /* Tail on top-left corner */
    kHMBalloonTailTopRight = 6,      /* Tail on top-right corner */
    kHMBalloonTailBottomLeft = 7,    /* Tail on bottom-left corner */
    kHMBalloonTailBottomRight = 8    /* Tail on bottom-right corner */
} HMBalloonTailPosition;

/* Balloon appearance styles */
typedef enum {
    kHMBalloonStyleClassic = 0,      /* Classic yellow balloon */
    kHMBalloonStyleSystem = 1,       /* System-styled balloon */
    kHMBalloonStyleApplication = 2,  /* Application-styled balloon */
    kHMBalloonStyleAccessible = 3    /* High-contrast accessible style */
} HMBalloonStyle;

/* Balloon animation types */
typedef enum {
    kHMBalloonAnimNone = 0,          /* No animation */
    kHMBalloonAnimFade = 1,          /* Fade in/out */
    kHMBalloonAnimSlide = 2,         /* Slide in/out */
    kHMBalloonAnimPop = 3            /* Pop in/out */
} HMBalloonAnimation;

/* Balloon state information */
typedef struct HMBalloonInfo {
    Boolean isVisible;               /* Is balloon currently visible */
    WindowPtr balloonWindow;         /* Balloon window reference */
    Rect balloonRect;                /* Balloon rectangle */
    Rect contentRect;                /* Content area rectangle */
    Point tipPoint;                  /* Tip point location */
    HMBalloonTailPosition tailPos;   /* Tail position */
    HMBalloonStyle style;            /* Balloon visual style */
    short variant;                   /* Balloon variant number */
    long showTime;                   /* Time balloon was shown */
    long hideDelay;                  /* Delay before auto-hide */
    Boolean autoHide;                /* Auto-hide enabled */
} HMBalloonInfo;

/* Balloon positioning parameters */
typedef struct HMBalloonPlacement {
    Point tipPoint;                  /* Where balloon points to */
    Rect hotRect;                    /* Hot rectangle for positioning */
    Rect alternateRect;              /* Alternate positioning rectangle */
    Rect screenBounds;               /* Screen boundaries */
    short preferredSide;             /* Preferred side for balloon */
    Boolean allowOffscreen;          /* Allow balloon off-screen */
    Boolean constrainToScreen;       /* Keep balloon on screen */
} HMBalloonPlacement;

/* Balloon content rendering information */
typedef struct HMBalloonContent {
    HMMessageRecord *message;        /* Help message to display */
    short fontID;                    /* Font to use */
    short fontSize;                  /* Font size */
    short fontStyle;                 /* Font style (bold, italic, etc.) */
    RGBColor textColor;              /* Text color */
    RGBColor backgroundColor;        /* Background color */
    RGBColor borderColor;            /* Border color */
    Boolean hasIcon;                 /* Include help icon */
    short iconID;                    /* Icon resource ID */
} HMBalloonContent;

/* Modern balloon configuration */
typedef struct HMModernBalloonConfig {
    HMBalloonStyle style;            /* Visual style */
    HMBalloonAnimation animation;    /* Animation type */
    Boolean useDropShadow;           /* Use drop shadow */
    Boolean useGradient;             /* Use gradient background */
    Boolean useTransparency;         /* Use transparency effects */
    float opacity;                   /* Balloon opacity (0.0-1.0) */
    short cornerRadius;              /* Corner radius in pixels */
    Boolean respectAccessibility;    /* Respect accessibility settings */
} HMModernBalloonConfig;

/* Balloon display functions */
OSErr HMBalloonCreate(const HMBalloonContent *content,
                     const HMBalloonPlacement *placement,
                     HMBalloonInfo *balloonInfo);

OSErr HMBalloonShow(HMBalloonInfo *balloonInfo, short method);
OSErr HMBalloonHide(HMBalloonInfo *balloonInfo);
OSErr HMBalloonUpdate(HMBalloonInfo *balloonInfo,
                     const HMBalloonContent *newContent);

/* Balloon positioning functions */
OSErr HMBalloonCalculateRect(const HMBalloonContent *content,
                           const HMBalloonPlacement *placement,
                           Rect *balloonRect, Point *tipPoint,
                           HMBalloonTailPosition *tailPos);

OSErr HMBalloonFindBestPosition(const Rect *hotRect, const Rect *screenBounds,
                              const Size *balloonSize, Point *tipPoint,
                              Rect *balloonRect, HMBalloonTailPosition *tailPos);

Boolean HMBalloonRectFitsOnScreen(const Rect *balloonRect, const Rect *screenBounds);
OSErr HMBalloonAdjustForScreen(Rect *balloonRect, const Rect *screenBounds);

/* Balloon drawing functions */
OSErr HMBalloonDrawBackground(const Rect *balloonRect,
                            HMBalloonTailPosition tailPos,
                            const HMBalloonContent *content);

OSErr HMBalloonDrawContent(const Rect *contentRect,
                         const HMBalloonContent *content);

OSErr HMBalloonDrawBorder(const Rect *balloonRect,
                        HMBalloonTailPosition tailPos,
                        const HMBalloonContent *content);

OSErr HMBalloonDrawTail(const Rect *balloonRect, Point tipPoint,
                      HMBalloonTailPosition tailPos,
                      const HMBalloonContent *content);

/* Balloon region functions */
OSErr HMBalloonCreateRegions(const Rect *balloonRect, Point tipPoint,
                           HMBalloonTailPosition tailPos,
                           RgnHandle balloonRgn, RgnHandle structRgn);

OSErr HMBalloonCreateContentRgn(const Rect *balloonRect, RgnHandle contentRgn);
OSErr HMBalloonCreateTailRgn(const Rect *balloonRect, Point tipPoint,
                           HMBalloonTailPosition tailPos, RgnHandle tailRgn);

/* Balloon animation functions */
OSErr HMBalloonAnimate(HMBalloonInfo *balloonInfo, HMBalloonAnimation animation,
                     Boolean showing, long duration);

OSErr HMBalloonFadeIn(HMBalloonInfo *balloonInfo, long duration);
OSErr HMBalloonFadeOut(HMBalloonInfo *balloonInfo, long duration);
OSErr HMBalloonSlideIn(HMBalloonInfo *balloonInfo, HMBalloonTailPosition fromSide,
                     long duration);
OSErr HMBalloonSlideOut(HMBalloonInfo *balloonInfo, HMBalloonTailPosition toSide,
                      long duration);

/* Balloon utilities */
Boolean HMBalloonHitTest(const HMBalloonInfo *balloonInfo, Point mousePoint);
OSErr HMBalloonInvalidate(const HMBalloonInfo *balloonInfo);
OSErr HMBalloonGetWindow(const HMBalloonInfo *balloonInfo, WindowPtr *window);

/* Modern balloon functions */
OSErr HMBalloonConfigureModern(const HMModernBalloonConfig *config);
OSErr HMBalloonSetStyle(HMBalloonInfo *balloonInfo, HMBalloonStyle style);
OSErr HMBalloonSetAnimation(HMBalloonInfo *balloonInfo, HMBalloonAnimation animation);
OSErr HMBalloonSetOpacity(HMBalloonInfo *balloonInfo, float opacity);

/* Accessibility support */
OSErr HMBalloonSetAccessibilityDescription(HMBalloonInfo *balloonInfo,
                                         const char *description);
OSErr HMBalloonAnnounceToScreenReader(const HMBalloonInfo *balloonInfo);
Boolean HMBalloonIsAccessibilityEnabled(void);

/* Balloon management */
OSErr HMBalloonManagerInit(void);
void HMBalloonManagerShutdown(void);
OSErr HMBalloonGetCurrent(HMBalloonInfo **currentBalloon);
OSErr HMBalloonSetCurrent(HMBalloonInfo *balloonInfo);
void HMBalloonCleanupAll(void);

/* Content measurement functions */
OSErr HMBalloonMeasureContent(const HMBalloonContent *content, Size *contentSize);
OSErr HMBalloonMeasureText(const char *text, short fontID, short fontSize,
                         short fontStyle, Size *textSize);
OSErr HMBalloonMeasurePicture(PicHandle picture, Size *pictureSize);

/* Platform-specific balloon rendering */
#ifdef __APPLE__
OSErr HMBalloonDrawCocoa(const HMBalloonInfo *balloonInfo);
#endif

#ifdef _WIN32
OSErr HMBalloonDrawWin32(const HMBalloonInfo *balloonInfo);
#endif

#ifdef __linux__
OSErr HMBalloonDrawGTK(const HMBalloonInfo *balloonInfo);
#endif

#ifdef __cplusplus
}
#endif

#endif /* HELPBALLOONS_H */