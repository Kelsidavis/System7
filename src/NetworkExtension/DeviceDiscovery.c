/*
 * DeviceDiscovery.c - Network Device Discovery and Service Advertising
 * Mac OS 7.1 Network Extension for device and service discovery
 *
 * This module implements network device discovery using AppleTalk NBP protocol,
 * service advertising, and modern protocol integration including Bonjour.
 */

#include "DeviceDiscovery.h"
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>

/*
 * Internal Constants
 */
#define DEFAULT_DISCOVERY_INTERVAL 60
#define DEFAULT_SERVICE_TIMEOUT 300
#define MAX_CONCURRENT_QUERIES 32
#define NBP_LOOKUP_RETRY_COUNT 3
#define NBP_LOOKUP_INTERVAL 8

/*
 * Device Discovery Structure
 */
struct DeviceDiscovery {
    // State
    bool initialized;
    bool started;
    pthread_mutex_t mutex;

    // AppleTalk integration
    AppleTalkStack* appleTalkStack;

    // Service database
    ServiceInfo services[DEVICE_MAX_SERVICES];
    int serviceCount;

    // Device database
    DeviceInfo devices[256];
    int deviceCount;

    // Advertised services
    ServiceInfo advertisedServices[64];
    int advertisedServiceCount;

    // Configuration
    uint32_t discoveryInterval;
    uint32_t serviceTimeout;
    bool passiveMode;

    // Modern protocol support
    bool bonjourEnabled;
    bool upnpEnabled;

    // Query management
    struct DiscoveryQueryContext {
        DiscoveryQuery query;
        DiscoveryQueryCallback callback;
        void* userData;
        time_t timestamp;
        bool active;
    } queries[MAX_CONCURRENT_QUERIES];
    int activeQueries;

    // Callbacks
    ServiceFoundCallback serviceFoundCallback;
    void* serviceFoundUserData;
    ServiceLostCallback serviceLostCallback;
    void* serviceLostUserData;
    DeviceFoundCallback deviceFoundCallback;
    void* deviceFoundUserData;

    // Threading
    pthread_t discoveryThread;
    bool discoveryRunning;

    // Statistics
    int totalQueries;
    int successfulQueries;
    time_t lastDiscovery;
};

/*
 * Global state
 */
static bool g_deviceDiscoveryGlobalInit = false;
static pthread_mutex_t g_globalMutex = PTHREAD_MUTEX_INITIALIZER;

/*
 * Private function declarations
 */
static void NBPLookupCallback(const NBPTuple* tuples, int count, NetworkExtensionError error, void* userData);
static NetworkExtensionError DiscoverServicesInternal(DeviceDiscovery* discovery, const DiscoveryQuery* query);
static ServiceInfo* FindService(DeviceDiscovery* discovery, const char* name, const char* type, const char* zone);
static NetworkExtensionError AddService(DeviceDiscovery* discovery, const ServiceInfo* service);
static NetworkExtensionError RemoveService(DeviceDiscovery* discovery, const char* name, const char* type, const char* zone);
static void RemoveExpiredServices(DeviceDiscovery* discovery);
static DeviceInfo* FindDevice(DeviceDiscovery* discovery, const AppleTalkAddress* address);
static NetworkExtensionError AddDevice(DeviceDiscovery* discovery, const DeviceInfo* device);
static void* DiscoveryThread(void* arg);
static void ProcessNBPResults(DeviceDiscovery* discovery, const NBPTuple* tuples, int count, const DiscoveryQuery* query);
static DiscoveryServiceType GuessServiceType(const char* serviceType);
static void BuildEntityName(const ServiceInfo* service, EntityName* entity);
static void ParseEntityName(const EntityName* entity, ServiceInfo* service);

/*
 * Global initialization
 */
void DeviceDiscoveryGlobalInit(void) {
    pthread_mutex_lock(&g_globalMutex);

    if (!g_deviceDiscoveryGlobalInit) {
        // Initialize global device discovery resources
        g_deviceDiscoveryGlobalInit = true;
    }

    pthread_mutex_unlock(&g_globalMutex);
}

/*
 * Create device discovery
 */
NetworkExtensionError DeviceDiscoveryCreate(DeviceDiscovery** discovery,
                                             AppleTalkStack* appleTalkStack) {
    if (!discovery || !appleTalkStack) {
        return kNetworkExtensionInvalidParam;
    }

    DeviceDiscovery* newDiscovery = calloc(1, sizeof(DeviceDiscovery));
    if (!newDiscovery) {
        return kNetworkExtensionInternalError;
    }

    // Initialize mutex
    if (pthread_mutex_init(&newDiscovery->mutex, NULL) != 0) {
        free(newDiscovery);
        return kNetworkExtensionInternalError;
    }

    newDiscovery->appleTalkStack = appleTalkStack;
    newDiscovery->discoveryInterval = DEFAULT_DISCOVERY_INTERVAL;
    newDiscovery->serviceTimeout = DEFAULT_SERVICE_TIMEOUT;
    newDiscovery->passiveMode = false;
    newDiscovery->initialized = true;

    *discovery = newDiscovery;
    return kNetworkExtensionNoError;
}

/*
 * Destroy device discovery
 */
void DeviceDiscoveryDestroy(DeviceDiscovery* discovery) {
    if (!discovery || !discovery->initialized) {
        return;
    }

    // Stop if running
    if (discovery->started) {
        DeviceDiscoveryStop(discovery);
    }

    pthread_mutex_lock(&discovery->mutex);
    discovery->initialized = false;
    pthread_mutex_unlock(&discovery->mutex);

    pthread_mutex_destroy(&discovery->mutex);
    free(discovery);
}

/*
 * Start device discovery
 */
NetworkExtensionError DeviceDiscoveryStart(DeviceDiscovery* discovery) {
    if (!discovery || !discovery->initialized) {
        return kNetworkExtensionInvalidParam;
    }

    pthread_mutex_lock(&discovery->mutex);

    if (discovery->started) {
        pthread_mutex_unlock(&discovery->mutex);
        return kNetworkExtensionNoError;
    }

    // Start discovery thread
    discovery->discoveryRunning = true;
    if (pthread_create(&discovery->discoveryThread, NULL, DiscoveryThread, discovery) != 0) {
        discovery->discoveryRunning = false;
        pthread_mutex_unlock(&discovery->mutex);
        return kNetworkExtensionInternalError;
    }

    discovery->started = true;
    pthread_mutex_unlock(&discovery->mutex);

    return kNetworkExtensionNoError;
}

/*
 * Stop device discovery
 */
void DeviceDiscoveryStop(DeviceDiscovery* discovery) {
    if (!discovery || !discovery->initialized || !discovery->started) {
        return;
    }

    pthread_mutex_lock(&discovery->mutex);

    // Stop discovery thread
    discovery->discoveryRunning = false;
    pthread_mutex_unlock(&discovery->mutex);

    // Wait for thread to finish
    pthread_join(discovery->discoveryThread, NULL);

    pthread_mutex_lock(&discovery->mutex);

    // Stop advertising all services
    for (int i = 0; i < discovery->advertisedServiceCount; i++) {
        EntityName entity;
        BuildEntityName(&discovery->advertisedServices[i], &entity);
        NBPRemoveName(discovery->appleTalkStack, &entity);
    }
    discovery->advertisedServiceCount = 0;

    discovery->started = false;
    pthread_mutex_unlock(&discovery->mutex);
}

/*
 * Browse for services
 */
NetworkExtensionError DeviceDiscoveryBrowseServices(DeviceDiscovery* discovery,
                                                     const DiscoveryQuery* query,
                                                     DiscoveryQueryCallback callback,
                                                     void* userData) {
    if (!discovery || !discovery->initialized || !query || !callback) {
        return kNetworkExtensionInvalidParam;
    }

    if (!discovery->started) {
        return kNetworkExtensionNetworkDown;
    }

    // Find available query slot
    pthread_mutex_lock(&discovery->mutex);

    int queryIndex = -1;
    for (int i = 0; i < MAX_CONCURRENT_QUERIES; i++) {
        if (!discovery->queries[i].active) {
            queryIndex = i;
            break;
        }
    }

    if (queryIndex == -1) {
        pthread_mutex_unlock(&discovery->mutex);
        return kNetworkExtensionInternalError;
    }

    // Setup query
    discovery->queries[queryIndex].query = *query;
    discovery->queries[queryIndex].callback = callback;
    discovery->queries[queryIndex].userData = userData;
    discovery->queries[queryIndex].timestamp = time(NULL);
    discovery->queries[queryIndex].active = true;
    discovery->activeQueries++;

    pthread_mutex_unlock(&discovery->mutex);

    // Start discovery
    return DiscoverServicesInternal(discovery, query);
}

/*
 * Resolve service
 */
NetworkExtensionError DeviceDiscoveryResolveService(DeviceDiscovery* discovery,
                                                     const char* serviceName,
                                                     const char* serviceType,
                                                     const char* zone,
                                                     ServiceInfo* serviceInfo) {
    if (!discovery || !discovery->initialized || !serviceName || !serviceType || !serviceInfo) {
        return kNetworkExtensionInvalidParam;
    }

    pthread_mutex_lock(&discovery->mutex);

    ServiceInfo* service = FindService(discovery, serviceName, serviceType, zone);
    if (!service) {
        pthread_mutex_unlock(&discovery->mutex);
        return kNetworkExtensionNotFound;
    }

    *serviceInfo = *service;

    pthread_mutex_unlock(&discovery->mutex);
    return kNetworkExtensionNoError;
}

/*
 * Get service list
 */
NetworkExtensionError DeviceDiscoveryGetServiceList(DeviceDiscovery* discovery,
                                                     ServiceInfo* services,
                                                     int maxServices,
                                                     int* actualServices) {
    if (!discovery || !discovery->initialized || !services || !actualServices) {
        return kNetworkExtensionInvalidParam;
    }

    pthread_mutex_lock(&discovery->mutex);

    int count = discovery->serviceCount < maxServices ? discovery->serviceCount : maxServices;
    memcpy(services, discovery->services, count * sizeof(ServiceInfo));
    *actualServices = count;

    pthread_mutex_unlock(&discovery->mutex);
    return kNetworkExtensionNoError;
}

/*
 * Advertise service
 */
NetworkExtensionError DeviceDiscoveryAdvertiseService(DeviceDiscovery* discovery,
                                                       const ServiceInfo* service) {
    if (!discovery || !discovery->initialized || !service) {
        return kNetworkExtensionInvalidParam;
    }

    if (!DeviceDiscoveryValidateServiceName(service->name) ||
        !DeviceDiscoveryValidateServiceType(service->type)) {
        return kNetworkExtensionInvalidParam;
    }

    pthread_mutex_lock(&discovery->mutex);

    // Check if already advertising
    for (int i = 0; i < discovery->advertisedServiceCount; i++) {
        if (DeviceDiscoveryCompareServiceNames(discovery->advertisedServices[i].name, service->name)) {
            pthread_mutex_unlock(&discovery->mutex);
            return kNetworkExtensionInternalError;
        }
    }

    // Add to advertised services
    if (discovery->advertisedServiceCount >= 64) {
        pthread_mutex_unlock(&discovery->mutex);
        return kNetworkExtensionInternalError;
    }

    discovery->advertisedServices[discovery->advertisedServiceCount] = *service;
    discovery->advertisedServices[discovery->advertisedServiceCount].flags |= kServiceFlagAdvertised;
    discovery->advertisedServices[discovery->advertisedServiceCount].advertisedSince = time(NULL);
    discovery->advertisedServiceCount++;

    // Register with NBP
    EntityName entity;
    BuildEntityName(service, &entity);

    NetworkExtensionError error = NBPRegisterName(discovery->appleTalkStack, &entity, service->address.socket);

    pthread_mutex_unlock(&discovery->mutex);

    return error;
}

/*
 * Stop advertising service
 */
NetworkExtensionError DeviceDiscoveryStopAdvertising(DeviceDiscovery* discovery,
                                                      const char* serviceName) {
    if (!discovery || !discovery->initialized || !serviceName) {
        return kNetworkExtensionInvalidParam;
    }

    pthread_mutex_lock(&discovery->mutex);

    // Find and remove from advertised services
    for (int i = 0; i < discovery->advertisedServiceCount; i++) {
        if (DeviceDiscoveryCompareServiceNames(discovery->advertisedServices[i].name, serviceName)) {
            // Unregister from NBP
            EntityName entity;
            BuildEntityName(&discovery->advertisedServices[i], &entity);
            NBPRemoveName(discovery->appleTalkStack, &entity);

            // Remove from array
            memmove(&discovery->advertisedServices[i], &discovery->advertisedServices[i + 1],
                    (discovery->advertisedServiceCount - i - 1) * sizeof(ServiceInfo));
            discovery->advertisedServiceCount--;

            pthread_mutex_unlock(&discovery->mutex);
            return kNetworkExtensionNoError;
        }
    }

    pthread_mutex_unlock(&discovery->mutex);
    return kNetworkExtensionNotFound;
}

/*
 * Enable Bonjour integration
 */
NetworkExtensionError DeviceDiscoveryEnableBonjour(DeviceDiscovery* discovery) {
    if (!discovery || !discovery->initialized) {
        return kNetworkExtensionInvalidParam;
    }

    pthread_mutex_lock(&discovery->mutex);
    discovery->bonjourEnabled = true;
    pthread_mutex_unlock(&discovery->mutex);

    return kNetworkExtensionNoError;
}

/*
 * Disable Bonjour integration
 */
void DeviceDiscoveryDisableBonjour(DeviceDiscovery* discovery) {
    if (!discovery || !discovery->initialized) {
        return;
    }

    pthread_mutex_lock(&discovery->mutex);
    discovery->bonjourEnabled = false;
    pthread_mutex_unlock(&discovery->mutex);
}

/*
 * Discovery thread
 */
static void* DiscoveryThread(void* arg) {
    DeviceDiscovery* discovery = (DeviceDiscovery*)arg;

    while (discovery->discoveryRunning) {
        pthread_mutex_lock(&discovery->mutex);

        // Remove expired services
        RemoveExpiredServices(discovery);

        // Perform periodic discovery if not in passive mode
        if (!discovery->passiveMode) {
            time_t now = time(NULL);
            if (now - discovery->lastDiscovery > discovery->discoveryInterval) {
                discovery->lastDiscovery = now;

                // Discover common service types
                DiscoveryQuery query;
                memset(&query, 0, sizeof(query));
                strcpy(query.typePattern, "AFPServer");
                strcpy(query.zonePattern, "*");
                query.exactMatch = false;
                query.localOnly = false;
                query.includeUnavailable = false;
                query.timeout = 10;

                pthread_mutex_unlock(&discovery->mutex);
                DiscoverServicesInternal(discovery, &query);
                pthread_mutex_lock(&discovery->mutex);
            }
        }

        pthread_mutex_unlock(&discovery->mutex);

        // Sleep for a while
        sleep(5);
    }

    return NULL;
}

/*
 * Discover services internally
 */
static NetworkExtensionError DiscoverServicesInternal(DeviceDiscovery* discovery,
                                                       const DiscoveryQuery* query) {
    // Build entity name for NBP lookup
    EntityName entity;
    strcpy(entity.object, query->namePattern);
    strcpy(entity.type, query->typePattern);
    strcpy(entity.zone, query->zonePattern);

    // Perform NBP lookup
    return NBPLookupName(discovery->appleTalkStack, &entity, NBPLookupCallback, discovery);
}

/*
 * NBP lookup callback
 */
static void NBPLookupCallback(const NBPTuple* tuples, int count, NetworkExtensionError error, void* userData) {
    DeviceDiscovery* discovery = (DeviceDiscovery*)userData;

    if (error != kNetworkExtensionNoError || !tuples || count == 0) {
        return;
    }

    pthread_mutex_lock(&discovery->mutex);

    // Process results
    for (int i = 0; i < count; i++) {
        ServiceInfo service;
        memset(&service, 0, sizeof(service));

        ParseEntityName(&tuples[i].entity, &service);
        service.address = tuples[i].address;
        service.lastSeen = time(NULL);
        service.status = kServiceStatusAvailable;
        service.serviceType = GuessServiceType(service.type);

        AddService(discovery, &service);
    }

    discovery->totalQueries++;
    discovery->successfulQueries++;

    pthread_mutex_unlock(&discovery->mutex);
}

/*
 * Find service
 */
static ServiceInfo* FindService(DeviceDiscovery* discovery, const char* name, const char* type, const char* zone) {
    for (int i = 0; i < discovery->serviceCount; i++) {
        if (DeviceDiscoveryCompareServiceNames(discovery->services[i].name, name) &&
            strcmp(discovery->services[i].type, type) == 0 &&
            strcmp(discovery->services[i].zone, zone) == 0) {
            return &discovery->services[i];
        }
    }
    return NULL;
}

/*
 * Add service
 */
static NetworkExtensionError AddService(DeviceDiscovery* discovery, const ServiceInfo* service) {
    // Check if service already exists
    ServiceInfo* existing = FindService(discovery, service->name, service->type, service->zone);
    if (existing) {
        // Update existing service
        existing->lastSeen = time(NULL);
        existing->status = service->status;
        existing->address = service->address;
        return kNetworkExtensionNoError;
    }

    // Add new service
    if (discovery->serviceCount >= DEVICE_MAX_SERVICES) {
        return kNetworkExtensionInternalError;
    }

    discovery->services[discovery->serviceCount++] = *service;

    // Notify callback
    if (discovery->serviceFoundCallback) {
        discovery->serviceFoundCallback(service, discovery->serviceFoundUserData);
    }

    return kNetworkExtensionNoError;
}

/*
 * Remove expired services
 */
static void RemoveExpiredServices(DeviceDiscovery* discovery) {
    time_t now = time(NULL);

    for (int i = 0; i < discovery->serviceCount; i++) {
        if (now - discovery->services[i].lastSeen > discovery->serviceTimeout) {
            // Notify callback
            if (discovery->serviceLostCallback) {
                discovery->serviceLostCallback(&discovery->services[i], discovery->serviceLostUserData);
            }

            // Remove service
            memmove(&discovery->services[i], &discovery->services[i + 1],
                    (discovery->serviceCount - i - 1) * sizeof(ServiceInfo));
            discovery->serviceCount--;
            i--; // Check this index again
        }
    }
}

/*
 * Guess service type from type string
 */
static DiscoveryServiceType GuessServiceType(const char* serviceType) {
    if (strstr(serviceType, "AFP") || strstr(serviceType, "File")) {
        return kServiceTypeFileServer;
    } else if (strstr(serviceType, "Print")) {
        return kServiceTypePrintServer;
    } else if (strstr(serviceType, "Mail")) {
        return kServiceTypeMailServer;
    } else if (strstr(serviceType, "Web") || strstr(serviceType, "HTTP")) {
        return kServiceTypeWebServer;
    } else if (strstr(serviceType, "FTP")) {
        return kServiceTypeFTPServer;
    } else if (strstr(serviceType, "Database") || strstr(serviceType, "SQL")) {
        return kServiceTypeDatabase;
    }

    return kServiceTypeGeneric;
}

/*
 * Build entity name from service
 */
static void BuildEntityName(const ServiceInfo* service, EntityName* entity) {
    strcpy(entity->object, service->name);
    strcpy(entity->type, service->type);
    strcpy(entity->zone, service->zone);
}

/*
 * Parse entity name to service
 */
static void ParseEntityName(const EntityName* entity, ServiceInfo* service) {
    strcpy(service->name, entity->object);
    strcpy(service->type, entity->type);
    strcpy(service->zone, entity->zone);
}

/*
 * Utility Functions
 */

bool DeviceDiscoveryValidateServiceName(const char* serviceName) {
    if (!serviceName) {
        return false;
    }

    size_t len = strlen(serviceName);
    return len > 0 && len < DEVICE_MAX_NAME_LENGTH;
}

bool DeviceDiscoveryValidateServiceType(const char* serviceType) {
    if (!serviceType) {
        return false;
    }

    size_t len = strlen(serviceType);
    return len > 0 && len < DEVICE_MAX_TYPE_LENGTH;
}

bool DeviceDiscoveryCompareServiceNames(const char* name1, const char* name2) {
    if (!name1 || !name2) {
        return false;
    }

    return strcasecmp(name1, name2) == 0;
}

const char* DeviceDiscoveryGetServiceTypeString(DiscoveryServiceType serviceType) {
    switch (serviceType) {
        case kServiceTypeFileServer:
            return "File Server";
        case kServiceTypePrintServer:
            return "Print Server";
        case kServiceTypeMailServer:
            return "Mail Server";
        case kServiceTypeWebServer:
            return "Web Server";
        case kServiceTypeFTPServer:
            return "FTP Server";
        case kServiceTypeDatabase:
            return "Database";
        case kServiceTypeGeneric:
        default:
            return "Generic";
    }
}

const char* DeviceDiscoveryGetStatusString(ServiceStatus status) {
    switch (status) {
        case kServiceStatusAvailable:
            return "Available";
        case kServiceStatusBusy:
            return "Busy";
        case kServiceStatusUnavailable:
            return "Unavailable";
        case kServiceStatusError:
            return "Error";
        case kServiceStatusUnknown:
        default:
            return "Unknown";
    }
}