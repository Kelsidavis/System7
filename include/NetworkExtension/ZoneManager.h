/*
 * ZoneManager.h - AppleTalk Zone Management and Network Browsing
 * Mac OS 7.1 Network Extension for zone management
 *
 * Provides:
 * - AppleTalk zone information management
 * - Zone list maintenance and discovery
 * - Network browsing capabilities
 * - Zone-based service filtering
 * - Modern network discovery integration
 */

#ifndef ZONEMANAGER_H
#define ZONEMANAGER_H

#include "NetworkExtension.h"
#include "AppleTalkStack.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Zone Manager Constants
 */
#define ZONE_MAX_NAME_LENGTH 32
#define ZONE_MAX_ZONES 256
#define ZONE_MAX_NETWORKS_PER_ZONE 64

/*
 * Zone Flags
 */
typedef enum {
    kZoneFlagLocal = 0x0001,
    kZoneFlagDefault = 0x0002,
    kZoneFlagAvailable = 0x0004,
    kZoneFlagMulticast = 0x0008
} ZoneFlags;

/*
 * Zone Network Range
 */
typedef struct {
    uint16_t startNetwork;
    uint16_t endNetwork;
} ZoneNetworkRange;

/*
 * Extended Zone Information
 */
typedef struct {
    char name[ZONE_MAX_NAME_LENGTH];
    ZoneFlags flags;
    ZoneNetworkRange ranges[ZONE_MAX_NETWORKS_PER_ZONE];
    int rangeCount;
    time_t lastSeen;
    uint16_t hopCount;
    bool multicast;
} ExtendedZoneInfo;

/*
 * Zone Query Structure
 */
typedef struct {
    char pattern[ZONE_MAX_NAME_LENGTH];
    bool exactMatch;
    bool localOnly;
    bool includeHidden;
} ZoneQuery;

/*
 * Forward Declarations
 */
typedef struct ZoneManager ZoneManager;

/*
 * Callback Types
 */
typedef void (*ZoneUpdateCallback)(const ExtendedZoneInfo* zone, bool added, void* userData);
typedef void (*ZoneQueryCallback)(const ExtendedZoneInfo* zones, int count, NetworkExtensionError error, void* userData);

/*
 * Zone Manager Functions
 */

/**
 * Create zone manager
 */
NetworkExtensionError ZoneManagerCreate(ZoneManager** manager,
                                         AppleTalkStack* appleTalkStack);

/**
 * Destroy zone manager
 */
void ZoneManagerDestroy(ZoneManager* manager);

/**
 * Start zone manager
 */
NetworkExtensionError ZoneManagerStart(ZoneManager* manager);

/**
 * Stop zone manager
 */
void ZoneManagerStop(ZoneManager* manager);

/**
 * Set local zone
 */
NetworkExtensionError ZoneManagerSetLocalZone(ZoneManager* manager,
                                               const char* zoneName);

/**
 * Get local zone
 */
NetworkExtensionError ZoneManagerGetLocalZone(ZoneManager* manager,
                                               char* zoneName,
                                               size_t bufferSize);

/**
 * Get zone list
 */
NetworkExtensionError ZoneManagerGetZoneList(ZoneManager* manager,
                                              ZoneQueryCallback callback,
                                              void* userData);

/**
 * Query zones
 */
NetworkExtensionError ZoneManagerQueryZones(ZoneManager* manager,
                                             const ZoneQuery* query,
                                             ZoneQueryCallback callback,
                                             void* userData);

/**
 * Get zone information
 */
NetworkExtensionError ZoneManagerGetZoneInfo(ZoneManager* manager,
                                              const char* zoneName,
                                              ExtendedZoneInfo* info);

/**
 * Get zone for network
 */
NetworkExtensionError ZoneManagerGetNetworkZone(ZoneManager* manager,
                                                 uint16_t network,
                                                 char* zoneName,
                                                 size_t bufferSize);

/**
 * Add zone
 */
NetworkExtensionError ZoneManagerAddZone(ZoneManager* manager,
                                          const ExtendedZoneInfo* zone);

/**
 * Remove zone
 */
NetworkExtensionError ZoneManagerRemoveZone(ZoneManager* manager,
                                             const char* zoneName);

/**
 * Update zone information
 */
NetworkExtensionError ZoneManagerUpdateZone(ZoneManager* manager,
                                             const ExtendedZoneInfo* zone);

/**
 * Set zone update callback
 */
void ZoneManagerSetUpdateCallback(ZoneManager* manager,
                                  ZoneUpdateCallback callback,
                                  void* userData);

/**
 * Refresh zone list
 */
NetworkExtensionError ZoneManagerRefreshZones(ZoneManager* manager);

/**
 * Get zone statistics
 */
NetworkExtensionError ZoneManagerGetStatistics(ZoneManager* manager,
                                                int* totalZones,
                                                int* localZones,
                                                int* availableZones);

/*
 * Network Browsing Functions
 */

/**
 * Browse network services in zone
 */
NetworkExtensionError ZoneManagerBrowseServices(ZoneManager* manager,
                                                 const char* zoneName,
                                                 const char* serviceType,
                                                 ServiceDiscoveryCallback callback,
                                                 void* userData);

/**
 * Resolve service in zone
 */
NetworkExtensionError ZoneManagerResolveService(ZoneManager* manager,
                                                 const char* zoneName,
                                                 const char* serviceName,
                                                 const char* serviceType,
                                                 AppleTalkAddress* address);

/**
 * Advertise service in zone
 */
NetworkExtensionError ZoneManagerAdvertiseService(ZoneManager* manager,
                                                   const char* zoneName,
                                                   const char* serviceName,
                                                   const char* serviceType,
                                                   uint8_t socket);

/**
 * Stop advertising service
 */
NetworkExtensionError ZoneManagerStopAdvertising(ZoneManager* manager,
                                                  const char* serviceName);

/*
 * Modern Integration Functions
 */

/**
 * Enable Bonjour integration
 */
NetworkExtensionError ZoneManagerEnableBonjour(ZoneManager* manager);

/**
 * Disable Bonjour integration
 */
void ZoneManagerDisableBonjour(ZoneManager* manager);

/**
 * Map AppleTalk zone to DNS-SD domain
 */
NetworkExtensionError ZoneManagerMapZoneToDomain(ZoneManager* manager,
                                                  const char* zoneName,
                                                  const char* domain);

/**
 * Enable zone filtering
 */
NetworkExtensionError ZoneManagerEnableFiltering(ZoneManager* manager,
                                                  bool enable);

/*
 * Utility Functions
 */

/**
 * Validate zone name
 */
bool ZoneManagerValidateZoneName(const char* zoneName);

/**
 * Compare zone names
 */
bool ZoneManagerCompareZoneNames(const char* zone1, const char* zone2);

/**
 * Check if network is in zone
 */
bool ZoneManagerNetworkInZone(const ExtendedZoneInfo* zone, uint16_t network);

/**
 * Get default zone name
 */
const char* ZoneManagerGetDefaultZoneName(void);

#ifdef __cplusplus
}
#endif

#endif /* ZONEMANAGER_H */