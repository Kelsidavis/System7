/*
 * PlatformAbstraction.h
 *
 * Platform abstraction layer for Edition Manager
 * Provides cross-platform compatibility for modern data sharing systems
 */

#ifndef __PLATFORM_ABSTRACTION_H__
#define __PLATFORM_ABSTRACTION_H__

#include "EditionManager/EditionManager.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Platform-Specific Data Sharing Integration
 */

/* Windows OLE/COM integration */
#ifdef _WIN32
typedef struct {
    void* oleObject;                /* IDataObject interface */
    void* oleContainer;             /* IOleContainer interface */
    uint32_t clipboardFormat;       /* Windows clipboard format */
} WindowsDataSharingHandle;

OSErr InitializeWindowsOLE(void);
OSErr CreateOLEDataObject(SectionHandle sectionH, void** oleObject);
OSErr UpdateOLEData(void* oleObject, FormatType format, const void* data, Size dataSize);
OSErr ConnectToOLEContainer(const EditionContainerSpec* container, void** oleContainer);
#endif

/* macOS Pasteboard/NSPasteboard integration */
#ifdef __APPLE__
typedef struct {
    void* pasteboard;               /* NSPasteboard reference */
    void* pasteboardItem;           /* NSPasteboardItem reference */
    char* uniformTypeIdentifier;    /* UTI for the data type */
} MacOSDataSharingHandle;

OSErr InitializeMacOSPasteboard(void);
OSErr CreatePasteboardItem(SectionHandle sectionH, void** pasteboardItem);
OSErr UpdatePasteboardData(void* pasteboardItem, FormatType format, const void* data, Size dataSize);
OSErr ReadFromPasteboard(void* pasteboard, FormatType format, void** data, Size* dataSize);
#endif

/* Linux D-Bus integration */
#ifdef __linux__
typedef struct {
    void* dbusConnection;           /* D-Bus connection */
    char* serviceName;              /* D-Bus service name */
    char* objectPath;               /* D-Bus object path */
    void* interface;                /* D-Bus interface */
} LinuxDataSharingHandle;

OSErr InitializeDBusConnection(void);
OSErr CreateDBusService(SectionHandle sectionH, const char* serviceName);
OSErr PublishDataViaDBus(const char* serviceName, FormatType format, const void* data, Size dataSize);
OSErr SubscribeToDBusService(const char* serviceName, SectionHandle subscriberH);
#endif

/*
 * Cloud Storage Integration
 */

/* Cloud storage providers */
typedef enum {
    kCloudProviderLocal,            /* Local storage only */
    kCloudProviderDropbox,          /* Dropbox integration */
    kCloudProviderGoogleDrive,      /* Google Drive integration */
    kCloudProviderOneDrive,         /* Microsoft OneDrive integration */
    kCloudProviderICloud,           /* Apple iCloud integration */
    kCloudProviderGeneric           /* Generic cloud storage */
} CloudStorageProvider;

typedef struct {
    CloudStorageProvider provider;  /* Cloud storage provider */
    char* apiKey;                   /* API key for authentication */
    char* accessToken;              /* Access token */
    char* refreshToken;             /* Refresh token */
    char* basePath;                 /* Base path for edition files */
    bool syncEnabled;               /* Automatic sync enabled */
    uint32_t syncInterval;          /* Sync interval in seconds */
} CloudStorageConfig;

OSErr InitializeCloudStorage(CloudStorageProvider provider, const CloudStorageConfig* config);
OSErr UploadEditionToCloud(const EditionContainerSpec* container);
OSErr DownloadEditionFromCloud(const EditionContainerSpec* container);
OSErr SyncEditionWithCloud(const EditionContainerSpec* container, bool forceSync);
OSErr GetCloudSyncStatus(const EditionContainerSpec* container, bool* isSynced, TimeStamp* lastSync);

/*
 * Network Data Sharing
 */

/* Network protocols */
typedef enum {
    kNetworkProtocolTCP,            /* TCP/IP */
    kNetworkProtocolUDP,            /* UDP */
    kNetworkProtocolHTTP,           /* HTTP/HTTPS */
    kNetworkProtocolWebSocket,      /* WebSocket */
    kNetworkProtocolCustom          /* Custom protocol */
} NetworkProtocol;

typedef struct {
    NetworkProtocol protocol;       /* Network protocol */
    char* serverAddress;            /* Server address */
    uint16_t port;                  /* Server port */
    char* username;                 /* Authentication username */
    char* password;                 /* Authentication password */
    bool useEncryption;             /* Use encrypted communication */
    uint32_t timeoutMs;             /* Connection timeout */
} NetworkConfig;

OSErr InitializeNetworkSharing(const NetworkConfig* config);
OSErr PublishEditionOnNetwork(SectionHandle publisherH, const NetworkConfig* config);
OSErr SubscribeToNetworkEdition(const char* networkAddress, SectionHandle subscriberH);
OSErr SendNetworkNotification(const char* targetAddress, const NotificationEvent* event);

/*
 * Real-Time Collaboration
 */

/* Collaboration frameworks */
typedef enum {
    kCollabFrameworkWebRTC,         /* WebRTC for real-time communication */
    kCollabFrameworkShareJS,        /* ShareJS for operational transforms */
    kCollabFrameworkYJS,            /* Y.js for CRDTs */
    kCollabFrameworkCustom          /* Custom collaboration framework */
} CollaborationFramework;

typedef struct {
    CollaborationFramework framework; /* Collaboration framework */
    char* sessionId;                /* Collaboration session ID */
    char* serverUrl;                /* Collaboration server URL */
    void* collaborationContext;     /* Framework-specific context */
    bool conflictResolution;        /* Automatic conflict resolution */
} CollaborationConfig;

OSErr InitializeCollaboration(const CollaborationConfig* config);
OSErr JoinCollaborationSession(const char* sessionId, SectionHandle sectionH);
OSErr LeaveCollaborationSession(SectionHandle sectionH);
OSErr BroadcastChange(SectionHandle sectionH, FormatType format, const void* changeData, Size dataSize);
OSErr ReceiveCollaborationUpdate(SectionHandle sectionH, void** updateData, Size* dataSize);

/*
 * Modern UI Framework Integration
 */

/* UI frameworks */
typedef enum {
    kUIFrameworkNative,             /* Native platform UI */
    kUIFrameworkQt,                 /* Qt framework */
    kUIFrameworkGTK,                /* GTK+ framework */
    kUIFrameworkElectron,           /* Electron framework */
    kUIFrameworkWeb                 /* Web-based UI */
} UIFramework;

typedef struct {
    UIFramework framework;          /* UI framework */
    void* parentWindow;             /* Parent window handle */
    void* uiContext;                /* Framework-specific context */
    bool useNativeDialogs;          /* Use native file dialogs */
} UIConfig;

OSErr InitializePlatformUI(const UIConfig* config);
OSErr ShowPlatformFileDialog(bool isOpen, const char* title, FSSpec* selectedFile);
OSErr ShowPlatformMessageBox(const char* title, const char* message, int* result);
OSErr CreatePlatformPreviewWidget(const EditionContainerSpec* container, void** widget);

/*
 * Security and Permissions
 */

/* Security models */
typedef enum {
    kSecurityModelNone,             /* No security */
    kSecurityModelBasic,            /* Basic file permissions */
    kSecurityModelSandbox,          /* Sandboxed access */
    kSecurityModelEntitlements      /* Entitlement-based access */
} SecurityModel;

typedef struct {
    SecurityModel model;            /* Security model */
    char* appIdentifier;            /* Application identifier */
    char* certificatePath;          /* Security certificate path */
    bool requireEncryption;         /* Require data encryption */
    bool requireSigning;            /* Require digital signatures */
} SecurityConfig;

OSErr InitializePlatformSecurity(const SecurityConfig* config);
OSErr ValidateFileAccess(const FSSpec* fileSpec, bool forWriting, bool* hasAccess);
OSErr EncryptEditionData(const void* inputData, Size inputSize, void** outputData, Size* outputSize);
OSErr DecryptEditionData(const void* inputData, Size inputSize, void** outputData, Size* outputSize);
OSErr SignEditionData(const void* data, Size dataSize, void** signature, Size* signatureSize);
OSErr VerifyEditionSignature(const void* data, Size dataSize, const void* signature, Size signatureSize, bool* isValid);

/*
 * File System Integration
 */

/* File system features */
typedef struct {
    bool supportsExtendedAttributes; /* Extended file attributes */
    bool supportsFileWatching;      /* File system watching */
    bool supportsSymlinks;          /* Symbolic links */
    bool supportsHardlinks;         /* Hard links */
    bool supportsCompression;       /* File compression */
    bool supportsEncryption;        /* File encryption */
} FileSystemCapabilities;

OSErr GetFileSystemCapabilities(const char* path, FileSystemCapabilities* capabilities);
OSErr SetFileExtendedAttribute(const FSSpec* fileSpec, const char* attributeName, const void* value, Size valueSize);
OSErr GetFileExtendedAttribute(const FSSpec* fileSpec, const char* attributeName, void** value, Size* valueSize);
OSErr StartFileWatching(const char* path, void* callback, void* userData);
OSErr StopFileWatching(const char* path);

/*
 * Threading and Concurrency
 */

/* Thread pool for background operations */
typedef struct {
    int32_t threadCount;            /* Number of worker threads */
    int32_t maxQueueSize;           /* Maximum task queue size */
    bool useThreadPool;             /* Use thread pool vs individual threads */
} ThreadingConfig;

OSErr InitializePlatformThreading(const ThreadingConfig* config);
OSErr SubmitBackgroundTask(void (*taskFunction)(void*), void* taskData);
OSErr WaitForBackgroundTasks(uint32_t timeoutMs);
OSErr GetThreadingStatistics(uint32_t* activeTasks, uint32_t* queuedTasks);

/*
 * Memory Management
 */

/* Memory allocation strategies */
typedef enum {
    kMemoryStrategyStandard,        /* Standard malloc/free */
    kMemoryStrategyPooled,          /* Memory pooling */
    kMemoryStrategyMapped,          /* Memory-mapped files */
    kMemoryStrategyShared           /* Shared memory */
} MemoryStrategy;

typedef struct {
    MemoryStrategy strategy;        /* Memory allocation strategy */
    Size poolSize;                  /* Memory pool size */
    Size maxAllocation;             /* Maximum single allocation */
    bool useGarbageCollection;      /* Use garbage collection */
} MemoryConfig;

OSErr InitializePlatformMemory(const MemoryConfig* config);
void* PlatformAllocateMemory(Size size);
void PlatformFreeMemory(void* ptr);
OSErr PlatformResizeMemory(void** ptr, Size newSize);
OSErr GetMemoryStatistics(Size* totalAllocated, Size* peakUsage, Size* currentUsage);

/*
 * Logging and Diagnostics
 */

/* Logging levels */
typedef enum {
    kLogLevelNone,                  /* No logging */
    kLogLevelError,                 /* Error messages only */
    kLogLevelWarning,               /* Warnings and errors */
    kLogLevelInfo,                  /* Informational messages */
    kLogLevelDebug,                 /* Debug messages */
    kLogLevelVerbose                /* Verbose debugging */
} LogLevel;

typedef struct {
    LogLevel level;                 /* Logging level */
    char* logFilePath;              /* Log file path */
    bool useSystemLog;              /* Use system logging */
    bool useConsole;                /* Log to console */
    Size maxLogSize;                /* Maximum log file size */
    int32_t maxLogFiles;            /* Maximum number of log files */
} LoggingConfig;

OSErr InitializePlatformLogging(const LoggingConfig* config);
void PlatformLog(LogLevel level, const char* format, ...);
OSErr RotateLogFiles(void);
OSErr GetLogStatistics(Size* logSize, int32_t* logCount);

/*
 * Platform Detection and Capabilities
 */

/* Platform types */
typedef enum {
    kPlatformUnknown,
    kPlatformMacOS,
    kPlatformWindows,
    kPlatformLinux,
    kPlatformFreeBSD,
    kPlatformWeb
} PlatformType;

typedef struct {
    PlatformType platform;          /* Platform type */
    int32_t majorVersion;           /* Major OS version */
    int32_t minorVersion;           /* Minor OS version */
    int32_t buildNumber;            /* Build number */
    char* platformString;           /* Platform string */
    bool is64Bit;                   /* 64-bit platform */
    bool isLittleEndian;            /* Little endian byte order */
} PlatformInfo;

OSErr GetPlatformInfo(PlatformInfo* info);
bool IsPlatformFeatureSupported(const char* featureName);
OSErr GetPlatformSpecificPath(const char* pathType, char* path, Size pathSize);

/*
 * Constants and Configuration
 */

#define kMaxNetworkConnections 100      /* Maximum network connections */
#define kDefaultNetworkTimeout 30000    /* Default network timeout (ms) */
#define kMaxCloudSyncRetries 3          /* Maximum cloud sync retries */
#define kDefaultThreadPoolSize 4        /* Default thread pool size */
#define kMaxMemoryPoolSize (64*1024*1024) /* Maximum memory pool size */

/* Configuration file paths */
#define kConfigFileName "EditionManager.conf"
#define kLogFileName "EditionManager.log"
#define kCacheDirectoryName "EditionCache"

/* Error codes for platform abstraction */
enum {
    platformNotSupportedErr = -600, /* Platform not supported */
    platformConfigErr = -601,       /* Platform configuration error */
    platformSecurityErr = -602,     /* Platform security error */
    platformNetworkErr = -603,      /* Platform network error */
    platformUIErr = -604            /* Platform UI error */
};

#ifdef __cplusplus
}
#endif

#endif /* __PLATFORM_ABSTRACTION_H__ */