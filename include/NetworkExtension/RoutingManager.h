/*
 * RoutingManager.h - Network Routing and Address Resolution
 * Mac OS 7.1 Network Extension for routing and address resolution
 *
 * Provides:
 * - AppleTalk routing table management (RTMP)
 * - Address resolution (AARP)
 * - Network discovery and topology mapping
 * - Route optimization and load balancing
 * - Modern protocol bridging and translation
 */

#ifndef ROUTINGMANAGER_H
#define ROUTINGMANAGER_H

#include "NetworkExtension.h"
#include "AppleTalkStack.h"
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Routing Constants
 */
#define ROUTING_MAX_ROUTES 1024
#define ROUTING_MAX_NETWORKS 65534
#define ROUTING_MAX_HOPS 15
#define AARP_MAX_ENTRIES 256
#define AARP_MAX_HARDWARE_ADDRESS_SIZE 8

/*
 * Route Types
 */
typedef enum {
    kRouteTypeDirect = 1,
    kRouteTypeRouter = 2,
    kRouteTypeStatic = 3,
    kRouteTypeDefault = 4
} RouteType;

/*
 * Route Flags
 */
typedef enum {
    kRouteFlagActive = 0x0001,
    kRouteFlagStatic = 0x0002,
    kRouteFlagDefault = 0x0004,
    kRouteFlagLoopback = 0x0008,
    kRouteFlagBridge = 0x0010
} RouteFlags;

/*
 * Address Resolution State
 */
typedef enum {
    kAARPStateIncomplete = 0,
    kAARPStateReachable = 1,
    kAARPStateStale = 2,
    kAARPStateProbe = 3
} AARPState;

/*
 * Network Interface Type
 */
typedef enum {
    kNetworkInterfaceAppleTalk = 1,
    kNetworkInterfaceEthernet = 2,
    kNetworkInterfaceTokenRing = 3,
    kNetworkInterfaceWiFi = 4,
    kNetworkInterfaceLoopback = 5
} NetworkInterfaceType;

/*
 * Route Entry Structure
 */
typedef struct {
    uint16_t networkStart;
    uint16_t networkEnd;
    uint8_t nextHop;
    uint8_t hops;
    RouteType type;
    RouteFlags flags;
    time_t timestamp;
    time_t lastUsed;
    uint32_t useCount;
    uint16_t metric;
    char interfaceName[16];
} RouteEntry;

/*
 * AARP Entry Structure
 */
typedef struct {
    AppleTalkAddress protocolAddress;
    uint8_t hardwareAddress[AARP_MAX_HARDWARE_ADDRESS_SIZE];
    size_t hardwareAddressSize;
    AARPState state;
    time_t timestamp;
    time_t lastProbe;
    uint16_t probeCount;
    NetworkInterfaceType interfaceType;
    char interfaceName[16];
} AARPEntry;

/*
 * Network Interface Structure
 */
typedef struct {
    char name[16];
    NetworkInterfaceType type;
    bool active;
    uint8_t hardwareAddress[AARP_MAX_HARDWARE_ADDRESS_SIZE];
    size_t hardwareAddressSize;
    uint16_t mtu;
    uint32_t speed;
    time_t lastActivity;
} NetworkInterface;

/*
 * Routing Statistics
 */
typedef struct {
    uint32_t totalRoutes;
    uint32_t activeRoutes;
    uint32_t staticRoutes;
    uint32_t packetsRouted;
    uint32_t packetsDropped;
    uint32_t routingErrors;
    uint32_t aarpRequests;
    uint32_t aarpReplies;
    uint32_t aarpTimeouts;
} RoutingStatistics;

/*
 * Forward Declarations
 */
typedef struct RoutingManager RoutingManager;

/*
 * Callback Types
 */
typedef void (*RouteUpdateCallback)(const RouteEntry* route, bool added, void* userData);
typedef void (*AARPUpdateCallback)(const AARPEntry* entry, bool resolved, void* userData);
typedef void (*NetworkTopologyCallback)(const RouteEntry* routes, int count, void* userData);

/*
 * Routing Manager Functions
 */

/**
 * Create routing manager
 */
NetworkExtensionError RoutingManagerCreate(RoutingManager** manager,
                                            AppleTalkStack* appleTalkStack);

/**
 * Destroy routing manager
 */
void RoutingManagerDestroy(RoutingManager* manager);

/**
 * Start routing manager
 */
NetworkExtensionError RoutingManagerStart(RoutingManager* manager);

/**
 * Stop routing manager
 */
void RoutingManagerStop(RoutingManager* manager);

/*
 * Route Management Functions
 */

/**
 * Add route
 */
NetworkExtensionError RoutingManagerAddRoute(RoutingManager* manager,
                                              const RouteEntry* route);

/**
 * Remove route
 */
NetworkExtensionError RoutingManagerRemoveRoute(RoutingManager* manager,
                                                 uint16_t networkStart,
                                                 uint16_t networkEnd);

/**
 * Update route
 */
NetworkExtensionError RoutingManagerUpdateRoute(RoutingManager* manager,
                                                 const RouteEntry* route);

/**
 * Find route
 */
NetworkExtensionError RoutingManagerFindRoute(RoutingManager* manager,
                                               uint16_t network,
                                               RouteEntry* route);

/**
 * Get routing table
 */
NetworkExtensionError RoutingManagerGetRoutingTable(RoutingManager* manager,
                                                     RouteEntry* routes,
                                                     int maxRoutes,
                                                     int* actualRoutes);

/**
 * Get best route
 */
NetworkExtensionError RoutingManagerGetBestRoute(RoutingManager* manager,
                                                  const AppleTalkAddress* destination,
                                                  RouteEntry* route);

/**
 * Set default route
 */
NetworkExtensionError RoutingManagerSetDefaultRoute(RoutingManager* manager,
                                                     uint8_t nextHop);

/**
 * Remove default route
 */
NetworkExtensionError RoutingManagerRemoveDefaultRoute(RoutingManager* manager);

/*
 * Address Resolution Functions
 */

/**
 * Resolve address
 */
NetworkExtensionError RoutingManagerResolveAddress(RoutingManager* manager,
                                                    const AppleTalkAddress* protocolAddress,
                                                    uint8_t* hardwareAddress,
                                                    size_t* hardwareAddressSize);

/**
 * Add AARP entry
 */
NetworkExtensionError RoutingManagerAddAARPEntry(RoutingManager* manager,
                                                  const AARPEntry* entry);

/**
 * Remove AARP entry
 */
NetworkExtensionError RoutingManagerRemoveAARPEntry(RoutingManager* manager,
                                                     const AppleTalkAddress* protocolAddress);

/**
 * Get AARP table
 */
NetworkExtensionError RoutingManagerGetAARPTable(RoutingManager* manager,
                                                  AARPEntry* entries,
                                                  int maxEntries,
                                                  int* actualEntries);

/**
 * Probe address
 */
NetworkExtensionError RoutingManagerProbeAddress(RoutingManager* manager,
                                                  const AppleTalkAddress* address);

/**
 * Announce address
 */
NetworkExtensionError RoutingManagerAnnounceAddress(RoutingManager* manager,
                                                     const AppleTalkAddress* address,
                                                     const uint8_t* hardwareAddress,
                                                     size_t hardwareAddressSize);

/*
 * Network Discovery Functions
 */

/**
 * Discover network topology
 */
NetworkExtensionError RoutingManagerDiscoverTopology(RoutingManager* manager,
                                                      NetworkTopologyCallback callback,
                                                      void* userData);

/**
 * Get network neighbors
 */
NetworkExtensionError RoutingManagerGetNeighbors(RoutingManager* manager,
                                                  AppleTalkAddress* neighbors,
                                                  int maxNeighbors,
                                                  int* actualNeighbors);

/**
 * Trace route
 */
NetworkExtensionError RoutingManagerTraceRoute(RoutingManager* manager,
                                                const AppleTalkAddress* destination,
                                                AppleTalkAddress* hops,
                                                int maxHops,
                                                int* actualHops);

/*
 * Interface Management Functions
 */

/**
 * Add network interface
 */
NetworkExtensionError RoutingManagerAddInterface(RoutingManager* manager,
                                                  const NetworkInterface* interface);

/**
 * Remove network interface
 */
NetworkExtensionError RoutingManagerRemoveInterface(RoutingManager* manager,
                                                     const char* interfaceName);

/**
 * Get interface list
 */
NetworkExtensionError RoutingManagerGetInterfaces(RoutingManager* manager,
                                                   NetworkInterface* interfaces,
                                                   int maxInterfaces,
                                                   int* actualInterfaces);

/**
 * Set interface state
 */
NetworkExtensionError RoutingManagerSetInterfaceState(RoutingManager* manager,
                                                       const char* interfaceName,
                                                       bool active);

/*
 * Protocol Bridging Functions
 */

/**
 * Enable protocol bridging
 */
NetworkExtensionError RoutingManagerSetBridging(RoutingManager* manager,
                                                 bool enable);

/**
 * Bridge to IP network
 */
NetworkExtensionError RoutingManagerBridgeToIP(RoutingManager* manager,
                                                const char* ipInterface,
                                                const char* subnet);

/**
 * Map AppleTalk to IP address
 */
NetworkExtensionError RoutingManagerMapToIPAddress(RoutingManager* manager,
                                                    const AppleTalkAddress* appleTalkAddr,
                                                    uint32_t* ipAddress);

/**
 * Map IP to AppleTalk address
 */
NetworkExtensionError RoutingManagerMapFromIPAddress(RoutingManager* manager,
                                                      uint32_t ipAddress,
                                                      AppleTalkAddress* appleTalkAddr);

/*
 * Callback Management
 */

/**
 * Set route update callback
 */
void RoutingManagerSetRouteUpdateCallback(RoutingManager* manager,
                                           RouteUpdateCallback callback,
                                           void* userData);

/**
 * Set AARP update callback
 */
void RoutingManagerSetAARPUpdateCallback(RoutingManager* manager,
                                          AARPUpdateCallback callback,
                                          void* userData);

/*
 * Configuration Functions
 */

/**
 * Set routing table size
 */
NetworkExtensionError RoutingManagerSetTableSize(RoutingManager* manager,
                                                  int maxRoutes);

/**
 * Set AARP timeout
 */
NetworkExtensionError RoutingManagerSetAARPTimeout(RoutingManager* manager,
                                                    uint32_t timeoutSeconds);

/**
 * Set route timeout
 */
NetworkExtensionError RoutingManagerSetRouteTimeout(RoutingManager* manager,
                                                     uint32_t timeoutSeconds);

/**
 * Enable/disable route optimization
 */
NetworkExtensionError RoutingManagerSetOptimization(RoutingManager* manager,
                                                     bool enable);

/*
 * Statistics and Monitoring
 */

/**
 * Get routing statistics
 */
NetworkExtensionError RoutingManagerGetStatistics(RoutingManager* manager,
                                                   RoutingStatistics* stats);

/**
 * Reset statistics
 */
void RoutingManagerResetStatistics(RoutingManager* manager);

/**
 * Get route metrics
 */
NetworkExtensionError RoutingManagerGetRouteMetrics(RoutingManager* manager,
                                                     const RouteEntry* route,
                                                     uint32_t* latency,
                                                     uint32_t* throughput,
                                                     uint32_t* reliability);

/*
 * Utility Functions
 */

/**
 * Validate route entry
 */
bool RoutingManagerValidateRoute(const RouteEntry* route);

/**
 * Compare routes
 */
int RoutingManagerCompareRoutes(const RouteEntry* route1, const RouteEntry* route2);

/**
 * Calculate route metric
 */
uint16_t RoutingManagerCalculateMetric(const RouteEntry* route);

/**
 * Check if address is reachable
 */
bool RoutingManagerIsReachable(RoutingManager* manager,
                               const AppleTalkAddress* address);

/**
 * Get route type string
 */
const char* RoutingManagerGetRouteTypeString(RouteType type);

/**
 * Get AARP state string
 */
const char* RoutingManagerGetAARPStateString(AARPState state);

#ifdef __cplusplus
}
#endif

#endif /* ROUTINGMANAGER_H */