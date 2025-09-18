/*
 * RoutingManager.c - Network Routing and Address Resolution
 * Mac OS 7.1 Network Extension for routing and address resolution
 *
 * This module implements AppleTalk routing table management (RTMP),
 * address resolution (AARP), and modern protocol bridging.
 */

#include "RoutingManager.h"
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>

/*
 * Internal Constants
 */
#define DEFAULT_ROUTE_TIMEOUT 180
#define DEFAULT_AARP_TIMEOUT 30
#define RTMP_UPDATE_INTERVAL 10
#define AARP_PROBE_INTERVAL 1
#define MAX_PROBE_COUNT 3

/*
 * Routing Manager Structure
 */
struct RoutingManager {
    // State
    bool initialized;
    bool started;
    pthread_mutex_t mutex;

    // AppleTalk integration
    AppleTalkStack* appleTalkStack;
    DDPSocket* rtmpSocket;
    DDPSocket* aarpSocket;

    // Routing table
    RouteEntry routes[ROUTING_MAX_ROUTES];
    int routeCount;
    uint32_t routeTimeout;

    // AARP table
    AARPEntry aarpTable[AARP_MAX_ENTRIES];
    int aarpEntryCount;
    uint32_t aarpTimeout;

    // Network interfaces
    NetworkInterface interfaces[16];
    int interfaceCount;

    // Configuration
    bool bridgingEnabled;
    bool optimizationEnabled;
    int maxRoutes;

    // Callbacks
    RouteUpdateCallback routeUpdateCallback;
    void* routeUpdateUserData;
    AARPUpdateCallback aarpUpdateCallback;
    void* aarpUpdateUserData;

    // Threading
    pthread_t maintenanceThread;
    bool maintenanceRunning;

    // Statistics
    RoutingStatistics statistics;
};

/*
 * Private function declarations
 */
static void RTMPReceiveHandler(const DDPPacket* packet, void* userData);
static void AARPReceiveHandler(const DDPPacket* packet, void* userData);
static NetworkExtensionError ProcessRTMPPacket(RoutingManager* manager, const DDPPacket* packet);
static NetworkExtensionError ProcessAARPPacket(RoutingManager* manager, const DDPPacket* packet);
static NetworkExtensionError SendRTMPUpdate(RoutingManager* manager);
static NetworkExtensionError SendAARPRequest(RoutingManager* manager, const AppleTalkAddress* address);
static NetworkExtensionError SendAARPReply(RoutingManager* manager, const AARPEntry* entry, const AppleTalkAddress* requester);
static RouteEntry* FindRoute(RoutingManager* manager, uint16_t network);
static RouteEntry* FindBestRoute(RoutingManager* manager, uint16_t network);
static NetworkExtensionError AddRouteInternal(RoutingManager* manager, const RouteEntry* route);
static NetworkExtensionError RemoveRouteInternal(RoutingManager* manager, uint16_t networkStart, uint16_t networkEnd);
static AARPEntry* FindAARPEntry(RoutingManager* manager, const AppleTalkAddress* address);
static NetworkExtensionError AddAARPEntryInternal(RoutingManager* manager, const AARPEntry* entry);
static void RemoveExpiredRoutes(RoutingManager* manager);
static void RemoveExpiredAARPEntries(RoutingManager* manager);
static void ProbeStaleAARPEntries(RoutingManager* manager);
static void* MaintenanceThread(void* arg);
static uint16_t CalculateRouteMetric(const RouteEntry* route);

/*
 * RTMP and AARP packet structures
 */
typedef struct {
    uint16_t senderNetwork;
    uint8_t senderIDLength;
    uint8_t senderID;
} RTMPHeader;

typedef struct {
    uint16_t hardwareType;
    uint16_t protocolType;
    uint8_t hardwareLength;
    uint8_t protocolLength;
    uint16_t operation;
    uint8_t senderHardwareAddress[6];
    AppleTalkAddress senderProtocolAddress;
    uint8_t targetHardwareAddress[6];
    AppleTalkAddress targetProtocolAddress;
} AARPPacket;

/*
 * Create routing manager
 */
NetworkExtensionError RoutingManagerCreate(RoutingManager** manager,
                                            AppleTalkStack* appleTalkStack) {
    if (!manager || !appleTalkStack) {
        return kNetworkExtensionInvalidParam;
    }

    RoutingManager* newManager = calloc(1, sizeof(RoutingManager));
    if (!newManager) {
        return kNetworkExtensionInternalError;
    }

    // Initialize mutex
    if (pthread_mutex_init(&newManager->mutex, NULL) != 0) {
        free(newManager);
        return kNetworkExtensionInternalError;
    }

    newManager->appleTalkStack = appleTalkStack;
    newManager->routeTimeout = DEFAULT_ROUTE_TIMEOUT;
    newManager->aarpTimeout = DEFAULT_AARP_TIMEOUT;
    newManager->maxRoutes = ROUTING_MAX_ROUTES;
    newManager->optimizationEnabled = true;
    newManager->initialized = true;

    *manager = newManager;
    return kNetworkExtensionNoError;
}

/*
 * Destroy routing manager
 */
void RoutingManagerDestroy(RoutingManager* manager) {
    if (!manager || !manager->initialized) {
        return;
    }

    // Stop if running
    if (manager->started) {
        RoutingManagerStop(manager);
    }

    pthread_mutex_lock(&manager->mutex);
    manager->initialized = false;
    pthread_mutex_unlock(&manager->mutex);

    pthread_mutex_destroy(&manager->mutex);
    free(manager);
}

/*
 * Start routing manager
 */
NetworkExtensionError RoutingManagerStart(RoutingManager* manager) {
    if (!manager || !manager->initialized) {
        return kNetworkExtensionInvalidParam;
    }

    pthread_mutex_lock(&manager->mutex);

    if (manager->started) {
        pthread_mutex_unlock(&manager->mutex);
        return kNetworkExtensionNoError;
    }

    // Open RTMP socket
    NetworkExtensionError error = DDPOpenSocket(manager->appleTalkStack,
                                                 1, // RTMP socket
                                                 RTMPReceiveHandler,
                                                 manager,
                                                 &manager->rtmpSocket);
    if (error != kNetworkExtensionNoError) {
        pthread_mutex_unlock(&manager->mutex);
        return error;
    }

    // Open AARP socket
    error = DDPOpenSocket(manager->appleTalkStack,
                          0, // Dynamic socket for AARP
                          AARPReceiveHandler,
                          manager,
                          &manager->aarpSocket);
    if (error != kNetworkExtensionNoError) {
        DDPCloseSocket(manager->rtmpSocket);
        manager->rtmpSocket = NULL;
        pthread_mutex_unlock(&manager->mutex);
        return error;
    }

    // Start maintenance thread
    manager->maintenanceRunning = true;
    if (pthread_create(&manager->maintenanceThread, NULL, MaintenanceThread, manager) != 0) {
        manager->maintenanceRunning = false;
        DDPCloseSocket(manager->aarpSocket);
        DDPCloseSocket(manager->rtmpSocket);
        manager->aarpSocket = NULL;
        manager->rtmpSocket = NULL;
        pthread_mutex_unlock(&manager->mutex);
        return kNetworkExtensionInternalError;
    }

    manager->started = true;
    pthread_mutex_unlock(&manager->mutex);

    return kNetworkExtensionNoError;
}

/*
 * Stop routing manager
 */
void RoutingManagerStop(RoutingManager* manager) {
    if (!manager || !manager->initialized || !manager->started) {
        return;
    }

    pthread_mutex_lock(&manager->mutex);

    // Stop maintenance thread
    manager->maintenanceRunning = false;
    pthread_mutex_unlock(&manager->mutex);

    // Wait for thread to finish
    pthread_join(manager->maintenanceThread, NULL);

    pthread_mutex_lock(&manager->mutex);

    // Close sockets
    if (manager->rtmpSocket) {
        DDPCloseSocket(manager->rtmpSocket);
        manager->rtmpSocket = NULL;
    }

    if (manager->aarpSocket) {
        DDPCloseSocket(manager->aarpSocket);
        manager->aarpSocket = NULL;
    }

    manager->started = false;
    pthread_mutex_unlock(&manager->mutex);
}

/*
 * Add route
 */
NetworkExtensionError RoutingManagerAddRoute(RoutingManager* manager,
                                              const RouteEntry* route) {
    if (!manager || !manager->initialized || !route) {
        return kNetworkExtensionInvalidParam;
    }

    if (!RoutingManagerValidateRoute(route)) {
        return kNetworkExtensionInvalidParam;
    }

    pthread_mutex_lock(&manager->mutex);

    NetworkExtensionError error = AddRouteInternal(manager, route);

    pthread_mutex_unlock(&manager->mutex);

    return error;
}

/*
 * Remove route
 */
NetworkExtensionError RoutingManagerRemoveRoute(RoutingManager* manager,
                                                 uint16_t networkStart,
                                                 uint16_t networkEnd) {
    if (!manager || !manager->initialized) {
        return kNetworkExtensionInvalidParam;
    }

    pthread_mutex_lock(&manager->mutex);

    NetworkExtensionError error = RemoveRouteInternal(manager, networkStart, networkEnd);

    pthread_mutex_unlock(&manager->mutex);

    return error;
}

/*
 * Find route
 */
NetworkExtensionError RoutingManagerFindRoute(RoutingManager* manager,
                                               uint16_t network,
                                               RouteEntry* route) {
    if (!manager || !manager->initialized || !route) {
        return kNetworkExtensionInvalidParam;
    }

    pthread_mutex_lock(&manager->mutex);

    RouteEntry* found = FindRoute(manager, network);
    if (!found) {
        pthread_mutex_unlock(&manager->mutex);
        return kNetworkExtensionNotFound;
    }

    *route = *found;
    found->lastUsed = time(NULL);
    found->useCount++;

    pthread_mutex_unlock(&manager->mutex);

    return kNetworkExtensionNoError;
}

/*
 * Get best route
 */
NetworkExtensionError RoutingManagerGetBestRoute(RoutingManager* manager,
                                                  const AppleTalkAddress* destination,
                                                  RouteEntry* route) {
    if (!manager || !manager->initialized || !destination || !route) {
        return kNetworkExtensionInvalidParam;
    }

    pthread_mutex_lock(&manager->mutex);

    RouteEntry* found = FindBestRoute(manager, destination->network);
    if (!found) {
        pthread_mutex_unlock(&manager->mutex);
        return kNetworkExtensionNotFound;
    }

    *route = *found;
    found->lastUsed = time(NULL);
    found->useCount++;

    pthread_mutex_unlock(&manager->mutex);

    return kNetworkExtensionNoError;
}

/*
 * Resolve address
 */
NetworkExtensionError RoutingManagerResolveAddress(RoutingManager* manager,
                                                    const AppleTalkAddress* protocolAddress,
                                                    uint8_t* hardwareAddress,
                                                    size_t* hardwareAddressSize) {
    if (!manager || !manager->initialized || !protocolAddress || !hardwareAddress || !hardwareAddressSize) {
        return kNetworkExtensionInvalidParam;
    }

    pthread_mutex_lock(&manager->mutex);

    // Check AARP table first
    AARPEntry* entry = FindAARPEntry(manager, protocolAddress);
    if (entry && entry->state == kAARPStateReachable) {
        if (*hardwareAddressSize < entry->hardwareAddressSize) {
            pthread_mutex_unlock(&manager->mutex);
            return kNetworkExtensionBufferTooSmall;
        }

        memcpy(hardwareAddress, entry->hardwareAddress, entry->hardwareAddressSize);
        *hardwareAddressSize = entry->hardwareAddressSize;

        pthread_mutex_unlock(&manager->mutex);
        return kNetworkExtensionNoError;
    }

    // Send AARP request if not found or stale
    NetworkExtensionError error = SendAARPRequest(manager, protocolAddress);

    pthread_mutex_unlock(&manager->mutex);

    if (error == kNetworkExtensionNoError) {
        return kNetworkExtensionTimeout; // Indicate async resolution
    }

    return error;
}

/*
 * Add AARP entry
 */
NetworkExtensionError RoutingManagerAddAARPEntry(RoutingManager* manager,
                                                  const AARPEntry* entry) {
    if (!manager || !manager->initialized || !entry) {
        return kNetworkExtensionInvalidParam;
    }

    pthread_mutex_lock(&manager->mutex);

    NetworkExtensionError error = AddAARPEntryInternal(manager, entry);

    pthread_mutex_unlock(&manager->mutex);

    return error;
}

/*
 * Set protocol bridging
 */
NetworkExtensionError RoutingManagerSetBridging(RoutingManager* manager,
                                                 bool enable) {
    if (!manager || !manager->initialized) {
        return kNetworkExtensionInvalidParam;
    }

    pthread_mutex_lock(&manager->mutex);
    manager->bridgingEnabled = enable;
    pthread_mutex_unlock(&manager->mutex);

    return kNetworkExtensionNoError;
}

/*
 * Get routing statistics
 */
NetworkExtensionError RoutingManagerGetStatistics(RoutingManager* manager,
                                                   RoutingStatistics* stats) {
    if (!manager || !manager->initialized || !stats) {
        return kNetworkExtensionInvalidParam;
    }

    pthread_mutex_lock(&manager->mutex);

    *stats = manager->statistics;
    stats->totalRoutes = manager->routeCount;
    stats->activeRoutes = 0;
    stats->staticRoutes = 0;

    // Count active and static routes
    for (int i = 0; i < manager->routeCount; i++) {
        if (manager->routes[i].flags & kRouteFlagActive) {
            stats->activeRoutes++;
        }
        if (manager->routes[i].flags & kRouteFlagStatic) {
            stats->staticRoutes++;
        }
    }

    pthread_mutex_unlock(&manager->mutex);

    return kNetworkExtensionNoError;
}

/*
 * RTMP receive handler
 */
static void RTMPReceiveHandler(const DDPPacket* packet, void* userData) {
    RoutingManager* manager = (RoutingManager*)userData;

    if (!manager || !packet || packet->type != kDDPTypeRTMP) {
        return;
    }

    ProcessRTMPPacket(manager, packet);
}

/*
 * AARP receive handler
 */
static void AARPReceiveHandler(const DDPPacket* packet, void* userData) {
    RoutingManager* manager = (RoutingManager*)userData;

    if (!manager || !packet) {
        return;
    }

    ProcessAARPPacket(manager, packet);
}

/*
 * Process RTMP packet
 */
static NetworkExtensionError ProcessRTMPPacket(RoutingManager* manager,
                                                const DDPPacket* packet) {
    if (packet->length < sizeof(RTMPHeader)) {
        return kNetworkExtensionInvalidParam;
    }

    const RTMPHeader* header = (const RTMPHeader*)packet->data;

    pthread_mutex_lock(&manager->mutex);

    // Process routing table entries
    const uint8_t* data = packet->data + sizeof(RTMPHeader);
    size_t remaining = packet->length - sizeof(RTMPHeader);

    while (remaining >= 3) {
        uint16_t network = (data[0] << 8) | data[1];
        uint8_t hops = data[2];

        if (network != 0 && hops < ROUTING_MAX_HOPS) {
            RouteEntry route;
            memset(&route, 0, sizeof(route));

            route.networkStart = network;
            route.networkEnd = network;
            route.nextHop = packet->sourceNode;
            route.hops = hops + 1;
            route.type = kRouteTypeRouter;
            route.flags = kRouteFlagActive;
            route.timestamp = time(NULL);
            route.metric = CalculateRouteMetric(&route);

            AddRouteInternal(manager, &route);
        }

        data += 3;
        remaining -= 3;
    }

    manager->statistics.packetsRouted++;

    pthread_mutex_unlock(&manager->mutex);

    return kNetworkExtensionNoError;
}

/*
 * Process AARP packet
 */
static NetworkExtensionError ProcessAARPPacket(RoutingManager* manager,
                                                const DDPPacket* packet) {
    if (packet->length < sizeof(AARPPacket)) {
        return kNetworkExtensionInvalidParam;
    }

    const AARPPacket* aarpPacket = (const AARPPacket*)packet->data;

    pthread_mutex_lock(&manager->mutex);

    switch (aarpPacket->operation) {
        case 1: // AARP Request
            manager->statistics.aarpRequests++;

            // Check if we are the target
            AppleTalkAddress localAddress;
            AppleTalkStackGetLocalAddress(manager->appleTalkStack, &localAddress);

            if (AppleTalkAddressEqual(&aarpPacket->targetProtocolAddress, &localAddress)) {
                // Send AARP reply
                AARPEntry entry;
                memset(&entry, 0, sizeof(entry));
                entry.protocolAddress = aarpPacket->senderProtocolAddress;
                memcpy(entry.hardwareAddress, aarpPacket->senderHardwareAddress, 6);
                entry.hardwareAddressSize = 6;
                entry.state = kAARPStateReachable;
                entry.timestamp = time(NULL);

                SendAARPReply(manager, &entry, &aarpPacket->senderProtocolAddress);
            }
            break;

        case 2: // AARP Reply
            manager->statistics.aarpReplies++;

            // Update AARP table
            AARPEntry entry;
            memset(&entry, 0, sizeof(entry));
            entry.protocolAddress = aarpPacket->senderProtocolAddress;
            memcpy(entry.hardwareAddress, aarpPacket->senderHardwareAddress, 6);
            entry.hardwareAddressSize = 6;
            entry.state = kAARPStateReachable;
            entry.timestamp = time(NULL);

            AddAARPEntryInternal(manager, &entry);
            break;
    }

    pthread_mutex_unlock(&manager->mutex);

    return kNetworkExtensionNoError;
}

/*
 * Send AARP request
 */
static NetworkExtensionError SendAARPRequest(RoutingManager* manager,
                                              const AppleTalkAddress* address) {
    AARPPacket packet;
    memset(&packet, 0, sizeof(packet));

    packet.hardwareType = 1; // Ethernet
    packet.protocolType = 0x809B; // AppleTalk
    packet.hardwareLength = 6;
    packet.protocolLength = 4;
    packet.operation = 1; // Request

    // Fill in sender information
    AppleTalkAddress localAddress;
    AppleTalkStackGetLocalAddress(manager->appleTalkStack, &localAddress);
    packet.senderProtocolAddress = localAddress;

    // Target
    packet.targetProtocolAddress = *address;

    // Send as broadcast
    AppleTalkAddress broadcast = {0, 255, 0};
    return DDPSendPacket(manager->aarpSocket, &broadcast, kDDPTypeRTMP, &packet, sizeof(packet));
}

/*
 * Find route
 */
static RouteEntry* FindRoute(RoutingManager* manager, uint16_t network) {
    for (int i = 0; i < manager->routeCount; i++) {
        if (network >= manager->routes[i].networkStart &&
            network <= manager->routes[i].networkEnd &&
            (manager->routes[i].flags & kRouteFlagActive)) {
            return &manager->routes[i];
        }
    }
    return NULL;
}

/*
 * Find best route
 */
static RouteEntry* FindBestRoute(RoutingManager* manager, uint16_t network) {
    RouteEntry* bestRoute = NULL;
    uint16_t bestMetric = UINT16_MAX;

    for (int i = 0; i < manager->routeCount; i++) {
        if (network >= manager->routes[i].networkStart &&
            network <= manager->routes[i].networkEnd &&
            (manager->routes[i].flags & kRouteFlagActive)) {

            if (manager->routes[i].metric < bestMetric) {
                bestMetric = manager->routes[i].metric;
                bestRoute = &manager->routes[i];
            }
        }
    }

    return bestRoute;
}

/*
 * Add route internally
 */
static NetworkExtensionError AddRouteInternal(RoutingManager* manager,
                                               const RouteEntry* route) {
    // Check if route already exists
    RouteEntry* existing = FindRoute(manager, route->networkStart);
    if (existing) {
        // Update existing route if this one is better
        if (route->metric < existing->metric || (route->flags & kRouteFlagStatic)) {
            *existing = *route;
            existing->timestamp = time(NULL);

            if (manager->routeUpdateCallback) {
                manager->routeUpdateCallback(existing, false, manager->routeUpdateUserData);
            }
        }
        return kNetworkExtensionNoError;
    }

    // Add new route
    if (manager->routeCount >= manager->maxRoutes) {
        return kNetworkExtensionInternalError;
    }

    manager->routes[manager->routeCount] = *route;
    manager->routes[manager->routeCount].timestamp = time(NULL);
    manager->routes[manager->routeCount].lastUsed = time(NULL);
    manager->routes[manager->routeCount].useCount = 0;
    manager->routeCount++;

    if (manager->routeUpdateCallback) {
        manager->routeUpdateCallback(&manager->routes[manager->routeCount - 1], true, manager->routeUpdateUserData);
    }

    return kNetworkExtensionNoError;
}

/*
 * Find AARP entry
 */
static AARPEntry* FindAARPEntry(RoutingManager* manager,
                                 const AppleTalkAddress* address) {
    for (int i = 0; i < manager->aarpEntryCount; i++) {
        if (AppleTalkAddressEqual(&manager->aarpTable[i].protocolAddress, address)) {
            return &manager->aarpTable[i];
        }
    }
    return NULL;
}

/*
 * Add AARP entry internally
 */
static NetworkExtensionError AddAARPEntryInternal(RoutingManager* manager,
                                                   const AARPEntry* entry) {
    // Check if entry already exists
    AARPEntry* existing = FindAARPEntry(manager, &entry->protocolAddress);
    if (existing) {
        // Update existing entry
        *existing = *entry;
        existing->timestamp = time(NULL);

        if (manager->aarpUpdateCallback) {
            manager->aarpUpdateCallback(existing, true, manager->aarpUpdateUserData);
        }

        return kNetworkExtensionNoError;
    }

    // Add new entry
    if (manager->aarpEntryCount >= AARP_MAX_ENTRIES) {
        return kNetworkExtensionInternalError;
    }

    manager->aarpTable[manager->aarpEntryCount] = *entry;
    manager->aarpTable[manager->aarpEntryCount].timestamp = time(NULL);
    manager->aarpEntryCount++;

    if (manager->aarpUpdateCallback) {
        manager->aarpUpdateCallback(&manager->aarpTable[manager->aarpEntryCount - 1], true, manager->aarpUpdateUserData);
    }

    return kNetworkExtensionNoError;
}

/*
 * Maintenance thread
 */
static void* MaintenanceThread(void* arg) {
    RoutingManager* manager = (RoutingManager*)arg;
    time_t lastRTMPUpdate = 0;

    while (manager->maintenanceRunning) {
        pthread_mutex_lock(&manager->mutex);

        // Remove expired routes and AARP entries
        RemoveExpiredRoutes(manager);
        RemoveExpiredAARPEntries(manager);

        // Probe stale AARP entries
        ProbeStaleAARPEntries(manager);

        // Send RTMP updates periodically
        time_t now = time(NULL);
        if (now - lastRTMPUpdate > RTMP_UPDATE_INTERVAL) {
            lastRTMPUpdate = now;
            pthread_mutex_unlock(&manager->mutex);
            SendRTMPUpdate(manager);
            pthread_mutex_lock(&manager->mutex);
        }

        pthread_mutex_unlock(&manager->mutex);

        // Sleep for a while
        sleep(1);
    }

    return NULL;
}

/*
 * Remove expired routes
 */
static void RemoveExpiredRoutes(RoutingManager* manager) {
    time_t now = time(NULL);

    for (int i = 0; i < manager->routeCount; i++) {
        if (!(manager->routes[i].flags & kRouteFlagStatic) &&
            (now - manager->routes[i].timestamp > manager->routeTimeout)) {

            if (manager->routeUpdateCallback) {
                manager->routeUpdateCallback(&manager->routes[i], false, manager->routeUpdateUserData);
            }

            // Remove route
            memmove(&manager->routes[i], &manager->routes[i + 1],
                    (manager->routeCount - i - 1) * sizeof(RouteEntry));
            manager->routeCount--;
            i--; // Check this index again
        }
    }
}

/*
 * Remove expired AARP entries
 */
static void RemoveExpiredAARPEntries(RoutingManager* manager) {
    time_t now = time(NULL);

    for (int i = 0; i < manager->aarpEntryCount; i++) {
        if (now - manager->aarpTable[i].timestamp > manager->aarpTimeout) {
            if (manager->aarpUpdateCallback) {
                manager->aarpUpdateCallback(&manager->aarpTable[i], false, manager->aarpUpdateUserData);
            }

            // Remove entry
            memmove(&manager->aarpTable[i], &manager->aarpTable[i + 1],
                    (manager->aarpEntryCount - i - 1) * sizeof(AARPEntry));
            manager->aarpEntryCount--;
            i--; // Check this index again
        }
    }
}

/*
 * Probe stale AARP entries
 */
static void ProbeStaleAARPEntries(RoutingManager* manager) {
    time_t now = time(NULL);

    for (int i = 0; i < manager->aarpEntryCount; i++) {
        AARPEntry* entry = &manager->aarpTable[i];

        if (entry->state == kAARPStateReachable &&
            now - entry->lastProbe > AARP_PROBE_INTERVAL) {

            if (entry->probeCount < MAX_PROBE_COUNT) {
                entry->state = kAARPStateProbe;
                entry->lastProbe = now;
                entry->probeCount++;

                // Send probe
                SendAARPRequest(manager, &entry->protocolAddress);
            } else {
                entry->state = kAARPStateStale;
            }
        }
    }
}

/*
 * Send RTMP update
 */
static NetworkExtensionError SendRTMPUpdate(RoutingManager* manager) {
    // TODO: Implement RTMP update transmission
    return kNetworkExtensionNoError;
}

/*
 * Send AARP reply
 */
static NetworkExtensionError SendAARPReply(RoutingManager* manager,
                                            const AARPEntry* entry,
                                            const AppleTalkAddress* requester) {
    // TODO: Implement AARP reply transmission
    return kNetworkExtensionNoError;
}

/*
 * Calculate route metric
 */
static uint16_t CalculateRouteMetric(const RouteEntry* route) {
    uint16_t metric = route->hops;

    // Adjust for route type
    switch (route->type) {
        case kRouteTypeDirect:
            metric = 1;
            break;
        case kRouteTypeStatic:
            metric = route->hops;
            break;
        case kRouteTypeDefault:
            metric = 100;
            break;
        default:
            break;
    }

    return metric;
}

/*
 * Utility Functions
 */

bool RoutingManagerValidateRoute(const RouteEntry* route) {
    if (!route) {
        return false;
    }

    if (route->networkStart > route->networkEnd) {
        return false;
    }

    if (route->hops > ROUTING_MAX_HOPS) {
        return false;
    }

    return true;
}

const char* RoutingManagerGetRouteTypeString(RouteType type) {
    switch (type) {
        case kRouteTypeDirect:
            return "Direct";
        case kRouteTypeRouter:
            return "Router";
        case kRouteTypeStatic:
            return "Static";
        case kRouteTypeDefault:
            return "Default";
        default:
            return "Unknown";
    }
}

const char* RoutingManagerGetAARPStateString(AARPState state) {
    switch (state) {
        case kAARPStateIncomplete:
            return "Incomplete";
        case kAARPStateReachable:
            return "Reachable";
        case kAARPStateStale:
            return "Stale";
        case kAARPStateProbe:
            return "Probe";
        default:
            return "Unknown";
    }
}