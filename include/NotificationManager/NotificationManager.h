/*
 * NotificationManager.h - Mac OS Notification Manager
 *
 * The Notification Manager allows applications to notify the user
 * of events that occur while the application is in the background.
 * Based on Inside Macintosh: Processes.
 */

#ifndef __NOTIFICATIONMANAGER_H__
#define __NOTIFICATIONMANAGER_H__

#include "SystemTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Notification Manager function prototypes */

/*
 * NMInstall - Install a notification request
 *
 * Parameters:
 *   nmReqPtr - Pointer to notification record
 *
 * Returns:
 *   OSErr - noErr on success, error code on failure
 */
OSErr NMInstall(NMRecPtr nmReqPtr);

/*
 * NMRemove - Remove a notification request
 *
 * Parameters:
 *   nmReqPtr - Pointer to notification record to remove
 *
 * Returns:
 *   OSErr - noErr on success, error code on failure
 */
OSErr NMRemove(NMRecPtr nmReqPtr);

/* Notification flags */
#define nmType          8       /* Notification queue type */

/* Notification Manager error codes */
#ifndef nmTypeErr
#define nmTypeErr       -299    /* Notification Manager type error */
#endif

/* Initialize Notification Manager */
void InitNotificationManager(void);

#ifdef __cplusplus
}
#endif

#endif /* __NOTIFICATIONMANAGER_H__ */
