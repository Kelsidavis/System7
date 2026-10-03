#ifndef SYSTEM_ALERTS_H
#define SYSTEM_ALERTS_H

#include "SystemTypes.h"

/* Forward declarations */


#include "NotificationManager/NotificationManager.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Alert Types */

/* Alert Button Types */

/* Alert Response Codes */

/* Alert Configuration */
               /* Alert type */
    AlertButtonType buttonType;         /* Button configuration */
    StringPtr       title;              /* Alert title */
    StringPtr       message;            /* Alert message */
    StringPtr       detailText;         /* Additional detail text */
    Handle          icon;               /* Custom icon (for alertTypeCustom) */
    Handle          sound;              /* Alert sound */
    Boolean         modal;              /* Modal alert */
    Boolean         movable;            /* User can move alert */
    Boolean         hasTimeout;         /* Alert has timeout */
    UInt32          timeout;            /* Timeout in ticks */
    Point           position;           /* Alert position (0,0 = center) */
    short           defaultButton;      /* Default button (1-based) */
    short           cancelButton;       /* Cancel button (1-based) */
    StringPtr       customButtons[4];   /* Custom button titles */
    short           customButtonCount;  /* Number of custom buttons */
    long            refCon;             /* Reference constant */
} AlertConfig, *AlertConfigPtr;

/* Alert Instance */
             /* Alert configuration */
    DialogPtr       dialog;             /* Dialog pointer */
    Boolean         isVisible;          /* Alert is visible */
    Boolean         isModal;            /* Alert is modal */
    UInt32          showTime;           /* When alert was shown */
    UInt32          timeoutTime;        /* When alert times out */
    AlertResponse   response;           /* User response */
    Boolean         responded;          /* User has responded */
    NMExtendedRecPtr notification;      /* Associated notification */
    void           *platformData;       /* Platform-specific data */
    struct AlertInstance *next;         /* Next alert in chain */
} AlertInstance, *AlertInstancePtr;

/* Alert Manager State */

/* Alert Callback Functions */


/* Constants */
#define ALERT_MAX_CONCURRENT        10      /* Maximum concurrent alerts */
#define ALERT_DEFAULT_TIMEOUT       300     /* Default timeout (5 seconds) */
#define ALERT_MIN_WIDTH             200     /* Minimum alert width */
#define ALERT_MIN_HEIGHT            100     /* Minimum alert height */

#define ALERT_BUTTON_HEIGHT         20      /* Standard button height */

#define ALERT_BUTTON_WIDTH          60      /* Standard button width */
#define ALERT_MARGIN                12      /* Alert margin */
#define ALERT_SPACING               8       /* Element spacing */
#define ALERT_CASCADE_OFFSET        20      /* Cascade offset */

/* Error Codes */
#define alertErrNotInitialized      -42000  /* Alert manager not initialized */
#define alertErrInvalidConfig       -42001  /* Invalid alert configuration */
#define alertErrTooManyAlerts       -42002  /* Too many concurrent alerts */
#define alertErrAlertNotFound       -42003  /* Alert not found */
#define alertErrModalActive         -42004  /* Modal alert already active */
#define alertErrPlatformFailure     -42005  /* Platform alert failure */
#define alertErrInvalidResponse     -42006  /* Invalid alert response */
#define alertErrTimeout             -42007  /* Alert timed out */

#ifdef __cplusplus
}
#endif

#endif /* SYSTEM_ALERTS_H */
