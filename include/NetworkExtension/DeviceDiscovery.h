/*
 * DeviceDiscovery.h - Network Device Discovery and Service Advertising
 * Mac OS 7.1 Network Extension for device and service discovery
 *
 * Provides:
 * - AppleTalk service discovery via NBP
 * - Service advertisement and registration
 * - Device enumeration and browsing
 * - Service resolution and connection
 * - Modern protocol integration (Bonjour, UPnP, DNS-SD)
 */

#ifndef DEVICEDISCOVERY_H
#define DEVICEDISCOVERY_H

#include "NetworkExtension.h"
#include "AppleTalkStack.h"
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Device Discovery Constants
 */
#define DEVICE_MAX_SERVICES 512
#define DEVICE_MAX_NAME_LENGTH 32
#define DEVICE_MAX_TYPE_LENGTH 32
#define DEVICE_MAX_DESCRIPTION_LENGTH 256

/*
 * Service Types
 */
typedef enum {
    kServiceTypeFileServer = 1,
    kServiceTypePrintServer = 2,
    kServiceTypeMailServer = 3,
    kServiceTypeWebServer = 4,
    kServiceTypeFTPServer = 5,
    kServiceTypeDatabase = 6,
    kServiceTypeGeneric = 100
} DiscoveryServiceType;

/*
 * Service Status
 */
typedef enum {
    kServiceStatusUnknown = 0,
    kServiceStatusAvailable = 1,
    kServiceStatusBusy = 2,
    kServiceStatusUnavailable = 3,
    kServiceStatusError = 4
} ServiceStatus;

/*
 * Service Flags
 */
typedef enum {
    kServiceFlagAdvertised = 0x0001,
    kServiceFlagSecure = 0x0002,
    kServiceFlagAuthenticated = 0x0004,
    kServiceFlagTCP = 0x0008,
    kServiceFlagUDP = 0x0010,
    kServiceFlagBonjour = 0x0020
} ServiceFlags;

/*
 * Service Information Structure
 */
typedef struct {
    char name[DEVICE_MAX_NAME_LENGTH];
    char type[DEVICE_MAX_TYPE_LENGTH];
    char zone[ZONE_MAX_NAME_LENGTH];
    char description[DEVICE_MAX_DESCRIPTION_LENGTH];
    AppleTalkAddress address;
    uint16_t port;
    DiscoveryServiceType serviceType;
    ServiceStatus status;
    ServiceFlags flags;
    time_t lastSeen;
    time_t advertisedSince;

    // Extended information
    char version[32];
    char vendor[64];
    uint32_t capabilities;

    // Modern protocol info
    char bonjourName[DEVICE_MAX_NAME_LENGTH];
    char bonjourType[DEVICE_MAX_TYPE_LENGTH];
    char domain[64];
} ServiceInfo;

/*
 * Device Information Structure
 */
typedef struct {
    char name[DEVICE_MAX_NAME_LENGTH];
    char type[32];
    char zone[ZONE_MAX_NAME_LENGTH];
    AppleTalkAddress address;
    time_t lastSeen;

    // Device details
    char model[64];
    char version[32];
    char serialNumber[32];
    uint32_t capabilities;

    // Services offered
    ServiceInfo services[16];
    int serviceCount;
} DeviceInfo;

/*
 * Discovery Query Structure
 */
typedef struct {
    char namePattern[DEVICE_MAX_NAME_LENGTH];
    char typePattern[DEVICE_MAX_TYPE_LENGTH];
    char zonePattern[ZONE_MAX_NAME_LENGTH];
    DiscoveryServiceType serviceType;
    bool exactMatch;
    bool localOnly;
    bool includeUnavailable;
    uint32_t timeout;
} DiscoveryQuery;

/*
 * Forward Declarations
 */
typedef struct DeviceDiscovery DeviceDiscovery;

/*
 * Callback Types
 */
typedef void (*ServiceFoundCallback)(const ServiceInfo* service, void* userData);
typedef void (*ServiceLostCallback)(const ServiceInfo* service, void* userData);
typedef void (*DeviceFoundCallback)(const DeviceInfo* device, void* userData);
typedef void (*DiscoveryQueryCallback)(const ServiceInfo* services, int count, NetworkExtensionError error, void* userData);

/*
 * Device Discovery Functions
 */

/**
 * Global initialization
 */
void DeviceDiscoveryGlobalInit(void);

/**
 * Create device discovery
 */
NetworkExtensionError DeviceDiscoveryCreate(DeviceDiscovery** discovery,
                                             AppleTalkStack* appleTalkStack);

/**
 * Destroy device discovery
 */
void DeviceDiscoveryDestroy(DeviceDiscovery* discovery);

/**
 * Start device discovery
 */
NetworkExtensionError DeviceDiscoveryStart(DeviceDiscovery* discovery);

/**
 * Stop device discovery
 */
void DeviceDiscoveryStop(DeviceDiscovery* discovery);

/*
 * Service Discovery Functions
 */

/**
 * Browse for services
 */
NetworkExtensionError DeviceDiscoveryBrowseServices(DeviceDiscovery* discovery,
                                                     const DiscoveryQuery* query,
                                                     DiscoveryQueryCallback callback,
                                                     void* userData);

/**
 * Resolve service
 */
NetworkExtensionError DeviceDiscoveryResolveService(DeviceDiscovery* discovery,
                                                     const char* serviceName,
                                                     const char* serviceType,
                                                     const char* zone,
                                                     ServiceInfo* serviceInfo);

/**
 * Get service list
 */
NetworkExtensionError DeviceDiscoveryGetServiceList(DeviceDiscovery* discovery,
                                                     ServiceInfo* services,
                                                     int maxServices,
                                                     int* actualServices);

/**
 * Find services by type
 */
NetworkExtensionError DeviceDiscoveryFindServicesByType(DeviceDiscovery* discovery,
                                                         DiscoveryServiceType serviceType,
                                                         const char* zone,
                                                         ServiceInfo* services,
                                                         int maxServices,
                                                         int* actualServices);

/*
 * Service Advertisement Functions
 */

/**
 * Advertise service
 */
NetworkExtensionError DeviceDiscoveryAdvertiseService(DeviceDiscovery* discovery,
                                                       const ServiceInfo* service);

/**
 * Stop advertising service
 */
NetworkExtensionError DeviceDiscoveryStopAdvertising(DeviceDiscovery* discovery,
                                                      const char* serviceName);

/**
 * Update service information
 */
NetworkExtensionError DeviceDiscoveryUpdateService(DeviceDiscovery* discovery,
                                                    const ServiceInfo* service);

/**
 * Set service status
 */
NetworkExtensionError DeviceDiscoverySetServiceStatus(DeviceDiscovery* discovery,
                                                       const char* serviceName,
                                                       ServiceStatus status);

/*
 * Device Discovery Functions
 */

/**
 * Discover devices
 */
NetworkExtensionError DeviceDiscoveryDiscoverDevices(DeviceDiscovery* discovery,
                                                      const char* zone,
                                                      DeviceFoundCallback callback,
                                                      void* userData);

/**
 * Get device list
 */
NetworkExtensionError DeviceDiscoveryGetDeviceList(DeviceDiscovery* discovery,
                                                    DeviceInfo* devices,
                                                    int maxDevices,
                                                    int* actualDevices);

/**
 * Get device information
 */
NetworkExtensionError DeviceDiscoveryGetDeviceInfo(DeviceDiscovery* discovery,
                                                    const AppleTalkAddress* address,
                                                    DeviceInfo* deviceInfo);

/*
 * Callback Management
 */

/**
 * Set service found callback
 */
void DeviceDiscoverySetServiceFoundCallback(DeviceDiscovery* discovery,
                                             ServiceFoundCallback callback,
                                             void* userData);

/**
 * Set service lost callback
 */
void DeviceDiscoverySetServiceLostCallback(DeviceDiscovery* discovery,
                                            ServiceLostCallback callback,
                                            void* userData);

/**
 * Set device found callback
 */
void DeviceDiscoverySetDeviceFoundCallback(DeviceDiscovery* discovery,
                                            DeviceFoundCallback callback,
                                            void* userData);

/*
 * Modern Protocol Integration
 */

/**
 * Enable Bonjour integration
 */
NetworkExtensionError DeviceDiscoveryEnableBonjour(DeviceDiscovery* discovery);

/**
 * Disable Bonjour integration
 */
void DeviceDiscoveryDisableBonjour(DeviceDiscovery* discovery);

/**
 * Enable UPnP integration
 */
NetworkExtensionError DeviceDiscoveryEnableUPnP(DeviceDiscovery* discovery);

/**
 * Disable UPnP integration
 */
void DeviceDiscoveryDisableUPnP(DeviceDiscovery* discovery);

/**
 * Map AppleTalk service to Bonjour
 */
NetworkExtensionError DeviceDiscoveryMapToBonjourService(DeviceDiscovery* discovery,
                                                          const char* appleTalkType,
                                                          const char* bonjourType);

/**
 * Bridge service to modern protocols
 */
NetworkExtensionError DeviceDiscoveryBridgeService(DeviceDiscovery* discovery,
                                                    const ServiceInfo* service,
                                                    bool enable);

/*
 * Configuration Functions
 */

/**
 * Set discovery interval
 */
NetworkExtensionError DeviceDiscoverySetInterval(DeviceDiscovery* discovery,
                                                  uint32_t intervalSeconds);

/**
 * Set service timeout
 */
NetworkExtensionError DeviceDiscoverySetServiceTimeout(DeviceDiscovery* discovery,
                                                        uint32_t timeoutSeconds);

/**
 * Enable passive discovery
 */
NetworkExtensionError DeviceDiscoverySetPassiveMode(DeviceDiscovery* discovery,
                                                     bool passive);

/**
 * Get discovery statistics
 */
NetworkExtensionError DeviceDiscoveryGetStatistics(DeviceDiscovery* discovery,
                                                    int* totalServices,
                                                    int* advertisedServices,
                                                    int* availableServices,
                                                    int* totalQueries);

/*
 * Utility Functions
 */

/**
 * Validate service name
 */
bool DeviceDiscoveryValidateServiceName(const char* serviceName);

/**
 * Validate service type
 */
bool DeviceDiscoveryValidateServiceType(const char* serviceType);

/**
 * Compare service names
 */
bool DeviceDiscoveryCompareServiceNames(const char* name1, const char* name2);

/**
 * Get service type string
 */
const char* DeviceDiscoveryGetServiceTypeString(DiscoveryServiceType serviceType);

/**
 * Parse service type from string
 */
DiscoveryServiceType DeviceDiscoveryParseServiceType(const char* typeString);

/**
 * Get status string
 */
const char* DeviceDiscoveryGetStatusString(ServiceStatus status);

/**
 * Check if service matches query
 */
bool DeviceDiscoveryServiceMatchesQuery(const ServiceInfo* service,
                                         const DiscoveryQuery* query);

#ifdef __cplusplus
}
#endif

#endif /* DEVICEDISCOVERY_H */