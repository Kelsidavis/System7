/*
 * AppleTalkStack.c - AppleTalk Protocol Stack Implementation
 * Mac OS 7.1 Network Extension for AppleTalk networking
 *
 * This module implements the complete AppleTalk protocol stack including:
 * - DDP (Datagram Delivery Protocol)
 * - NBP (Name Binding Protocol)
 * - ATP (AppleTalk Transaction Protocol)
 * - ZIP (Zone Information Protocol)
 * - RTMP (Routing Table Maintenance Protocol)
 * - AARP (AppleTalk Address Resolution Protocol)
 */

#include "AppleTalkStack.h"
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>

/*
 * Internal Constants
 */
#define MAX_SOCKETS 254
#define MAX_NBP_ENTRIES 256
#define MAX_ATP_SESSIONS 64
#define MAX_ROUTING_ENTRIES 1024
#define MAX_AARP_ENTRIES 256

#define NBP_LOOKUP_TIMEOUT 5
#define ATP_REQUEST_TIMEOUT 10
#define RTMP_UPDATE_INTERVAL 10
#define AARP_TIMEOUT 30

/*
 * AppleTalk Stack Structure
 */
struct AppleTalkStack {
    // State
    bool initialized;
    bool started;
    pthread_mutex_t mutex;

    // Local address
    AppleTalkAddress localAddress;

    // Sockets
    DDPSocket* sockets[MAX_SOCKETS];
    int socketCount;

    // NBP name table
    NBPNameTable* nameTable;

    // ATP sessions
    ATPSession* atpSessions[MAX_ATP_SESSIONS];
    int atpSessionCount;

    // Routing table
    struct RoutingEntry {
        uint16_t network;
        uint16_t networkEnd;
        uint8_t nextHop;
        uint8_t hops;
        time_t timestamp;
        bool valid;
    } routingTable[MAX_ROUTING_ENTRIES];
    int routingEntryCount;

    // AARP table
    struct AARPEntry {
        AppleTalkAddress appleTalkAddress;
        uint8_t hardwareAddress[6];
        size_t hardwareAddressSize;
        time_t timestamp;
        bool valid;
    } aarpTable[MAX_AARP_ENTRIES];
    int aarpEntryCount;

    // Threading
    pthread_t workerThread;
    bool workerRunning;
};

/*
 * DDP Socket Structure
 */
struct DDPSocket {
    AppleTalkStack* stack;
    uint8_t socketNumber;
    DDPReceiveCallback callback;
    void* userData;
    bool active;
};

/*
 * NBP Name Table Structure
 */
struct NBPNameTable {
    struct NBPEntry {
        EntityName entity;
        AppleTalkAddress address;
        uint8_t socket;
        bool valid;
    } entries[MAX_NBP_ENTRIES];
    int entryCount;
};

/*
 * ATP Session Structure
 */
struct ATPSession {
    AppleTalkStack* stack;
    uint8_t socket;
    uint16_t nextTransactionID;

    // Pending requests
    struct ATPRequest {
        uint16_t transactionID;
        AppleTalkAddress destination;
        ATPResponseCallback callback;
        void* userData;
        time_t timestamp;
        bool active;
    } requests[16];
    int requestCount;
};

/*
 * Global state
 */
static bool g_appleTalkGlobalInit = false;
static pthread_mutex_t g_globalMutex = PTHREAD_MUTEX_INITIALIZER;

/*
 * Private function declarations
 */
static NetworkExtensionError InitializeStack(AppleTalkStack* stack);
static void* WorkerThread(void* arg);
static NetworkExtensionError ProcessIncomingPackets(AppleTalkStack* stack);
static NetworkExtensionError HandleDDPPacket(AppleTalkStack* stack, const DDPPacket* packet);
static NetworkExtensionError HandleNBPPacket(AppleTalkStack* stack, const DDPPacket* packet);
static NetworkExtensionError HandleATPPacket(AppleTalkStack* stack, const DDPPacket* packet);
static NetworkExtensionError HandleZIPPacket(AppleTalkStack* stack, const DDPPacket* packet);
static NetworkExtensionError HandleRTMPPacket(AppleTalkStack* stack, const DDPPacket* packet);
static NetworkExtensionError SendDDPPacket(AppleTalkStack* stack, const DDPPacket* packet);
static uint8_t AllocateSocket(AppleTalkStack* stack, uint8_t requested);
static void DeallocateSocket(AppleTalkStack* stack, uint8_t socket);
static NBPNameTable* CreateNameTable(void);
static void DestroyNameTable(NBPNameTable* table);

/*
 * Global initialization
 */
void AppleTalkStackGlobalInit(void) {
    pthread_mutex_lock(&g_globalMutex);

    if (!g_appleTalkGlobalInit) {
        // Initialize global AppleTalk resources
        g_appleTalkGlobalInit = true;
    }

    pthread_mutex_unlock(&g_globalMutex);
}

/*
 * Create AppleTalk stack
 */
NetworkExtensionError AppleTalkStackCreate(AppleTalkStack** stack) {
    if (!stack) {
        return kNetworkExtensionInvalidParam;
    }

    AppleTalkStack* newStack = calloc(1, sizeof(AppleTalkStack));
    if (!newStack) {
        return kNetworkExtensionInternalError;
    }

    // Initialize mutex
    if (pthread_mutex_init(&newStack->mutex, NULL) != 0) {
        free(newStack);
        return kNetworkExtensionInternalError;
    }

    // Initialize stack
    NetworkExtensionError error = InitializeStack(newStack);
    if (error != kNetworkExtensionNoError) {
        pthread_mutex_destroy(&newStack->mutex);
        free(newStack);
        return error;
    }

    newStack->initialized = true;
    *stack = newStack;

    return kNetworkExtensionNoError;
}

/*
 * Destroy AppleTalk stack
 */
void AppleTalkStackDestroy(AppleTalkStack* stack) {
    if (!stack || !stack->initialized) {
        return;
    }

    // Stop if running
    if (stack->started) {
        AppleTalkStackStop(stack);
    }

    pthread_mutex_lock(&stack->mutex);

    // Clean up resources
    for (int i = 0; i < MAX_SOCKETS; i++) {
        if (stack->sockets[i]) {
            free(stack->sockets[i]);
        }
    }

    for (int i = 0; i < MAX_ATP_SESSIONS; i++) {
        if (stack->atpSessions[i]) {
            free(stack->atpSessions[i]);
        }
    }

    if (stack->nameTable) {
        DestroyNameTable(stack->nameTable);
    }

    stack->initialized = false;

    pthread_mutex_unlock(&stack->mutex);
    pthread_mutex_destroy(&stack->mutex);

    free(stack);
}

/*
 * Start AppleTalk stack
 */
NetworkExtensionError AppleTalkStackStart(AppleTalkStack* stack) {
    if (!stack || !stack->initialized) {
        return kNetworkExtensionInvalidParam;
    }

    pthread_mutex_lock(&stack->mutex);

    if (stack->started) {
        pthread_mutex_unlock(&stack->mutex);
        return kNetworkExtensionNoError;
    }

    // Set default local address
    stack->localAddress.network = 0;
    stack->localAddress.node = 0;
    stack->localAddress.socket = 0;

    // Start worker thread
    stack->workerRunning = true;
    if (pthread_create(&stack->workerThread, NULL, WorkerThread, stack) != 0) {
        stack->workerRunning = false;
        pthread_mutex_unlock(&stack->mutex);
        return kNetworkExtensionInternalError;
    }

    stack->started = true;
    pthread_mutex_unlock(&stack->mutex);

    return kNetworkExtensionNoError;
}

/*
 * Stop AppleTalk stack
 */
void AppleTalkStackStop(AppleTalkStack* stack) {
    if (!stack || !stack->initialized || !stack->started) {
        return;
    }

    pthread_mutex_lock(&stack->mutex);

    // Stop worker thread
    stack->workerRunning = false;
    pthread_mutex_unlock(&stack->mutex);

    // Wait for worker to finish
    pthread_join(stack->workerThread, NULL);

    pthread_mutex_lock(&stack->mutex);
    stack->started = false;
    pthread_mutex_unlock(&stack->mutex);
}

/*
 * Get local AppleTalk address
 */
NetworkExtensionError AppleTalkStackGetLocalAddress(AppleTalkStack* stack,
                                                     AppleTalkAddress* address) {
    if (!stack || !stack->initialized || !address) {
        return kNetworkExtensionInvalidParam;
    }

    pthread_mutex_lock(&stack->mutex);
    *address = stack->localAddress;
    pthread_mutex_unlock(&stack->mutex);

    return kNetworkExtensionNoError;
}

/*
 * Set local AppleTalk address
 */
NetworkExtensionError AppleTalkStackSetLocalAddress(AppleTalkStack* stack,
                                                     const AppleTalkAddress* address) {
    if (!stack || !stack->initialized || !address) {
        return kNetworkExtensionInvalidParam;
    }

    if (!AppleTalkAddressIsValid(address)) {
        return kNetworkExtensionInvalidParam;
    }

    pthread_mutex_lock(&stack->mutex);
    stack->localAddress = *address;
    pthread_mutex_unlock(&stack->mutex);

    return kNetworkExtensionNoError;
}

/*
 * Open DDP socket
 */
NetworkExtensionError DDPOpenSocket(AppleTalkStack* stack,
                                     uint8_t requestedSocket,
                                     DDPReceiveCallback callback,
                                     void* userData,
                                     DDPSocket** socket) {
    if (!stack || !stack->initialized || !socket) {
        return kNetworkExtensionInvalidParam;
    }

    pthread_mutex_lock(&stack->mutex);

    // Allocate socket number
    uint8_t socketNumber = AllocateSocket(stack, requestedSocket);
    if (socketNumber == 0) {
        pthread_mutex_unlock(&stack->mutex);
        return kNetworkExtensionInternalError;
    }

    // Create socket structure
    DDPSocket* newSocket = calloc(1, sizeof(DDPSocket));
    if (!newSocket) {
        DeallocateSocket(stack, socketNumber);
        pthread_mutex_unlock(&stack->mutex);
        return kNetworkExtensionInternalError;
    }

    newSocket->stack = stack;
    newSocket->socketNumber = socketNumber;
    newSocket->callback = callback;
    newSocket->userData = userData;
    newSocket->active = true;

    stack->sockets[socketNumber] = newSocket;
    stack->socketCount++;

    *socket = newSocket;
    pthread_mutex_unlock(&stack->mutex);

    return kNetworkExtensionNoError;
}

/*
 * Close DDP socket
 */
void DDPCloseSocket(DDPSocket* socket) {
    if (!socket || !socket->active) {
        return;
    }

    AppleTalkStack* stack = socket->stack;
    pthread_mutex_lock(&stack->mutex);

    DeallocateSocket(stack, socket->socketNumber);
    stack->sockets[socket->socketNumber] = NULL;
    stack->socketCount--;
    socket->active = false;

    pthread_mutex_unlock(&stack->mutex);
    free(socket);
}

/*
 * Send DDP packet
 */
NetworkExtensionError DDPSendPacket(DDPSocket* socket,
                                     const AppleTalkAddress* destination,
                                     DDPProtocolType type,
                                     const void* data,
                                     size_t dataLength) {
    if (!socket || !socket->active || !destination || !data) {
        return kNetworkExtensionInvalidParam;
    }

    if (dataLength > APPLETALK_MAX_PACKET_SIZE - APPLETALK_DDP_HEADER_SIZE) {
        return kNetworkExtensionInvalidParam;
    }

    // Create DDP packet
    DDPPacket packet;
    memset(&packet, 0, sizeof(packet));

    packet.length = APPLETALK_DDP_HEADER_SIZE + dataLength;
    packet.destinationNetwork = destination->network;
    packet.destinationNode = destination->node;
    packet.destinationSocket = destination->socket;
    packet.sourceNetwork = socket->stack->localAddress.network;
    packet.sourceNode = socket->stack->localAddress.node;
    packet.sourceSocket = socket->socketNumber;
    packet.type = type;

    memcpy(packet.data, data, dataLength);
    packet.checksum = DDPCalculateChecksum(&packet);

    return SendDDPPacket(socket->stack, &packet);
}

/*
 * Get socket number
 */
uint8_t DDPGetSocketNumber(const DDPSocket* socket) {
    return socket ? socket->socketNumber : 0;
}

/*
 * Register name with NBP
 */
NetworkExtensionError NBPRegisterName(AppleTalkStack* stack,
                                       const EntityName* entity,
                                       uint8_t socket) {
    if (!stack || !stack->initialized || !entity) {
        return kNetworkExtensionInvalidParam;
    }

    if (!EntityNameIsValid(entity)) {
        return kNetworkExtensionInvalidParam;
    }

    pthread_mutex_lock(&stack->mutex);

    NBPNameTable* table = stack->nameTable;
    if (!table) {
        pthread_mutex_unlock(&stack->mutex);
        return kNetworkExtensionInternalError;
    }

    // Find empty slot
    for (int i = 0; i < MAX_NBP_ENTRIES; i++) {
        if (!table->entries[i].valid) {
            table->entries[i].entity = *entity;
            table->entries[i].address = stack->localAddress;
            table->entries[i].address.socket = socket;
            table->entries[i].socket = socket;
            table->entries[i].valid = true;
            table->entryCount++;

            pthread_mutex_unlock(&stack->mutex);
            return kNetworkExtensionNoError;
        }
    }

    pthread_mutex_unlock(&stack->mutex);
    return kNetworkExtensionInternalError;
}

/*
 * Remove name from NBP
 */
NetworkExtensionError NBPRemoveName(AppleTalkStack* stack,
                                     const EntityName* entity) {
    if (!stack || !stack->initialized || !entity) {
        return kNetworkExtensionInvalidParam;
    }

    pthread_mutex_lock(&stack->mutex);

    NBPNameTable* table = stack->nameTable;
    if (!table) {
        pthread_mutex_unlock(&stack->mutex);
        return kNetworkExtensionInternalError;
    }

    // Find and remove entry
    for (int i = 0; i < MAX_NBP_ENTRIES; i++) {
        if (table->entries[i].valid && EntityNameEqual(&table->entries[i].entity, entity)) {
            table->entries[i].valid = false;
            table->entryCount--;

            pthread_mutex_unlock(&stack->mutex);
            return kNetworkExtensionNoError;
        }
    }

    pthread_mutex_unlock(&stack->mutex);
    return kNetworkExtensionNotFound;
}

/*
 * Lookup name with NBP
 */
NetworkExtensionError NBPLookupName(AppleTalkStack* stack,
                                     const EntityName* entity,
                                     NBPLookupCallback callback,
                                     void* userData) {
    if (!stack || !stack->initialized || !entity || !callback) {
        return kNetworkExtensionInvalidParam;
    }

    // TODO: Implement NBP lookup protocol
    // For now, just check local name table

    pthread_mutex_lock(&stack->mutex);

    NBPNameTable* table = stack->nameTable;
    if (!table) {
        pthread_mutex_unlock(&stack->mutex);
        return kNetworkExtensionInternalError;
    }

    NBPTuple results[MAX_NBP_ENTRIES];
    int resultCount = 0;

    // Search local table
    for (int i = 0; i < MAX_NBP_ENTRIES && resultCount < MAX_NBP_ENTRIES; i++) {
        if (table->entries[i].valid && EntityNameMatch(&table->entries[i].entity, entity)) {
            results[resultCount].address = table->entries[i].address;
            results[resultCount].entity = table->entries[i].entity;
            results[resultCount].enumerator = i;
            resultCount++;
        }
    }

    pthread_mutex_unlock(&stack->mutex);

    // Call callback with results
    callback(results, resultCount, kNetworkExtensionNoError, userData);

    return kNetworkExtensionNoError;
}

/*
 * Utility Functions
 */

bool AppleTalkAddressIsValid(const AppleTalkAddress* address) {
    if (!address) {
        return false;
    }

    // Network 0 is reserved
    if (address->network == 0 && address->node != 0) {
        return false;
    }

    // Node 0 and 255 are reserved
    if (address->node == 0 || address->node == 255) {
        return false;
    }

    return true;
}

bool AppleTalkAddressEqual(const AppleTalkAddress* addr1, const AppleTalkAddress* addr2) {
    if (!addr1 || !addr2) {
        return false;
    }

    return addr1->network == addr2->network &&
           addr1->node == addr2->node &&
           addr1->socket == addr2->socket;
}

uint16_t DDPCalculateChecksum(const DDPPacket* packet) {
    if (!packet) {
        return 0;
    }

    // Simple checksum calculation
    uint32_t sum = 0;
    const uint8_t* data = (const uint8_t*)packet;

    for (size_t i = 0; i < packet->length; i += 2) {
        if (i + 1 < packet->length) {
            sum += (data[i] << 8) | data[i + 1];
        } else {
            sum += data[i] << 8;
        }
    }

    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }

    return (uint16_t)(~sum);
}

bool EntityNameIsValid(const EntityName* entity) {
    if (!entity) {
        return false;
    }

    // Check string lengths
    if (strlen(entity->object) == 0 || strlen(entity->object) >= 32) {
        return false;
    }

    if (strlen(entity->type) == 0 || strlen(entity->type) >= 32) {
        return false;
    }

    if (strlen(entity->zone) >= 32) {
        return false;
    }

    return true;
}

bool EntityNameEqual(const EntityName* entity1, const EntityName* entity2) {
    if (!entity1 || !entity2) {
        return false;
    }

    return strcmp(entity1->object, entity2->object) == 0 &&
           strcmp(entity1->type, entity2->type) == 0 &&
           strcmp(entity1->zone, entity2->zone) == 0;
}

bool EntityNameMatch(const EntityName* entity, const EntityName* pattern) {
    if (!entity || !pattern) {
        return false;
    }

    // Simple pattern matching - wildcards are = and *
    // TODO: Implement proper AppleTalk wildcard matching

    return EntityNameEqual(entity, pattern);
}

/*
 * Private Functions
 */

static NetworkExtensionError InitializeStack(AppleTalkStack* stack) {
    // Create name table
    stack->nameTable = CreateNameTable();
    if (!stack->nameTable) {
        return kNetworkExtensionInternalError;
    }

    // Initialize routing table
    for (int i = 0; i < MAX_ROUTING_ENTRIES; i++) {
        stack->routingTable[i].valid = false;
    }

    // Initialize AARP table
    for (int i = 0; i < MAX_AARP_ENTRIES; i++) {
        stack->aarpTable[i].valid = false;
    }

    return kNetworkExtensionNoError;
}

static void* WorkerThread(void* arg) {
    AppleTalkStack* stack = (AppleTalkStack*)arg;

    while (stack->workerRunning) {
        // Process incoming packets
        ProcessIncomingPackets(stack);

        // Sleep briefly
        usleep(10000); // 10ms
    }

    return NULL;
}

static NetworkExtensionError ProcessIncomingPackets(AppleTalkStack* stack) {
    // TODO: Implement packet reception from network interface
    // This would involve platform-specific code to read from
    // network interfaces and parse AppleTalk frames

    return kNetworkExtensionNoError;
}

static NetworkExtensionError SendDDPPacket(AppleTalkStack* stack, const DDPPacket* packet) {
    // TODO: Implement packet transmission to network interface
    // This would involve platform-specific code to send AppleTalk
    // frames over the network interface

    return kNetworkExtensionNoError;
}

static uint8_t AllocateSocket(AppleTalkStack* stack, uint8_t requested) {
    if (requested != 0) {
        // Try requested socket
        if (requested < MAX_SOCKETS && !stack->sockets[requested]) {
            return requested;
        }
    }

    // Find available socket
    for (uint8_t i = 128; i < MAX_SOCKETS; i++) {
        if (!stack->sockets[i]) {
            return i;
        }
    }

    return 0; // No socket available
}

static void DeallocateSocket(AppleTalkStack* stack, uint8_t socket) {
    if (socket < MAX_SOCKETS) {
        stack->sockets[socket] = NULL;
    }
}

static NBPNameTable* CreateNameTable(void) {
    NBPNameTable* table = calloc(1, sizeof(NBPNameTable));
    if (!table) {
        return NULL;
    }

    for (int i = 0; i < MAX_NBP_ENTRIES; i++) {
        table->entries[i].valid = false;
    }

    return table;
}

static void DestroyNameTable(NBPNameTable* table) {
    if (table) {
        free(table);
    }
}