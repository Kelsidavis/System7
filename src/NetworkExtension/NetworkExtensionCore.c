/*
 * NetworkExtensionCore.c - Core Network Extension Management
 * Mac OS 7.1 Network Extension for AppleTalk networking and file sharing
 *
 * This module provides the core functionality for network extension management,
 * including initialization, protocol management, and abstraction layer for
 * bridging classic AppleTalk with modern networking protocols.
 */

#include "NetworkExtension.h"
#include "AppleTalkStack.h"
#include "FileSharing.h"
#include "ZoneManager.h"
#include "DeviceDiscovery.h"
#include "RoutingManager.h"
#include "NetworkSecurity.h"

#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>

/*
 * Network Extension Context Structure
 */
struct NetworkExtensionContext {
    // Core state
    bool initialized;
    bool started;
    pthread_mutex_t mutex;

    // AppleTalk state
    AppleTalkStack* appleTalkStack;
    uint16_t localNetwork;
    uint8_t localNode;
    char localZone[32];

    // Protocol managers
    ZoneManager* zoneManager;
    DeviceDiscovery* deviceDiscovery;
    RoutingManager* routingManager;
    NetworkSecurity* networkSecurity;
    FileSharingServer* fileSharingServer;

    // Modern protocol support
    bool tcpipEnabled;
    bool bonjourEnabled;
    bool protocolBridging;

    // Callback management
    ServiceDiscoveryCallback discoveryCallback;
    void* discoveryUserData;

    // Error handling
    NetworkExtensionError lastError;
    char errorMessage[256];
};

/*
 * Global state
 */
static bool g_networkExtensionGlobalInit = false;
static pthread_mutex_t g_globalMutex = PTHREAD_MUTEX_INITIALIZER;

/*
 * Private function declarations
 */
static NetworkExtensionError InitializeAppleTalk(NetworkExtensionContext* context);
static void ShutdownAppleTalk(NetworkExtensionContext* context);
static NetworkExtensionError StartProtocolManagers(NetworkExtensionContext* context);
static void StopProtocolManagers(NetworkExtensionContext* context);
static void SetError(NetworkExtensionContext* context, NetworkExtensionError error, const char* message);
static bool ValidateContext(NetworkExtensionContext* context);

/*
 * Initialize the Network Extension
 */
NetworkExtensionError NetworkExtensionInit(NetworkExtensionContext** context) {
    if (!context) {
        return kNetworkExtensionInvalidParam;
    }

    pthread_mutex_lock(&g_globalMutex);

    // Global initialization
    if (!g_networkExtensionGlobalInit) {
        // Initialize global resources
        AppleTalkStackGlobalInit();
        FileSharingGlobalInit();
        DeviceDiscoveryGlobalInit();
        g_networkExtensionGlobalInit = true;
    }

    pthread_mutex_unlock(&g_globalMutex);

    // Allocate context
    NetworkExtensionContext* ctx = calloc(1, sizeof(NetworkExtensionContext));
    if (!ctx) {
        return kNetworkExtensionInternalError;
    }

    // Initialize mutex
    if (pthread_mutex_init(&ctx->mutex, NULL) != 0) {
        free(ctx);
        return kNetworkExtensionInternalError;
    }

    // Initialize default values
    strcpy(ctx->localZone, "*");
    ctx->localNetwork = 0;
    ctx->localNode = 0;
    ctx->tcpipEnabled = false;
    ctx->bonjourEnabled = false;
    ctx->protocolBridging = false;

    // Initialize AppleTalk stack
    NetworkExtensionError error = InitializeAppleTalk(ctx);
    if (error != kNetworkExtensionNoError) {
        pthread_mutex_destroy(&ctx->mutex);
        free(ctx);
        return error;
    }

    ctx->initialized = true;
    *context = ctx;

    return kNetworkExtensionNoError;
}

/*
 * Shutdown the Network Extension
 */
void NetworkExtensionShutdown(NetworkExtensionContext* context) {
    if (!ValidateContext(context)) {
        return;
    }

    pthread_mutex_lock(&context->mutex);

    if (context->started) {
        StopProtocolManagers(context);
        context->started = false;
    }

    if (context->initialized) {
        ShutdownAppleTalk(context);
        context->initialized = false;
    }

    pthread_mutex_unlock(&context->mutex);
    pthread_mutex_destroy(&context->mutex);

    free(context);
}

/*
 * Start networking services
 */
NetworkExtensionError NetworkExtensionStart(NetworkExtensionContext* context) {
    if (!ValidateContext(context)) {
        return kNetworkExtensionInvalidParam;
    }

    pthread_mutex_lock(&context->mutex);

    if (context->started) {
        pthread_mutex_unlock(&context->mutex);
        return kNetworkExtensionNoError;
    }

    // Start AppleTalk stack
    NetworkExtensionError error = AppleTalkStackStart(context->appleTalkStack);
    if (error != kNetworkExtensionNoError) {
        SetError(context, error, "Failed to start AppleTalk stack");
        pthread_mutex_unlock(&context->mutex);
        return error;
    }

    // Start protocol managers
    error = StartProtocolManagers(context);
    if (error != kNetworkExtensionNoError) {
        AppleTalkStackStop(context->appleTalkStack);
        pthread_mutex_unlock(&context->mutex);
        return error;
    }

    context->started = true;
    pthread_mutex_unlock(&context->mutex);

    return kNetworkExtensionNoError;
}

/*
 * Stop networking services
 */
void NetworkExtensionStop(NetworkExtensionContext* context) {
    if (!ValidateContext(context)) {
        return;
    }

    pthread_mutex_lock(&context->mutex);

    if (context->started) {
        StopProtocolManagers(context);
        AppleTalkStackStop(context->appleTalkStack);
        context->started = false;
    }

    pthread_mutex_unlock(&context->mutex);
}

/*
 * Get local AppleTalk address
 */
NetworkExtensionError NetworkExtensionGetLocalAddress(NetworkExtensionContext* context,
                                                       AppleTalkAddress* address) {
    if (!ValidateContext(context) || !address) {
        return kNetworkExtensionInvalidParam;
    }

    pthread_mutex_lock(&context->mutex);

    if (!context->started) {
        SetError(context, kNetworkExtensionNetworkDown, "Network not started");
        pthread_mutex_unlock(&context->mutex);
        return kNetworkExtensionNetworkDown;
    }

    NetworkExtensionError error = AppleTalkStackGetLocalAddress(context->appleTalkStack, address);

    pthread_mutex_unlock(&context->mutex);
    return error;
}

/*
 * Set local zone name
 */
NetworkExtensionError NetworkExtensionSetZone(NetworkExtensionContext* context,
                                               const char* zoneName) {
    if (!ValidateContext(context) || !zoneName) {
        return kNetworkExtensionInvalidParam;
    }

    pthread_mutex_lock(&context->mutex);

    if (strlen(zoneName) >= sizeof(context->localZone)) {
        SetError(context, kNetworkExtensionInvalidParam, "Zone name too long");
        pthread_mutex_unlock(&context->mutex);
        return kNetworkExtensionInvalidParam;
    }

    strcpy(context->localZone, zoneName);

    NetworkExtensionError error = kNetworkExtensionNoError;
    if (context->started && context->zoneManager) {
        error = ZoneManagerSetLocalZone(context->zoneManager, zoneName);
    }

    pthread_mutex_unlock(&context->mutex);
    return error;
}

/*
 * Get current zone name
 */
NetworkExtensionError NetworkExtensionGetZone(NetworkExtensionContext* context,
                                               char* zoneName, size_t bufferSize) {
    if (!ValidateContext(context) || !zoneName || bufferSize == 0) {
        return kNetworkExtensionInvalidParam;
    }

    pthread_mutex_lock(&context->mutex);

    if (strlen(context->localZone) >= bufferSize) {
        SetError(context, kNetworkExtensionBufferTooSmall, "Buffer too small for zone name");
        pthread_mutex_unlock(&context->mutex);
        return kNetworkExtensionBufferTooSmall;
    }

    strcpy(zoneName, context->localZone);

    pthread_mutex_unlock(&context->mutex);
    return kNetworkExtensionNoError;
}

/*
 * Enable TCP/IP networking
 */
NetworkExtensionError NetworkExtensionEnableTCPIP(NetworkExtensionContext* context) {
    if (!ValidateContext(context)) {
        return kNetworkExtensionInvalidParam;
    }

    pthread_mutex_lock(&context->mutex);

    context->tcpipEnabled = true;

    // Initialize TCP/IP stack abstraction if needed
    NetworkExtensionError error = kNetworkExtensionNoError;
    if (context->started) {
        // TODO: Initialize TCP/IP bridge
    }

    pthread_mutex_unlock(&context->mutex);
    return error;
}

/*
 * Enable Bonjour service discovery
 */
NetworkExtensionError NetworkExtensionEnableBonjour(NetworkExtensionContext* context) {
    if (!ValidateContext(context)) {
        return kNetworkExtensionInvalidParam;
    }

    pthread_mutex_lock(&context->mutex);

    context->bonjourEnabled = true;

    // Initialize Bonjour bridge if needed
    NetworkExtensionError error = kNetworkExtensionNoError;
    if (context->started && context->deviceDiscovery) {
        error = DeviceDiscoveryEnableBonjour(context->deviceDiscovery);
    }

    pthread_mutex_unlock(&context->mutex);
    return error;
}

/*
 * Bridge AppleTalk to modern protocols
 */
NetworkExtensionError NetworkExtensionBridgeProtocols(NetworkExtensionContext* context,
                                                       bool enable) {
    if (!ValidateContext(context)) {
        return kNetworkExtensionInvalidParam;
    }

    pthread_mutex_lock(&context->mutex);

    context->protocolBridging = enable;

    NetworkExtensionError error = kNetworkExtensionNoError;
    if (context->started) {
        // Configure protocol bridging
        if (context->routingManager) {
            error = RoutingManagerSetBridging(context->routingManager, enable);
        }
    }

    pthread_mutex_unlock(&context->mutex);
    return error;
}

/*
 * Get error description
 */
const char* NetworkExtensionGetErrorString(NetworkExtensionError error) {
    switch (error) {
        case kNetworkExtensionNoError:
            return "No error";
        case kNetworkExtensionInitError:
            return "Initialization error";
        case kNetworkExtensionInvalidParam:
            return "Invalid parameter";
        case kNetworkExtensionNotFound:
            return "Not found";
        case kNetworkExtensionTimeout:
            return "Timeout";
        case kNetworkExtensionNetworkDown:
            return "Network down";
        case kNetworkExtensionAccessDenied:
            return "Access denied";
        case kNetworkExtensionBufferTooSmall:
            return "Buffer too small";
        case kNetworkExtensionInternalError:
            return "Internal error";
        case kNetworkExtensionNotSupported:
            return "Not supported";
        case kNetworkExtensionSessionClosed:
            return "Session closed";
        default:
            return "Unknown error";
    }
}

/*
 * Private Functions
 */

static NetworkExtensionError InitializeAppleTalk(NetworkExtensionContext* context) {
    // Create AppleTalk stack
    NetworkExtensionError error = AppleTalkStackCreate(&context->appleTalkStack);
    if (error != kNetworkExtensionNoError) {
        return error;
    }

    // Create zone manager
    error = ZoneManagerCreate(&context->zoneManager, context->appleTalkStack);
    if (error != kNetworkExtensionNoError) {
        AppleTalkStackDestroy(context->appleTalkStack);
        return error;
    }

    // Create device discovery
    error = DeviceDiscoveryCreate(&context->deviceDiscovery, context->appleTalkStack);
    if (error != kNetworkExtensionNoError) {
        ZoneManagerDestroy(context->zoneManager);
        AppleTalkStackDestroy(context->appleTalkStack);
        return error;
    }

    // Create routing manager
    error = RoutingManagerCreate(&context->routingManager, context->appleTalkStack);
    if (error != kNetworkExtensionNoError) {
        DeviceDiscoveryDestroy(context->deviceDiscovery);
        ZoneManagerDestroy(context->zoneManager);
        AppleTalkStackDestroy(context->appleTalkStack);
        return error;
    }

    // Create network security
    error = NetworkSecurityCreate(&context->networkSecurity);
    if (error != kNetworkExtensionNoError) {
        RoutingManagerDestroy(context->routingManager);
        DeviceDiscoveryDestroy(context->deviceDiscovery);
        ZoneManagerDestroy(context->zoneManager);
        AppleTalkStackDestroy(context->appleTalkStack);
        return error;
    }

    // Create file sharing server
    error = FileSharingServerCreate(&context->fileSharingServer, context->appleTalkStack);
    if (error != kNetworkExtensionNoError) {
        NetworkSecurityDestroy(context->networkSecurity);
        RoutingManagerDestroy(context->routingManager);
        DeviceDiscoveryDestroy(context->deviceDiscovery);
        ZoneManagerDestroy(context->zoneManager);
        AppleTalkStackDestroy(context->appleTalkStack);
        return error;
    }

    return kNetworkExtensionNoError;
}

static void ShutdownAppleTalk(NetworkExtensionContext* context) {
    if (context->fileSharingServer) {
        FileSharingServerDestroy(context->fileSharingServer);
        context->fileSharingServer = NULL;
    }

    if (context->networkSecurity) {
        NetworkSecurityDestroy(context->networkSecurity);
        context->networkSecurity = NULL;
    }

    if (context->routingManager) {
        RoutingManagerDestroy(context->routingManager);
        context->routingManager = NULL;
    }

    if (context->deviceDiscovery) {
        DeviceDiscoveryDestroy(context->deviceDiscovery);
        context->deviceDiscovery = NULL;
    }

    if (context->zoneManager) {
        ZoneManagerDestroy(context->zoneManager);
        context->zoneManager = NULL;
    }

    if (context->appleTalkStack) {
        AppleTalkStackDestroy(context->appleTalkStack);
        context->appleTalkStack = NULL;
    }
}

static NetworkExtensionError StartProtocolManagers(NetworkExtensionContext* context) {
    NetworkExtensionError error;

    // Start zone manager
    error = ZoneManagerStart(context->zoneManager);
    if (error != kNetworkExtensionNoError) {
        return error;
    }

    // Start device discovery
    error = DeviceDiscoveryStart(context->deviceDiscovery);
    if (error != kNetworkExtensionNoError) {
        ZoneManagerStop(context->zoneManager);
        return error;
    }

    // Start routing manager
    error = RoutingManagerStart(context->routingManager);
    if (error != kNetworkExtensionNoError) {
        DeviceDiscoveryStop(context->deviceDiscovery);
        ZoneManagerStop(context->zoneManager);
        return error;
    }

    return kNetworkExtensionNoError;
}

static void StopProtocolManagers(NetworkExtensionContext* context) {
    if (context->routingManager) {
        RoutingManagerStop(context->routingManager);
    }

    if (context->deviceDiscovery) {
        DeviceDiscoveryStop(context->deviceDiscovery);
    }

    if (context->zoneManager) {
        ZoneManagerStop(context->zoneManager);
    }

    if (context->fileSharingServer) {
        FileSharingServerStop(context->fileSharingServer);
    }
}

static void SetError(NetworkExtensionContext* context, NetworkExtensionError error, const char* message) {
    context->lastError = error;
    if (message) {
        strncpy(context->errorMessage, message, sizeof(context->errorMessage) - 1);
        context->errorMessage[sizeof(context->errorMessage) - 1] = '\0';
    } else {
        context->errorMessage[0] = '\0';
    }
}

static bool ValidateContext(NetworkExtensionContext* context) {
    return context != NULL && context->initialized;
}