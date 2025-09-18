#ifndef NOTIFICATION_SCHEDULER_H
#define NOTIFICATION_SCHEDULER_H

#include "NotificationManager/NotificationManager.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Notification Scheduler API */
OSErr NMSchedulerInit(void);
void NMSchedulerCleanup(void);
void NMScheduleNotifications(void);
OSErr NMSetSchedulerEnabled(Boolean enabled);
Boolean NMIsSchedulerEnabled(void);
OSErr NMSetMaxConcurrentNotifications(short maxCount);
short NMGetMaxConcurrentNotifications(void);
OSErr NMSetSchedulingMode(Boolean priorityMode);
Boolean NMIsPriorityScheduling(void);

#ifdef __cplusplus
}
#endif

#endif /* NOTIFICATION_SCHEDULER_H */