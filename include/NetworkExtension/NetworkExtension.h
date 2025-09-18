/*
 * NetworkExtension.h - Main Network Extension API
 * Mac OS 7.1 Network Extension for AppleTalk networking and file sharing
 *
 * This is the complete Network Extension implementation providing:
 * - Complete AppleTalk protocol stack (DDP, NBP, ATP, ZIP, RTMP, AARP)
 * - Network file sharing with AFP (Apple Filing Protocol)
 * - Zone management and network browsing
 * - Device discovery and service advertising
 * - Network routing and address resolution
 * - Network security and access control
 * - Modern network protocol abstraction and bridging
 *
 * This represents the FINAL COMPONENT of the Mac OS 7.1 portable conversion,
 * completing 100% of the classic Mac networking experience.
 */

#ifndef NETWORKEXTENSION_H
#define NETWORKEXTENSION_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

/* Include all Network Extension component headers */
#include "AppleTalkStack.h"
#include "FileSharing.h"
#include "ZoneManager.h"
#include "DeviceDiscovery.h"
#include "RoutingManager.h"
#include "NetworkSecurity.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Network Extension Error Codes
 * (Imported from AppleTalkStack.h - used across all components)
 */
#ifndef NETWORKEXTENSION_ERROR_DEFINED
#define NETWORKEXTENSION_ERROR_DEFINED
typedef enum {
    kNetworkExtensionNoError = 0,
    kNetworkExtensionInitError = -1,
    kNetworkExtensionInvalidParam = -2,
    kNetworkExtensionNotFound = -3,
    kNetworkExtensionTimeout = -4,
    kNetworkExtensionNetworkDown = -5,
    kNetworkExtensionAccessDenied = -6,
    kNetworkExtensionBufferTooSmall = -7,
    kNetworkExtensionInternalError = -8,
    kNetworkExtensionNotSupported = -9,
    kNetworkExtensionSessionClosed = -10,
    kNetworkExtensionAuthenticationFailed = -11,
    kNetworkExtensionDuplicateEntry = -12
} NetworkExtensionError;
#endif

/*
 * Core AppleTalk Structures
 * (Imported from AppleTalkStack.h)
 */
#ifndef APPLETALK_STRUCTURES_DEFINED
#define APPLETALK_STRUCTURES_DEFINED
typedef struct {
    uint16_t network;
    uint8_t node;
    uint8_t socket;
} AppleTalkAddress;

typedef struct {
    char object[32];
    char type[32];
    char zone[32];
} EntityName;

typedef struct {
    char name[32];
    uint16_t networkStart;
    uint16_t networkEnd;
    bool isLocalZone;
    bool isDefaultZone;
} ZoneInfo;
#endif

/*
 * Network Protocol Types
 */
typedef enum {
    kNetworkProtocolAppleTalk = 1,
    kNetworkProtocolTCP = 2,
    kNetworkProtocolUDP = 3,
    kNetworkProtocolBonjour = 4
} NetworkProtocolType;

/*
 * Service Types
 */
typedef enum {
    kServiceTypeFileSharing = 1,
    kServiceTypePrinting = 2,
    kServiceTypeEmail = 3,
    kServiceTypeGeneric = 4
} ServiceType;

/*
 * Forward Declarations
 */
typedef struct NetworkExtensionContext NetworkExtensionContext;

/*
 * Network Extension Context Structure
 * This is the main context that manages all network extension components
 */
struct NetworkExtensionContext {
    // Core components
    AppleTalkStack* appleTalkStack;
    FileSharing* fileSharing;
    ZoneManager* zoneManager;
    DeviceDiscovery* deviceDiscovery;
    RoutingManager* routingManager;
    NetworkSecurity* networkSecurity;

    // State
    bool initialized;
    bool started;

    // Configuration
    char zoneName[32];
    AppleTalkAddress localAddress;

    // Modern protocol support
    bool tcpipEnabled;
    bool bonjourEnabled;
    bool protocolBridging;
};

/*
 * Callback Function Types
 */
typedef void (*NetworkExtensionCallback)(NetworkExtensionError error, void* context, void* userData);
typedef void (*NetworkExtensionStatusCallback)(const char* component, bool active, void* userData);

/*
 * Core Network Extension Functions
 */

/**
 * Initialize the Network Extension
 */
NetworkExtensionError NetworkExtensionInit(NetworkExtensionContext** context);

/**
 * Shutdown the Network Extension
 */
void NetworkExtensionShutdown(NetworkExtensionContext* context);

/**
 * Start networking services
 */
NetworkExtensionError NetworkExtensionStart(NetworkExtensionContext* context);

/**
 * Stop networking services
 */
void NetworkExtensionStop(NetworkExtensionContext* context);

/**
 * Get local AppleTalk address
 */
NetworkExtensionError NetworkExtensionGetLocalAddress(NetworkExtensionContext* context,
                                                      AppleTalkAddress* address);

/**
 * Set local zone name
 */
NetworkExtensionError NetworkExtensionSetZone(NetworkExtensionContext* context,
                                               const char* zoneName);

/**
 * Get current zone name
 */
NetworkExtensionError NetworkExtensionGetZone(NetworkExtensionContext* context,
                                               char* zoneName, size_t bufferSize);

/*
 * AppleTalk Protocol Functions
 */

/**
 * Open a DDP socket
 */
NetworkExtensionError AppleTalkOpenSocket(NetworkExtensionContext* context,
                                          uint8_t requestedSocket,
                                          uint8_t* actualSocket);

/**
 * Close a DDP socket
 */
NetworkExtensionError AppleTalkCloseSocket(NetworkExtensionContext* context,
                                           uint8_t socket);

/**
 * Send DDP packet
 */
NetworkExtensionError AppleTalkSendPacket(NetworkExtensionContext* context,
                                          uint8_t socket,
                                          const AppleTalkAddress* destination,
                                          const void* data,
                                          size_t dataLength);

/**
 * Receive DDP packet
 */
NetworkExtensionError AppleTalkReceivePacket(NetworkExtensionContext* context,
                                             uint8_t socket,
                                             AppleTalkAddress* source,
                                             void* buffer,
                                             size_t bufferSize,
                                             size_t* actualLength);

/*
 * Name Binding Protocol (NBP) Functions
 */

/**
 * Register a name with NBP
 */
NetworkExtensionError NBPRegisterName(NetworkExtensionContext* context,
                                       const EntityName* entity,
                                       uint8_t socket);

/**
 * Remove a name from NBP
 */
NetworkExtensionError NBPRemoveName(NetworkExtensionContext* context,
                                     const EntityName* entity);

/**
 * Lookup names with NBP
 */
NetworkExtensionError NBPLookupName(NetworkExtensionContext* context,
                                     const EntityName* entity,
                                     EntityName* results,
                                     AppleTalkAddress* addresses,
                                     int maxResults,
                                     int* actualResults);

/*
 * Zone Information Protocol (ZIP) Functions
 */

/**
 * Get list of zones
 */
NetworkExtensionError ZIPGetZoneList(NetworkExtensionContext* context,
                                      ZoneListCallback callback,
                                      void* userData);

/**
 * Get zone for network
 */
NetworkExtensionError ZIPGetNetworkZone(NetworkExtensionContext* context,
                                         uint16_t network,
                                         char* zoneName,
                                         size_t bufferSize);

/*
 * Service Discovery Functions
 */

/**
 * Start service discovery
 */
NetworkExtensionError ServiceDiscoveryStart(NetworkExtensionContext* context,
                                             ServiceType serviceType,
                                             ServiceDiscoveryCallback callback,
                                             void* userData);

/**
 * Stop service discovery
 */
void ServiceDiscoveryStop(NetworkExtensionContext* context);

/**
 * Advertise a service
 */
NetworkExtensionError ServiceAdvertise(NetworkExtensionContext* context,
                                        const char* serviceName,
                                        ServiceType serviceType,
                                        uint16_t port,
                                        const char* description);

/**
 * Stop advertising a service
 */
void ServiceStopAdvertising(NetworkExtensionContext* context,
                            const char* serviceName);

/*
 * File Sharing Functions
 */

/**
 * Start file sharing server
 */
NetworkExtensionError FileSharingStart(NetworkExtensionContext* context,
                                        const char* serverName,
                                        const char* shareName,
                                        const char* sharePath);

/**
 * Stop file sharing server
 */
void FileSharingStop(NetworkExtensionContext* context);

/**
 * Connect to file server
 */
NetworkExtensionError FileSharingConnect(NetworkExtensionContext* context,
                                          const char* serverName,
                                          const char* userName,
                                          const char* password,
                                          NetworkSession** session);

/**
 * Disconnect from file server
 */
void FileSharingDisconnect(NetworkSession* session);

/*
 * Modern Network Abstraction
 */

/**
 * Enable TCP/IP networking
 */
NetworkExtensionError NetworkExtensionEnableTCPIP(NetworkExtensionContext* context);

/**
 * Enable Bonjour service discovery
 */
NetworkExtensionError NetworkExtensionEnableBonjour(NetworkExtensionContext* context);

/**
 * Bridge AppleTalk to modern protocols
 */
NetworkExtensionError NetworkExtensionBridgeProtocols(NetworkExtensionContext* context,
                                                       bool enable);

/*
 * Utility Functions
 */

/**
 * Convert AppleTalk address to string
 */
void AppleTalkAddressToString(const AppleTalkAddress* address,
                              char* buffer,
                              size_t bufferSize);

/**
 * Parse AppleTalk address from string
 */
bool AppleTalkAddressFromString(const char* string,
                                AppleTalkAddress* address);

/**
 * Get error description
 */
const char* NetworkExtensionGetErrorString(NetworkExtensionError error);

/*
 * Network Extension Statistics
 */
typedef struct {
    // AppleTalk Stack Statistics
    uint32_t ddpPacketsSent;
    uint32_t ddpPacketsReceived;
    uint32_t nbpLookups;
    uint32_t atpTransactions;

    // File Sharing Statistics
    uint32_t afpConnections;
    uint32_t afpFilesSent;
    uint32_t afpFilesReceived;

    // Zone Management Statistics
    uint32_t zonesDiscovered;
    uint32_t zipQueries;

    // Device Discovery Statistics
    uint32_t servicesAdvertised;
    uint32_t servicesDiscovered;

    // Routing Statistics
    uint32_t routesLearned;
    uint32_t aarpResolutions;

    // Security Statistics
    uint32_t authentications;
    uint32_t authorizationChecks;
    uint32_t securityEvents;
} NetworkExtensionStatistics;

/*
 * Additional Network Extension Management Functions
 */

/**
 * Get Network Extension version
 */
const char* NetworkExtensionGetVersion(void);

/**
 * Get Network Extension build date
 */
const char* NetworkExtensionGetBuildDate(void);

/**
 * Check if component is available
 */
bool NetworkExtensionIsComponentAvailable(NetworkExtensionContext* context,
                                           const char* componentName);

/**
 * Get component status
 */
NetworkExtensionError NetworkExtensionGetComponentStatus(NetworkExtensionContext* context,
                                                          const char* componentName,
                                                          bool* active,
                                                          char* statusDescription,
                                                          size_t descriptionSize);

/**
 * Set status callback
 */
void NetworkExtensionSetStatusCallback(NetworkExtensionContext* context,
                                        NetworkExtensionStatusCallback callback,
                                        void* userData);

/**
 * Get comprehensive statistics
 */
NetworkExtensionError NetworkExtensionGetStatistics(NetworkExtensionContext* context,
                                                     NetworkExtensionStatistics* stats);

/**
 * Reset all statistics
 */
void NetworkExtensionResetStatistics(NetworkExtensionContext* context);

/**
 * Save network configuration
 */
NetworkExtensionError NetworkExtensionSaveConfiguration(NetworkExtensionContext* context,
                                                         const char* configFile);

/**
 * Load network configuration
 */
NetworkExtensionError NetworkExtensionLoadConfiguration(NetworkExtensionContext* context,
                                                         const char* configFile);

/**
 * Enable/disable debug logging
 */
void NetworkExtensionSetDebugLogging(NetworkExtensionContext* context,
                                      bool enabled,
                                      const char* logFile);

/**
 * Perform network diagnostics
 */
NetworkExtensionError NetworkExtensionRunDiagnostics(NetworkExtensionContext* context,
                                                      char* results,
                                                      size_t resultsSize);

/**
 * Get supported features
 */
NetworkExtensionError NetworkExtensionGetSupportedFeatures(NetworkExtensionContext* context,
                                                            char** features,
                                                            int* featureCount);

/*
 * Network Extension Constants
 */
#define NETWORK_EXTENSION_VERSION "1.0.0"
#define NETWORK_EXTENSION_BUILD_DATE __DATE__ " " __TIME__
#define NETWORK_EXTENSION_MAX_COMPONENTS 16

/*
 * Component Names
 */
#define NETWORK_EXTENSION_COMPONENT_APPLETALK "AppleTalk Stack"
#define NETWORK_EXTENSION_COMPONENT_FILESHARING "File Sharing"
#define NETWORK_EXTENSION_COMPONENT_ZONES "Zone Manager"
#define NETWORK_EXTENSION_COMPONENT_DISCOVERY "Device Discovery"
#define NETWORK_EXTENSION_COMPONENT_ROUTING "Routing Manager"
#define NETWORK_EXTENSION_COMPONENT_SECURITY "Network Security"

#ifdef __cplusplus
}
#endif

#endif /* NETWORKEXTENSION_H */