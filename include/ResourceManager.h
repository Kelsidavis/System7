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

#include "SystemTypes.h"

#include "SystemTypes.h"

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

/* Resource Map Attributes */
#define mapReadOnly     0x0080  /* Map is read-only */
#define mapCompact      0x0040  /* Map needs compaction */
#define mapChanged      0x0020  /* Map has been changed */

/* ---- Resource Information Structures --------------------------------------------- */

/* Resource specification */

/* Resource information */

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
SInt32 GetResourceSizeOnDisk(Handle theResource);

/* Get actual size of resource in memory */
SInt32 GetMaxResourceSize(Handle theResource);

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

/* String resource functions */
void GetString(StringPtr theString, SInt16 stringID);
void GetIndString(StringPtr theString, SInt16 strListID, SInt16 index);

/* ---- Resource File Management Functions ------------------------------------------ */

/* Open a resource file */
SInt16 OpenResFile(const unsigned char* fileName);

/* Open a resource fork */
RefNum OpenRFPerm(const char* fileName, UInt8 vRefNum, SInt8 permission);

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
void SetResLoad(Boolean load);

/* Get current resource load state */
Boolean GetResLoad(void);

/* Update resource file */
void UpdateResFile(RefNum refNum);

/* Write a resource */
void WriteResource(Handle theResource);

/* Set resource file attributes */
void SetResFileAttrs(RefNum refNum, UInt16 attrs);

/* Get resource file attributes */
UInt16 GetResFileAttrs(RefNum refNum);

/* ---- Resource Creation and Modification Functions ------------------------------- */

/* Add a resource to current file */
void AddResource(Handle theData, ResType theType, ResID theID, const char* name);

/* Remove a resource */
void RemoveResource(Handle theResource);

/* Mark resource as changed */
void ChangedResource(Handle theResource);

/* Set resource purge level */
void SetResPurge(Boolean install);

/* Get resource purge state */
Boolean GetResPurge(void);

/* ---- Resource Enumeration Functions ---------------------------------------------- */

/* Count resources of a type */
SInt16 CountResources(ResType theType);

/* Count resources in current file */
SInt16 Count1Resources(ResType theType);

/* Get indexed resource */
Handle GetIndResource(ResType theType, SInt16 index);

/* Get indexed resource from current file */
Handle Get1IndResource(ResType theType, SInt16 index);

/* Count resource types */
SInt16 CountTypes(void);

/* Count types in current file */
SInt16 Count1Types(void);

/* Get indexed type */
void GetIndType(ResType* theType, SInt16 index);

/* Get indexed type from current file */
void Get1IndType(ResType* theType, SInt16 index);

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
SInt16 ResError(void);

/* Set Resource Manager error procedure */

void SetResErrProc(ResErrProcPtr proc);

/* ---- Compatibility Functions ----------------------------------------------------- */

/* Enable/disable ROM resource map */
void SetROMMapInsert(Boolean insert);

/* Get ROM map insert state */
Boolean GetROMMapInsert(void);

/* Set search depth for resources */
void SetResOneDeep(Boolean oneDeep);

/* Get resource search depth */
Boolean GetResOneDeep(void);

/* ---- Memory Manager Integration -------------------------------------------------- */

/* These functions integrate with the Memory Manager */
// Handle NewHandle(SInt32 size); // Moved to MacTypes.h
// void DisposeHandle(Handle h); // Moved to MacTypes.h
/* SetHandleSize defined in SystemTypes.h */
/* GetHandleSize defined in SystemTypes.h */
void HLock(Handle h);
void HUnlock(Handle h);
void HPurge(Handle h);
void HNoPurge(Handle h);
UInt8 HGetState(Handle h);
void HSetState(Handle h, UInt8 state);
void* StripAddress(void* ptr);

/* ---- Internal Structures (Exposed for Debugging) -------------------------------- */

/* Resource map entry */

/* Resource type entry */

/* Resource map */

/* ---- Global Variables (Thread-Local Storage Recommended) ------------------------ */

/* These are implemented as thread-local in the .c file but exposed as regular externs here */

/* ---- Decompression Support ------------------------------------------------------- */

/* Decompression password bit for resource maps that can provide decompressors */
#define decompressionPasswordBit 7

/* Initialize Resource Manager */
void InitResourceManager(void);

/* Open in-memory resource data as a resource file.
 * Data must remain valid for lifetime of the resource file.
 * Returns refNum >= 0 on success, -1 on error. */
SInt16 OpenResMemory(const unsigned char* data, UInt32 size);

/* Close an in-memory resource file opened with OpenResMemory */
void CloseResMemory(SInt16 refNum);

/* Cleanup Resource Manager */
void CleanupResourceManager(void);


void InstallDecompressHook(DecompressHookProc proc);

/* ---- Automatic Decompression Support --------------------------------------------- */

/* Enable/disable automatic decompression */
void SetAutoDecompression(Boolean enable);
Boolean GetAutoDecompression(void);

/* Flush decompression cache */
void ResourceManager_FlushDecompressionCache(void);

/* Set maximum decompression cache size */
void ResourceManager_SetDecompressionCacheSize(Size maxItems);

/* Register a custom decompressor defproc */
int ResourceManager_RegisterDecompressor(UInt16 id, Handle defProcHandle);

/* CheckLoad hook for automatic decompression (internal but exposed for patching) */
Handle ResourceManager_CheckLoadHook(ResourceEntry* entry, ResourceMap* map);

#ifdef __cplusplus
}
#endif

#endif /* RESOURCE_MANAGER_H */