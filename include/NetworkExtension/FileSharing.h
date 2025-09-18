/*
 * FileSharing.h - Network File Sharing (AFP) Implementation
 * Mac OS 7.1 Network Extension for AppleTalk file sharing
 *
 * Provides:
 * - Apple Filing Protocol (AFP) implementation
 * - Network file server and client functionality
 * - Volume and file access control
 * - Authentication and security
 * - Modern protocol bridging (SMB, NFS, WebDAV)
 */

#ifndef FILESHARING_H
#define FILESHARING_H

#include "NetworkExtension.h"
#include "AppleTalkStack.h"
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * AFP Constants
 */
#define AFP_MAX_VOLUMES 32
#define AFP_MAX_SESSIONS 64
#define AFP_MAX_FORKS 256
#define AFP_MAX_FILENAME_LENGTH 31
#define AFP_MAX_PATH_LENGTH 255
#define AFP_MAX_PASSWORD_LENGTH 32
#define AFP_MAX_USERNAME_LENGTH 31

/*
 * AFP Protocol Version
 */
#define AFP_VERSION_20 0x0014
#define AFP_VERSION_21 0x0015
#define AFP_VERSION_22 0x0016

/*
 * AFP Command Codes
 */
typedef enum {
    // Session commands
    kAFPByteRangeLock = 1,
    kAFPCloseVol = 2,
    kAFPCloseDir = 3,
    kAFPCloseFork = 4,
    kAFPCopyFile = 5,
    kAFPCreateDir = 6,
    kAFPCreateFile = 7,
    kAFPDelete = 8,
    kAFPEnumerate = 9,
    kAFPFlush = 10,
    kAFPFlushFork = 11,

    // File/Directory operations
    kAFPGetDirParms = 12,
    kAFPGetFileParms = 13,
    kAFPGetForkParms = 14,
    kAFPGetSrvrInfo = 15,
    kAFPGetSrvrParms = 16,
    kAFPGetVolParms = 17,
    kAFPLogin = 18,
    kAFPLoginCont = 19,
    kAFPLogout = 20,
    kAFPMapID = 21,
    kAFPMapName = 22,
    kAFPMoveAndRename = 23,
    kAFPOpenVol = 24,
    kAFPOpenDir = 25,
    kAFPOpenFork = 26,
    kAFPRead = 27,
    kAFPRename = 28,
    kAFPSetDirParms = 29,
    kAFPSetFileParms = 30,
    kAFPSetForkParms = 31,
    kAFPSetVolParms = 32,
    kAFPWrite = 33,
    kAFPGetFlDrParms = 34,
    kAFPSetFlDrParms = 35,

    // Desktop database
    kAFPDTOpen = 48,
    kAFPDTClose = 49,
    kAFPGetIcon = 51,
    kAFPGetIconInfo = 52,
    kAFPAddAPPL = 53,
    kAFPRmvAPPL = 54,
    kAFPGetAPPL = 55,
    kAFPAddComment = 56,
    kAFPRmvComment = 57,
    kAFPGetComment = 58
} AFPCommandCode;

/*
 * AFP Result Codes
 */
typedef enum {
    kAFPNoErr = 0,
    kAFPASPSessClosed = -1072,
    kAFPServerGoingDown = -1073,
    kAFPCantMove = -1074,
    kAFPDenyConflict = -1075,
    kAFPDirNotEmpty = -1076,
    kAFPDiskFull = -1077,
    kAFPEofErr = -1078,
    kAFPFileBusy = -1079,
    kAFPFlatVol = -1080,
    kAFPItemNotFound = -1081,
    kAFPLockErr = -1082,
    kAFPMiscErr = -1083,
    kAFPNoMoreLocks = -1084,
    kAFPNoServer = -1085,
    kAFPObjectExists = -1086,
    kAFPObjectNotFound = -1087,
    kAFPParmErr = -1088,
    kAFPRangeNotLocked = -1089,
    kAFPRangeOverlap = -1090,
    kAFPSessClosed = -1091,
    kAFPUserNotAuth = -1092,
    kAFPCallNotSupported = -1093,
    kAFPObjectTypeErr = -1094,
    kAFPTooManyFilesOpen = -1095,
    kAFPServerNotFound = -1096,
    kAFPVolumeNotFound = -1097,
    kAFPWrongVolumeType = -1098,
    kAFPObjectLocked = -1099,
    kAFPVolumeLocked = -1100,
    kAFPFilenamesTooLong = -1101,
    kAFPBadIDErr = -1102,
    kAFPPwdSameErr = -1103,
    kAFPPwdTooShortErr = -1104,
    kAFPPwdExpiredErr = -1105,
    kAFPInsideSharedErr = -1106,
    kAFPInsideTrashErr = -1107,
    kAFPPwdNeedsChangeErr = -1108,
    kAFPPwdPolicyErr = -1109,
    kAFPDiskQuotaExceeded = -1110
} AFPResultCode;

/*
 * AFP Volume Attributes
 */
typedef enum {
    kAFPVolReadOnly = 0x0001,
    kAFPVolHasVolumePassword = 0x0002,
    kAFPVolSupportsFileIDs = 0x0004,
    kAFPVolSupportsCatSearch = 0x0008,
    kAFPVolSupportsBlankAccessPrivs = 0x0010,
    kAFPVolSupportsUnixPrivs = 0x0020,
    kAFPVolSupportsUTF8Names = 0x0040
} AFPVolumeAttributes;

/*
 * AFP File/Directory Attributes
 */
typedef enum {
    kAFPInvisible = 0x0001,
    kAFPMultiUser = 0x0002,
    kAFPSystem = 0x0004,
    kAFPDAlreadyOpen = 0x0008,
    kAFPRAlreadyOpen = 0x0010,
    kAFPWriteInhibit = 0x0020,
    kAFPBackUpNeeded = 0x0040,
    kAFPRenameInhibit = 0x0080,
    kAFPDeleteInhibit = 0x0100,
    kAFPCopyProtected = 0x0400,
    kAFPSetClear = 0x8000
} AFPFileAttributes;

/*
 * AFP User Authentication Methods
 */
typedef enum {
    kAFPNoUserAuthent = 0,
    kAFPClearTxtPasswd = 1,
    kAFPRandNum = 2,
    kAFP2WayRandNum = 3
} AFPAuthMethod;

/*
 * Forward Declarations
 */
typedef struct FileSharingServer FileSharingServer;
typedef struct FileSharingClient FileSharingClient;
typedef struct AFPSession AFPSession;
typedef struct AFPVolume AFPVolume;
typedef struct AFPFile AFPFile;
typedef struct AFPDirectory AFPDirectory;

/*
 * File Information Structure
 */
typedef struct {
    char name[AFP_MAX_FILENAME_LENGTH + 1];
    uint32_t fileID;
    uint32_t parentID;
    uint16_t attributes;
    time_t createDate;
    time_t modifyDate;
    time_t backupDate;
    uint32_t dataForkLength;
    uint32_t resourceForkLength;
    uint32_t fileType;
    uint32_t fileCreator;
    uint16_t finderFlags;
    uint16_t unixPermissions;
} AFPFileInfo;

/*
 * Directory Information Structure
 */
typedef struct {
    char name[AFP_MAX_FILENAME_LENGTH + 1];
    uint32_t dirID;
    uint32_t parentID;
    uint16_t attributes;
    time_t createDate;
    time_t modifyDate;
    time_t backupDate;
    uint16_t ownerID;
    uint16_t groupID;
    uint16_t accessRights;
    uint16_t unixPermissions;
} AFPDirectoryInfo;

/*
 * Volume Information Structure
 */
typedef struct {
    char name[AFP_MAX_FILENAME_LENGTH + 1];
    uint16_t attributes;
    uint16_t signature;
    time_t createDate;
    time_t modifyDate;
    time_t backupDate;
    uint32_t volumeID;
    uint32_t bytesTotal;
    uint32_t bytesFree;
    uint32_t directoryCount;
    uint32_t fileCount;
} AFPVolumeInfo;

/*
 * Server Information Structure
 */
typedef struct {
    char serverName[AFP_MAX_FILENAME_LENGTH + 1];
    char machineType[16];
    char afpVersions[64];
    char uams[128];
    uint16_t flags;
    char iconBitmap[32];
} AFPServerInfo;

/*
 * Session Information Structure
 */
typedef struct {
    uint32_t sessionID;
    char userName[AFP_MAX_USERNAME_LENGTH + 1];
    uint16_t userID;
    uint16_t groupID;
    time_t loginTime;
    AppleTalkAddress clientAddress;
    AFPAuthMethod authMethod;
    bool authenticated;
} AFPSessionInfo;

/*
 * Callback Types
 */
typedef void (*FileSharingServerCallback)(AFPSession* session, AFPCommandCode command, void* userData);
typedef void (*FileSharingClientCallback)(NetworkExtensionError error, const void* data, size_t dataLength, void* userData);

/*
 * Global File Sharing Functions
 */

/**
 * Global initialization
 */
void FileSharingGlobalInit(void);

/*
 * File Sharing Server Functions
 */

/**
 * Create file sharing server
 */
NetworkExtensionError FileSharingServerCreate(FileSharingServer** server,
                                               AppleTalkStack* appleTalkStack);

/**
 * Destroy file sharing server
 */
void FileSharingServerDestroy(FileSharingServer* server);

/**
 * Start file sharing server
 */
NetworkExtensionError FileSharingServerStart(FileSharingServer* server,
                                              const char* serverName);

/**
 * Stop file sharing server
 */
void FileSharingServerStop(FileSharingServer* server);

/**
 * Add volume to server
 */
NetworkExtensionError FileSharingServerAddVolume(FileSharingServer* server,
                                                  const char* volumeName,
                                                  const char* volumePath,
                                                  AFPVolumeAttributes attributes,
                                                  const char* password);

/**
 * Remove volume from server
 */
NetworkExtensionError FileSharingServerRemoveVolume(FileSharingServer* server,
                                                     const char* volumeName);

/**
 * Set server callback
 */
void FileSharingServerSetCallback(FileSharingServer* server,
                                  FileSharingServerCallback callback,
                                  void* userData);

/**
 * Get server information
 */
NetworkExtensionError FileSharingServerGetInfo(FileSharingServer* server,
                                                AFPServerInfo* info);

/**
 * Get active sessions
 */
NetworkExtensionError FileSharingServerGetSessions(FileSharingServer* server,
                                                    AFPSessionInfo* sessions,
                                                    int maxSessions,
                                                    int* actualSessions);

/*
 * File Sharing Client Functions
 */

/**
 * Create file sharing client
 */
NetworkExtensionError FileSharingClientCreate(FileSharingClient** client,
                                               AppleTalkStack* appleTalkStack);

/**
 * Destroy file sharing client
 */
void FileSharingClientDestroy(FileSharingClient* client);

/**
 * Connect to server
 */
NetworkExtensionError FileSharingClientConnect(FileSharingClient* client,
                                                const char* serverName,
                                                const char* zoneName,
                                                AFPSession** session);

/**
 * Disconnect from server
 */
void FileSharingClientDisconnect(AFPSession* session);

/**
 * Login to server
 */
NetworkExtensionError FileSharingClientLogin(AFPSession* session,
                                              const char* userName,
                                              const char* password,
                                              AFPAuthMethod authMethod);

/**
 * Logout from server
 */
NetworkExtensionError FileSharingClientLogout(AFPSession* session);

/**
 * Get server information
 */
NetworkExtensionError FileSharingClientGetServerInfo(AFPSession* session,
                                                      AFPServerInfo* info);

/**
 * Get volume list
 */
NetworkExtensionError FileSharingClientGetVolumeList(AFPSession* session,
                                                      AFPVolumeInfo* volumes,
                                                      int maxVolumes,
                                                      int* actualVolumes);

/**
 * Open volume
 */
NetworkExtensionError FileSharingClientOpenVolume(AFPSession* session,
                                                   const char* volumeName,
                                                   const char* password,
                                                   AFPVolume** volume);

/**
 * Close volume
 */
void FileSharingClientCloseVolume(AFPVolume* volume);

/*
 * File and Directory Operations
 */

/**
 * Create directory
 */
NetworkExtensionError AFPCreateDirectory(AFPVolume* volume,
                                          uint32_t parentDirID,
                                          const char* dirName,
                                          uint32_t* newDirID);

/**
 * Delete file or directory
 */
NetworkExtensionError AFPDelete(AFPVolume* volume,
                                 uint32_t parentDirID,
                                 const char* name);

/**
 * Rename file or directory
 */
NetworkExtensionError AFPRename(AFPVolume* volume,
                                 uint32_t parentDirID,
                                 const char* oldName,
                                 const char* newName);

/**
 * Move file or directory
 */
NetworkExtensionError AFPMove(AFPVolume* volume,
                               uint32_t sourceDirID,
                               uint32_t destDirID,
                               const char* sourceName,
                               const char* destName);

/**
 * Get file parameters
 */
NetworkExtensionError AFPGetFileParams(AFPVolume* volume,
                                        uint32_t parentDirID,
                                        const char* fileName,
                                        AFPFileInfo* fileInfo);

/**
 * Set file parameters
 */
NetworkExtensionError AFPSetFileParams(AFPVolume* volume,
                                        uint32_t parentDirID,
                                        const char* fileName,
                                        const AFPFileInfo* fileInfo);

/**
 * Get directory parameters
 */
NetworkExtensionError AFPGetDirParams(AFPVolume* volume,
                                       uint32_t dirID,
                                       AFPDirectoryInfo* dirInfo);

/**
 * Set directory parameters
 */
NetworkExtensionError AFPSetDirParams(AFPVolume* volume,
                                       uint32_t dirID,
                                       const AFPDirectoryInfo* dirInfo);

/**
 * Enumerate directory
 */
NetworkExtensionError AFPEnumerate(AFPVolume* volume,
                                    uint32_t dirID,
                                    uint16_t startIndex,
                                    uint16_t maxCount,
                                    uint16_t bitmap,
                                    void* buffer,
                                    size_t bufferSize,
                                    uint16_t* actualCount);

/*
 * File I/O Operations
 */

/**
 * Create file
 */
NetworkExtensionError AFPCreateFile(AFPVolume* volume,
                                     uint32_t parentDirID,
                                     const char* fileName,
                                     bool hardCreate);

/**
 * Open fork
 */
NetworkExtensionError AFPOpenFork(AFPVolume* volume,
                                   uint32_t parentDirID,
                                   const char* fileName,
                                   bool resourceFork,
                                   uint16_t accessMode,
                                   AFPFile** file);

/**
 * Close fork
 */
void AFPCloseFork(AFPFile* file);

/**
 * Read from fork
 */
NetworkExtensionError AFPRead(AFPFile* file,
                               uint32_t offset,
                               uint32_t count,
                               void* buffer,
                               uint32_t* actualCount);

/**
 * Write to fork
 */
NetworkExtensionError AFPWrite(AFPFile* file,
                                uint32_t offset,
                                uint32_t count,
                                const void* buffer,
                                uint32_t* actualCount);

/**
 * Get fork parameters
 */
NetworkExtensionError AFPGetForkParams(AFPFile* file,
                                        uint32_t* forkLength,
                                        uint32_t* resourceLength);

/**
 * Set fork length
 */
NetworkExtensionError AFPSetForkParams(AFPFile* file,
                                        uint32_t forkLength);

/**
 * Flush fork
 */
NetworkExtensionError AFPFlushFork(AFPFile* file);

/*
 * Modern Protocol Bridge Functions
 */

/**
 * Enable SMB/CIFS bridge
 */
NetworkExtensionError FileSharingEnableSMB(FileSharingServer* server,
                                            uint16_t port);

/**
 * Enable NFS bridge
 */
NetworkExtensionError FileSharingEnableNFS(FileSharingServer* server,
                                            uint16_t port);

/**
 * Enable WebDAV bridge
 */
NetworkExtensionError FileSharingEnableWebDAV(FileSharingServer* server,
                                               uint16_t port,
                                               bool useSSL);

/**
 * Convert AFP path to modern path
 */
NetworkExtensionError FileSharingConvertPath(const char* afpPath,
                                              char* modernPath,
                                              size_t bufferSize);

#ifdef __cplusplus
}
#endif

#endif /* FILESHARING_H */