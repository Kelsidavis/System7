/*
 * ZoneManager.c - AppleTalk Zone Management and Network Browsing
 * Mac OS 7.1 Network Extension for zone management
 *
 * This module implements AppleTalk zone management including zone discovery,
 * network browsing, service resolution, and modern protocol integration.
 */

#include "ZoneManager.h"
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>

/*
 * Internal Constants
 */
#define ZONE_REFRESH_INTERVAL 30
#define ZONE_TIMEOUT 300
#define ZONE_QUERY_TIMEOUT 10
#define MAX_CONCURRENT_QUERIES 16

/*
 * Zone Manager Structure
 */
struct ZoneManager {
    // State
    bool initialized;
    bool started;
    pthread_mutex_t mutex;

    // AppleTalk integration
    AppleTalkStack* appleTalkStack;
    DDPSocket* zipSocket;

    // Zone database
    ExtendedZoneInfo zones[ZONE_MAX_ZONES];
    int zoneCount;
    char localZone[ZONE_MAX_NAME_LENGTH];

    // Callbacks
    ZoneUpdateCallback updateCallback;
    void* updateUserData;

    // Query management
    struct ZoneQuery {
        ZoneQueryCallback callback;
        void* userData;
        time_t timestamp;
        bool active;
    } queries[MAX_CONCURRENT_QUERIES];
    int activeQueries;

    // Modern integration
    bool bonjourEnabled;
    bool filteringEnabled;

    // Threading
    pthread_t workerThread;
    bool workerRunning;

    // Statistics
    time_t lastRefresh;
    int totalQueries;
    int successfulQueries;
};

/*
 * Private function declarations
 */
static void ZIPReceiveHandler(const DDPPacket* packet, void* userData);
static NetworkExtensionError SendZIPQuery(ZoneManager* manager, uint8_t command, const void* data, size_t dataLength);
static NetworkExtensionError ProcessZIPReply(ZoneManager* manager, const DDPPacket* packet);
static NetworkExtensionError HandleGetZoneList(ZoneManager* manager, const void* data, size_t dataLength);
static NetworkExtensionError HandleGetNetInfo(ZoneManager* manager, const void* data, size_t dataLength);
static ExtendedZoneInfo* FindZone(ZoneManager* manager, const char* zoneName);
static NetworkExtensionError AddZoneInternal(ZoneManager* manager, const ExtendedZoneInfo* zone);
static void RemoveExpiredZones(ZoneManager* manager);
static void* WorkerThread(void* arg);
static NetworkExtensionError RefreshZoneList(ZoneManager* manager);
static bool MatchZoneQuery(const ExtendedZoneInfo* zone, const ZoneQuery* query);

/*
 * ZIP Command Codes
 */
enum {
    kZIPQuery = 1,
    kZIPReply = 2,
    kZIPGetNetInfo = 5,
    kZIPGetNetInfoReply = 6,
    kZIPGetZoneList = 8,
    kZIPGetLocalZones = 9
};

/*
 * Create zone manager
 */
NetworkExtensionError ZoneManagerCreate(ZoneManager** manager,
                                         AppleTalkStack* appleTalkStack) {
    if (!manager || !appleTalkStack) {
        return kNetworkExtensionInvalidParam;
    }

    ZoneManager* newManager = calloc(1, sizeof(ZoneManager));
    if (!newManager) {
        return kNetworkExtensionInternalError;
    }

    // Initialize mutex
    if (pthread_mutex_init(&newManager->mutex, NULL) != 0) {
        free(newManager);
        return kNetworkExtensionInternalError;
    }

    newManager->appleTalkStack = appleTalkStack;
    strcpy(newManager->localZone, "*");
    newManager->initialized = true;

    *manager = newManager;
    return kNetworkExtensionNoError;
}

/*
 * Destroy zone manager
 */
void ZoneManagerDestroy(ZoneManager* manager) {
    if (!manager || !manager->initialized) {
        return;
    }

    // Stop if running
    if (manager->started) {
        ZoneManagerStop(manager);
    }

    pthread_mutex_lock(&manager->mutex);
    manager->initialized = false;
    pthread_mutex_unlock(&manager->mutex);

    pthread_mutex_destroy(&manager->mutex);
    free(manager);
}

/*
 * Start zone manager
 */
NetworkExtensionError ZoneManagerStart(ZoneManager* manager) {
    if (!manager || !manager->initialized) {
        return kNetworkExtensionInvalidParam;
    }

    pthread_mutex_lock(&manager->mutex);

    if (manager->started) {
        pthread_mutex_unlock(&manager->mutex);
        return kNetworkExtensionNoError;
    }

    // Open ZIP socket
    NetworkExtensionError error = DDPOpenSocket(manager->appleTalkStack,
                                                 6, // ZIP socket
                                                 ZIPReceiveHandler,
                                                 manager,
                                                 &manager->zipSocket);
    if (error != kNetworkExtensionNoError) {
        pthread_mutex_unlock(&manager->mutex);
        return error;
    }

    // Start worker thread
    manager->workerRunning = true;
    if (pthread_create(&manager->workerThread, NULL, WorkerThread, manager) != 0) {
        manager->workerRunning = false;
        DDPCloseSocket(manager->zipSocket);
        manager->zipSocket = NULL;
        pthread_mutex_unlock(&manager->mutex);
        return kNetworkExtensionInternalError;
    }

    manager->started = true;
    pthread_mutex_unlock(&manager->mutex);

    // Initial zone refresh
    ZoneManagerRefreshZones(manager);

    return kNetworkExtensionNoError;
}

/*
 * Stop zone manager
 */
void ZoneManagerStop(ZoneManager* manager) {
    if (!manager || !manager->initialized || !manager->started) {
        return;
    }

    pthread_mutex_lock(&manager->mutex);

    // Stop worker thread
    manager->workerRunning = false;
    pthread_mutex_unlock(&manager->mutex);

    // Wait for worker to finish
    pthread_join(manager->workerThread, NULL);

    pthread_mutex_lock(&manager->mutex);

    // Close ZIP socket
    if (manager->zipSocket) {
        DDPCloseSocket(manager->zipSocket);
        manager->zipSocket = NULL;
    }

    manager->started = false;
    pthread_mutex_unlock(&manager->mutex);
}

/*
 * Set local zone
 */
NetworkExtensionError ZoneManagerSetLocalZone(ZoneManager* manager,
                                               const char* zoneName) {
    if (!manager || !manager->initialized || !zoneName) {
        return kNetworkExtensionInvalidParam;
    }

    if (!ZoneManagerValidateZoneName(zoneName)) {
        return kNetworkExtensionInvalidParam;
    }

    pthread_mutex_lock(&manager->mutex);

    strncpy(manager->localZone, zoneName, sizeof(manager->localZone) - 1);
    manager->localZone[sizeof(manager->localZone) - 1] = '\0';

    pthread_mutex_unlock(&manager->mutex);

    return kNetworkExtensionNoError;
}

/*
 * Get local zone
 */
NetworkExtensionError ZoneManagerGetLocalZone(ZoneManager* manager,
                                               char* zoneName,
                                               size_t bufferSize) {
    if (!manager || !manager->initialized || !zoneName || bufferSize == 0) {
        return kNetworkExtensionInvalidParam;
    }

    pthread_mutex_lock(&manager->mutex);

    if (strlen(manager->localZone) >= bufferSize) {
        pthread_mutex_unlock(&manager->mutex);
        return kNetworkExtensionBufferTooSmall;
    }

    strcpy(zoneName, manager->localZone);

    pthread_mutex_unlock(&manager->mutex);

    return kNetworkExtensionNoError;
}

/*
 * Get zone list
 */
NetworkExtensionError ZoneManagerGetZoneList(ZoneManager* manager,
                                              ZoneQueryCallback callback,
                                              void* userData) {
    if (!manager || !manager->initialized || !callback) {
        return kNetworkExtensionInvalidParam;
    }

    pthread_mutex_lock(&manager->mutex);

    // Copy current zone list
    ExtendedZoneInfo* zones = malloc(manager->zoneCount * sizeof(ExtendedZoneInfo));
    if (!zones) {
        pthread_mutex_unlock(&manager->mutex);
        return kNetworkExtensionInternalError;
    }

    memcpy(zones, manager->zones, manager->zoneCount * sizeof(ExtendedZoneInfo));
    int count = manager->zoneCount;

    pthread_mutex_unlock(&manager->mutex);

    // Call callback
    callback(zones, count, kNetworkExtensionNoError, userData);

    free(zones);
    return kNetworkExtensionNoError;
}

/*
 * Query zones
 */
NetworkExtensionError ZoneManagerQueryZones(ZoneManager* manager,
                                             const ZoneQuery* query,
                                             ZoneQueryCallback callback,
                                             void* userData) {
    if (!manager || !manager->initialized || !query || !callback) {
        return kNetworkExtensionInvalidParam;
    }

    pthread_mutex_lock(&manager->mutex);

    // Filter zones based on query
    ExtendedZoneInfo results[ZONE_MAX_ZONES];
    int resultCount = 0;

    for (int i = 0; i < manager->zoneCount && resultCount < ZONE_MAX_ZONES; i++) {
        if (MatchZoneQuery(&manager->zones[i], query)) {
            results[resultCount++] = manager->zones[i];
        }
    }

    pthread_mutex_unlock(&manager->mutex);

    // Call callback with results
    callback(results, resultCount, kNetworkExtensionNoError, userData);

    return kNetworkExtensionNoError;
}

/*
 * Get zone information
 */
NetworkExtensionError ZoneManagerGetZoneInfo(ZoneManager* manager,
                                              const char* zoneName,
                                              ExtendedZoneInfo* info) {
    if (!manager || !manager->initialized || !zoneName || !info) {
        return kNetworkExtensionInvalidParam;
    }

    pthread_mutex_lock(&manager->mutex);

    ExtendedZoneInfo* zone = FindZone(manager, zoneName);
    if (!zone) {
        pthread_mutex_unlock(&manager->mutex);
        return kNetworkExtensionNotFound;
    }

    *info = *zone;

    pthread_mutex_unlock(&manager->mutex);

    return kNetworkExtensionNoError;
}

/*
 * Get zone for network
 */
NetworkExtensionError ZoneManagerGetNetworkZone(ZoneManager* manager,
                                                 uint16_t network,
                                                 char* zoneName,
                                                 size_t bufferSize) {
    if (!manager || !manager->initialized || !zoneName || bufferSize == 0) {
        return kNetworkExtensionInvalidParam;
    }

    pthread_mutex_lock(&manager->mutex);

    // Search for zone containing this network
    for (int i = 0; i < manager->zoneCount; i++) {
        if (ZoneManagerNetworkInZone(&manager->zones[i], network)) {
            if (strlen(manager->zones[i].name) >= bufferSize) {
                pthread_mutex_unlock(&manager->mutex);
                return kNetworkExtensionBufferTooSmall;
            }

            strcpy(zoneName, manager->zones[i].name);
            pthread_mutex_unlock(&manager->mutex);
            return kNetworkExtensionNoError;
        }
    }

    pthread_mutex_unlock(&manager->mutex);
    return kNetworkExtensionNotFound;
}

/*
 * Refresh zone list
 */
NetworkExtensionError ZoneManagerRefreshZones(ZoneManager* manager) {
    if (!manager || !manager->initialized || !manager->started) {
        return kNetworkExtensionInvalidParam;
    }

    // Send ZIP GetZoneList request
    uint8_t queryData = kZIPGetZoneList;
    return SendZIPQuery(manager, kZIPQuery, &queryData, 1);
}

/*
 * ZIP receive handler
 */
static void ZIPReceiveHandler(const DDPPacket* packet, void* userData) {
    ZoneManager* manager = (ZoneManager*)userData;

    if (!manager || !packet || packet->type != kDDPTypeZIP) {
        return;
    }

    ProcessZIPReply(manager, packet);
}

/*
 * Send ZIP query
 */
static NetworkExtensionError SendZIPQuery(ZoneManager* manager,
                                           uint8_t command,
                                           const void* data,
                                           size_t dataLength) {
    if (!manager->zipSocket) {
        return kNetworkExtensionNetworkDown;
    }

    // Build ZIP packet
    uint8_t packet[APPLETALK_MAX_PACKET_SIZE];
    packet[0] = command;

    if (data && dataLength > 0) {
        memcpy(packet + 1, data, dataLength);
    }

    // Send to Zone Information Socket
    AppleTalkAddress destination = {0, 0, 6}; // Broadcast to ZIS
    return DDPSendPacket(manager->zipSocket, &destination, kDDPTypeZIP, packet, 1 + dataLength);
}

/*
 * Process ZIP reply
 */
static NetworkExtensionError ProcessZIPReply(ZoneManager* manager,
                                             const DDPPacket* packet) {
    if (packet->length < 1) {
        return kNetworkExtensionInvalidParam;
    }

    uint8_t command = packet->data[0];

    switch (command) {
        case kZIPReply:
            return HandleGetZoneList(manager, packet->data + 1, packet->length - 1);

        case kZIPGetNetInfoReply:
            return HandleGetNetInfo(manager, packet->data + 1, packet->length - 1);

        default:
            return kNetworkExtensionNotSupported;
    }
}

/*
 * Handle zone list reply
 */
static NetworkExtensionError HandleGetZoneList(ZoneManager* manager,
                                                const void* data,
                                                size_t dataLength) {
    const uint8_t* ptr = (const uint8_t*)data;
    const uint8_t* end = ptr + dataLength;

    pthread_mutex_lock(&manager->mutex);

    while (ptr < end) {
        // Parse zone name
        if (ptr >= end) break;

        uint8_t zoneLength = *ptr++;
        if (ptr + zoneLength > end || zoneLength == 0 || zoneLength >= ZONE_MAX_NAME_LENGTH) {
            break;
        }

        // Create zone info
        ExtendedZoneInfo zone;
        memset(&zone, 0, sizeof(zone));

        memcpy(zone.name, ptr, zoneLength);
        zone.name[zoneLength] = '\0';
        ptr += zoneLength;

        zone.flags = kZoneFlagAvailable;
        zone.lastSeen = time(NULL);
        zone.hopCount = 1;

        // Add or update zone
        AddZoneInternal(manager, &zone);
    }

    manager->lastRefresh = time(NULL);
    pthread_mutex_unlock(&manager->mutex);

    return kNetworkExtensionNoError;
}

/*
 * Handle network info reply
 */
static NetworkExtensionError HandleGetNetInfo(ZoneManager* manager,
                                               const void* data,
                                               size_t dataLength) {
    // TODO: Parse GetNetInfo reply
    return kNetworkExtensionNoError;
}

/*
 * Find zone by name
 */
static ExtendedZoneInfo* FindZone(ZoneManager* manager, const char* zoneName) {
    for (int i = 0; i < manager->zoneCount; i++) {
        if (ZoneManagerCompareZoneNames(manager->zones[i].name, zoneName)) {
            return &manager->zones[i];
        }
    }
    return NULL;
}

/*
 * Add zone internally
 */
static NetworkExtensionError AddZoneInternal(ZoneManager* manager,
                                              const ExtendedZoneInfo* zone) {
    // Check if zone already exists
    ExtendedZoneInfo* existing = FindZone(manager, zone->name);
    if (existing) {
        // Update existing zone
        existing->lastSeen = time(NULL);
        existing->flags |= zone->flags;
        return kNetworkExtensionNoError;
    }

    // Add new zone
    if (manager->zoneCount >= ZONE_MAX_ZONES) {
        return kNetworkExtensionInternalError;
    }

    manager->zones[manager->zoneCount++] = *zone;

    // Notify callback if set
    if (manager->updateCallback) {
        manager->updateCallback(zone, true, manager->updateUserData);
    }

    return kNetworkExtensionNoError;
}

/*
 * Worker thread
 */
static void* WorkerThread(void* arg) {
    ZoneManager* manager = (ZoneManager*)arg;

    while (manager->workerRunning) {
        pthread_mutex_lock(&manager->mutex);

        // Remove expired zones
        RemoveExpiredZones(manager);

        // Refresh zones periodically
        time_t now = time(NULL);
        if (now - manager->lastRefresh > ZONE_REFRESH_INTERVAL) {
            pthread_mutex_unlock(&manager->mutex);
            RefreshZoneList(manager);
            pthread_mutex_lock(&manager->mutex);
        }

        pthread_mutex_unlock(&manager->mutex);

        // Sleep for a while
        sleep(5);
    }

    return NULL;
}

/*
 * Remove expired zones
 */
static void RemoveExpiredZones(ZoneManager* manager) {
    time_t now = time(NULL);

    for (int i = 0; i < manager->zoneCount; i++) {
        if (now - manager->zones[i].lastSeen > ZONE_TIMEOUT) {
            // Notify callback
            if (manager->updateCallback) {
                manager->updateCallback(&manager->zones[i], false, manager->updateUserData);
            }

            // Remove zone by shifting array
            memmove(&manager->zones[i], &manager->zones[i + 1],
                    (manager->zoneCount - i - 1) * sizeof(ExtendedZoneInfo));
            manager->zoneCount--;
            i--; // Check this index again
        }
    }
}

/*
 * Refresh zone list
 */
static NetworkExtensionError RefreshZoneList(ZoneManager* manager) {
    return ZoneManagerRefreshZones(manager);
}

/*
 * Match zone query
 */
static bool MatchZoneQuery(const ExtendedZoneInfo* zone, const ZoneQuery* query) {
    // Check exact match
    if (query->exactMatch) {
        return ZoneManagerCompareZoneNames(zone->name, query->pattern);
    }

    // Check pattern match (simple substring for now)
    return strstr(zone->name, query->pattern) != NULL;
}

/*
 * Utility Functions
 */

bool ZoneManagerValidateZoneName(const char* zoneName) {
    if (!zoneName) {
        return false;
    }

    size_t len = strlen(zoneName);
    if (len == 0 || len >= ZONE_MAX_NAME_LENGTH) {
        return false;
    }

    // Check for valid characters (basic validation)
    for (size_t i = 0; i < len; i++) {
        char c = zoneName[i];
        if (c < 32 || c > 126) {
            return false;
        }
    }

    return true;
}

bool ZoneManagerCompareZoneNames(const char* zone1, const char* zone2) {
    if (!zone1 || !zone2) {
        return false;
    }

    return strcasecmp(zone1, zone2) == 0;
}

bool ZoneManagerNetworkInZone(const ExtendedZoneInfo* zone, uint16_t network) {
    if (!zone) {
        return false;
    }

    for (int i = 0; i < zone->rangeCount; i++) {
        if (network >= zone->ranges[i].startNetwork &&
            network <= zone->ranges[i].endNetwork) {
            return true;
        }
    }

    return false;
}

const char* ZoneManagerGetDefaultZoneName(void) {
    return "*";
}

/*
 * Stub implementations for remaining functions
 */

NetworkExtensionError ZoneManagerBrowseServices(ZoneManager* manager,
                                                 const char* zoneName,
                                                 const char* serviceType,
                                                 ServiceDiscoveryCallback callback,
                                                 void* userData) {
    return kNetworkExtensionNotSupported;
}

NetworkExtensionError ZoneManagerEnableBonjour(ZoneManager* manager) {
    if (!manager || !manager->initialized) {
        return kNetworkExtensionInvalidParam;
    }

    pthread_mutex_lock(&manager->mutex);
    manager->bonjourEnabled = true;
    pthread_mutex_unlock(&manager->mutex);

    return kNetworkExtensionNoError;
}