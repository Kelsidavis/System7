/*
 * AppleTalkStack.h - AppleTalk Protocol Stack Implementation
 * Mac OS 7.1 Network Extension for AppleTalk networking
 *
 * Provides:
 * - Complete AppleTalk protocol stack
 * - DDP (Datagram Delivery Protocol)
 * - NBP (Name Binding Protocol)
 * - ATP (AppleTalk Transaction Protocol)
 * - ZIP (Zone Information Protocol)
 * - RTMP (Routing Table Maintenance Protocol)
 * - AARP (AppleTalk Address Resolution Protocol)
 */

#ifndef APPLETALKSTACK_H
#define APPLETALKSTACK_H

#include "NetworkExtension.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * AppleTalk Protocol Constants
 */
#define APPLETALK_MAX_PACKET_SIZE 586
#define APPLETALK_MAX_SOCKETS 254
#define APPLETALK_MAX_NETWORKS 65534
#define APPLETALK_DDP_HEADER_SIZE 13
#define APPLETALK_SHORT_DDP_HEADER_SIZE 5

/*
 * DDP Protocol Types
 */
typedef enum {
    kDDPTypeRTMP = 1,
    kDDPTypeNBP = 2,
    kDDPTypeATP = 3,
    kDDPTypeEcho = 4,
    kDDPTypeRTMPRequest = 5,
    kDDPTypeZIP = 6,
    kDDPTypeADSP = 7
} DDPProtocolType;

/*
 * Socket Types
 */
typedef enum {
    kSocketTypeRTMP = 1,
    kSocketTypeNIS = 2,
    kSocketTypeEcho = 4,
    kSocketTypeZIS = 6
} SocketType;

/*
 * DDP Packet Structure
 */
typedef struct {
    uint16_t length;
    uint16_t checksum;
    uint16_t destinationNetwork;
    uint16_t sourceNetwork;
    uint8_t destinationNode;
    uint8_t sourceNode;
    uint8_t destinationSocket;
    uint8_t sourceSocket;
    uint8_t type;
    uint8_t data[APPLETALK_MAX_PACKET_SIZE - APPLETALK_DDP_HEADER_SIZE];
} DDPPacket;

/*
 * NBP Tuple Structure
 */
typedef struct {
    AppleTalkAddress address;
    uint8_t enumerator;
    EntityName entity;
} NBPTuple;

/*
 * ATP Transaction Structure
 */
typedef struct {
    uint16_t transactionID;
    uint8_t function;
    uint8_t bitmap;
    uint32_t userData;
    bool exactlyOnce;
    bool endOfMessage;
    bool sendTransmissionStatus;
} ATPTransaction;

/*
 * Forward Declarations
 */
typedef struct AppleTalkStack AppleTalkStack;
typedef struct DDPSocket DDPSocket;
typedef struct NBPNameTable NBPNameTable;
typedef struct ATPSession ATPSession;

/*
 * Callback Types
 */
typedef void (*DDPReceiveCallback)(const DDPPacket* packet, void* userData);
typedef void (*NBPLookupCallback)(const NBPTuple* tuples, int count, NetworkExtensionError error, void* userData);
typedef void (*ATPResponseCallback)(const ATPTransaction* transaction, const void* data, size_t dataLength, NetworkExtensionError error, void* userData);

/*
 * AppleTalk Stack Functions
 */

/**
 * Global initialization
 */
void AppleTalkStackGlobalInit(void);

/**
 * Create AppleTalk stack
 */
NetworkExtensionError AppleTalkStackCreate(AppleTalkStack** stack);

/**
 * Destroy AppleTalk stack
 */
void AppleTalkStackDestroy(AppleTalkStack* stack);

/**
 * Start AppleTalk stack
 */
NetworkExtensionError AppleTalkStackStart(AppleTalkStack* stack);

/**
 * Stop AppleTalk stack
 */
void AppleTalkStackStop(AppleTalkStack* stack);

/**
 * Get local AppleTalk address
 */
NetworkExtensionError AppleTalkStackGetLocalAddress(AppleTalkStack* stack,
                                                     AppleTalkAddress* address);

/**
 * Set local AppleTalk address
 */
NetworkExtensionError AppleTalkStackSetLocalAddress(AppleTalkStack* stack,
                                                     const AppleTalkAddress* address);

/*
 * DDP (Datagram Delivery Protocol) Functions
 */

/**
 * Open DDP socket
 */
NetworkExtensionError DDPOpenSocket(AppleTalkStack* stack,
                                     uint8_t requestedSocket,
                                     DDPReceiveCallback callback,
                                     void* userData,
                                     DDPSocket** socket);

/**
 * Close DDP socket
 */
void DDPCloseSocket(DDPSocket* socket);

/**
 * Send DDP packet
 */
NetworkExtensionError DDPSendPacket(DDPSocket* socket,
                                     const AppleTalkAddress* destination,
                                     DDPProtocolType type,
                                     const void* data,
                                     size_t dataLength);

/**
 * Get socket number
 */
uint8_t DDPGetSocketNumber(const DDPSocket* socket);

/*
 * NBP (Name Binding Protocol) Functions
 */

/**
 * Register name with NBP
 */
NetworkExtensionError NBPRegisterName(AppleTalkStack* stack,
                                       const EntityName* entity,
                                       uint8_t socket);

/**
 * Remove name from NBP
 */
NetworkExtensionError NBPRemoveName(AppleTalkStack* stack,
                                     const EntityName* entity);

/**
 * Lookup name with NBP
 */
NetworkExtensionError NBPLookupName(AppleTalkStack* stack,
                                     const EntityName* entity,
                                     NBPLookupCallback callback,
                                     void* userData);

/**
 * Confirm name with NBP
 */
NetworkExtensionError NBPConfirmName(AppleTalkStack* stack,
                                      const EntityName* entity,
                                      const AppleTalkAddress* address);

/*
 * ATP (AppleTalk Transaction Protocol) Functions
 */

/**
 * Open ATP session
 */
NetworkExtensionError ATPOpenSession(AppleTalkStack* stack,
                                      uint8_t socket,
                                      ATPSession** session);

/**
 * Close ATP session
 */
void ATPCloseSession(ATPSession* session);

/**
 * Send ATP request
 */
NetworkExtensionError ATPSendRequest(ATPSession* session,
                                      const AppleTalkAddress* destination,
                                      const void* data,
                                      size_t dataLength,
                                      uint8_t bitmap,
                                      bool exactlyOnce,
                                      ATPResponseCallback callback,
                                      void* userData);

/**
 * Send ATP response
 */
NetworkExtensionError ATPSendResponse(ATPSession* session,
                                       const ATPTransaction* transaction,
                                       const void* data,
                                       size_t dataLength,
                                       uint8_t responseNumber,
                                       bool moreFollowing);

/*
 * ZIP (Zone Information Protocol) Functions
 */

/**
 * Get zone list
 */
NetworkExtensionError ZIPGetZoneList(AppleTalkStack* stack,
                                      ZoneListCallback callback,
                                      void* userData);

/**
 * Get network zone
 */
NetworkExtensionError ZIPGetNetworkZone(AppleTalkStack* stack,
                                         uint16_t network,
                                         char* zoneName,
                                         size_t bufferSize);

/**
 * Get network info
 */
NetworkExtensionError ZIPGetNetworkInfo(AppleTalkStack* stack,
                                         uint16_t network,
                                         ZoneInfo* info);

/*
 * RTMP (Routing Table Maintenance Protocol) Functions
 */

/**
 * Get routing table
 */
NetworkExtensionError RTMPGetRoutingTable(AppleTalkStack* stack,
                                           void* buffer,
                                           size_t bufferSize,
                                           size_t* actualSize);

/**
 * Add route
 */
NetworkExtensionError RTMPAddRoute(AppleTalkStack* stack,
                                    uint16_t network,
                                    uint16_t networkEnd,
                                    uint8_t nextHop,
                                    uint8_t hops);

/**
 * Remove route
 */
NetworkExtensionError RTMPRemoveRoute(AppleTalkStack* stack,
                                       uint16_t network);

/*
 * AARP (AppleTalk Address Resolution Protocol) Functions
 */

/**
 * Resolve hardware address
 */
NetworkExtensionError AARPResolveAddress(AppleTalkStack* stack,
                                          const AppleTalkAddress* address,
                                          uint8_t* hardwareAddress,
                                          size_t hardwareAddressSize);

/**
 * Add AARP entry
 */
NetworkExtensionError AARPAddEntry(AppleTalkStack* stack,
                                    const AppleTalkAddress* address,
                                    const uint8_t* hardwareAddress,
                                    size_t hardwareAddressSize);

/*
 * Utility Functions
 */

/**
 * Validate AppleTalk address
 */
bool AppleTalkAddressIsValid(const AppleTalkAddress* address);

/**
 * Compare AppleTalk addresses
 */
bool AppleTalkAddressEqual(const AppleTalkAddress* addr1, const AppleTalkAddress* addr2);

/**
 * Calculate DDP checksum
 */
uint16_t DDPCalculateChecksum(const DDPPacket* packet);

/**
 * Validate entity name
 */
bool EntityNameIsValid(const EntityName* entity);

/**
 * Compare entity names
 */
bool EntityNameEqual(const EntityName* entity1, const EntityName* entity2);

/**
 * Match entity name pattern
 */
bool EntityNameMatch(const EntityName* entity, const EntityName* pattern);

#ifdef __cplusplus
}
#endif

#endif /* APPLETALKSTACK_H */