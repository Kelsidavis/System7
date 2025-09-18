/*
 * FileSharing.c - Network File Sharing (AFP) Implementation
 * Mac OS 7.1 Network Extension for AppleTalk file sharing
 *
 * This module implements the Apple Filing Protocol (AFP) for network file sharing,
 * including server and client functionality, authentication, and modern protocol bridges.
 */

#include "FileSharing.h"
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <sys/stat.h>
#include <dirent.h>

/*
 * Constants
 */
#define MAX_VOLUMES 32
#define MAX_SESSIONS 64
#define MAX_OPEN_FILES 256
#define AFP_DEFAULT_PORT 548
#define AFP_WORKER_THREADS 4
#define AFP_REQUEST_TIMEOUT 30

/*
 * AFP Request/Response Header
 */
typedef struct {
    uint8_t flags;
    uint8_t command;
    uint16_t requestID;
    uint32_t errorCode;
    uint32_t totalDataLength;
    uint32_t reserved;
} AFPHeader;

/*
 * File Sharing Server Structure
 */
struct FileSharingServer {
    // State
    bool initialized;
    bool started;
    pthread_mutex_t mutex;

    // AppleTalk integration
    AppleTalkStack* appleTalkStack;
    DDPSocket* afpSocket;

    // Server configuration
    char serverName[AFP_MAX_FILENAME_LENGTH + 1];
    AFPServerInfo serverInfo;

    // Volumes
    struct VolumeEntry {
        char name[AFP_MAX_FILENAME_LENGTH + 1];
        char path[AFP_MAX_PATH_LENGTH + 1];
        AFPVolumeAttributes attributes;
        char password[AFP_MAX_PASSWORD_LENGTH + 1];
        bool active;
        AFPVolumeInfo info;
    } volumes[MAX_VOLUMES];
    int volumeCount;

    // Sessions
    AFPSession* sessions[MAX_SESSIONS];
    int sessionCount;

    // Threading
    pthread_t workerThreads[AFP_WORKER_THREADS];
    bool workersRunning;

    // Callback
    FileSharingServerCallback callback;
    void* callbackUserData;
};

/*
 * File Sharing Client Structure
 */
struct FileSharingClient {
    bool initialized;
    pthread_mutex_t mutex;
    AppleTalkStack* appleTalkStack;
};

/*
 * AFP Session Structure
 */
struct AFPSession {
    // Basic info
    uint32_t sessionID;
    FileSharingServer* server;
    AppleTalkAddress clientAddress;
    time_t loginTime;

    // Authentication
    char userName[AFP_MAX_USERNAME_LENGTH + 1];
    uint16_t userID;
    uint16_t groupID;
    AFPAuthMethod authMethod;
    bool authenticated;

    // Open volumes
    AFPVolume* openVolumes[MAX_VOLUMES];
    int openVolumeCount;

    // Open files
    AFPFile* openFiles[MAX_OPEN_FILES];
    int openFileCount;

    // State
    bool active;
    pthread_mutex_t mutex;
};

/*
 * AFP Volume Structure
 */
struct AFPVolume {
    AFPSession* session;
    struct VolumeEntry* volumeEntry;
    uint16_t volumeID;
    bool readOnly;
    time_t openTime;
};

/*
 * AFP File Structure
 */
struct AFPFile {
    AFPSession* session;
    AFPVolume* volume;
    uint16_t forkRefNum;
    char fileName[AFP_MAX_FILENAME_LENGTH + 1];
    char fullPath[AFP_MAX_PATH_LENGTH + 1];
    bool resourceFork;
    uint16_t accessMode;
    FILE* fileHandle;
    uint32_t forkLength;
    time_t openTime;
};

/*
 * Global state
 */
static bool g_fileSharingGlobalInit = false;
static pthread_mutex_t g_globalMutex = PTHREAD_MUTEX_INITIALIZER;
static uint32_t g_nextSessionID = 1;
static uint16_t g_nextVolumeID = 1;
static uint16_t g_nextForkRefNum = 1;

/*
 * Private function declarations
 */
static void DDPReceiveHandler(const DDPPacket* packet, void* userData);
static NetworkExtensionError ProcessAFPRequest(FileSharingServer* server, const AFPHeader* header, const void* data, size_t dataLength, const AppleTalkAddress* clientAddress);
static NetworkExtensionError HandleLogin(FileSharingServer* server, const void* data, size_t dataLength, const AppleTalkAddress* clientAddress);
static NetworkExtensionError HandleGetServerInfo(FileSharingServer* server, const AppleTalkAddress* clientAddress);
static NetworkExtensionError HandleOpenVolume(AFPSession* session, const void* data, size_t dataLength);
static NetworkExtensionError SendAFPResponse(FileSharingServer* server, const AppleTalkAddress* clientAddress, uint16_t requestID, AFPResultCode resultCode, const void* data, size_t dataLength);
static AFPSession* FindSession(FileSharingServer* server, const AppleTalkAddress* clientAddress);
static AFPSession* CreateSession(FileSharingServer* server, const AppleTalkAddress* clientAddress);
static void DestroySession(AFPSession* session);
static struct VolumeEntry* FindVolume(FileSharingServer* server, const char* volumeName);
static NetworkExtensionError ValidateUserAuthentication(const char* userName, const char* password);
static void* WorkerThread(void* arg);
static void BuildAFPPath(const char* volumePath, uint32_t dirID, const char* fileName, char* fullPath, size_t bufferSize);

/*
 * Global initialization
 */
void FileSharingGlobalInit(void) {
    pthread_mutex_lock(&g_globalMutex);

    if (!g_fileSharingGlobalInit) {
        // Initialize global file sharing resources
        g_fileSharingGlobalInit = true;
    }

    pthread_mutex_unlock(&g_globalMutex);
}

/*
 * Create file sharing server
 */
NetworkExtensionError FileSharingServerCreate(FileSharingServer** server,
                                               AppleTalkStack* appleTalkStack) {
    if (!server || !appleTalkStack) {
        return kNetworkExtensionInvalidParam;
    }

    FileSharingServer* newServer = calloc(1, sizeof(FileSharingServer));
    if (!newServer) {
        return kNetworkExtensionInternalError;
    }

    // Initialize mutex
    if (pthread_mutex_init(&newServer->mutex, NULL) != 0) {
        free(newServer);
        return kNetworkExtensionInternalError;
    }

    newServer->appleTalkStack = appleTalkStack;

    // Initialize server info
    strcpy(newServer->serverInfo.serverName, "Mac OS 7.1 Server");
    strcpy(newServer->serverInfo.machineType, "Macintosh");
    strcpy(newServer->serverInfo.afpVersions, "AFP2.0,AFP2.1,AFP2.2");
    strcpy(newServer->serverInfo.uams, "No User Authent,Cleartxt Passwrd");
    newServer->serverInfo.flags = 0;

    // Initialize volumes
    for (int i = 0; i < MAX_VOLUMES; i++) {
        newServer->volumes[i].active = false;
    }

    // Initialize sessions
    for (int i = 0; i < MAX_SESSIONS; i++) {
        newServer->sessions[i] = NULL;
    }

    newServer->initialized = true;
    *server = newServer;

    return kNetworkExtensionNoError;
}

/*
 * Destroy file sharing server
 */
void FileSharingServerDestroy(FileSharingServer* server) {
    if (!server || !server->initialized) {
        return;
    }

    // Stop if running
    if (server->started) {
        FileSharingServerStop(server);
    }

    pthread_mutex_lock(&server->mutex);

    // Clean up sessions
    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (server->sessions[i]) {
            DestroySession(server->sessions[i]);
        }
    }

    server->initialized = false;

    pthread_mutex_unlock(&server->mutex);
    pthread_mutex_destroy(&server->mutex);

    free(server);
}

/*
 * Start file sharing server
 */
NetworkExtensionError FileSharingServerStart(FileSharingServer* server,
                                              const char* serverName) {
    if (!server || !server->initialized) {
        return kNetworkExtensionInvalidParam;
    }

    pthread_mutex_lock(&server->mutex);

    if (server->started) {
        pthread_mutex_unlock(&server->mutex);
        return kNetworkExtensionNoError;
    }

    // Set server name
    if (serverName) {
        strncpy(server->serverName, serverName, sizeof(server->serverName) - 1);
        server->serverName[sizeof(server->serverName) - 1] = '\0';
        strcpy(server->serverInfo.serverName, server->serverName);
    }

    // Open AFP socket
    NetworkExtensionError error = DDPOpenSocket(server->appleTalkStack,
                                                 AFP_DEFAULT_PORT,
                                                 DDPReceiveHandler,
                                                 server,
                                                 &server->afpSocket);
    if (error != kNetworkExtensionNoError) {
        pthread_mutex_unlock(&server->mutex);
        return error;
    }

    // Register server name with NBP
    EntityName entity;
    strcpy(entity.object, server->serverName);
    strcpy(entity.type, "AFPServer");
    strcpy(entity.zone, "*");

    error = NBPRegisterName(server->appleTalkStack, &entity, DDPGetSocketNumber(server->afpSocket));
    if (error != kNetworkExtensionNoError) {
        DDPCloseSocket(server->afpSocket);
        server->afpSocket = NULL;
        pthread_mutex_unlock(&server->mutex);
        return error;
    }

    // Start worker threads
    server->workersRunning = true;
    for (int i = 0; i < AFP_WORKER_THREADS; i++) {
        if (pthread_create(&server->workerThreads[i], NULL, WorkerThread, server) != 0) {
            server->workersRunning = false;
            // Clean up already created threads
            for (int j = 0; j < i; j++) {
                pthread_join(server->workerThreads[j], NULL);
            }
            DDPCloseSocket(server->afpSocket);
            server->afpSocket = NULL;
            pthread_mutex_unlock(&server->mutex);
            return kNetworkExtensionInternalError;
        }
    }

    server->started = true;
    pthread_mutex_unlock(&server->mutex);

    return kNetworkExtensionNoError;
}

/*
 * Stop file sharing server
 */
void FileSharingServerStop(FileSharingServer* server) {
    if (!server || !server->initialized || !server->started) {
        return;
    }

    pthread_mutex_lock(&server->mutex);

    // Stop worker threads
    server->workersRunning = false;
    pthread_mutex_unlock(&server->mutex);

    // Wait for workers to finish
    for (int i = 0; i < AFP_WORKER_THREADS; i++) {
        pthread_join(server->workerThreads[i], NULL);
    }

    pthread_mutex_lock(&server->mutex);

    // Unregister server name
    if (server->afpSocket) {
        EntityName entity;
        strcpy(entity.object, server->serverName);
        strcpy(entity.type, "AFPServer");
        strcpy(entity.zone, "*");
        NBPRemoveName(server->appleTalkStack, &entity);

        // Close AFP socket
        DDPCloseSocket(server->afpSocket);
        server->afpSocket = NULL;
    }

    // Close all sessions
    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (server->sessions[i]) {
            DestroySession(server->sessions[i]);
            server->sessions[i] = NULL;
        }
    }
    server->sessionCount = 0;

    server->started = false;
    pthread_mutex_unlock(&server->mutex);
}

/*
 * Add volume to server
 */
NetworkExtensionError FileSharingServerAddVolume(FileSharingServer* server,
                                                  const char* volumeName,
                                                  const char* volumePath,
                                                  AFPVolumeAttributes attributes,
                                                  const char* password) {
    if (!server || !server->initialized || !volumeName || !volumePath) {
        return kNetworkExtensionInvalidParam;
    }

    pthread_mutex_lock(&server->mutex);

    // Check if volume already exists
    if (FindVolume(server, volumeName)) {
        pthread_mutex_unlock(&server->mutex);
        return kNetworkExtensionInternalError;
    }

    // Find empty slot
    struct VolumeEntry* volume = NULL;
    for (int i = 0; i < MAX_VOLUMES; i++) {
        if (!server->volumes[i].active) {
            volume = &server->volumes[i];
            break;
        }
    }

    if (!volume) {
        pthread_mutex_unlock(&server->mutex);
        return kNetworkExtensionInternalError;
    }

    // Verify path exists
    struct stat st;
    if (stat(volumePath, &st) != 0 || !S_ISDIR(st.st_mode)) {
        pthread_mutex_unlock(&server->mutex);
        return kNetworkExtensionInvalidParam;
    }

    // Initialize volume
    strncpy(volume->name, volumeName, sizeof(volume->name) - 1);
    volume->name[sizeof(volume->name) - 1] = '\0';

    strncpy(volume->path, volumePath, sizeof(volume->path) - 1);
    volume->path[sizeof(volume->path) - 1] = '\0';

    volume->attributes = attributes;

    if (password) {
        strncpy(volume->password, password, sizeof(volume->password) - 1);
        volume->password[sizeof(volume->password) - 1] = '\0';
    } else {
        volume->password[0] = '\0';
    }

    // Initialize volume info
    strcpy(volume->info.name, volumeName);
    volume->info.attributes = attributes;
    volume->info.signature = 0x4D41;  // 'MA' for Mac
    volume->info.createDate = time(NULL);
    volume->info.modifyDate = volume->info.createDate;
    volume->info.backupDate = 0;
    volume->info.volumeID = g_nextVolumeID++;

    // Get volume statistics
    struct statvfs vfs;
    if (statvfs(volumePath, &vfs) == 0) {
        volume->info.bytesTotal = vfs.f_blocks * vfs.f_frsize;
        volume->info.bytesFree = vfs.f_bavail * vfs.f_frsize;
    } else {
        volume->info.bytesTotal = 0;
        volume->info.bytesFree = 0;
    }

    volume->info.directoryCount = 0;
    volume->info.fileCount = 0;

    volume->active = true;
    server->volumeCount++;

    pthread_mutex_unlock(&server->mutex);

    return kNetworkExtensionNoError;
}

/*
 * Remove volume from server
 */
NetworkExtensionError FileSharingServerRemoveVolume(FileSharingServer* server,
                                                     const char* volumeName) {
    if (!server || !server->initialized || !volumeName) {
        return kNetworkExtensionInvalidParam;
    }

    pthread_mutex_lock(&server->mutex);

    struct VolumeEntry* volume = FindVolume(server, volumeName);
    if (!volume) {
        pthread_mutex_unlock(&server->mutex);
        return kNetworkExtensionNotFound;
    }

    // TODO: Close all open sessions/files for this volume

    volume->active = false;
    server->volumeCount--;

    pthread_mutex_unlock(&server->mutex);

    return kNetworkExtensionNoError;
}

/*
 * DDP receive handler
 */
static void DDPReceiveHandler(const DDPPacket* packet, void* userData) {
    FileSharingServer* server = (FileSharingServer*)userData;

    if (!server || !packet || packet->type != kDDPTypeATP) {
        return;
    }

    // Extract AFP header
    if (packet->length < sizeof(AFPHeader)) {
        return;
    }

    const AFPHeader* header = (const AFPHeader*)packet->data;
    const void* requestData = packet->data + sizeof(AFPHeader);
    size_t requestDataLength = packet->length - sizeof(AFPHeader);

    AppleTalkAddress clientAddress = {
        .network = packet->sourceNetwork,
        .node = packet->sourceNode,
        .socket = packet->sourceSocket
    };

    // Process AFP request
    ProcessAFPRequest(server, header, requestData, requestDataLength, &clientAddress);
}

/*
 * Process AFP request
 */
static NetworkExtensionError ProcessAFPRequest(FileSharingServer* server,
                                                const AFPHeader* header,
                                                const void* data,
                                                size_t dataLength,
                                                const AppleTalkAddress* clientAddress) {
    NetworkExtensionError error = kNetworkExtensionNoError;

    switch (header->command) {
        case kAFPGetSrvrInfo:
            error = HandleGetServerInfo(server, clientAddress);
            break;

        case kAFPLogin:
            error = HandleLogin(server, data, dataLength, clientAddress);
            break;

        case kAFPOpenVol: {
            AFPSession* session = FindSession(server, clientAddress);
            if (session) {
                error = HandleOpenVolume(session, data, dataLength);
            } else {
                SendAFPResponse(server, clientAddress, header->requestID, kAFPUserNotAuth, NULL, 0);
            }
            break;
        }

        default:
            // Send "call not supported" response
            SendAFPResponse(server, clientAddress, header->requestID, kAFPCallNotSupported, NULL, 0);
            break;
    }

    return error;
}

/*
 * Handle login request
 */
static NetworkExtensionError HandleLogin(FileSharingServer* server,
                                          const void* data,
                                          size_t dataLength,
                                          const AppleTalkAddress* clientAddress) {
    // Parse login request
    if (dataLength < 4) {
        return kNetworkExtensionInvalidParam;
    }

    const uint8_t* requestData = (const uint8_t*)data;
    uint8_t afpVersion = requestData[0];
    uint8_t uamLength = requestData[1];

    if (dataLength < 2 + uamLength) {
        return kNetworkExtensionInvalidParam;
    }

    // For simplicity, accept any login for now
    AFPSession* session = CreateSession(server, clientAddress);
    if (!session) {
        return kNetworkExtensionInternalError;
    }

    session->authenticated = true;
    strcpy(session->userName, "Guest");
    session->userID = 1;
    session->groupID = 1;
    session->authMethod = kAFPNoUserAuthent;

    // Send successful login response
    return SendAFPResponse(server, clientAddress, 0, kAFPNoErr, NULL, 0);
}

/*
 * Handle get server info request
 */
static NetworkExtensionError HandleGetServerInfo(FileSharingServer* server,
                                                  const AppleTalkAddress* clientAddress) {
    AFPServerInfo* info = &server->serverInfo;

    // Build server info response
    uint8_t response[512];
    uint8_t* ptr = response;

    // Server name
    *ptr++ = strlen(info->serverName);
    memcpy(ptr, info->serverName, strlen(info->serverName));
    ptr += strlen(info->serverName);

    // Machine type
    *ptr++ = strlen(info->machineType);
    memcpy(ptr, info->machineType, strlen(info->machineType));
    ptr += strlen(info->machineType);

    // AFP versions
    *ptr++ = strlen(info->afpVersions);
    memcpy(ptr, info->afpVersions, strlen(info->afpVersions));
    ptr += strlen(info->afpVersions);

    // UAMs
    *ptr++ = strlen(info->uams);
    memcpy(ptr, info->uams, strlen(info->uams));
    ptr += strlen(info->uams);

    size_t responseLength = ptr - response;

    return SendAFPResponse(server, clientAddress, 0, kAFPNoErr, response, responseLength);
}

/*
 * Send AFP response
 */
static NetworkExtensionError SendAFPResponse(FileSharingServer* server,
                                              const AppleTalkAddress* clientAddress,
                                              uint16_t requestID,
                                              AFPResultCode resultCode,
                                              const void* data,
                                              size_t dataLength) {
    // Build AFP response header
    AFPHeader header;
    header.flags = 0x01;  // Response flag
    header.command = 0;
    header.requestID = requestID;
    header.errorCode = resultCode;
    header.totalDataLength = sizeof(AFPHeader) + dataLength;
    header.reserved = 0;

    // Send response
    uint8_t buffer[APPLETALK_MAX_PACKET_SIZE];
    memcpy(buffer, &header, sizeof(header));
    if (data && dataLength > 0) {
        memcpy(buffer + sizeof(header), data, dataLength);
    }

    return DDPSendPacket(server->afpSocket, clientAddress, kDDPTypeATP, buffer, sizeof(header) + dataLength);
}

/*
 * Find session by client address
 */
static AFPSession* FindSession(FileSharingServer* server, const AppleTalkAddress* clientAddress) {
    for (int i = 0; i < MAX_SESSIONS; i++) {
        AFPSession* session = server->sessions[i];
        if (session && session->active && AppleTalkAddressEqual(&session->clientAddress, clientAddress)) {
            return session;
        }
    }
    return NULL;
}

/*
 * Create new session
 */
static AFPSession* CreateSession(FileSharingServer* server, const AppleTalkAddress* clientAddress) {
    // Find empty slot
    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (!server->sessions[i]) {
            AFPSession* session = calloc(1, sizeof(AFPSession));
            if (!session) {
                return NULL;
            }

            if (pthread_mutex_init(&session->mutex, NULL) != 0) {
                free(session);
                return NULL;
            }

            session->sessionID = g_nextSessionID++;
            session->server = server;
            session->clientAddress = *clientAddress;
            session->loginTime = time(NULL);
            session->active = true;

            server->sessions[i] = session;
            server->sessionCount++;

            return session;
        }
    }

    return NULL;
}

/*
 * Destroy session
 */
static void DestroySession(AFPSession* session) {
    if (!session) {
        return;
    }

    pthread_mutex_lock(&session->mutex);

    // Close all open files
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        if (session->openFiles[i]) {
            AFPCloseFork(session->openFiles[i]);
        }
    }

    // Close all open volumes
    for (int i = 0; i < MAX_VOLUMES; i++) {
        if (session->openVolumes[i]) {
            free(session->openVolumes[i]);
        }
    }

    session->active = false;

    pthread_mutex_unlock(&session->mutex);
    pthread_mutex_destroy(&session->mutex);

    free(session);
}

/*
 * Find volume by name
 */
static struct VolumeEntry* FindVolume(FileSharingServer* server, const char* volumeName) {
    for (int i = 0; i < MAX_VOLUMES; i++) {
        if (server->volumes[i].active && strcmp(server->volumes[i].name, volumeName) == 0) {
            return &server->volumes[i];
        }
    }
    return NULL;
}

/*
 * Worker thread
 */
static void* WorkerThread(void* arg) {
    FileSharingServer* server = (FileSharingServer*)arg;

    while (server->workersRunning) {
        // Process pending requests, timeouts, etc.
        usleep(100000); // 100ms
    }

    return NULL;
}

/*
 * Handle open volume request
 */
static NetworkExtensionError HandleOpenVolume(AFPSession* session,
                                               const void* data,
                                               size_t dataLength) {
    // TODO: Implement volume opening
    return kNetworkExtensionNoError;
}

/*
 * File I/O Operations - Stub implementations
 */

NetworkExtensionError AFPCreateFile(AFPVolume* volume,
                                     uint32_t parentDirID,
                                     const char* fileName,
                                     bool hardCreate) {
    // TODO: Implement file creation
    return kNetworkExtensionNotSupported;
}

NetworkExtensionError AFPOpenFork(AFPVolume* volume,
                                   uint32_t parentDirID,
                                   const char* fileName,
                                   bool resourceFork,
                                   uint16_t accessMode,
                                   AFPFile** file) {
    // TODO: Implement fork opening
    return kNetworkExtensionNotSupported;
}

void AFPCloseFork(AFPFile* file) {
    if (file && file->fileHandle) {
        fclose(file->fileHandle);
        free(file);
    }
}

NetworkExtensionError AFPRead(AFPFile* file,
                               uint32_t offset,
                               uint32_t count,
                               void* buffer,
                               uint32_t* actualCount) {
    // TODO: Implement file reading
    return kNetworkExtensionNotSupported;
}

NetworkExtensionError AFPWrite(AFPFile* file,
                                uint32_t offset,
                                uint32_t count,
                                const void* buffer,
                                uint32_t* actualCount) {
    // TODO: Implement file writing
    return kNetworkExtensionNotSupported;
}