#ifndef NOTIFICATION_RESOURCE_MANAGER_H
#define NOTIFICATION_RESOURCE_MANAGER_H

#include "NotificationManager/NotificationManager.h"
#include "NotificationManager/SystemAlerts.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Resource Management API */
OSErr NMLoadNotificationResources(void);
OSErr NMUnloadNotificationResources(void);
Handle NMGetDefaultIcon(void);
Handle NMGetDefaultAlertIcon(AlertType type);
Handle NMGetDefaultSound(void);
OSErr NMSetDefaultAlertIcon(AlertType type, Handle icon);
OSErr NMSetDefaultAlertSound(Handle sound);
Handle NMLoadNotificationIcon(short iconID);
Handle NMLoadNotificationSound(short soundID);
StringPtr NMLoadNotificationString(short stringID);
OSErr NMSetResourceCaching(Boolean enabled);
Boolean NMIsResourceCachingEnabled(void);
OSErr NMSetMaxCacheSize(short maxSize);
short NMGetCacheSize(void);
OSErr NMPurgeResourceCache(void);
OSErr NMGetResourceStatistics(UInt32 *hits, UInt32 *misses, UInt32 *total);
OSErr NMValidateIconResource(Handle iconHandle);
OSErr NMValidateSoundResource(Handle soundHandle);
Handle NMDuplicateResource(Handle original);
OSErr NMReleaseResource(Handle resourceHandle);

/* Error Codes */
#define nmErrInvalidResource    -40909      /* Invalid resource */

#ifdef __cplusplus
}
#endif

#endif /* NOTIFICATION_RESOURCE_MANAGER_H */