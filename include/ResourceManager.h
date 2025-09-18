/*
 * ResourceManager.h - Apple Macintosh System 7.1 Resource Manager Public API
 *
 * Portable C implementation for ARM64 and x86_64 platforms
 * Based on analysis of Mac OS System 7.1 source code
 *
 * This implementation provides complete Resource Manager functionality including:
 * - Resource fork access and management
 * - Automatic decompression of compressed resources ('dcmp' resources)
 * - Handle-based memory management
 * - Multi-file resource chain support
 *
 * Copyright Notice: This is a reimplementation for research and compatibility purposes.
 */

#ifndef RESOURCE_MANAGER_H
#define RESOURCE_MANAGER_H

#include "MacTypes.h"
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Type Definitions ------------------------------------------------------------ */
/* Note: Basic types are defined in MacTypes.h */

/* Additional resource attributes beyond those in MacTypes.h */
#define resExtended     0x01    /* Resource has extended attributes (internal) */
#define resCompressed   0x01    /* Resource is compressed (extended attribute) */
#define resIsResource   0x20    /* Handle is a resource handle (internal MemMgr bit) */

/* Additional Resource Manager Error Codes beyond MacTypes.h */
enum {
    badExtResource      = -185,   /* Bad extended resource format */
    inputOutOfBounds    = -190,   /* Input out of bounds during decompression */
    outputOutOfBounds   = -191    /* Output out of bounds during decompression */
};

/* Resource Map Attributes */
#define mapReadOnly     0x0080  /* Map is read-only */
#define mapCompact      0x0040  /* Map needs compaction */
#define mapChanged      0x0020  /* Map has been changed */

/* ---- Resource Information Structures --------------------------------------------- */

/* Resource specification */
typedef struct {
    ResType  resType;           /* Resource type */
    ResID    resID;             /* Resource ID */
} ResSpec;

/* Resource information */
typedef struct {
    ResType  resType;           /* Resource type */
    ResID    resID;             /* Resource ID */
    char     name[256];         /* Resource name (Pascal string converted to C string) */
    ResAttributes attrs;        /* Resource attributes */
    uint32_t dataSize;          /* Size of resource data */
    RefNum   homeFile;          /* Home resource file */
} ResInfo;

/* ---- Core Resource Loading Functions --------------------------------------------- */

/* Get a resource by type and ID */
Handle GetResource(ResType theType, ResID theID);

/* Get a resource from the current resource file only */
Handle Get1Resource(ResType theType, ResID theID);

/* Get a resource by name */
Handle GetNamedResource(ResType theType, const char* name);

/* Get a resource from current file by name */
Handle Get1NamedResource(ResType theType, const char* name);

/* Load a resource into memory */
void LoadResource(Handle theResource);

/* Release resource memory (make purgeable) */
void ReleaseResource(Handle theResource);

/* Detach resource from Resource Manager */
void DetachResource(Handle theResource);

/* Get size of resource on disk */
int32_t GetResourceSizeOnDisk(Handle theResource);

/* Get actual size of resource in memory */
int32_t GetMaxResourceSize(Handle theResource);

/* ---- Resource Information Functions ---------------------------------------------- */

/* Get information about a resource */
void GetResInfo(Handle theResource, ResID* theID, ResType* theType, char* name);

/* Set resource information */
void SetResInfo(Handle theResource, ResID theID, const char* name);

/* Get resource attributes */
ResAttributes GetResAttrs(Handle theResource);

/* Set resource attributes */
void SetResAttrs(Handle theResource, ResAttributes attrs);

/* Get resource data (for reading) */
void* GetResourceData(Handle theResource);

/* ---- Resource File Management Functions ------------------------------------------ */

/* Open a resource file */
RefNum OpenResFile(const char* fileName);

/* Open a resource fork */
RefNum OpenRFPerm(const char* fileName, uint8_t vRefNum, int8_t permission);

/* Close a resource file */
void CloseResFile(RefNum refNum);

/* Create a new resource file */
void CreateResFile(const char* fileName);

/* Use a specific resource file */
void UseResFile(RefNum refNum);

/* Get current resource file */
RefNum CurResFile(void);

/* Get home file of a resource */
RefNum HomeResFile(Handle theResource);

/* Set whether to load resource data */
void SetResLoad(bool load);

/* Get current resource load state */
bool GetResLoad(void);

/* Update resource file */
void UpdateResFile(RefNum refNum);

/* Write a resource */
void WriteResource(Handle theResource);

/* Set resource file attributes */
void SetResFileAttrs(RefNum refNum, uint16_t attrs);

/* Get resource file attributes */
uint16_t GetResFileAttrs(RefNum refNum);

/* ---- Resource Creation and Modification Functions ------------------------------- */

/* Add a resource to current file */
void AddResource(Handle theData, ResType theType, ResID theID, const char* name);

/* Remove a resource */
void RemoveResource(Handle theResource);

/* Mark resource as changed */
void ChangedResource(Handle theResource);

/* Set resource purge level */
void SetResPurge(bool install);

/* Get resource purge state */
bool GetResPurge(void);

/* ---- Resource Enumeration Functions ---------------------------------------------- */

/* Count resources of a type */
int16_t CountResources(ResType theType);

/* Count resources in current file */
int16_t Count1Resources(ResType theType);

/* Get indexed resource */
Handle GetIndResource(ResType theType, int16_t index);

/* Get indexed resource from current file */
Handle Get1IndResource(ResType theType, int16_t index);

/* Count resource types */
int16_t CountTypes(void);

/* Count types in current file */
int16_t Count1Types(void);

/* Get indexed type */
void GetIndType(ResType* theType, int16_t index);

/* Get indexed type from current file */
void Get1IndType(ResType* theType, int16_t index);

/* ---- Unique ID Functions --------------------------------------------------------- */

/* Get a unique resource ID */
ResID UniqueID(ResType theType);

/* Get a unique ID in current file */
ResID Unique1ID(ResType theType);

/* ---- Resource Chain Management --------------------------------------------------- */

/* Get next resource file in chain */
RefNum GetNextResourceFile(RefNum curFile);

/* Get top resource file in chain */
RefNum GetTopResourceFile(void);

/* ---- Error Handling -------------------------------------------------------------- */

/* Get last Resource Manager error */
int16_t ResError(void);

/* Set Resource Manager error procedure */
typedef void (*ResErrProcPtr)(int16_t error);
void SetResErrProc(ResErrProcPtr proc);

/* ---- Compatibility Functions ----------------------------------------------------- */

/* Enable/disable ROM resource map */
void SetROMMapInsert(bool insert);

/* Get ROM map insert state */
bool GetROMMapInsert(void);

/* Set search depth for resources */
void SetResOneDeep(bool oneDeep);

/* Get resource search depth */
bool GetResOneDeep(void);

/* ---- Memory Manager Integration -------------------------------------------------- */

/* These functions integrate with the Memory Manager */
Handle NewHandle(int32_t size);
void DisposeHandle(Handle h);
void SetHandleSize(Handle h, int32_t newSize);
int32_t GetHandleSize(Handle h);
void HLock(Handle h);
void HUnlock(Handle h);
void HPurge(Handle h);
void HNoPurge(Handle h);
uint8_t HGetState(Handle h);
void HSetState(Handle h, uint8_t state);
void* StripAddress(void* ptr);

/* ---- Internal Structures (Exposed for Debugging) -------------------------------- */

/* Resource map entry */
typedef struct ResourceEntry {
    ResType  resType;           /* Resource type */
    ResID    resID;             /* Resource ID */
    ResAttributes attributes;   /* Resource attributes */
    uint32_t dataOffset;        /* Offset to data in resource fork */
    uint32_t dataSize;          /* Size of resource data */
    Handle   handle;            /* Handle to resource in memory */
    char*    name;              /* Resource name */
    struct ResourceEntry* next; /* Next resource in type list */
} ResourceEntry;

/* Resource type entry */
typedef struct ResourceType {
    ResType resType;            /* Resource type */
    int16_t count;              /* Number of resources of this type */
    ResourceEntry* resources;   /* List of resources */
    struct ResourceType* next;  /* Next type in map */
} ResourceType;

/* Resource map */
typedef struct ResourceMap {
    RefNum   fileRefNum;        /* File reference number */
    uint16_t attributes;        /* Map attributes */
    ResourceType* types;        /* List of resource types */
    uint32_t dataOffset;        /* Offset to resource data in file */
    uint32_t mapOffset;         /* Offset to resource map in file */
    struct ResourceMap* next;   /* Next map in chain */
    char*    fileName;          /* File name */
    FILE*    fileHandle;        /* C file handle */
    bool     inMemoryAttr;      /* Map loaded in memory */
    bool     decompressionEnabled; /* Allow decompression from this file */
} ResourceMap;

/* ---- Global Variables (Thread-Local Storage Recommended) ------------------------ */

/* These are implemented as thread-local in the .c file but exposed as regular externs here */

/* ---- Decompression Support ------------------------------------------------------- */

/* Decompression password bit for resource maps that can provide decompressors */
#define decompressionPasswordBit 7

/* Initialize Resource Manager */
void InitResourceManager(void);

/* Cleanup Resource Manager */
void CleanupResourceManager(void);

/* Install decompression hook (for BeforePatches.a compatibility) */
typedef Handle (*DecompressHookProc)(Handle compressedResource, ResourceEntry* entry);
void InstallDecompressHook(DecompressHookProc proc);

/* ---- Automatic Decompression Support --------------------------------------------- */

/* Enable/disable automatic decompression */
void SetAutoDecompression(bool enable);
bool GetAutoDecompression(void);

/* Flush decompression cache */
void ResourceManager_FlushDecompressionCache(void);

/* Set maximum decompression cache size */
void ResourceManager_SetDecompressionCacheSize(size_t maxItems);

/* Register a custom decompressor defproc */
int ResourceManager_RegisterDecompressor(uint16_t id, Handle defProcHandle);

/* CheckLoad hook for automatic decompression (internal but exposed for patching) */
Handle ResourceManager_CheckLoadHook(ResourceEntry* entry, ResourceMap* map);

#ifdef __cplusplus
}
#endif

#endif /* RESOURCE_MANAGER_H */