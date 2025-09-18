#ifndef APP_REGISTRATION_H
#define APP_REGISTRATION_H

#include "NotificationManager/NotificationManager.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Application Registration API */
OSErr NMAppRegistrationInit(void);
void NMAppRegistrationCleanup(void);
OSErr NMRegisterApplication(OSType signature, StringPtr appName);
OSErr NMUnregisterApplication(OSType signature);
OSErr NMSetAppNotificationPreferences(OSType signature, Boolean notifications, Boolean sounds, Boolean alerts);
OSErr NMGetAppNotificationPreferences(OSType signature, Boolean *notifications, Boolean *sounds, Boolean *alerts);
OSErr NMSetAppDefaultIcon(OSType signature, Handle icon);
Handle NMGetAppDefaultIcon(OSType signature);
OSErr NMSetAppDefaultSound(OSType signature, Handle sound);
Handle NMGetAppDefaultSound(OSType signature);
OSErr NMGetRegisteredApps(OSType *signatures, StringPtr *names, short *count);
Boolean NMIsAppRegistered(OSType signature);
OSErr NMSetAppMaxNotifications(OSType signature, short maxNotifications);
short NMGetAppActiveNotifications(OSType signature);
OSErr NMUpdateAppActivity(OSType signature);
OSErr NMSetAutoRegister(Boolean autoRegister);
Boolean NMGetAutoRegister(void);
OSErr NMSetMaxRegistrations(short maxRegistrations);
short NMGetRegistrationCount(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_REGISTRATION_H */