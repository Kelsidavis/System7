#ifndef CHOOSER_H
#define CHOOSER_H

#include "SystemTypes.h"

/* Forward declarations */

#include "SystemTypes.h"

/*
 * Chooser.h - Chooser Desk Accessory
 *
 * Provides device selection interface for printers, network devices, and other
 * shared resources. Allows users to browse available devices, configure
 * connections, and manage device preferences.
 *
 * Derived from ROM analysis (System 7)
 */

#include "DeskAccessory.h"

/* Chooser Constants */
#define CHOOSER_VERSION         0x0100      /* Chooser version 1.0 */
#define MAX_DEVICES             256         /* Maximum devices */
#define MAX_ZONES               64          /* Maximum AppleTalk zones */
#define DEVICE_NAME_LENGTH      64          /* Maximum device name length */
#define ZONE_NAME_LENGTH        32          /* Maximum zone name length */
#define DRIVER_NAME_LENGTH      32          /* Maximum driver name length */

/* Device Types */
typedef enum {
    DEVICE_TYPE_UNKNOWN = 0,
    DEVICE_TYPE_PRINTER = 1,
    DEVICE_TYPE_FILE_SERVER = 2,
    DEVICE_TYPE_SHARED_DISK = 3,
    DEVICE_TYPE_SCANNER = 4,
    DEVICE_TYPE_FAX = 5,
    DEVICE_TYPE_NETWORK = 6,
    DEVICE_TYPE_SERIAL = 7,
    DEVICE_TYPE_USB = 8
} DeviceType;

/* Connection Types */
typedef enum {
    CONNECTION_APPLETALK = 0,
    CONNECTION_SERIAL = 1,
    CONNECTION_PARALLEL = 2,
    CONNECTION_USB = 3,
    CONNECTION_ETHERNET = 4,
    CONNECTION_WIRELESS = 5,
    CONNECTION_BLUETOOTH = 6,
    CONNECTION_LOCAL = 7
} ConnectionType;

/* Device States */
typedef enum {
    DEVICE_STATE_OFFLINE = 0,
    DEVICE_STATE_AVAILABLE = 1,
    DEVICE_STATE_BUSY = 2,
    DEVICE_STATE_ERROR = 3
} DeviceState;

/* AppleTalk Zone */
typedef struct ATZone {
    char name[ZONE_NAME_LENGTH];
    Boolean isDefault;
    SInt16 deviceCount;
    struct ATZone *next;
} ATZone;

/* Device Information */
typedef struct DeviceInfo {
    char name[DEVICE_NAME_LENGTH];
    char type[DEVICE_NAME_LENGTH];
    char driver[DRIVER_NAME_LENGTH];
    DeviceType deviceType;
    ConnectionType connectionType;
    DeviceState state;
    char zone[ZONE_NAME_LENGTH];
    char address[DEVICE_NAME_LENGTH];
    Boolean canPrint;
    Boolean canShare;
    Boolean supportsColor;
    Boolean supportsDuplex;
    char status[64];
    SInt32 lastSeen;
    Boolean isSelected;
    SInt16 iconID;
    Handle icon;
    struct DeviceInfo *next;
} DeviceInfo;

/* Printer Information (extends DeviceInfo) */

/* Device Discovery Callback */
typedef int (*DeviceDiscoveryCallback)(DeviceInfo *device, void *context);

/* Chooser State */
typedef struct Chooser {
    Rect windowBounds;
    Rect deviceListRect;
    Rect zoneListRect;
    Rect deviceInfoRect;
    DeviceInfo *devices;
    ATZone *zones;
    ATZone *currentZone;
    DeviceInfo *selectedDevice;
    SInt16 selectedDeviceIndex;
    SInt16 selectedZoneIndex;
    SInt16 deviceCount;
    SInt16 zoneCount;
    char lastSelectedPrinter[DEVICE_NAME_LENGTH];
    char lastSelectedZone[ZONE_NAME_LENGTH];
    Boolean appleTalkActive;
    Boolean backgroundScan;
    SInt16 scanInterval;
    SInt32 lastScan;
    Boolean autoSelect;
    Boolean showOffline;
    Boolean useBackground;
    Boolean showZones;
    Boolean showDetails;
    DeviceDiscoveryCallback discoveryCallback;
    void *callbackContext;
} Chooser;

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
int Chooser_StartBackgroundScan(Chooser *chooser, SInt16 interval);

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


/* Event Handling */

/**
 * Handle mouse click in chooser window
 * @param chooser Pointer to chooser structure
 * @param point Click location
 * @param modifiers Modifier keys
 * @return 0 on success, negative on error
 */
int Chooser_HandleClick(Chooser *chooser, Point point, UInt16 modifiers);


/**
 * Handle key press
 * @param chooser Pointer to chooser structure
 * @param key Key character
 * @param modifiers Modifier keys
 * @return 0 on success, negative on error
 */
int Chooser_HandleKeyPress(Chooser *chooser, char key, UInt16 modifiers);

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
                         SInt16 *port);

/**
 * Format device address
 * @param host Host name
 * @param port Port number
 * @param buffer Buffer for formatted address
 * @param bufferSize Size of buffer
 */
void Chooser_FormatAddress(const char *host, SInt16 port,
                           char *buffer, int bufferSize);

/* Settings Functions */


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