/*
 * File: VoiceResources.h
 *
 * Contains: Voice resource loading and management for Speech Manager
 *
 * Written by: Claude Code (Portable Implementation)
 *
 * Copyright: Based on Apple Computer, Inc. Speech Manager
 *
 * Description: This header provides voice resource management functionality
 *              including loading, caching, and resource format handling.
 */

#ifndef _VOICERESOURCES_H_
#define _VOICERESOURCES_H_

#include "SpeechManager.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ===== Voice Resource Constants ===== */

/* Voice resource types */
#define kVoiceResourceType      0x766F6963  /* 'voic' */
#define kVoiceDataType          0x76646174  /* 'vdat' */
#define kVoiceInfoType          0x76696E66  /* 'vinf' */
#define kVoicePhonemeType       0x7670686E  /* 'vphn' */
#define kVoiceLanguageType      0x766C616E  /* 'vlan' */
#define kVoiceVersionType       0x76657273  /* 'vers' */

/* Voice file formats */
typedef enum {
    kVoiceFormat_Unknown    = 0,
    kVoiceFormat_Classic    = 1,  /* Classic Mac OS voice format */
    kVoiceFormat_SAPI       = 2,  /* Microsoft SAPI voice format */
    kVoiceFormat_Festival   = 3,  /* Festival voice format */
    kVoiceFormat_eSpeak     = 4,  /* eSpeak voice format */
    kVoiceFormat_Flite      = 5,  /* CMU Flite voice format */
    kVoiceFormat_Custom     = 6   /* Custom voice format */
} VoiceResourceFormat;

/* Resource loading flags */
typedef enum {
    kVoiceLoad_Immediate    = (1 << 0),  /* Load immediately */
    kVoiceLoad_OnDemand     = (1 << 1),  /* Load on first use */
    kVoiceLoad_Cache        = (1 << 2),  /* Cache in memory */
    kVoiceLoad_Compressed   = (1 << 3),  /* Voice data is compressed */
    kVoiceLoad_Encrypted    = (1 << 4),  /* Voice data is encrypted */
    kVoiceLoad_Streaming    = (1 << 5),  /* Support streaming */
    kVoiceLoad_Validate     = (1 << 6)   /* Validate on load */
} VoiceLoadFlags;

/* Resource priorities */
typedef enum {
    kVoicePriority_Low      = 1,
    kVoicePriority_Normal   = 2,
    kVoicePriority_High     = 3,
    kVoicePriority_Critical = 4
} VoiceResourcePriority;

/* ===== Voice Resource Structures ===== */

/* Voice resource header */
typedef struct VoiceResourceHeader {
    OSType resourceType;        /* Resource type identifier */
    OSType resourceSubType;     /* Resource subtype */
    long resourceVersion;       /* Resource format version */
    long resourceSize;          /* Total resource size */
    long dataOffset;            /* Offset to voice data */
    long dataSize;              /* Size of voice data */
    VoiceResourceFormat format; /* Voice data format */
    VoiceLoadFlags flags;       /* Loading flags */
    long checksum;              /* Resource checksum */
    char reserved[32];          /* Reserved for future use */
} VoiceResourceHeader;

/* Voice resource information */
typedef struct VoiceResourceInfo {
    VoiceSpec voiceSpec;        /* Voice specification */
    VoiceResourceHeader header; /* Resource header */
    char resourceName[64];      /* Resource name */
    char filePath[256];         /* Path to resource file */
    long fileOffset;            /* Offset within file */
    time_t dateCreated;         /* Creation date */
    time_t dateModified;        /* Last modification date */
    void *platformInfo;         /* Platform-specific info */
} VoiceResourceInfo;

/* Voice resource data */
typedef struct VoiceResourceData {
    VoiceResourceInfo info;     /* Resource information */
    void *resourceData;         /* Loaded resource data */
    long resourceSize;          /* Size of loaded data */
    bool isLoaded;              /* Whether data is loaded */
    bool isCompressed;          /* Whether data is compressed */
    long refCount;              /* Reference count */
    time_t lastAccessed;        /* Last access time */
    void *decompressedData;     /* Decompressed data cache */
    long decompressedSize;      /* Size of decompressed data */
} VoiceResourceData;

/* Voice resource manager */
typedef struct VoiceResourceManager VoiceResourceManager;

/* Resource loading callback */
typedef OSErr (*VoiceResourceLoadProc)(const VoiceResourceInfo *info,
                                        void **resourceData, long *resourceSize,
                                        void *userData);

/* Resource validation callback */
typedef bool (*VoiceResourceValidateProc)(const VoiceResourceInfo *info,
                                           const void *resourceData, long resourceSize,
                                           void *userData);

/* ===== Voice Resource Management ===== */

/* Resource manager creation and disposal */
OSErr CreateVoiceResourceManager(VoiceResourceManager **manager);
OSErr DisposeVoiceResourceManager(VoiceResourceManager *manager);

/* Resource loading and unloading */
OSErr LoadVoiceResource(VoiceResourceManager *manager, const VoiceSpec *voice,
                        VoiceResourceData **resourceData);
OSErr UnloadVoiceResource(VoiceResourceManager *manager, const VoiceSpec *voice);
OSErr ReloadVoiceResource(VoiceResourceManager *manager, const VoiceSpec *voice);

/* Resource information */
OSErr GetVoiceResourceInfo(VoiceResourceManager *manager, const VoiceSpec *voice,
                           VoiceResourceInfo *info);
OSErr SetVoiceResourceInfo(VoiceResourceManager *manager, const VoiceSpec *voice,
                           const VoiceResourceInfo *info);

/* Resource enumeration */
OSErr CountVoiceResources(VoiceResourceManager *manager, long *resourceCount);
OSErr GetIndVoiceResource(VoiceResourceManager *manager, long index,
                          VoiceResourceInfo *info);

/* Resource searching */
OSErr FindVoiceResourceBySpec(VoiceResourceManager *manager, const VoiceSpec *voice,
                              VoiceResourceInfo *info);
OSErr FindVoiceResourceByName(VoiceResourceManager *manager, const char *resourceName,
                              VoiceResourceInfo *info);
OSErr FindVoiceResourceByPath(VoiceResourceManager *manager, const char *filePath,
                              VoiceResourceInfo *info);

/* ===== Resource File Management ===== */

/* Resource file operations */
OSErr OpenVoiceResourceFile(const char *filePath, short *fileRef);
OSErr CloseVoiceResourceFile(short fileRef);
OSErr ReadVoiceResourceFromFile(short fileRef, long resourceID, OSType resourceType,
                                void **resourceData, long *resourceSize);
OSErr WriteVoiceResourceToFile(short fileRef, long resourceID, OSType resourceType,
                               const void *resourceData, long resourceSize);

/* Resource file information */
OSErr GetVoiceResourceFileInfo(const char *filePath, VoiceResourceFormat *format,
                               long *resourceCount, long *fileVersion);
OSErr ValidateVoiceResourceFile(const char *filePath, bool *isValid, char **errorMessage);

/* Resource file scanning */
OSErr ScanVoiceResourceFile(const char *filePath, VoiceResourceInfo **resources,
                            long *resourceCount);
OSErr ScanVoiceResourceDirectory(const char *directoryPath, VoiceResourceInfo **resources,
                                 long *resourceCount, bool recursive);

/* ===== Resource Caching ===== */

/* Cache management */
OSErr CreateVoiceResourceCache(long maxCacheSize, VoiceResourceManager **cacheManager);
OSErr FlushVoiceResourceCache(VoiceResourceManager *manager);
OSErr PurgeVoiceResourceCache(VoiceResourceManager *manager, VoiceResourcePriority minPriority);

/* Cache statistics */
OSErr GetVoiceResourceCacheStats(VoiceResourceManager *manager, long *totalSize,
                                 long *usedSize, long *hitCount, long *missCount);
OSErr SetVoiceResourceCacheSize(VoiceResourceManager *manager, long maxCacheSize);

/* Cache control */
OSErr SetVoiceResourcePriority(VoiceResourceManager *manager, const VoiceSpec *voice,
                               VoiceResourcePriority priority);
OSErr GetVoiceResourcePriority(VoiceResourceManager *manager, const VoiceSpec *voice,
                               VoiceResourcePriority *priority);

/* ===== Resource Compression ===== */

/* Compression support */
OSErr CompressVoiceResource(const void *sourceData, long sourceSize,
                            void **compressedData, long *compressedSize);
OSErr DecompressVoiceResource(const void *compressedData, long compressedSize,
                              void **decompressedData, long *decompressedSize);

/* Compression information */
OSErr GetCompressionInfo(const void *compressedData, long compressedSize,
                         long *originalSize, OSType *compressionType);
bool IsVoiceResourceCompressed(const void *resourceData, long resourceSize);

/* ===== Resource Security ===== */

/* Encryption support */
OSErr EncryptVoiceResource(const void *sourceData, long sourceSize,
                           const char *password, void **encryptedData, long *encryptedSize);
OSErr DecryptVoiceResource(const void *encryptedData, long encryptedSize,
                           const char *password, void **decryptedData, long *decryptedSize);

/* Digital signatures */
OSErr SignVoiceResource(const void *resourceData, long resourceSize,
                        const void *privateKey, void **signature, long *signatureSize);
OSErr VerifyVoiceResourceSignature(const void *resourceData, long resourceSize,
                                   const void *signature, long signatureSize,
                                   const void *publicKey, bool *isValid);

/* Resource integrity */
OSErr CalculateVoiceResourceChecksum(const void *resourceData, long resourceSize,
                                     long *checksum);
OSErr ValidateVoiceResourceChecksum(const void *resourceData, long resourceSize,
                                    long expectedChecksum, bool *isValid);

/* ===== Resource Format Conversion ===== */

/* Format detection */
OSErr DetectVoiceResourceFormat(const void *resourceData, long resourceSize,
                                VoiceResourceFormat *format);
OSErr GetVoiceResourceFormatInfo(VoiceResourceFormat format, char **formatName,
                                 char **formatDescription, long *formatVersion);

/* Format conversion */
OSErr ConvertVoiceResourceFormat(const void *sourceData, long sourceSize,
                                 VoiceResourceFormat sourceFormat,
                                 VoiceResourceFormat targetFormat,
                                 void **convertedData, long *convertedSize);

/* Format validation */
OSErr ValidateVoiceResourceFormat(const void *resourceData, long resourceSize,
                                  VoiceResourceFormat expectedFormat, bool *isValid);

/* ===== Resource Callbacks ===== */

/* Resource loading progress callback */
typedef void (*VoiceResourceProgressProc)(const VoiceSpec *voice, long bytesLoaded,
                                           long totalBytes, void *userData);

/* Resource error callback */
typedef void (*VoiceResourceErrorProc)(const VoiceSpec *voice, OSErr error,
                                        const char *errorMessage, void *userData);

/* Callback registration */
OSErr SetVoiceResourceLoadCallback(VoiceResourceManager *manager,
                                   VoiceResourceLoadProc callback, void *userData);
OSErr SetVoiceResourceValidateCallback(VoiceResourceManager *manager,
                                       VoiceResourceValidateProc callback, void *userData);
OSErr SetVoiceResourceProgressCallback(VoiceResourceManager *manager,
                                       VoiceResourceProgressProc callback, void *userData);
OSErr SetVoiceResourceErrorCallback(VoiceResourceManager *manager,
                                    VoiceResourceErrorProc callback, void *userData);

/* ===== Resource Utilities ===== */

/* Resource copying */
OSErr CopyVoiceResource(const VoiceResourceData *source, VoiceResourceData **dest);
OSErr DuplicateVoiceResource(VoiceResourceManager *manager, const VoiceSpec *sourceVoice,
                             const VoiceSpec *destVoice);

/* Resource comparison */
int CompareVoiceResources(const VoiceResourceData *resource1,
                          const VoiceResourceData *resource2);
bool AreVoiceResourcesEqual(const VoiceResourceData *resource1,
                            const VoiceResourceData *resource2);

/* Resource serialization */
OSErr SerializeVoiceResource(const VoiceResourceData *resource, void **serializedData,
                             long *serializedSize);
OSErr DeserializeVoiceResource(const void *serializedData, long serializedSize,
                               VoiceResourceData **resource);

/* Resource metadata */
OSErr GetVoiceResourceMetadata(const VoiceResourceData *resource, OSType metadataType,
                               void **metadata, long *metadataSize);
OSErr SetVoiceResourceMetadata(VoiceResourceData *resource, OSType metadataType,
                               const void *metadata, long metadataSize);

/* ===== Resource Installation ===== */

/* Resource installation */
OSErr InstallVoiceResourceFromFile(const char *sourceFile, const char *destDirectory,
                                   VoiceSpec *installedVoice);
OSErr InstallVoiceResourceFromData(const void *resourceData, long resourceSize,
                                   const char *destDirectory, const char *resourceName,
                                   VoiceSpec *installedVoice);

/* Resource removal */
OSErr RemoveVoiceResource(const VoiceSpec *voice, const char *resourceDirectory);
OSErr UninstallVoiceResource(const VoiceSpec *voice);

/* Installation validation */
OSErr ValidateVoiceResourceInstallation(const VoiceSpec *voice, bool *isValid,
                                        char **errorMessage);

#ifdef __cplusplus
}
#endif

#endif /* _VOICERESOURCES_H_ */