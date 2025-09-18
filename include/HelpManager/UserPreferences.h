/*
 * UserPreferences.h - Help Manager User Preferences and Settings
 *
 * This file defines structures and functions for managing user preferences
 * related to help balloons, timing, appearance, and behavior.
 */

#ifndef USERPREFERENCES_H
#define USERPREFERENCES_H

#include "MacTypes.h"
#include "HelpManager.h"
#include "HelpBalloons.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Preference categories */
typedef enum {
    kHMPrefCategoryGeneral = 1,      /* General help preferences */
    kHMPrefCategoryAppearance = 2,   /* Balloon appearance preferences */
    kHMPrefCategoryTiming = 3,       /* Help timing preferences */
    kHMPrefCategoryBehavior = 4,     /* Help behavior preferences */
    kHMPrefCategoryAccessibility = 5, /* Accessibility preferences */
    kHMPrefCategoryAdvanced = 6      /* Advanced preferences */
} HMPrefCategory;

/* Preference data types */
typedef enum {
    kHMPrefTypeBoolean = 1,          /* Boolean value */
    kHMPrefTypeInteger = 2,          /* Integer value */
    kHMPrefTypeFloat = 3,            /* Floating point value */
    kHMPrefTypeString = 4,           /* String value */
    kHMPrefTypeRect = 5,             /* Rectangle value */
    kHMPrefTypePoint = 6,            /* Point value */
    kHMPrefTypeColor = 7,            /* Color value */
    kHMPrefTypeEnum = 8              /* Enumerated value */
} HMPrefType;

/* Preference storage locations */
typedef enum {
    kHMPrefStorageSystem = 1,        /* System preferences */
    kHMPrefStorageUser = 2,          /* User preferences */
    kHMPrefStorageApplication = 3,   /* Application preferences */
    kHMPrefStorageTemporary = 4      /* Temporary session preferences */
} HMPrefStorage;

/* General help preferences */
typedef struct HMGeneralPrefs {
    Boolean balloonsEnabled;         /* Show help balloons */
    Boolean soundEnabled;            /* Play help sounds */
    Boolean animationsEnabled;       /* Enable balloon animations */
    Boolean autoHideEnabled;         /* Auto-hide balloons */
    Boolean rememberWindowPos;       /* Remember help window positions */
    Boolean showWelcomeHelp;         /* Show welcome help on first run */
    Boolean checkForUpdates;         /* Check for help content updates */
    char preferredLanguage[8];       /* Preferred help language */
    char fallbackLanguage[8];        /* Fallback help language */
} HMGeneralPrefs;

/* Appearance preferences */
typedef struct HMAppearancePrefs {
    HMBalloonStyle balloonStyle;     /* Balloon visual style */
    short fontID;                    /* Help text font */
    short fontSize;                  /* Help text size */
    short fontStyle;                 /* Help text style */
    RGBColor textColor;              /* Text color */
    RGBColor backgroundColor;        /* Background color */
    RGBColor borderColor;            /* Border color */
    RGBColor shadowColor;            /* Shadow color */
    Boolean useDropShadow;           /* Use drop shadow */
    Boolean useGradient;             /* Use gradient background */
    Boolean useTransparency;         /* Use transparency effects */
    float opacity;                   /* Balloon opacity */
    short cornerRadius;              /* Corner radius */
    short borderWidth;               /* Border width */
} HMAppearancePrefs;

/* Timing preferences */
typedef struct HMTimingPrefs {
    long hoverDelay;                 /* Delay before showing (ticks) */
    long displayDuration;            /* How long to display (ticks) */
    long fadeInTime;                 /* Fade in duration (ticks) */
    long fadeOutTime;                /* Fade out duration (ticks) */
    long retriggerDelay;             /* Delay before retriggering (ticks) */
    long autoHideDelay;              /* Auto-hide delay (ticks) */
    Boolean respectSystemTiming;     /* Use system timing settings */
    Boolean adaptiveTimming;         /* Adjust timing based on content */
} HMTimingPrefs;

/* Behavior preferences */
typedef struct HMBehaviorPrefs {
    Boolean followMouse;             /* Balloons follow mouse */
    Boolean stayVisible;             /* Keep balloons visible when moving */
    Boolean clickToDismiss;          /* Click to dismiss balloons */
    Boolean escToDismiss;            /* Escape key to dismiss balloons */
    Boolean preventOffscreen;        /* Prevent balloons going offscreen */
    Boolean respectWindowBounds;     /* Keep balloons within window */
    Boolean showForDisabledItems;    /* Show help for disabled items */
    Boolean showInMenus;             /* Show help in menus */
    Boolean showInDialogs;           /* Show help in dialogs */
    Boolean showInWindows;           /* Show help in windows */
    HMBalloonAnimation animation;    /* Balloon animation type */
} HMBehaviorPrefs;

/* Accessibility preferences */
typedef struct HMAccessibilityPrefs {
    Boolean accessibilityEnabled;    /* Accessibility support enabled */
    Boolean highContrast;            /* Use high contrast colors */
    Boolean largeText;               /* Use larger text size */
    Boolean announceToScreenReader;  /* Announce to screen reader */
    Boolean useAlternativeText;      /* Use alt text for images */
    Boolean simplifiedLayout;        /* Use simplified balloon layout */
    Boolean respectSystemSettings;   /* Respect system accessibility */
    short minimumTextSize;           /* Minimum text size */
    float contrastMultiplier;        /* Contrast multiplier */
} HMAccessibilityPrefs;

/* Advanced preferences */
typedef struct HMAdvancedPrefs {
    Boolean enableLogging;           /* Enable help system logging */
    Boolean enableProfiling;         /* Enable performance profiling */
    Boolean cacheResources;          /* Cache help resources */
    Boolean preloadContent;          /* Preload help content */
    short maxCacheSize;              /* Maximum cache size (KB) */
    short maxHistoryEntries;         /* Maximum history entries */
    Boolean allowCustomBalloons;     /* Allow custom balloon types */
    Boolean enableDebugging;         /* Enable debugging features */
} HMAdvancedPrefs;

/* Complete preference set */
typedef struct HMPreferences {
    short version;                   /* Preference structure version */
    long signature;                  /* Preference signature */
    HMGeneralPrefs general;          /* General preferences */
    HMAppearancePrefs appearance;    /* Appearance preferences */
    HMTimingPrefs timing;            /* Timing preferences */
    HMBehaviorPrefs behavior;        /* Behavior preferences */
    HMAccessibilityPrefs accessibility; /* Accessibility preferences */
    HMAdvancedPrefs advanced;        /* Advanced preferences */
    long lastModified;               /* Last modification time */
    char applicationID[64];          /* Application identifier */
} HMPreferences;

/* Preference change notification */
typedef struct HMPrefChangeInfo {
    HMPrefCategory category;         /* Category that changed */
    char prefName[64];               /* Preference name */
    HMPrefType prefType;             /* Preference type */
    void *oldValue;                  /* Previous value */
    void *newValue;                  /* New value */
    Boolean userInitiated;           /* Change was user-initiated */
} HMPrefChangeInfo;

/* Preference change callback */
typedef OSErr (*HMPrefChangeCallback)(const HMPrefChangeInfo *changeInfo, void *userData);

/* Preference validation callback */
typedef Boolean (*HMPrefValidateCallback)(const char *prefName, HMPrefType prefType,
                                         const void *value, void *userData);

/* Preference initialization */
OSErr HMPrefInit(void);
void HMPrefShutdown(void);

/* Preference loading and saving */
OSErr HMPrefLoad(HMPrefStorage storage);
OSErr HMPrefSave(HMPrefStorage storage);
OSErr HMPrefResetToDefaults(void);
OSErr HMPrefResetCategory(HMPrefCategory category);

/* General preference accessors */
OSErr HMPrefGet(const char *prefName, HMPrefType *prefType, void *value, short maxSize);
OSErr HMPrefSet(const char *prefName, HMPrefType prefType, const void *value);
Boolean HMPrefExists(const char *prefName);
OSErr HMPrefRemove(const char *prefName);

/* Typed preference accessors */
Boolean HMPrefGetBoolean(const char *prefName, Boolean defaultValue);
OSErr HMPrefSetBoolean(const char *prefName, Boolean value);

short HMPrefGetInteger(const char *prefName, short defaultValue);
OSErr HMPrefSetInteger(const char *prefName, short value);

float HMPrefGetFloat(const char *prefName, float defaultValue);
OSErr HMPrefSetFloat(const char *prefName, float value);

OSErr HMPrefGetString(const char *prefName, char *value, short maxLength,
                     const char *defaultValue);
OSErr HMPrefSetString(const char *prefName, const char *value);

Point HMPrefGetPoint(const char *prefName, Point defaultValue);
OSErr HMPrefSetPoint(const char *prefName, Point value);

Rect HMPrefGetRect(const char *prefName, Rect defaultValue);
OSErr HMPrefSetRect(const char *prefName, Rect value);

RGBColor HMPrefGetColor(const char *prefName, RGBColor defaultValue);
OSErr HMPrefSetColor(const char *prefName, RGBColor value);

/* Category-specific accessors */
OSErr HMPrefGetGeneral(HMGeneralPrefs *prefs);
OSErr HMPrefSetGeneral(const HMGeneralPrefs *prefs);

OSErr HMPrefGetAppearance(HMAppearancePrefs *prefs);
OSErr HMPrefSetAppearance(const HMAppearancePrefs *prefs);

OSErr HMPrefGetTiming(HMTimingPrefs *prefs);
OSErr HMPrefSetTiming(const HMTimingPrefs *prefs);

OSErr HMPrefGetBehavior(HMBehaviorPrefs *prefs);
OSErr HMPrefSetBehavior(const HMBehaviorPrefs *prefs);

OSErr HMPrefGetAccessibility(HMAccessibilityPrefs *prefs);
OSErr HMPrefSetAccessibility(const HMAccessibilityPrefs *prefs);

OSErr HMPrefGetAdvanced(HMAdvancedPrefs *prefs);
OSErr HMPrefSetAdvanced(const HMAdvancedPrefs *prefs);

/* Preference notification */
OSErr HMPrefRegisterChangeCallback(HMPrefCategory category, HMPrefChangeCallback callback,
                                  void *userData);
OSErr HMPrefUnregisterChangeCallback(HMPrefChangeCallback callback);

OSErr HMPrefRegisterValidateCallback(const char *prefName, HMPrefValidateCallback callback,
                                   void *userData);
OSErr HMPrefUnregisterValidateCallback(const char *prefName);

/* Preference enumeration */
OSErr HMPrefGetCategoryNames(HMPrefCategory category, char ***prefNames, short *count);
OSErr HMPrefGetAllNames(char ***prefNames, short *count);

/* Preference import/export */
OSErr HMPrefExport(const char *filePath, HMPrefCategory category);
OSErr HMPrefImport(const char *filePath, Boolean overwriteExisting);

OSErr HMPrefExportToText(const char *filePath, HMPrefCategory category);
OSErr HMPrefImportFromText(const char *filePath);

/* Preference synchronization */
OSErr HMPrefSyncWithSystem(void);
OSErr HMPrefSyncBetweenUsers(void);
OSErr HMPrefBackup(const char *backupPath);
OSErr HMPrefRestore(const char *backupPath);

/* Preference validation */
Boolean HMPrefValidate(void);
Boolean HMPrefValidateCategory(HMPrefCategory category);
OSErr HMPrefGetValidationErrors(char ***errors, short *errorCount);

/* Default preference management */
OSErr HMPrefGetDefault(const char *prefName, HMPrefType *prefType, void *value, short maxSize);
OSErr HMPrefSetDefault(const char *prefName, HMPrefType prefType, const void *value);
OSErr HMPrefResetToDefault(const char *prefName);

/* Preference migration */
OSErr HMPrefMigrateFromVersion(short fromVersion, short toVersion);
OSErr HMPrefGetVersion(short *version);
OSErr HMPrefSetVersion(short version);

/* System integration */
OSErr HMPrefSyncWithAccessibilitySettings(void);
OSErr HMPrefSyncWithAppearanceSettings(void);
OSErr HMPrefSyncWithLanguageSettings(void);

/* Preference security */
OSErr HMPrefLock(const char *prefName);
OSErr HMPrefUnlock(const char *prefName);
Boolean HMPrefIsLocked(const char *prefName);

/* Debugging and diagnostics */
OSErr HMPrefDump(void);
OSErr HMPrefGetDebugInfo(char *debugInfo, long maxLength);
OSErr HMPrefValidateIntegrity(void);

/* Memory management */
void HMPrefDisposeStringArray(char **strings, short count);

/* Platform-specific preferences */
#ifdef __APPLE__
OSErr HMPrefSyncWithNSUserDefaults(void);
#endif

#ifdef _WIN32
OSErr HMPrefSyncWithRegistry(void);
#endif

#ifdef __linux__
OSErr HMPrefSyncWithGSettings(void);
#endif

#ifdef __cplusplus
}
#endif

#endif /* USERPREFERENCES_H */