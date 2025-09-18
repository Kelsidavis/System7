#ifndef CHOOSER_H
#define CHOOSER_H

/*
 * Chooser.h - Chooser Desk Accessory
 *
 * Provides device selection interface for printers, network devices, and other
 * shared resources. Allows users to browse available devices, configure
 * connections, and manage device preferences.
 *
 * Based on Apple's Chooser DA from System 7.1
 */

#include "DeskAccessory.h"
#include <stdint.h>
#include <stdbool.h>

/* Chooser Constants */
#define CHOOSER_VERSION         0x0100      /* Chooser version 1.0 */
#define MAX_DEVICES             256         /* Maximum devices */
#define MAX_ZONES               64          /* Maximum AppleTalk zones */
#define DEVICE_NAME_LENGTH      64          /* Maximum device name length */
#define ZONE_NAME_LENGTH        32          /* Maximum zone name length */
#define DRIVER_NAME_LENGTH      32          /* Maximum driver name length */

/* Device Types */
typedef enum {
    DEVICE_TYPE_UNKNOWN     = 0,    /* Unknown device */
    DEVICE_TYPE_PRINTER     = 1,    /* Printer */
    DEVICE_TYPE_FILE_SERVER = 2,    /* File server */
    DEVICE_TYPE_SHARED_DISK = 3,    /* Shared disk */
    DEVICE_TYPE_SCANNER     = 4,    /* Scanner */
    DEVICE_TYPE_FAX         = 5,    /* Fax machine */
    DEVICE_TYPE_NETWORK     = 6,    /* Network device */
    DEVICE_TYPE_SERIAL      = 7,    /* Serial device */
    DEVICE_TYPE_USB         = 8,    /* USB device */
    DEVICE_TYPE_CUSTOM      = 100   /* Custom device type */
} DeviceType;

/* Connection Types */
typedef enum {
    CONNECTION_APPLETALK    = 0,    /* AppleTalk network */
    CONNECTION_SERIAL       = 1,    /* Serial connection */
    CONNECTION_PARALLEL     = 2,    /* Parallel connection */
    CONNECTION_USB          = 3,    /* USB connection */
    CONNECTION_ETHERNET     = 4,    /* Ethernet/TCP-IP */
    CONNECTION_WIRELESS     = 5,    /* Wireless connection */
    CONNECTION_BLUETOOTH    = 6,    /* Bluetooth connection */
    CONNECTION_LOCAL        = 7     /* Local/direct connection */
} ConnectionType;

/* Device States */
typedef enum {
    DEVICE_STATE_UNKNOWN    = 0,    /* Unknown state */
    DEVICE_STATE_AVAILABLE  = 1,    /* Available for use */
    DEVICE_STATE_BUSY       = 2,    /* Currently busy */
    DEVICE_STATE_OFFLINE    = 3,    /* Offline/disconnected */
    DEVICE_STATE_ERROR      = 4,    /* Error state */
    DEVICE_STATE_SELECTED   = 5     /* Currently selected */
} DeviceState;

/* AppleTalk Zone */
typedef struct ATZone {
    char            name[ZONE_NAME_LENGTH + 1];    /* Zone name */
    bool            isDefault;      /* Default zone */
    int16_t         deviceCount;    /* Number of devices in zone */

    struct ATZone   *next;          /* Next zone in list */
} ATZone;

/* Device Information */
typedef struct DeviceInfo {
    char            name[DEVICE_NAME_LENGTH + 1];  /* Device name */
    char            type[32];       /* Device type string */
    char            driver[DRIVER_NAME_LENGTH + 1]; /* Driver name */
    DeviceType      deviceType;     /* Device type */
    ConnectionType  connectionType; /* Connection type */
    DeviceState     state;          /* Current state */

    /* Network information */
    char            zone[ZONE_NAME_LENGTH + 1];    /* AppleTalk zone */
    char            address[64];    /* Network address */
    int16_t         port;           /* Network port */

    /* Capabilities */
    bool            canPrint;       /* Can print */
    bool            canScan;        /* Can scan */
    bool            canFax;         /* Can fax */
    bool            canShare;       /* Can share files */
    bool            supportsColor;  /* Supports color */
    bool            supportsDuplex; /* Supports duplex printing */

    /* Status */
    char            status[128];    /* Status string */
    int32_t         lastSeen;       /* Last seen timestamp */
    bool            isSelected;     /* Currently selected */

    /* Icon and resources */
    void            *icon;          /* Device icon */
    int16_t         iconID;         /* Icon resource ID */

    struct DeviceInfo *next;        /* Next device in list */
} DeviceInfo;

/* Printer Information (extends DeviceInfo) */
typedef struct PrinterInfo {
    DeviceInfo      base;           /* Base device info */

    /* Printer-specific */
    char            ppd[256];       /* PPD file path */
    bool            postScript;     /* PostScript printer */
    bool            networkPrinter; /* Network printer */
    int32_t         paperSizes;     /* Supported paper sizes (bit mask) */
    int16_t         resolution;     /* Print resolution (DPI) */
    int16_t         speed;          /* Print speed (PPM) */

    /* Consumables */
    int16_t         tonerLevel;     /* Toner level (0-100) */
    int16_t         paperLevel;     /* Paper level (0-100) */
    bool            lowToner;       /* Low toner warning */
    bool            outOfPaper;     /* Out of paper */

    /* Queue information */
    int16_t         queueLength;    /* Number of jobs in queue */
    char            currentJob[64]; /* Current job name */
} PrinterInfo;

/* Chooser State */
typedef struct Chooser {
    /* Device lists */
    DeviceInfo      *devices;       /* All discovered devices */
    DeviceInfo      *selectedDevice; /* Currently selected device */
    ATZone          *zones;         /* AppleTalk zones */
    ATZone          *currentZone;   /* Current zone */
    int16_t         deviceCount;    /* Number of devices */
    int16_t         zoneCount;      /* Number of zones */

    /* Display state */
    Rect            windowBounds;   /* Window bounds */
    Rect            deviceListRect; /* Device list area */
    Rect            zoneListRect;   /* Zone list area */
    Rect            infoRect;       /* Device info area */
    bool            windowVisible;  /* Window visibility */

    /* UI state */
    int16_t         selectedDeviceIndex; /* Selected device index */
    int16_t         selectedZoneIndex;   /* Selected zone index */
    bool            showZones;      /* Show zone list */
    bool            showDetails;    /* Show device details */

    /* Network state */
    bool            appleTalkActive; /* AppleTalk is active */
    bool            backgroundScan;  /* Background device scanning */
    int32_t         lastScan;       /* Last scan timestamp */
    int16_t         scanInterval;   /* Scan interval (seconds) */

    /* Settings */
    char            lastSelectedPrinter[DEVICE_NAME_LENGTH + 1];
    char            lastSelectedZone[ZONE_NAME_LENGTH + 1];
    bool            autoSelect;     /* Auto-select devices */
    bool            showOffline;    /* Show offline devices */
    bool            useBackground;  /* Use background processing */

    /* Drivers */
    void            *printerDrivers; /* Printer driver list */
    void            *deviceDrivers;  /* Device driver list */
} Chooser;

/* Device Discovery Callback */
typedef void (*DeviceDiscoveryCallback)(DeviceInfo *device, bool added, void *context);

/* Chooser Functions */

/**
 * Initialize Chooser
 * @param chooser Pointer to chooser structure
 * @return 0 on success, negative on error
 */
int Chooser_Initialize(Chooser *chooser);

/**
 * Shutdown Chooser
 * @param chooser Pointer to chooser structure
 */
void Chooser_Shutdown(Chooser *chooser);

/**
 * Reset Chooser to default state
 * @param chooser Pointer to chooser structure
 */
void Chooser_Reset(Chooser *chooser);

/* Device Discovery Functions */

/**
 * Scan for available devices
 * @param chooser Pointer to chooser structure
 * @param deviceType Type of devices to scan for (0 for all)
 * @return Number of devices found
 */
int Chooser_ScanDevices(Chooser *chooser, DeviceType deviceType);

/**
 * Start background device scanning
 * @param chooser Pointer to chooser structure
 * @param interval Scan interval in seconds
 * @return 0 on success, negative on error
 */
int Chooser_StartBackgroundScan(Chooser *chooser, int16_t interval);

/**
 * Stop background device scanning
 * @param chooser Pointer to chooser structure
 */
void Chooser_StopBackgroundScan(Chooser *chooser);

/**
 * Set device discovery callback
 * @param chooser Pointer to chooser structure
 * @param callback Callback function
 * @param context Callback context
 */
void Chooser_SetDiscoveryCallback(Chooser *chooser,
                                  DeviceDiscoveryCallback callback,
                                  void *context);

/* Device Management Functions */

/**
 * Add device to list
 * @param chooser Pointer to chooser structure
 * @param device Device information
 * @return 0 on success, negative on error
 */
int Chooser_AddDevice(Chooser *chooser, const DeviceInfo *device);

/**
 * Remove device from list
 * @param chooser Pointer to chooser structure
 * @param deviceName Device name
 * @return 0 on success, negative on error
 */
int Chooser_RemoveDevice(Chooser *chooser, const char *deviceName);

/**
 * Update device information
 * @param chooser Pointer to chooser structure
 * @param deviceName Device name
 * @param device Updated device information
 * @return 0 on success, negative on error
 */
int Chooser_UpdateDevice(Chooser *chooser, const char *deviceName,
                         const DeviceInfo *device);

/**
 * Get device by name
 * @param chooser Pointer to chooser structure
 * @param deviceName Device name
 * @return Pointer to device info or NULL if not found
 */
DeviceInfo *Chooser_GetDevice(Chooser *chooser, const char *deviceName);

/**
 * Get device by index
 * @param chooser Pointer to chooser structure
 * @param index Device index
 * @return Pointer to device info or NULL if invalid index
 */
DeviceInfo *Chooser_GetDeviceByIndex(Chooser *chooser, int16_t index);

/**
 * Select device
 * @param chooser Pointer to chooser structure
 * @param deviceName Device name
 * @return 0 on success, negative on error
 */
int Chooser_SelectDevice(Chooser *chooser, const char *deviceName);

/**
 * Get selected device
 * @param chooser Pointer to chooser structure
 * @return Pointer to selected device or NULL if none
 */
DeviceInfo *Chooser_GetSelectedDevice(Chooser *chooser);

/* Zone Management Functions */

/**
 * Scan for AppleTalk zones
 * @param chooser Pointer to chooser structure
 * @return Number of zones found
 */
int Chooser_ScanZones(Chooser *chooser);

/**
 * Add zone to list
 * @param chooser Pointer to chooser structure
 * @param zoneName Zone name
 * @param isDefault True if default zone
 * @return 0 on success, negative on error
 */
int Chooser_AddZone(Chooser *chooser, const char *zoneName, bool isDefault);

/**
 * Select zone
 * @param chooser Pointer to chooser structure
 * @param zoneName Zone name
 * @return 0 on success, negative on error
 */
int Chooser_SelectZone(Chooser *chooser, const char *zoneName);

/**
 * Get devices in zone
 * @param chooser Pointer to chooser structure
 * @param zoneName Zone name
 * @param devices Array to fill with device pointers
 * @param maxDevices Maximum number of devices
 * @return Number of devices returned
 */
int Chooser_GetDevicesInZone(Chooser *chooser, const char *zoneName,
                              DeviceInfo **devices, int maxDevices);

/* Printer Functions */

/**
 * Set default printer
 * @param chooser Pointer to chooser structure
 * @param printerName Printer name
 * @return 0 on success, negative on error
 */
int Chooser_SetDefaultPrinter(Chooser *chooser, const char *printerName);

/**
 * Get default printer
 * @param chooser Pointer to chooser structure
 * @return Pointer to default printer or NULL if none
 */
DeviceInfo *Chooser_GetDefaultPrinter(Chooser *chooser);

/**
 * Test printer connection
 * @param chooser Pointer to chooser structure
 * @param printerName Printer name
 * @return 0 if connected, negative on error
 */
int Chooser_TestPrinter(Chooser *chooser, const char *printerName);

/**
 * Get printer status
 * @param chooser Pointer to chooser structure
 * @param printerName Printer name
 * @param status Buffer for status string
 * @param statusSize Size of status buffer
 * @return 0 on success, negative on error
 */
int Chooser_GetPrinterStatus(Chooser *chooser, const char *printerName,
                             char *status, int statusSize);

/* Driver Management Functions */

/**
 * Load device driver
 * @param driverName Driver name
 * @return Driver handle or NULL on error
 */
void *Chooser_LoadDriver(const char *driverName);

/**
 * Unload device driver
 * @param driver Driver handle
 */
void Chooser_UnloadDriver(void *driver);

/**
 * Get available drivers
 * @param deviceType Device type
 * @param drivers Array to fill with driver names
 * @param maxDrivers Maximum number of drivers
 * @return Number of drivers returned
 */
int Chooser_GetAvailableDrivers(DeviceType deviceType, char **drivers,
                                 int maxDrivers);

/* Display Functions */

/**
 * Draw chooser window
 * @param chooser Pointer to chooser structure
 * @param updateRect Rectangle to update or NULL for all
 */
void Chooser_Draw(Chooser *chooser, const Rect *updateRect);

/**
 * Draw device list
 * @param chooser Pointer to chooser structure
 */
void Chooser_DrawDeviceList(Chooser *chooser);

/**
 * Draw zone list
 * @param chooser Pointer to chooser structure
 */
void Chooser_DrawZoneList(Chooser *chooser);

/**
 * Draw device information
 * @param chooser Pointer to chooser structure
 */
void Chooser_DrawDeviceInfo(Chooser *chooser);

/**
 * Update display
 * @param chooser Pointer to chooser structure
 */
void Chooser_UpdateDisplay(Chooser *chooser);

/* Event Handling */

/**
 * Handle mouse click in chooser window
 * @param chooser Pointer to chooser structure
 * @param point Click location
 * @param modifiers Modifier keys
 * @return 0 on success, negative on error
 */
int Chooser_HandleClick(Chooser *chooser, Point point, uint16_t modifiers);

/**
 * Handle double-click on device
 * @param chooser Pointer to chooser structure
 * @param deviceIndex Device index
 * @return 0 on success, negative on error
 */
int Chooser_HandleDoubleClick(Chooser *chooser, int16_t deviceIndex);

/**
 * Handle key press
 * @param chooser Pointer to chooser structure
 * @param key Key character
 * @param modifiers Modifier keys
 * @return 0 on success, negative on error
 */
int Chooser_HandleKeyPress(Chooser *chooser, char key, uint16_t modifiers);

/* Utility Functions */

/**
 * Get device type string
 * @param deviceType Device type
 * @return Device type string
 */
const char *Chooser_GetDeviceTypeString(DeviceType deviceType);

/**
 * Get connection type string
 * @param connectionType Connection type
 * @return Connection type string
 */
const char *Chooser_GetConnectionTypeString(ConnectionType connectionType);

/**
 * Parse device address
 * @param address Address string
 * @param host Buffer for host name
 * @param hostSize Size of host buffer
 * @param port Pointer to port number (output)
 * @return 0 on success, negative on error
 */
int Chooser_ParseAddress(const char *address, char *host, int hostSize,
                         int16_t *port);

/**
 * Format device address
 * @param host Host name
 * @param port Port number
 * @param buffer Buffer for formatted address
 * @param bufferSize Size of buffer
 */
void Chooser_FormatAddress(const char *host, int16_t port,
                           char *buffer, int bufferSize);

/* Settings Functions */

/**
 * Load chooser settings
 * @param chooser Pointer to chooser structure
 * @return 0 on success, negative on error
 */
int Chooser_LoadSettings(Chooser *chooser);

/**
 * Save chooser settings
 * @param chooser Pointer to chooser structure
 * @return 0 on success, negative on error
 */
int Chooser_SaveSettings(Chooser *chooser);

/**
 * Reset to default settings
 * @param chooser Pointer to chooser structure
 */
void Chooser_ResetSettings(Chooser *chooser);

/* Desk Accessory Integration */

/**
 * Register Chooser as a desk accessory
 * @return 0 on success, negative on error
 */
int Chooser_RegisterDA(void);

/**
 * Create Chooser DA instance
 * @return Pointer to DA instance or NULL on error
 */
DeskAccessory *Chooser_CreateDA(void);

/* Chooser Error Codes */
#define CHOOSER_ERR_NONE            0       /* No error */
#define CHOOSER_ERR_DEVICE_NOT_FOUND -1     /* Device not found */
#define CHOOSER_ERR_ZONE_NOT_FOUND  -2     /* Zone not found */
#define CHOOSER_ERR_CONNECTION_FAILED -3    /* Connection failed */
#define CHOOSER_ERR_DRIVER_ERROR    -4     /* Driver error */
#define CHOOSER_ERR_NETWORK_ERROR   -5     /* Network error */
#define CHOOSER_ERR_INVALID_DEVICE  -6     /* Invalid device */
#define CHOOSER_ERR_TOO_MANY_DEVICES -7    /* Too many devices */

#endif /* CHOOSER_H */