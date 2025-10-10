/*
 * StartupScreen.c - System 7 Startup Screen Implementation
 *
 * Provides the classic "Welcome to Macintosh" startup screen with
 * extension loading display and progress indication.
 *
 * Copyright (c) 2024 System7.1-Portable Project
 * MIT License
 */

#include <string.h>
#include <stdio.h>
#include "SystemTypes.h"
#include "System71StdLib.h"
#include "SystemInternal.h"

#include "StartupScreen/StartupScreen.h"
#include "WindowManager/WindowManager.h"
#include "QuickDraw/QuickDraw.h"
#include "FontManager/FontManager.h"
#include "FontManager/FontInternal.h"
#include "FontManager/FontTypes.h"
#include "Resources/happy_mac_icon.h"
#include "MemoryMgr/memory_manager_types.h"
#include "MemoryMgr/MemoryManager.h"
#include "EventManager/EventManager.h"

/* Forward declarations for functions not yet in headers */
extern void SysBeep(short duration);
extern void PlatformDrawRGBABitmap(const UInt8* rgba_data, int width, int height, int dest_x, int dest_y);


/* Classic "Welcome to Macintosh" text */
#define WELCOME_TEXT "Welcome to Macintosh"
#define SYSTEM_VERSION "System 7.1"

/* Screen dimensions and layout */
#define STARTUP_MARGIN 20
#define LOGO_SIZE 128
#define EXTENSION_ICON_SIZE 32
#define EXTENSION_SPACING 40
#define PROGRESS_BAR_HEIGHT 12
#define PROGRESS_BAR_WIDTH 300

/* Timing constants */
#define DEFAULT_WELCOME_DURATION 120  /* 2 seconds at 60Hz */
#define EXTENSION_DISPLAY_TICKS 2     /* 0.1 second per extension */

/* Global startup screen state */
static StartupScreenState gStartupScreen = {0};
static StartupScreenConfig gConfig = {
    .showWelcome = true,
    .showExtensions = true,
    .showProgress = true,
    .welcomeDuration = DEFAULT_WELCOME_DURATION,
    .enableSound = true,
    .backgroundColor = {0xEEEE, 0xEEEE, 0xEEEE},  /* Light gray */
    .textColor = {0x0000, 0x0000, 0x0000}         /* Black */
};

/* Progress callback */
static StartupProgressProc gProgressCallback = NULL;
static void* gProgressUserData = NULL;

/* Extension display state */
static struct {
    UInt16 totalExtensions;
    UInt16 currentExtension;
    Point currentIconPos;
    UInt16 iconsPerRow;
    UInt16 rowCount;
} gExtensionState = {0};

/* Forward declarations */
static OSErr CreateStartupWindow(void);
static void DrawWelcomeScreen(void);
static void DrawHappyMac(const Rect* bounds);
static void DrawExtensionIcon(const ExtensionInfo* extension, Point position);
static Point GetNextExtensionPosition(void);

/*
 * Initialize startup screen system
 */
OSErr InitStartupScreen(const StartupScreenConfig* config) {
    OSErr err = noErr;

    if (gStartupScreen.initialized) {
        return noErr;
    }

    /* Copy configuration */
    if (config) {
        gConfig = *config;
    }

    /* Get screen bounds */
    GDHandle mainDevice = GetMainDevice();
    if (mainDevice) {
        gStartupScreen.screenBounds = (**mainDevice).gdRect;
    } else {
        SetRect(&gStartupScreen.screenBounds, 0, 0, 640, 480);
    }

    /* Calculate layout rectangles */
    short screenWidth = gStartupScreen.screenBounds.right - gStartupScreen.screenBounds.left;
    short screenHeight = gStartupScreen.screenBounds.bottom - gStartupScreen.screenBounds.top;

    /* Logo rect (left side, centered vertically in upper portion) */
    SetRect(&gStartupScreen.logoRect,
            60,  /* Left side with margin */
            screenHeight / 4 - LOGO_SIZE / 2,
            60 + LOGO_SIZE,
            screenHeight / 4 + LOGO_SIZE / 2);

    /* Text rect (right side, aligned with logo vertically) */
    SetRect(&gStartupScreen.textRect,
            LOGO_SIZE + 100,
            screenHeight / 4 - 20,  /* Vertically aligned with logo */
            screenWidth - STARTUP_MARGIN - 40,
            screenHeight / 4 + 20);

    /* Extension rect (middle third) */
    SetRect(&gStartupScreen.extensionRect,
            STARTUP_MARGIN,
            screenHeight / 2 - 50,
            screenWidth - STARTUP_MARGIN,
            screenHeight * 3 / 4);

    /* Progress rect (bottom) */
    SetRect(&gStartupScreen.progressRect,
            (screenWidth - PROGRESS_BAR_WIDTH) / 2,
            screenHeight - 80,
            (screenWidth + PROGRESS_BAR_WIDTH) / 2,
            screenHeight - 60);

    /* Create startup window */
    err = CreateStartupWindow();
    if (err != noErr) {
        return err;
    }

    /* Create offscreen bitmap for smooth drawing */
    Size bitmapSize = (screenWidth * screenHeight) / 8;
    gStartupScreen.offscreenBitmap.baseAddr = NewPtr(bitmapSize);
    if (!gStartupScreen.offscreenBitmap.baseAddr) {
        return memFullErr;
    }

    gStartupScreen.offscreenBitmap.rowBytes = (screenWidth + 7) / 8;
    gStartupScreen.offscreenBitmap.bounds = gStartupScreen.screenBounds;

    gStartupScreen.initialized = true;
    gStartupScreen.currentPhase = kStartupPhaseInit;

    return noErr;
}

/*
 * Create startup window
 */
static OSErr CreateStartupWindow(void) {
    Rect windowBounds = gStartupScreen.screenBounds;

    /* Create plain window (no title bar) */
    static unsigned char emptyTitle[] = {0};  /* Empty Pascal string */
    gStartupScreen.window = NewWindow(NULL, &windowBounds,
                                      emptyTitle, true, plainDBox,
                                      (WindowPtr)-1, false, 0);

    if (!gStartupScreen.window) {
        return memFullErr;
    }

    /* Set as current port */
    SetPort((GrafPtr)gStartupScreen.window);

    /* Set background color */
    RGBBackColor(&gConfig.backgroundColor);
    EraseRect(&windowBounds);

    return noErr;
}

/*
 * Show welcome screen
 */
OSErr ShowWelcomeScreen(void) {
    if (!gStartupScreen.initialized) {
        OSErr err = InitStartupScreen(NULL);
        if (err != noErr) {
            return err;
        }
    }

    /* Set phase */
    gStartupScreen.currentPhase = kStartupPhaseWelcome;
    gStartupScreen.visible = true;

    /* Make window visible */
    ShowWindow(gStartupScreen.window);
    SelectWindow(gStartupScreen.window);
    SetPort((GrafPtr)gStartupScreen.window);

    /* Draw welcome screen */
    DrawWelcomeScreen();

    /* Play startup sound if enabled */
    if (gConfig.enableSound) {
        PlayStartupSound();
    }

    /* Show for configured duration */
    if (gConfig.welcomeDuration > 0) {
        /* Use simple delay instead of event loop during boot */
        UInt32 finalTicks;
        Delay(gConfig.welcomeDuration, &finalTicks);
    }

    return noErr;
}

/*
 * Draw welcome screen with Happy Mac
 */
static void DrawWelcomeScreen(void) {
    GrafPtr savePort;
    GetPort(&savePort);
    SetPort((GrafPtr)gStartupScreen.window);

    /* Clear screen */
    RGBBackColor(&gConfig.backgroundColor);
    EraseRect(&gStartupScreen.screenBounds);

    /* Draw Happy Mac icon */
    DrawHappyMac(&gStartupScreen.logoRect);

    /* Draw welcome text */
    RGBForeColor(&gConfig.textColor);
    TextFont(geneva);
    TextSize(24);
    TextFace(0);

    /* Draw text on left side, vertically centered */
    static unsigned char welcomeText[] = {21, 'W','e','l','c','o','m','e',' ','t','o',' ','M','a','c','i','n','t','o','s','h'};
    MoveTo(gStartupScreen.textRect.left,
           (gStartupScreen.textRect.top + gStartupScreen.textRect.bottom) / 2 + 8);
    DrawString(welcomeText);

    SetPort(savePort);
}

/*
 * Draw Happy Mac icon using the real Mac Picasso logo bitmap
 */
static void DrawHappyMac(const Rect* bounds) {
    /* Icon is drawn at the bounds position (already positioned on right side) */
    short iconWidth = HAPPY_MAC_WIDTH;
    short iconHeight = HAPPY_MAC_HEIGHT;

    /* Draw the Mac Picasso logo bitmap at the specified position */
    PlatformDrawRGBABitmap(gHappyMacIconData, iconWidth, iconHeight, bounds->left, bounds->top);
}

/*
 * Show loading extension
 */
OSErr ShowLoadingExtension(const ExtensionInfo* extension) {
    if (!gStartupScreen.initialized || !gStartupScreen.visible) {
        return noErr;
    }

    if (!extension) {
        return paramErr;
    }

    /* Update phase if needed */
    if (gStartupScreen.currentPhase != kStartupPhaseExtensions) {
        gStartupScreen.currentPhase = kStartupPhaseExtensions;

        /* Clear extension area */
        SetPort((GrafPtr)gStartupScreen.window);
        EraseRect(&gStartupScreen.extensionRect);
    }

    /* Get position for extension icon */
    Point iconPos = GetNextExtensionPosition();

    /* Draw extension icon and name */
    DrawExtensionIcon(extension, iconPos);

    /* Update progress */
    gExtensionState.currentExtension++;
    if (gExtensionState.totalExtensions > 0) {
        short percent = (gExtensionState.currentExtension * 100) /
                       gExtensionState.totalExtensions;
        DrawProgressBar(percent);
    }

    /* Brief pause to show extension */
    Delay(EXTENSION_DISPLAY_TICKS, NULL);

    return noErr;
}

/*
 * Draw extension icon at position
 */
static void DrawExtensionIcon(const ExtensionInfo* extension, Point position) {
    GrafPtr savePort;
    GetPort(&savePort);
    SetPort((GrafPtr)gStartupScreen.window);

    /* Draw icon if available */
    if (extension->iconID != 0) {
        /* DrawResourceIcon(extension->iconID, position.h, position.v); */ /* Commented out - ResourceData not available */
        /* Draw generic icon instead */
        /* Draw generic extension icon */
        Rect iconRect;
        SetRect(&iconRect,
                position.h, position.v,
                position.h + EXTENSION_ICON_SIZE,
                position.v + EXTENSION_ICON_SIZE);
        FrameRect(&iconRect);

        /* Draw 'E' for extension */
        MoveTo(position.h + 12, position.v + 22);
        DrawChar('E');
    }

    /* Draw extension name below icon */
    TextFont(geneva);
    TextSize(9);
    MoveTo(position.h, position.v + EXTENSION_ICON_SIZE + 12);

    /* Truncate name if needed */
    Str255 displayName;
    memcpy(displayName, extension->name, extension->name[0] + 1);
    if (displayName[0] > 10) {
        displayName[0] = 10;
    }
    DrawString(displayName);

    /* Show error indicator if failed */
    if (extension->error != noErr) {
        RGBColor red = {0xFFFF, 0x0000, 0x0000};
        RGBForeColor(&red);
        MoveTo(position.h + EXTENSION_ICON_SIZE - 8, position.v + 8);
        DrawChar('X');
        RGBForeColor(&gConfig.textColor);
    }

    SetPort(savePort);
}

/*
 * Get next position for extension icon
 */
static Point GetNextExtensionPosition(void) {
    Point pos;

    if (gExtensionState.iconsPerRow == 0) {
        /* Calculate icons per row */
        short areaWidth = gStartupScreen.extensionRect.right -
                         gStartupScreen.extensionRect.left;
        gExtensionState.iconsPerRow = areaWidth / EXTENSION_SPACING;
    }

    /* Calculate position */
    short row = gExtensionState.currentExtension / gExtensionState.iconsPerRow;
    short col = gExtensionState.currentExtension % gExtensionState.iconsPerRow;

    pos.h = gStartupScreen.extensionRect.left + (col * EXTENSION_SPACING);
    pos.v = gStartupScreen.extensionRect.top + (row * (EXTENSION_ICON_SIZE + 20));

    return pos;
}

/*
 * Draw progress bar
 */
void DrawProgressBar(short percent) {
    if (!gStartupScreen.initialized || !gStartupScreen.visible) {
        return;
    }

    GrafPtr savePort;
    GetPort(&savePort);
    SetPort((GrafPtr)gStartupScreen.window);

    /* Draw progress bar frame */
    FrameRect(&gStartupScreen.progressRect);

    /* Draw progress fill */
    if (percent > 0) {
        Rect fillRect = gStartupScreen.progressRect;
        InsetRect(&fillRect, 1, 1);
        fillRect.right = fillRect.left +
                        ((fillRect.right - fillRect.left) * percent) / 100;

        Pattern gray;
        GetIndPattern(&gray, 0, 4);  /* 50% gray */
        FillRect(&fillRect, &gray);
    }

    SetPort(savePort);

    /* Notify callback if registered */
    if (gProgressCallback) {
        gProgressCallback(gStartupScreen.currentPhase, percent, gProgressUserData);
    }
}

/*
 * Begin extension loading
 */
OSErr BeginExtensionLoading(UInt16 extensionCount) {
    gExtensionState.totalExtensions = extensionCount;
    gExtensionState.currentExtension = 0;
    gExtensionState.currentIconPos.h = gStartupScreen.extensionRect.left;
    gExtensionState.currentIconPos.v = gStartupScreen.extensionRect.top;

    /* Update phase */
    SetStartupPhase(kStartupPhaseExtensions);

    /* Clear extension area */
    SetPort((GrafPtr)gStartupScreen.window);
    EraseRect(&gStartupScreen.extensionRect);

    return noErr;
}

/*
 * Set startup phase
 */
OSErr SetStartupPhase(StartupPhase phase) {
    gStartupScreen.currentPhase = phase;

    /* Update display based on phase */
    switch (phase) {
        case kStartupPhaseWelcome:
            DrawWelcomeScreen();
            break;

        case kStartupPhaseExtensions:
            /* Draw "Loading Extensions..." text */
            {
                static unsigned char extText[] = {21, 'L','o','a','d','i','n','g',' ','E','x','t','e','n','s','i','o','n','s','.','.','.'};
                SetPort((GrafPtr)gStartupScreen.window);
                TextFont(geneva);
                TextSize(12);
                MoveTo(gStartupScreen.extensionRect.left,
                       gStartupScreen.extensionRect.top - 10);
                DrawString(extText);
            }
            break;

        case kStartupPhaseDrivers:
            /* Draw "Loading Drivers..." text */
            {
                static unsigned char drvText[] = {18, 'L','o','a','d','i','n','g',' ','D','r','i','v','e','r','s','.','.','.'};
                SetPort((GrafPtr)gStartupScreen.window);
                EraseRect(&gStartupScreen.extensionRect);
                MoveTo(gStartupScreen.extensionRect.left,
                       gStartupScreen.extensionRect.top + 20);
                DrawString(drvText);
            }
            break;

        case kStartupPhaseFinder:
            /* Draw "Starting Finder..." text */
            {
                static unsigned char finderText[] = {18, 'S','t','a','r','t','i','n','g',' ','F','i','n','d','e','r','.','.','.'};
                SetPort((GrafPtr)gStartupScreen.window);
                EraseRect(&gStartupScreen.extensionRect);
                MoveTo(gStartupScreen.extensionRect.left,
                       gStartupScreen.extensionRect.top + 20);
                DrawString(finderText);
            }
            break;

        case kStartupPhaseComplete:
            /* Hide screen */
            HideStartupScreen();
            break;

        default:
            break;
    }

    return noErr;
}

/*
 * Play startup sound
 */
OSErr PlayStartupSound(void) {
    /* Play system beep as startup sound */
    SysBeep(1);
    return noErr;
}

/*
 * Hide startup screen
 */
void HideStartupScreen(void) {
    if (gStartupScreen.window) {
        HideWindow(gStartupScreen.window);
    }
    gStartupScreen.visible = false;
}

/*
 * Cleanup startup screen
 */
void CleanupStartupScreen(void) {
    if (!gStartupScreen.initialized) {
        return;
    }

    /* Hide screen */
    HideStartupScreen();

    /* Dispose window */
    if (gStartupScreen.window) {
        DisposeWindow(gStartupScreen.window);
        gStartupScreen.window = NULL;
    }

    /* Free offscreen bitmap */
    if (gStartupScreen.offscreenBitmap.baseAddr) {
        DisposePtr(gStartupScreen.offscreenBitmap.baseAddr);
        gStartupScreen.offscreenBitmap.baseAddr = NULL;
    }

    gStartupScreen.initialized = false;
}

/*
 * Show startup item with icon
 */
OSErr ShowStartupItem(ConstStr255Param itemName, UInt16 iconID) {
    ExtensionInfo extension;
    memcpy(extension.name, itemName, itemName[0] + 1);
    extension.iconID = iconID;
    extension.loaded = true;
    extension.error = noErr;

    return ShowLoadingExtension(&extension);
}

/*
 * Update startup progress
 */
OSErr UpdateStartupProgress(const StartupProgress* progress) {
    if (!progress) {
        return paramErr;
    }

    gStartupScreen.progress = *progress;

    /* Update progress bar */
    DrawProgressBar(progress->percentComplete);

    /* Update phase if changed */
    if (progress->phase != gStartupScreen.currentPhase) {
        SetStartupPhase(progress->phase);
    }

    return noErr;
}

/*
 * Show startup error
 */
OSErr ShowStartupError(ConstStr255Param errorMessage, OSErr errorCode) {
    if (!gStartupScreen.initialized) {
        return noErr;
    }

    GrafPtr savePort;
    GetPort(&savePort);
    SetPort((GrafPtr)gStartupScreen.window);

    /* Draw error in red */
    RGBColor red = {0xFFFF, 0x0000, 0x0000};
    RGBForeColor(&red);

    /* Draw error message */
    MoveTo(gStartupScreen.extensionRect.left,
           gStartupScreen.extensionRect.bottom + 20);
    DrawString(errorMessage);

    /* Draw error code */
    Str255 errorStr;
    sprintf((char*)&errorStr[1], "Error: %d", errorCode);
    errorStr[0] = strlen((char*)&errorStr[1]);
    MoveTo(gStartupScreen.extensionRect.left,
           gStartupScreen.extensionRect.bottom + 40);
    DrawString(errorStr);

    RGBForeColor(&gConfig.textColor);
    SetPort(savePort);

    /* Wait for user acknowledgment */
    EventRecord event;
    while (!WaitNextEvent(mDownMask | keyDownMask, &event, 0, NULL)) {
        /* Wait for click or key */
    }

    return noErr;
}

/*
 * Register progress callback
 */
OSErr RegisterStartupProgressCallback(StartupProgressProc proc, void* userData) {
    gProgressCallback = proc;
    gProgressUserData = userData;
    return noErr;
}

/*
 * Set startup background pattern
 */
OSErr SetStartupBackgroundPattern(const Pattern* pattern) {
    if (!pattern) {
        return paramErr;
    }

    SetPort((GrafPtr)gStartupScreen.window);
    BackPat(pattern);
    EraseRect(&gStartupScreen.screenBounds);

    return noErr;
}

/*
 * Set startup colors
 */
OSErr SetStartupColors(const RGBColor* background, const RGBColor* text) {
    if (background) {
        gConfig.backgroundColor = *background;
    }
    if (text) {
        gConfig.textColor = *text;
    }

    /* Redraw if visible */
    if (gStartupScreen.visible) {
        DrawWelcomeScreen();
    }

    return noErr;
}

/*
 * Enable debug mode
 */
void EnableStartupDebugMode(Boolean enable) {
    /* In debug mode, show additional info */
    if (enable && gStartupScreen.visible) {
        static unsigned char debugText[] = {31, 'D','E','B','U','G',':',' ','S','y','s','t','e','m',' ','7','.','1',' ','S','t','a','r','t','u','p',' ','S','c','r','e','e','n'};
        SetPort((GrafPtr)gStartupScreen.window);
        TextFont(monaco);
        TextSize(9);
        MoveTo(10, gStartupScreen.screenBounds.bottom - 20);
        DrawString(debugText);
    }
}

/*
 * Dump startup screen state
 */
void DumpStartupScreenState(void) {
    serial_printf("StartupScreen State:\n");
    serial_printf("  Initialized: %s\n", gStartupScreen.initialized ? "Yes" : "No");
    serial_printf("  Visible: %s\n", gStartupScreen.visible ? "Yes" : "No");
    serial_printf("  Phase: %d\n", gStartupScreen.currentPhase);
    serial_printf("  Progress: %d%%\n", gStartupScreen.progress.percentComplete);
    serial_printf("  Extensions: %d/%d\n",
           gExtensionState.currentExtension,
           gExtensionState.totalExtensions);
}
