/*
 * Chooser.c - Chooser Desk Accessory Implementation
 *
 * Provides device selection interface for printers, network devices, and other
 * shared resources. Allows users to browse available devices, configure
 * connections, and manage device preferences.
 *
 * Based on Apple's Chooser DA from System 7.1
 */

#include "DeskManager/Chooser.h"
#include "DeskManager/DeskManager.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/*
 * Initialize Chooser
 */
int Chooser_Initialize(Chooser *chooser)
{
    if (!chooser) {
        return CHOOSER_ERR_INVALID_DEVICE;
    }

    memset(chooser, 0, sizeof(Chooser));

    /* Set window bounds */
    chooser->windowBounds.left = 100;
    chooser->windowBounds.top = 100;
    chooser->windowBounds.right = 500;
    chooser->windowBounds.bottom = 400;

    /* Set display areas */
    chooser->deviceListRect.left = 20;
    chooser->deviceListRect.top = 40;
    chooser->deviceListRect.right = 180;
    chooser->deviceListRect.bottom = 200;

    chooser->zoneListRect.left = 200;
    chooser->zoneListRect.top = 40;
    chooser->zoneListRect.right = 360;
    chooser->zoneListRect.bottom = 120;

    chooser->infoRect.left = 20;
    chooser->infoRect.top = 220;
    chooser->infoRect.right = 380;
    chooser->infoRect.bottom = 280;

    /* Set default settings */
    chooser->appleTalkActive = true;
    chooser->backgroundScan = true;
    chooser->scanInterval = 30;  /* 30 seconds */
    chooser->autoSelect = false;
    chooser->showOffline = true;
    chooser->useBackground = true;
    chooser->showZones = true;
    chooser->showDetails = true;

    chooser->selectedDeviceIndex = -1;
    chooser->selectedZoneIndex = -1;

    /* Initialize with default zone */
    ATZone *defaultZone = malloc(sizeof(ATZone));
    if (defaultZone) {
        strcpy(defaultZone->name, "*");
        defaultZone->isDefault = true;
        defaultZone->deviceCount = 0;
        defaultZone->next = NULL;

        chooser->zones = defaultZone;
        chooser->currentZone = defaultZone;
        chooser->zoneCount = 1;
    }

    return CHOOSER_ERR_NONE;
}

/*
 * Shutdown Chooser
 */
void Chooser_Shutdown(Chooser *chooser)
{
    if (!chooser) {
        return;
    }

    /* Stop background scanning */
    Chooser_StopBackgroundScan(chooser);

    /* Free all devices */
    DeviceInfo *device = chooser->devices;
    while (device) {
        DeviceInfo *next = device->next;
        free(device);
        device = next;
    }

    /* Free all zones */
    ATZone *zone = chooser->zones;
    while (zone) {
        ATZone *next = zone->next;
        free(zone);
        zone = next;
    }

    chooser->devices = NULL;
    chooser->zones = NULL;
}

/*
 * Reset Chooser to default state
 */
void Chooser_Reset(Chooser *chooser)
{
    if (!chooser) {
        return;
    }

    chooser->selectedDevice = NULL;
    chooser->selectedDeviceIndex = -1;
    chooser->selectedZoneIndex = -1;
    chooser->lastSelectedPrinter[0] = '\0';
    chooser->lastSelectedZone[0] = '\0';
}

/*
 * Scan for available devices
 */
int Chooser_ScanDevices(Chooser *chooser, DeviceType deviceType)
{
    if (!chooser) {
        return 0;
    }

    int devicesFound = 0;

    /* In a real implementation, this would scan for actual devices */
    /* For now, add some dummy devices for demonstration */

    if (deviceType == DEVICE_TYPE_UNKNOWN || deviceType == DEVICE_TYPE_PRINTER) {
        /* Add dummy printer */
        DeviceInfo *printer = malloc(sizeof(DeviceInfo));
        if (printer) {
            strcpy(printer->name, "LaserWriter");
            strcpy(printer->type, "PostScript Printer");
            strcpy(printer->driver, "LaserWriter");
            printer->deviceType = DEVICE_TYPE_PRINTER;
            printer->connectionType = CONNECTION_APPLETALK;
            printer->state = DEVICE_STATE_AVAILABLE;
            strcpy(printer->zone, "*");
            strcpy(printer->address, "LaserWriter@*");
            printer->canPrint = true;
            printer->supportsColor = false;
            printer->supportsDuplex = true;
            strcpy(printer->status, "Ready");
            printer->lastSeen = (int32_t)time(NULL);
            printer->isSelected = false;
            printer->iconID = 1;
            printer->icon = NULL;
            printer->next = chooser->devices;

            chooser->devices = printer;
            chooser->deviceCount++;
            devicesFound++;
        }
    }

    if (deviceType == DEVICE_TYPE_UNKNOWN || deviceType == DEVICE_TYPE_FILE_SERVER) {
        /* Add dummy file server */
        DeviceInfo *server = malloc(sizeof(DeviceInfo));
        if (server) {
            strcpy(server->name, "FileServer");
            strcpy(server->type, "AppleShare Server");
            strcpy(server->driver, "AppleShare");
            server->deviceType = DEVICE_TYPE_FILE_SERVER;
            server->connectionType = CONNECTION_APPLETALK;
            server->state = DEVICE_STATE_AVAILABLE;
            strcpy(server->zone, "*");
            strcpy(server->address, "FileServer@*");
            server->canShare = true;
            strcpy(server->status, "Available");
            server->lastSeen = (int32_t)time(NULL);
            server->isSelected = false;
            server->iconID = 2;
            server->icon = NULL;
            server->next = chooser->devices;

            chooser->devices = server;
            chooser->deviceCount++;
            devicesFound++;
        }
    }

    chooser->lastScan = (int32_t)time(NULL);
    return devicesFound;
}

/*
 * Start background device scanning
 */
int Chooser_StartBackgroundScan(Chooser *chooser, int16_t interval)
{
    if (!chooser) {
        return CHOOSER_ERR_INVALID_DEVICE;
    }

    chooser->backgroundScan = true;
    chooser->scanInterval = interval;

    /* In a real implementation, this would start a background thread */
    return CHOOSER_ERR_NONE;
}

/*
 * Stop background device scanning
 */
void Chooser_StopBackgroundScan(Chooser *chooser)
{
    if (chooser) {
        chooser->backgroundScan = false;
    }
}

/*
 * Add device to list
 */
int Chooser_AddDevice(Chooser *chooser, const DeviceInfo *device)
{
    if (!chooser || !device) {
        return CHOOSER_ERR_INVALID_DEVICE;
    }

    /* Check if device already exists */
    if (Chooser_GetDevice(chooser, device->name)) {
        return CHOOSER_ERR_INVALID_DEVICE;
    }

    DeviceInfo *newDevice = malloc(sizeof(DeviceInfo));
    if (!newDevice) {
        return CHOOSER_ERR_TOO_MANY_DEVICES;
    }

    *newDevice = *device;
    newDevice->next = chooser->devices;
    chooser->devices = newDevice;
    chooser->deviceCount++;

    return CHOOSER_ERR_NONE;
}

/*
 * Get device by name
 */
DeviceInfo *Chooser_GetDevice(Chooser *chooser, const char *deviceName)
{
    if (!chooser || !deviceName) {
        return NULL;
    }

    DeviceInfo *device = chooser->devices;
    while (device) {
        if (strcmp(device->name, deviceName) == 0) {
            return device;
        }
        device = device->next;
    }

    return NULL;
}

/*
 * Select device
 */
int Chooser_SelectDevice(Chooser *chooser, const char *deviceName)
{
    if (!chooser || !deviceName) {
        return CHOOSER_ERR_INVALID_DEVICE;
    }

    DeviceInfo *device = Chooser_GetDevice(chooser, deviceName);
    if (!device) {
        return CHOOSER_ERR_DEVICE_NOT_FOUND;
    }

    /* Deselect previous device */
    if (chooser->selectedDevice) {
        chooser->selectedDevice->isSelected = false;
    }

    /* Select new device */
    chooser->selectedDevice = device;
    device->isSelected = true;

    /* Update last selected */
    if (device->deviceType == DEVICE_TYPE_PRINTER) {
        strncpy(chooser->lastSelectedPrinter, deviceName,
                sizeof(chooser->lastSelectedPrinter) - 1);
        chooser->lastSelectedPrinter[sizeof(chooser->lastSelectedPrinter) - 1] = '\0';
    }

    return CHOOSER_ERR_NONE;
}

/*
 * Get selected device
 */
DeviceInfo *Chooser_GetSelectedDevice(Chooser *chooser)
{
    return chooser ? chooser->selectedDevice : NULL;
}

/*
 * Scan for AppleTalk zones
 */
int Chooser_ScanZones(Chooser *chooser)
{
    if (!chooser) {
        return 0;
    }

    /* In a real implementation, this would scan for AppleTalk zones */
    /* For now, just return the count of existing zones */
    return chooser->zoneCount;
}

/*
 * Select zone
 */
int Chooser_SelectZone(Chooser *chooser, const char *zoneName)
{
    if (!chooser || !zoneName) {
        return CHOOSER_ERR_ZONE_NOT_FOUND;
    }

    ATZone *zone = chooser->zones;
    while (zone) {
        if (strcmp(zone->name, zoneName) == 0) {
            chooser->currentZone = zone;
            strncpy(chooser->lastSelectedZone, zoneName,
                    sizeof(chooser->lastSelectedZone) - 1);
            chooser->lastSelectedZone[sizeof(chooser->lastSelectedZone) - 1] = '\0';
            return CHOOSER_ERR_NONE;
        }
        zone = zone->next;
    }

    return CHOOSER_ERR_ZONE_NOT_FOUND;
}

/*
 * Set default printer
 */
int Chooser_SetDefaultPrinter(Chooser *chooser, const char *printerName)
{
    if (!chooser || !printerName) {
        return CHOOSER_ERR_INVALID_DEVICE;
    }

    DeviceInfo *printer = Chooser_GetDevice(chooser, printerName);
    if (!printer || printer->deviceType != DEVICE_TYPE_PRINTER) {
        return CHOOSER_ERR_DEVICE_NOT_FOUND;
    }

    /* Select the printer */
    return Chooser_SelectDevice(chooser, printerName);
}

/*
 * Get default printer
 */
DeviceInfo *Chooser_GetDefaultPrinter(Chooser *chooser)
{
    if (!chooser || !chooser->selectedDevice) {
        return NULL;
    }

    if (chooser->selectedDevice->deviceType == DEVICE_TYPE_PRINTER) {
        return chooser->selectedDevice;
    }

    return NULL;
}

/*
 * Draw chooser window
 */
void Chooser_Draw(Chooser *chooser, const Rect *updateRect)
{
    if (!chooser) {
        return;
    }

    /* In a real implementation, this would draw the chooser interface */
    /* For now, just placeholder functions */
    Chooser_DrawDeviceList(chooser);
    if (chooser->showZones) {
        Chooser_DrawZoneList(chooser);
    }
    if (chooser->showDetails) {
        Chooser_DrawDeviceInfo(chooser);
    }
}

/*
 * Draw device list
 */
void Chooser_DrawDeviceList(Chooser *chooser)
{
    if (!chooser) {
        return;
    }

    /* In a real implementation, this would draw the device list */
    /* For now, just a placeholder */
}

/*
 * Draw zone list
 */
void Chooser_DrawZoneList(Chooser *chooser)
{
    if (!chooser) {
        return;
    }

    /* In a real implementation, this would draw the zone list */
    /* For now, just a placeholder */
}

/*
 * Draw device information
 */
void Chooser_DrawDeviceInfo(Chooser *chooser)
{
    if (!chooser) {
        return;
    }

    /* In a real implementation, this would draw device details */
    /* For now, just a placeholder */
}

/*
 * Handle mouse click in chooser window
 */
int Chooser_HandleClick(Chooser *chooser, Point point, uint16_t modifiers)
{
    if (!chooser) {
        return CHOOSER_ERR_INVALID_DEVICE;
    }

    /* Check which area was clicked */
    if (point.h >= chooser->deviceListRect.left &&
        point.h < chooser->deviceListRect.right &&
        point.v >= chooser->deviceListRect.top &&
        point.v < chooser->deviceListRect.bottom) {

        /* Click in device list */
        int itemHeight = 20;  /* Assumed item height */
        int clickedIndex = (point.v - chooser->deviceListRect.top) / itemHeight;

        DeviceInfo *device = chooser->devices;
        for (int i = 0; i < clickedIndex && device; i++) {
            device = device->next;
        }

        if (device) {
            Chooser_SelectDevice(chooser, device->name);
            chooser->selectedDeviceIndex = clickedIndex;
        }

        return CHOOSER_ERR_NONE;
    }

    if (chooser->showZones &&
        point.h >= chooser->zoneListRect.left &&
        point.h < chooser->zoneListRect.right &&
        point.v >= chooser->zoneListRect.top &&
        point.v < chooser->zoneListRect.bottom) {

        /* Click in zone list */
        int itemHeight = 15;  /* Assumed item height */
        int clickedIndex = (point.v - chooser->zoneListRect.top) / itemHeight;

        ATZone *zone = chooser->zones;
        for (int i = 0; i < clickedIndex && zone; i++) {
            zone = zone->next;
        }

        if (zone) {
            Chooser_SelectZone(chooser, zone->name);
            chooser->selectedZoneIndex = clickedIndex;
            /* Rescan devices in the new zone */
            Chooser_ScanDevices(chooser, DEVICE_TYPE_UNKNOWN);
        }

        return CHOOSER_ERR_NONE;
    }

    return CHOOSER_ERR_NONE;
}

/*
 * Handle key press
 */
int Chooser_HandleKeyPress(Chooser *chooser, char key, uint16_t modifiers)
{
    if (!chooser) {
        return CHOOSER_ERR_INVALID_DEVICE;
    }

    switch (key) {
        case '\r':  /* Return */
        case '\n':  /* Enter */
            if (chooser->selectedDevice) {
                /* Double-click equivalent - select device */
                return CHOOSER_ERR_NONE;
            }
            break;

        case 'r':  /* Refresh */
        case 'R':
            Chooser_ScanDevices(chooser, DEVICE_TYPE_UNKNOWN);
            break;

        default:
            break;
    }

    return CHOOSER_ERR_NONE;
}

/*
 * Get device type string
 */
const char *Chooser_GetDeviceTypeString(DeviceType deviceType)
{
    switch (deviceType) {
        case DEVICE_TYPE_PRINTER:       return "Printer";
        case DEVICE_TYPE_FILE_SERVER:   return "File Server";
        case DEVICE_TYPE_SHARED_DISK:   return "Shared Disk";
        case DEVICE_TYPE_SCANNER:       return "Scanner";
        case DEVICE_TYPE_FAX:           return "Fax";
        case DEVICE_TYPE_NETWORK:       return "Network Device";
        case DEVICE_TYPE_SERIAL:        return "Serial Device";
        case DEVICE_TYPE_USB:           return "USB Device";
        default:                        return "Unknown";
    }
}

/*
 * Get connection type string
 */
const char *Chooser_GetConnectionTypeString(ConnectionType connectionType)
{
    switch (connectionType) {
        case CONNECTION_APPLETALK:      return "AppleTalk";
        case CONNECTION_SERIAL:         return "Serial";
        case CONNECTION_PARALLEL:       return "Parallel";
        case CONNECTION_USB:            return "USB";
        case CONNECTION_ETHERNET:       return "Ethernet";
        case CONNECTION_WIRELESS:       return "Wireless";
        case CONNECTION_BLUETOOTH:      return "Bluetooth";
        case CONNECTION_LOCAL:          return "Local";
        default:                        return "Unknown";
    }
}

/*
 * Register Chooser as a desk accessory
 */
int Chooser_RegisterDA(void)
{
    /* This is handled in BuiltinDAs.c */
    return CHOOSER_ERR_NONE;
}

/*
 * Create Chooser DA instance
 */
DeskAccessory *Chooser_CreateDA(void)
{
    /* This is handled in BuiltinDAs.c */
    return NULL;
}