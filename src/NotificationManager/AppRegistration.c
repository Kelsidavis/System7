/*
 * AppRegistration.c
 *
 * Application notification registration and management
 * Tracks applications and their notification preferences
 *
 * Converted from original Mac OS System 7.1 source code
 */

#include "NotificationManager/NotificationManager.h"
#include "Memory.h"
#include "Errors.h"
#include "OSUtils.h"
#include "Processes.h"

/* Application registration entry */
typedef struct AppRegistration {
    OSType      signature;          /* Application signature */
    StringPtr   appName;           /* Application name */
    ProcessSerialNumber psn;       /* Process serial number */
    Boolean     notificationsEnabled; /* Notifications enabled for this app */
    Boolean     soundsEnabled;     /* Sounds enabled */
    Boolean     alertsEnabled;     /* Alerts enabled */
    UInt32      registrationTime;  /* When registered */
    UInt32      lastActivity;      /* Last notification activity */
    short       activeNotifications; /* Number of active notifications */
    short       maxNotifications; /* Maximum allowed notifications */
    Handle      defaultIcon;       /* Default notification icon */
    Handle      defaultSound;      /* Default notification sound */
    long        refCon;           /* Application reference */
    struct AppRegistration *next; /* Next in chain */
} AppRegistration, *AppRegistrationPtr;

/* Registration manager state */
typedef struct {
    Boolean             initialized;
    AppRegistrationPtr  registrations;
    short              registrationCount;
    short              maxRegistrations;
    Boolean            autoRegister;
    Boolean            inheritPreferences;
} RegistrationState;

static RegistrationState gRegState = {0};

/* Internal prototypes */
static AppRegistrationPtr NMFindAppRegistration(OSType signature);
static AppRegistrationPtr NMCreateAppRegistration(OSType signature, StringPtr appName);
static void NMDestroyAppRegistration(AppRegistrationPtr appReg);
static OSErr NMGetCurrentAppSignature(OSType *signature);
static OSErr NMAutoRegisterCurrentApp(void);

/*
 * Application Registration Management
 */

OSErr NMAppRegistrationInit(void)
{
    if (gRegState.initialized) {
        return noErr;
    }

    gRegState.registrations = NULL;
    gRegState.registrationCount = 0;
    gRegState.maxRegistrations = 100;
    gRegState.autoRegister = true;
    gRegState.inheritPreferences = true;

    gRegState.initialized = true;
    return noErr;
}

void NMAppRegistrationCleanup(void)
{
    AppRegistrationPtr appReg, nextReg;

    if (!gRegState.initialized) {
        return;
    }

    /* Clean up all registrations */
    appReg = gRegState.registrations;
    while (appReg) {
        nextReg = appReg->next;
        NMDestroyAppRegistration(appReg);
        appReg = nextReg;
    }

    gRegState.registrations = NULL;
    gRegState.registrationCount = 0;
    gRegState.initialized = false;
}

OSErr NMRegisterApplication(OSType signature, StringPtr appName)
{
    AppRegistrationPtr appReg;
    ProcessSerialNumber psn;

    if (!gRegState.initialized) {
        return nmErrNotInstalled;
    }

    /* Check if already registered */
    appReg = NMFindAppRegistration(signature);
    if (appReg) {
        return noErr; /* Already registered */
    }

    /* Check registration limit */
    if (gRegState.registrationCount >= gRegState.maxRegistrations) {
        return nmErrQueueFull;
    }

    /* Create new registration */
    appReg = NMCreateAppRegistration(signature, appName);
    if (!appReg) {
        return nmErrOutOfMemory;
    }

    /* Get process serial number if possible */
    if (GetCurrentProcess(&psn) == noErr) {
        appReg->psn = psn;
    }

    /* Add to registration chain */
    appReg->next = gRegState.registrations;
    gRegState.registrations = appReg;
    gRegState.registrationCount++;

    return noErr;
}

OSErr NMUnregisterApplication(OSType signature)
{
    AppRegistrationPtr appReg, prevReg = NULL;

    if (!gRegState.initialized) {
        return nmErrNotInstalled;
    }

    /* Find registration */
    appReg = gRegState.registrations;
    while (appReg) {
        if (appReg->signature == signature) {
            /* Remove from chain */
            if (prevReg) {
                prevReg->next = appReg->next;
            } else {
                gRegState.registrations = appReg->next;
            }

            /* Flush any pending notifications for this app */
            NMFlushApplication(signature);

            /* Clean up registration */
            NMDestroyAppRegistration(appReg);
            gRegState.registrationCount--;

            return noErr;
        }
        prevReg = appReg;
        appReg = appReg->next;
    }

    return nmErrNotFound;
}

OSErr NMSetAppNotificationPreferences(OSType signature, Boolean notifications, Boolean sounds, Boolean alerts)
{
    AppRegistrationPtr appReg;

    if (!gRegState.initialized) {
        return nmErrNotInstalled;
    }

    /* Auto-register if enabled and not found */
    appReg = NMFindAppRegistration(signature);
    if (!appReg && gRegState.autoRegister) {
        NMRegisterApplication(signature, NULL);
        appReg = NMFindAppRegistration(signature);
    }

    if (!appReg) {
        return nmErrNotFound;
    }

    appReg->notificationsEnabled = notifications;
    appReg->soundsEnabled = sounds;
    appReg->alertsEnabled = alerts;

    return noErr;
}

OSErr NMGetAppNotificationPreferences(OSType signature, Boolean *notifications, Boolean *sounds, Boolean *alerts)
{
    AppRegistrationPtr appReg;

    if (!gRegState.initialized) {
        return nmErrNotInstalled;
    }

    if (!notifications || !sounds || !alerts) {
        return nmErrInvalidParameter;
    }

    appReg = NMFindAppRegistration(signature);
    if (!appReg) {
        /* Return default preferences */
        *notifications = true;
        *sounds = true;
        *alerts = true;
        return nmErrNotFound;
    }

    *notifications = appReg->notificationsEnabled;
    *sounds = appReg->soundsEnabled;
    *alerts = appReg->alertsEnabled;

    return noErr;
}

OSErr NMSetAppDefaultIcon(OSType signature, Handle icon)
{
    AppRegistrationPtr appReg;

    if (!gRegState.initialized) {
        return nmErrNotInstalled;
    }

    appReg = NMFindAppRegistration(signature);
    if (!appReg && gRegState.autoRegister) {
        NMRegisterApplication(signature, NULL);
        appReg = NMFindAppRegistration(signature);
    }

    if (!appReg) {
        return nmErrNotFound;
    }

    /* Dispose old icon if present */
    if (appReg->defaultIcon) {
        DisposeHandle(appReg->defaultIcon);
    }

    /* Copy new icon */
    if (icon) {
        appReg->defaultIcon = NewHandle(GetHandleSize(icon));
        if (appReg->defaultIcon) {
            BlockMoveData(*icon, *appReg->defaultIcon, GetHandleSize(icon));
        }
    } else {
        appReg->defaultIcon = NULL;
    }

    return noErr;
}

Handle NMGetAppDefaultIcon(OSType signature)
{
    AppRegistrationPtr appReg;

    if (!gRegState.initialized) {
        return NULL;
    }

    appReg = NMFindAppRegistration(signature);
    if (!appReg) {
        return NULL;
    }

    return appReg->defaultIcon;
}

OSErr NMSetAppDefaultSound(OSType signature, Handle sound)
{
    AppRegistrationPtr appReg;

    if (!gRegState.initialized) {
        return nmErrNotInstalled;
    }

    appReg = NMFindAppRegistration(signature);
    if (!appReg && gRegState.autoRegister) {
        NMRegisterApplication(signature, NULL);
        appReg = NMFindAppRegistration(signature);
    }

    if (!appReg) {
        return nmErrNotFound;
    }

    /* Dispose old sound if present */
    if (appReg->defaultSound) {
        DisposeHandle(appReg->defaultSound);
    }

    /* Copy new sound */
    if (sound) {
        appReg->defaultSound = NewHandle(GetHandleSize(sound));
        if (appReg->defaultSound) {
            BlockMoveData(*sound, *appReg->defaultSound, GetHandleSize(sound));
        }
    } else {
        appReg->defaultSound = NULL;
    }

    return noErr;
}

Handle NMGetAppDefaultSound(OSType signature)
{
    AppRegistrationPtr appReg;

    if (!gRegState.initialized) {
        return NULL;
    }

    appReg = NMFindAppRegistration(signature);
    if (!appReg) {
        return NULL;
    }

    return appReg->defaultSound;
}

OSErr NMGetRegisteredApps(OSType *signatures, StringPtr *names, short *count)
{
    AppRegistrationPtr appReg;
    short i = 0;

    if (!gRegState.initialized) {
        return nmErrNotInstalled;
    }

    if (!signatures || !count) {
        return nmErrInvalidParameter;
    }

    *count = 0;

    appReg = gRegState.registrations;
    while (appReg && i < *count) {
        signatures[i] = appReg->signature;
        if (names && appReg->appName) {
            names[i] = appReg->appName;
        }
        i++;
        appReg = appReg->next;
    }

    *count = i;
    return noErr;
}

Boolean NMIsAppRegistered(OSType signature)
{
    if (!gRegState.initialized) {
        return false;
    }

    return (NMFindAppRegistration(signature) != NULL);
}

OSErr NMSetAppMaxNotifications(OSType signature, short maxNotifications)
{
    AppRegistrationPtr appReg;

    if (!gRegState.initialized) {
        return nmErrNotInstalled;
    }

    if (maxNotifications < 0 || maxNotifications > 50) {
        return nmErrInvalidParameter;
    }

    appReg = NMFindAppRegistration(signature);
    if (!appReg) {
        return nmErrNotFound;
    }

    appReg->maxNotifications = maxNotifications;
    return noErr;
}

short NMGetAppActiveNotifications(OSType signature)
{
    AppRegistrationPtr appReg;

    if (!gRegState.initialized) {
        return 0;
    }

    appReg = NMFindAppRegistration(signature);
    if (!appReg) {
        return 0;
    }

    return appReg->activeNotifications;
}

OSErr NMUpdateAppActivity(OSType signature)
{
    AppRegistrationPtr appReg;

    if (!gRegState.initialized) {
        return nmErrNotInstalled;
    }

    appReg = NMFindAppRegistration(signature);
    if (!appReg) {
        /* Auto-register if enabled */
        if (gRegState.autoRegister) {
            OSErr err = NMAutoRegisterCurrentApp();
            if (err == noErr) {
                appReg = NMFindAppRegistration(signature);
            }
        }
    }

    if (appReg) {
        appReg->lastActivity = NMGetTimestamp();
        return noErr;
    }

    return nmErrNotFound;
}

/*
 * Internal Implementation
 */

static AppRegistrationPtr NMFindAppRegistration(OSType signature)
{
    AppRegistrationPtr appReg;

    appReg = gRegState.registrations;
    while (appReg) {
        if (appReg->signature == signature) {
            return appReg;
        }
        appReg = appReg->next;
    }

    return NULL;
}

static AppRegistrationPtr NMCreateAppRegistration(OSType signature, StringPtr appName)
{
    AppRegistrationPtr appReg;

    appReg = (AppRegistrationPtr)NewPtrClear(sizeof(AppRegistration));
    if (!appReg) {
        return NULL;
    }

    appReg->signature = signature;
    appReg->notificationsEnabled = true;
    appReg->soundsEnabled = true;
    appReg->alertsEnabled = true;
    appReg->registrationTime = NMGetTimestamp();
    appReg->lastActivity = appReg->registrationTime;
    appReg->activeNotifications = 0;
    appReg->maxNotifications = 10;
    appReg->defaultIcon = NULL;
    appReg->defaultSound = NULL;
    appReg->refCon = 0;
    appReg->next = NULL;

    /* Copy application name if provided */
    if (appName && appName[0] > 0) {
        appReg->appName = (StringPtr)NewPtr(appName[0] + 1);
        if (appReg->appName) {
            BlockMoveData(appName, appReg->appName, appName[0] + 1);
        }
    } else {
        appReg->appName = NULL;
    }

    return appReg;
}

static void NMDestroyAppRegistration(AppRegistrationPtr appReg)
{
    if (!appReg) {
        return;
    }

    /* Clean up resources */
    if (appReg->appName) {
        DisposePtr((Ptr)appReg->appName);
    }

    if (appReg->defaultIcon) {
        DisposeHandle(appReg->defaultIcon);
    }

    if (appReg->defaultSound) {
        DisposeHandle(appReg->defaultSound);
    }

    DisposePtr((Ptr)appReg);
}

static OSErr NMGetCurrentAppSignature(OSType *signature)
{
    ProcessSerialNumber psn;
    ProcessInfoRec info;
    OSErr err;

    if (!signature) {
        return nmErrInvalidParameter;
    }

    err = GetCurrentProcess(&psn);
    if (err != noErr) {
        return err;
    }

    info.processInfoLength = sizeof(ProcessInfoRec);
    info.processName = NULL;
    info.processAppSpec = NULL;

    err = GetProcessInformation(&psn, &info);
    if (err != noErr) {
        return err;
    }

    *signature = info.processSignature;
    return noErr;
}

static OSErr NMAutoRegisterCurrentApp(void)
{
    OSType signature;
    OSErr err;

    err = NMGetCurrentAppSignature(&signature);
    if (err != noErr) {
        return err;
    }

    return NMRegisterApplication(signature, NULL);
}

/*
 * Configuration Functions
 */

OSErr NMSetAutoRegister(Boolean autoRegister)
{
    if (!gRegState.initialized) {
        return nmErrNotInstalled;
    }

    gRegState.autoRegister = autoRegister;
    return noErr;
}

Boolean NMGetAutoRegister(void)
{
    return gRegState.initialized && gRegState.autoRegister;
}

OSErr NMSetMaxRegistrations(short maxRegistrations)
{
    if (!gRegState.initialized) {
        return nmErrNotInstalled;
    }

    if (maxRegistrations < 1 || maxRegistrations > 500) {
        return nmErrInvalidParameter;
    }

    gRegState.maxRegistrations = maxRegistrations;
    return noErr;
}

short NMGetRegistrationCount(void)
{
    return gRegState.initialized ? gRegState.registrationCount : 0;
}