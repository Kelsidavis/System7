/*
 * HelpManagerCore.c - Mac OS Help Manager Core Implementation
 *
 * This file implements the core Help Manager functionality for displaying
 * context-sensitive help balloons and managing help resources.
 *
 * The Help Manager provides balloon help that appears when users hover over
 * interface elements, offering guidance and explanations for application features.
 */

#include "HelpManager.h"
#include "HelpBalloons.h"
#include "HelpContent.h"
#include "ContextHelp.h"
#include "HelpResources.h"
#include "UserPreferences.h"

#include "MacTypes.h"
#include "Quickdraw.h"
#include "Windows.h"
#include "Menus.h"
#include "Resources.h"
#include "Memory.h"
#include "Errors.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* Help Manager global state */
typedef struct HMGlobalState {
    Boolean initialized;             /* Help Manager is initialized */
    Boolean balloonsEnabled;         /* Balloon help is enabled */
    HMBalloonInfo *currentBalloon;   /* Currently displayed balloon */
    MenuHandle helpMenu;             /* Help menu handle */
    short helpMenuID;                /* Help menu ID */
    HMDetectionState detectionState; /* Context detection state */
    HMPreferences preferences;       /* User preferences */
    HMModernHelpConfig modernConfig; /* Modern help configuration */

    /* Resource management */
    short defaultResFile;            /* Default resource file */
    short dialogResID;               /* Current dialog resource ID */

    /* Font and appearance */
    short helpFont;                  /* Help text font */
    short helpFontSize;              /* Help text size */

    /* Timing and behavior */
    long lastBalloonTime;            /* Last balloon display time */
    Boolean modalDialogActive;       /* Modal dialog is active */

    /* Statistics */
    long balloonDisplayCount;        /* Number of balloons displayed */
    long helpRequestCount;           /* Number of help requests */
} HMGlobalState;

/* Global Help Manager state */
static HMGlobalState gHMState = {0};

/* Forward declarations */
static OSErr HMInitializeGlobals(void);
static OSErr HMCreateHelpMenu(void);
static OSErr HMUpdateHelpMenu(void);
static Boolean HMValidateMessageRecord(const HMMessageRecord *message);
static OSErr HMCalculateBalloonPosition(const HMMessageRecord *message, Point tip,
                                       RectPtr alternateRect, Rect *balloonRect,
                                       Point *tipPoint);

/*
 * HMInitialize - Initialize the Help Manager
 */
OSErr HMInitialize(void)
{
    OSErr err = noErr;

    if (gHMState.initialized) {
        return noErr;  /* Already initialized */
    }

    /* Initialize global state */
    err = HMInitializeGlobals();
    if (err != noErr) return err;

    /* Initialize subsystems */
    err = HMPrefInit();
    if (err != noErr) goto cleanup;

    err = HMResourceInit(50, 1024 * 1024);  /* 50 entries, 1MB cache */
    if (err != noErr) goto cleanup;

    err = HMContentCacheInit(100, 2 * 1024 * 1024);  /* 100 entries, 2MB cache */
    if (err != noErr) goto cleanup;

    err = HMContextDetectionInit(&gHMState.preferences.timing);
    if (err != noErr) goto cleanup;

    err = HMBalloonManagerInit();
    if (err != noErr) goto cleanup;

    /* Load user preferences */
    HMPrefLoad(kHMPrefStorageUser);
    HMPrefGetGeneral(&gHMState.preferences.general);
    HMPrefGetAppearance(&gHMState.preferences.appearance);
    HMPrefGetTiming(&gHMState.preferences.timing);
    HMPrefGetBehavior(&gHMState.preferences.behavior);

    /* Apply preferences */
    gHMState.balloonsEnabled = gHMState.preferences.general.balloonsEnabled;
    gHMState.helpFont = gHMState.preferences.appearance.fontID;
    gHMState.helpFontSize = gHMState.preferences.appearance.fontSize;

    /* Create help menu */
    err = HMCreateHelpMenu();
    if (err != noErr) goto cleanup;

    /* Start context detection if enabled */
    if (gHMState.balloonsEnabled) {
        HMContextStartTracking();
    }

    gHMState.initialized = true;
    return noErr;

cleanup:
    HMShutdown();
    return err;
}

/*
 * HMShutdown - Shutdown the Help Manager
 */
void HMShutdown(void)
{
    if (!gHMState.initialized) {
        return;
    }

    /* Hide any visible balloon */
    if (gHMState.currentBalloon) {
        HMRemoveBalloon();
    }

    /* Stop context detection */
    HMContextStopTracking();

    /* Save preferences */
    HMPrefSave(kHMPrefStorageUser);

    /* Shutdown subsystems */
    HMBalloonManagerShutdown();
    HMContextDetectionShutdown();
    HMContentCacheShutdown();
    HMResourceShutdown();
    HMPrefShutdown();

    /* Clean up global state */
    if (gHMState.currentBalloon) {
        free(gHMState.currentBalloon);
        gHMState.currentBalloon = NULL;
    }

    memset(&gHMState, 0, sizeof(gHMState));
}

/*
 * HMGetHelpMenuHandle - Get the help menu handle
 */
OSErr HMGetHelpMenuHandle(MenuHandle *mh)
{
    if (!gHMState.initialized) {
        return hmHelpManagerNotInited;
    }

    if (!mh) {
        return paramErr;
    }

    if (!gHMState.helpMenu) {
        OSErr err = HMCreateHelpMenu();
        if (err != noErr) return err;
    }

    *mh = gHMState.helpMenu;
    return noErr;
}

/*
 * HMShowBalloon - Display a help balloon
 */
OSErr HMShowBalloon(const HMMessageRecord *aHelpMsg, Point tip,
                   RectPtr alternateRect, Ptr tipProc, short theProc,
                   short variant, short method)
{
    OSErr err = noErr;
    HMBalloonInfo *balloonInfo = NULL;
    HMBalloonContent content = {0};
    HMBalloonPlacement placement = {0};
    Rect balloonRect;
    Point tipPoint;

    /* Check if Help Manager is initialized */
    if (!gHMState.initialized) {
        return hmHelpManagerNotInited;
    }

    /* Check if balloons are enabled */
    if (!gHMState.balloonsEnabled) {
        return hmHelpDisabled;
    }

    /* Validate parameters */
    if (!aHelpMsg) {
        return paramErr;
    }

    if (!HMValidateMessageRecord(aHelpMsg)) {
        return hmUnknownHelpType;
    }

    /* Check for unsupported methods */
    if (method < kHMRegularWindow || method > kHMSaveBitsWindow) {
        return hmOperationUnsupported;
    }

    /* Remove any existing balloon */
    if (gHMState.currentBalloon) {
        HMRemoveBalloon();
    }

    /* Set up balloon content */
    content.message = (HMMessageRecord *)aHelpMsg;
    content.fontID = gHMState.helpFont;
    content.fontSize = gHMState.helpFontSize;
    content.fontStyle = normal;
    content.textColor = gHMState.preferences.appearance.textColor;
    content.backgroundColor = gHMState.preferences.appearance.backgroundColor;
    content.borderColor = gHMState.preferences.appearance.borderColor;

    /* Set up balloon placement */
    placement.tipPoint = tip;
    if (alternateRect) {
        placement.alternateRect = *alternateRect;
    } else {
        SetRect(&placement.alternateRect, 0, 0, 0, 0);
    }

    /* Get screen bounds */
    BitMap screenBits;
    GetQDGlobalsScreenBits(&screenBits);
    placement.screenBounds = screenBits.bounds;
    placement.constrainToScreen = true;

    /* Calculate balloon position */
    err = HMCalculateBalloonPosition(aHelpMsg, tip, alternateRect,
                                    &balloonRect, &tipPoint);
    if (err != noErr) {
        return err;
    }

    /* Allocate balloon info */
    balloonInfo = (HMBalloonInfo *)calloc(1, sizeof(HMBalloonInfo));
    if (!balloonInfo) {
        return memFullErr;
    }

    /* Create the balloon */
    err = HMBalloonCreate(&content, &placement, balloonInfo);
    if (err != noErr) {
        free(balloonInfo);
        return err;
    }

    /* Show the balloon */
    err = HMBalloonShow(balloonInfo, method);
    if (err != noErr) {
        free(balloonInfo);
        return err;
    }

    /* Set as current balloon */
    gHMState.currentBalloon = balloonInfo;
    gHMState.lastBalloonTime = TickCount();
    gHMState.balloonDisplayCount++;

    return noErr;
}

/*
 * HMRemoveBalloon - Remove the currently displayed balloon
 */
OSErr HMRemoveBalloon(void)
{
    if (!gHMState.initialized) {
        return hmHelpManagerNotInited;
    }

    if (!gHMState.currentBalloon) {
        return hmNoBalloonUp;
    }

    /* Hide the balloon */
    HMBalloonHide(gHMState.currentBalloon);

    /* Clean up */
    free(gHMState.currentBalloon);
    gHMState.currentBalloon = NULL;

    return noErr;
}

/*
 * HMGetBalloons - Check if balloons are currently enabled
 */
Boolean HMGetBalloons(void)
{
    if (!gHMState.initialized) {
        return false;
    }

    return gHMState.balloonsEnabled;
}

/*
 * HMSetBalloons - Enable or disable balloon help
 */
OSErr HMSetBalloons(Boolean flag)
{
    if (!gHMState.initialized) {
        return hmHelpManagerNotInited;
    }

    /* Update state */
    gHMState.balloonsEnabled = flag;
    gHMState.preferences.general.balloonsEnabled = flag;

    /* Update preferences */
    HMPrefSetBoolean("general.balloonsEnabled", flag);

    /* Start or stop context detection */
    if (flag) {
        HMContextStartTracking();
    } else {
        HMContextStopTracking();
        /* Remove any visible balloon */
        if (gHMState.currentBalloon) {
            HMRemoveBalloon();
        }
    }

    /* Update help menu */
    HMUpdateHelpMenu();

    return noErr;
}

/*
 * HMIsBalloon - Check if a balloon is currently visible
 */
Boolean HMIsBalloon(void)
{
    if (!gHMState.initialized) {
        return false;
    }

    return (gHMState.currentBalloon != NULL &&
            gHMState.currentBalloon->isVisible);
}

/*
 * HMShowMenuBalloon - Show help for a menu item
 */
OSErr HMShowMenuBalloon(short itemNum, short itemMenuID, long itemFlags,
                       long itemReserved, Point tip, RectPtr alternateRect,
                       Ptr tipProc, short theProc, short variant)
{
    OSErr err = noErr;
    HMMessageRecord message;

    /* Check if this is the same as the last balloon */
    if (gHMState.currentBalloon) {
        /* Implementation would check if same menu item */
        return hmSameAsLastBalloon;
    }

    /* Extract help message for menu item */
    err = HMResourceExtractMenuMessage(itemMenuID, itemNum,
                                      (itemFlags & 0xFF), &message);
    if (err != noErr) {
        return err;
    }

    /* Show the balloon */
    return HMShowBalloon(&message, tip, alternateRect, tipProc,
                        theProc, variant, kHMRegularWindow);
}

/*
 * HMGetIndHelpMsg - Extract a help message from resources
 */
OSErr HMGetIndHelpMsg(ResType whichType, short whichResID, short whichMsg,
                     short whichState, long *options, Point *tip, Rect *altRect,
                     short *theProc, short *variant, HMMessageRecord *aHelpMsg,
                     short *count)
{
    if (!gHMState.initialized) {
        return hmHelpManagerNotInited;
    }

    /* Validate parameters */
    if (!aHelpMsg || !count) {
        return paramErr;
    }

    /* Extract message from resource */
    OSErr err = HMResourceExtractMessage(whichType, whichResID, whichMsg,
                                        whichState, aHelpMsg, tip, altRect,
                                        options);
    if (err != noErr) {
        return err;
    }

    /* Set procedure and variant if provided */
    if (theProc) *theProc = kBalloonWDEFID;
    if (variant) *variant = 0;

    /* Set count to 1 for single message */
    *count = 1;

    return noErr;
}

/*
 * HMExtractHelpMsg - Extract a help message from resources (simpler version)
 */
OSErr HMExtractHelpMsg(ResType whichType, short whichResID, short whichMsg,
                      short whichState, HMMessageRecord *aHelpMsg)
{
    return HMGetIndHelpMsg(whichType, whichResID, whichMsg, whichState,
                          NULL, NULL, NULL, NULL, NULL, aHelpMsg, NULL);
}

/*
 * Font and appearance functions
 */
OSErr HMSetFont(short font)
{
    if (!gHMState.initialized) {
        return hmHelpManagerNotInited;
    }

    gHMState.helpFont = font;
    gHMState.preferences.appearance.fontID = font;
    HMPrefSetInteger("appearance.fontID", font);

    return noErr;
}

OSErr HMSetFontSize(short fontSize)
{
    if (!gHMState.initialized) {
        return hmHelpManagerNotInited;
    }

    gHMState.helpFontSize = fontSize;
    gHMState.preferences.appearance.fontSize = fontSize;
    HMPrefSetInteger("appearance.fontSize", fontSize);

    return noErr;
}

OSErr HMGetFont(short *font)
{
    if (!gHMState.initialized) {
        return hmHelpManagerNotInited;
    }

    if (!font) {
        return paramErr;
    }

    *font = gHMState.helpFont;
    return noErr;
}

OSErr HMGetFontSize(short *fontSize)
{
    if (!gHMState.initialized) {
        return hmHelpManagerNotInited;
    }

    if (!fontSize) {
        return paramErr;
    }

    *fontSize = gHMState.helpFontSize;
    return noErr;
}

/*
 * Resource ID management
 */
OSErr HMSetDialogResID(short resID)
{
    if (!gHMState.initialized) {
        return hmHelpManagerNotInited;
    }

    gHMState.dialogResID = resID;
    return noErr;
}

OSErr HMGetDialogResID(short *resID)
{
    if (!gHMState.initialized) {
        return hmHelpManagerNotInited;
    }

    if (!resID) {
        return paramErr;
    }

    *resID = gHMState.dialogResID;
    return noErr;
}

OSErr HMSetMenuResID(short menuID, short resID)
{
    /* Implementation would store menu-specific resource IDs */
    return noErr;
}

OSErr HMGetMenuResID(short menuID, short *resID)
{
    if (!resID) {
        return paramErr;
    }

    /* Implementation would retrieve menu-specific resource ID */
    *resID = menuID;  /* Default: use menu ID as resource ID */
    return noErr;
}

/*
 * Balloon utilities
 */
OSErr HMBalloonRect(const HMMessageRecord *aHelpMsg, Rect *coolRect)
{
    if (!gHMState.initialized) {
        return hmHelpManagerNotInited;
    }

    if (!aHelpMsg || !coolRect) {
        return paramErr;
    }

    if (!HMValidateMessageRecord(aHelpMsg)) {
        return hmUnknownHelpType;
    }

    /* Calculate balloon rectangle for the message */
    HMBalloonContent content = {0};
    content.message = (HMMessageRecord *)aHelpMsg;
    content.fontID = gHMState.helpFont;
    content.fontSize = gHMState.helpFontSize;

    Size contentSize;
    OSErr err = HMBalloonMeasureContent(&content, &contentSize);
    if (err != noErr) {
        return err;
    }

    /* Set rectangle with margins */
    SetRect(coolRect, 0, 0,
            contentSize.h + 2 * kHMBalloonMargin,
            contentSize.v + 2 * kHMBalloonMargin);

    return noErr;
}

OSErr HMBalloonPict(const HMMessageRecord *aHelpMsg, PicHandle *coolPict)
{
    if (!gHMState.initialized) {
        return hmHelpManagerNotInited;
    }

    if (!aHelpMsg || !coolPict) {
        return paramErr;
    }

    if (!HMValidateMessageRecord(aHelpMsg)) {
        return hmUnknownHelpType;
    }

    /* Create a picture of the balloon */
    /* This would render the balloon to a picture handle */
    *coolPict = NULL;  /* Not implemented in this example */

    return unimpErr;
}

OSErr HMGetBalloonWindow(WindowPtr *window)
{
    if (!gHMState.initialized) {
        return hmHelpManagerNotInited;
    }

    if (!window) {
        return paramErr;
    }

    if (!gHMState.currentBalloon) {
        return hmNoBalloonUp;
    }

    *window = gHMState.currentBalloon->balloonWindow;
    return noErr;
}

OSErr HMScanTemplateItems(short whichID, short whichResFile, ResType whichType)
{
    if (!gHMState.initialized) {
        return hmHelpManagerNotInited;
    }

    /* Scan template resources for help items */
    return HMResourceScanTemplates(whichID, whichResFile, whichType);
}

/*
 * Modern help system support
 */
OSErr HMConfigureModernHelp(const HMModernHelpConfig *config)
{
    if (!gHMState.initialized) {
        return hmHelpManagerNotInited;
    }

    if (!config) {
        return paramErr;
    }

    gHMState.modernConfig = *config;
    return noErr;
}

OSErr HMShowModernHelp(const char *helpTopic, const char *anchor)
{
    if (!gHMState.initialized) {
        return hmHelpManagerNotInited;
    }

    /* Implementation would show modern help using configured system */
    return unimpErr;
}

OSErr HMSearchHelp(const char *searchTerm, Handle *results)
{
    if (!gHMState.initialized) {
        return hmHelpManagerNotInited;
    }

    /* Implementation would search help content */
    return unimpErr;
}

OSErr HMNavigateHelp(const char *linkTarget)
{
    if (!gHMState.initialized) {
        return hmHelpManagerNotInited;
    }

    /* Implementation would navigate to help link */
    return unimpErr;
}

/*
 * Private Helper Functions
 */

static OSErr HMInitializeGlobals(void)
{
    memset(&gHMState, 0, sizeof(gHMState));

    /* Set default values */
    gHMState.balloonsEnabled = true;
    gHMState.helpFont = systemFont;
    gHMState.helpFontSize = 12;
    gHMState.helpMenuID = kHMHelpMenuID;
    gHMState.defaultResFile = CurResFile();

    /* Initialize modern help config */
    gHMState.modernConfig.systemType = kHMHelpSystemClassic;
    gHMState.modernConfig.useAccessibility = true;
    gHMState.modernConfig.useMultiLanguage = false;
    gHMState.modernConfig.useSearch = false;
    gHMState.modernConfig.useNavigation = false;
    strcpy(gHMState.modernConfig.fallbackLanguage, "en");

    return noErr;
}

static OSErr HMCreateHelpMenu(void)
{
    if (gHMState.helpMenu) {
        return noErr;  /* Already created */
    }

    /* Create the help menu */
    gHMState.helpMenu = NewMenu(gHMState.helpMenuID, "\pHelp");
    if (!gHMState.helpMenu) {
        return memFullErr;
    }

    /* Add menu items */
    AppendMenu(gHMState.helpMenu, "\pAbout Balloon Help...");
    AppendMenu(gHMState.helpMenu, "\p(-");
    if (gHMState.balloonsEnabled) {
        AppendMenu(gHMState.helpMenu, "\pHide Balloons");
    } else {
        AppendMenu(gHMState.helpMenu, "\pShow Balloons");
    }

    return noErr;
}

static OSErr HMUpdateHelpMenu(void)
{
    if (!gHMState.helpMenu) {
        return hmHelpManagerNotInited;
    }

    /* Update Show/Hide Balloons menu item */
    if (gHMState.balloonsEnabled) {
        SetMenuItemText(gHMState.helpMenu, kHMShowBalloonsItem, "\pHide Balloons");
    } else {
        SetMenuItemText(gHMState.helpMenu, kHMShowBalloonsItem, "\pShow Balloons");
    }

    return noErr;
}

static Boolean HMValidateMessageRecord(const HMMessageRecord *message)
{
    if (!message) {
        return false;
    }

    switch (message->hmmHelpType) {
        case khmmString:
        case khmmPict:
        case khmmStringRes:
        case khmmTEHandle:
        case khmmPictHandle:
        case khmmTERes:
        case khmmSTRRes:
            return true;
        default:
            return false;
    }
}

static OSErr HMCalculateBalloonPosition(const HMMessageRecord *message, Point tip,
                                       RectPtr alternateRect, Rect *balloonRect,
                                       Point *tipPoint)
{
    if (!message || !balloonRect || !tipPoint) {
        return paramErr;
    }

    /* Calculate balloon size */
    HMBalloonContent content = {0};
    content.message = (HMMessageRecord *)message;
    content.fontID = gHMState.helpFont;
    content.fontSize = gHMState.helpFontSize;

    Size contentSize;
    OSErr err = HMBalloonMeasureContent(&content, &contentSize);
    if (err != noErr) {
        return err;
    }

    /* Set balloon rectangle */
    SetRect(balloonRect, tip.h, tip.v - contentSize.v - kHMBalloonMargin * 2,
            tip.h + contentSize.h + kHMBalloonMargin * 2, tip.v);

    /* Adjust for screen bounds */
    BitMap screenBits;
    GetQDGlobalsScreenBits(&screenBits);

    if (balloonRect->right > screenBits.bounds.right) {
        OffsetRect(balloonRect, screenBits.bounds.right - balloonRect->right, 0);
    }
    if (balloonRect->left < screenBits.bounds.left) {
        OffsetRect(balloonRect, screenBits.bounds.left - balloonRect->left, 0);
    }
    if (balloonRect->top < screenBits.bounds.top) {
        OffsetRect(balloonRect, 0, screenBits.bounds.top - balloonRect->top);
    }

    *tipPoint = tip;
    return noErr;
}