#ifndef SYSTEM_ALERTS_H
#define SYSTEM_ALERTS_H

#include "NotificationManager/NotificationManager.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Alert Types */
typedef enum {
    alertTypeNote = 0,          /* Note alert */
    alertTypeCaution = 1,       /* Caution alert */
    alertTypeStop = 2,          /* Stop alert */
    alertTypeCustom = 3         /* Custom alert with user-defined icon */
} AlertType;

/* Alert Button Types */
typedef enum {
    alertButtonOK = 1,          /* OK button only */
    alertButtonOKCancel = 2,    /* OK and Cancel buttons */
    alertButtonYesNo = 3,       /* Yes and No buttons */
    alertButtonYesNoCancel = 4, /* Yes, No, and Cancel buttons */
    alertButtonCustom = 5       /* Custom button configuration */
} AlertButtonType;

/* Alert Response Codes */
typedef enum {
    alertResponseOK = 1,        /* OK button pressed */
    alertResponseCancel = 2,    /* Cancel button pressed */
    alertResponseYes = 3,       /* Yes button pressed */
    alertResponseNo = 4,        /* No button pressed */
    alertResponseTimeout = 5,   /* Alert timed out */
    alertResponseError = 6,     /* Error occurred */
    alertResponseDismissed = 7  /* Alert dismissed programmatically */
} AlertResponse;

/* Alert Configuration */
typedef struct {
    AlertType       type;               /* Alert type */
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
typedef struct {
    AlertConfig     config;             /* Alert configuration */
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
typedef struct {
    Boolean         initialized;        /* Alert manager initialized */
    Boolean         enabled;            /* Alerts enabled */
    Boolean         soundEnabled;       /* Alert sounds enabled */
    short           maxConcurrentAlerts;/* Maximum concurrent alerts */
    short           currentAlertCount;  /* Current number of alerts */
    AlertInstancePtr alertChain;        /* Chain of active alerts */
    AlertInstancePtr modalAlert;        /* Currently modal alert */
    UInt32          nextAlertID;        /* Next alert ID */
    Handle          defaultIcon[4];     /* Default icons for each type */
    Handle          defaultSound;       /* Default alert sound */
    Point           defaultPosition;    /* Default alert position */
    Boolean         autoCenter;         /* Auto-center alerts */
    short           alertSpacing;       /* Spacing between multiple alerts */
} AlertManagerState;

/* Alert Callback Functions */
typedef void (*AlertResponseProc)(AlertInstancePtr alertPtr, AlertResponse response, void *context);
typedef Boolean (*AlertFilterProc)(AlertInstancePtr alertPtr, EventRecord *event, void *context);

/* System Alert Management */
OSErr NMSystemAlertsInit(void);
void NMSystemAlertsCleanup(void);

/* Alert Display */
OSErr NMShowSystemAlert(NMExtendedRecPtr nmExtPtr);
OSErr NMShowAlert(AlertConfigPtr config, AlertResponse *response);
OSErr NMShowAlertAsync(AlertConfigPtr config, AlertResponseProc responseProc, void *context);
OSErr NMShowAlertWithFilter(AlertConfigPtr config, AlertFilterProc filterProc, void *context, AlertResponse *response);

/* Alert Control */
OSErr NMDismissAlert(AlertInstancePtr alertPtr);
OSErr NMDismissAllAlerts(void);
OSErr NMBringAlertToFront(AlertInstancePtr alertPtr);
OSErr NMSetAlertTimeout(AlertInstancePtr alertPtr, UInt32 timeout);

/* Alert Configuration */
OSErr NMCreateAlertConfig(AlertConfigPtr *config);
OSErr NMDisposeAlertConfig(AlertConfigPtr config);
OSErr NMSetAlertTitle(AlertConfigPtr config, StringPtr title);
OSErr NMSetAlertMessage(AlertConfigPtr config, StringPtr message);
OSErr NMSetAlertButtons(AlertConfigPtr config, AlertButtonType buttonType);
OSErr NMSetAlertIcon(AlertConfigPtr config, AlertType type, Handle icon);
OSErr NMSetAlertSound(AlertConfigPtr config, Handle sound);
OSErr NMSetAlertPosition(AlertConfigPtr config, Point position);

/* Alert State Management */
OSErr NMGetActiveAlerts(AlertInstancePtr *alerts, short *count);
AlertInstancePtr NMFindAlert(UInt32 alertID);
Boolean NMIsAlertVisible(AlertInstancePtr alertPtr);
Boolean NMIsModalAlertActive(void);
AlertInstancePtr NMGetModalAlert(void);

/* Alert Processing */
void NMProcessAlerts(void);
void NMCheckAlertTimeouts(void);
Boolean NMHandleAlertEvent(EventRecord *event);
OSErr NMUpdateAlertDisplay(AlertInstancePtr alertPtr);

/* Alert Utilities */
OSErr NMCreateSimpleAlert(AlertType type, StringPtr message, AlertResponse *response);
OSErr NMCreateConfirmAlert(StringPtr message, Boolean *confirmed);
OSErr NMCreateErrorAlert(OSErr errorCode, StringPtr message);
OSErr NMCreateProgressAlert(StringPtr message, AlertInstancePtr *alertPtr);
OSErr NMUpdateProgressAlert(AlertInstancePtr alertPtr, short progress, short maximum);

/* Standard Alert Messages */
OSErr NMShowLowMemoryAlert(UInt32 freeMemory);
OSErr NMShowDiskFullAlert(StringPtr volumeName);
OSErr NMShowBatteryAlert(short batteryLevel);
OSErr NMShowNetworkAlert(StringPtr networkName);
OSErr NMShowApplicationAlert(StringPtr appName, StringPtr message);

/* Alert Customization */
OSErr NMSetAlertTheme(short themeID);
OSErr NMGetAlertTheme(short *themeID);
OSErr NMSetDefaultAlertFont(short fontID, short fontSize);
OSErr NMSetDefaultAlertColors(RGBColor *textColor, RGBColor *backgroundColor);

/* Alert Resource Management */
OSErr NMLoadAlertResources(void);
OSErr NMUnloadAlertResources(void);
Handle NMGetDefaultAlertIcon(AlertType type);
Handle NMGetDefaultAlertSound(void);
OSErr NMSetDefaultAlertIcon(AlertType type, Handle icon);
OSErr NMSetDefaultAlertSound(Handle sound);

/* Alert Positioning */
OSErr NMCenterAlert(AlertInstancePtr alertPtr);
OSErr NMPositionAlert(AlertInstancePtr alertPtr, Point position);
OSErr NMCascadeAlerts(Boolean cascade);
OSErr NMSetAlertSpacing(short spacing);

/* Alert Animation */
OSErr NMSetAlertAnimation(Boolean animate);
OSErr NMAnimateAlertShow(AlertInstancePtr alertPtr);
OSErr NMAnimateAlertHide(AlertInstancePtr alertPtr);

/* Accessibility */
OSErr NMSetAlertAccessible(AlertInstancePtr alertPtr, Boolean accessible);
OSErr NMSetAlertDescription(AlertInstancePtr alertPtr, StringPtr description);
OSErr NMSetAlertHelpText(AlertInstancePtr alertPtr, StringPtr helpText);

/* Platform Integration */
OSErr NMPlatformShowAlert(AlertInstancePtr alertPtr);
OSErr NMPlatformHideAlert(AlertInstancePtr alertPtr);
OSErr NMPlatformUpdateAlert(AlertInstancePtr alertPtr);
Boolean NMPlatformHandleAlertEvent(AlertInstancePtr alertPtr, EventRecord *event);

/* Internal Alert Management */
AlertInstancePtr NMCreateAlertInstance(AlertConfigPtr config);
void NMDestroyAlertInstance(AlertInstancePtr alertPtr);
OSErr NMAddToAlertChain(AlertInstancePtr alertPtr);
OSErr NMRemoveFromAlertChain(AlertInstancePtr alertPtr);
void NMUpdateAlertChain(void);

/* Alert Layout */
OSErr NMLayoutAlertButtons(AlertInstancePtr alertPtr);
OSErr NMLayoutAlertText(AlertInstancePtr alertPtr);
OSErr NMCalculateAlertSize(AlertConfigPtr config, Rect *bounds);
OSErr NMPositionAlertElements(AlertInstancePtr alertPtr);

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