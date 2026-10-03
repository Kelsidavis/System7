/*
 * ResourceManager.h - public Resource Manager declarations
 *
 * This is the canonical header for Resource Manager APIs. Implementations
 * are split across src/ResourceMgr/ and src/ResourceMgr/StringResources.c.
 */

#ifndef RESOURCE_MANAGER_H
#define RESOURCE_MANAGER_H

#include "SystemTypes.h"
#include "MemoryMgr/MemoryManager.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Errors the Resource Manager reports that SystemTypes.h does not define. */
enum {
    noMemForRsrc    = -188,  /* Not enough memory for the resource */
    badRefNum       = -1000, /* Bad resource file reference number */
    resFileNotOpen  = -1001  /* Resource file not open */
};

/* Resource map attributes */
#define mapReadOnly     0x0080
#define mapCompact      0x0040
#define mapChanged      0x0020

/* Setting up */
void InitResourceManager(void);
void ShutdownResourceManager(void);

/* Reading resources */
Handle GetResource(ResType theType, ResID theID);
Handle Get1Resource(ResType theType, ResID theID);
Handle GetNamedResource(ResType theType, ConstStr255Param name);
Handle Get1NamedResource(ResType theType, ConstStr255Param name);
void LoadResource(Handle theResource);
void ReleaseResource(Handle theResource);
void DetachResource(Handle theResource);
Size GetResourceSizeOnDisk(Handle theResource);
Size GetMaxResourceSize(Handle theResource);
void GetResInfo(Handle theResource, ResID *theID, ResType *theType, char* name);
SInt16 GetResAttrs(Handle theResource);
void SetResAttrs(Handle theResource, SInt16 attrs);
void ChangedResource(Handle theResource);
void SetResLoad(Boolean load);

/* Counting and indexing */
SInt16 CountResources(ResType theType);
SInt16 Count1Resources(ResType theType);
Handle GetIndResource(ResType theType, SInt16 index);
Handle Get1IndResource(ResType theType, SInt16 index);
SInt16 CountTypes(void);
SInt16 Count1Types(void);
void GetIndType(ResType *theType, SInt16 index);
void Get1IndType(ResType *theType, SInt16 index);
SInt16 UniqueID(ResType theType);
SInt16 Unique1ID(ResType theType);

/* Resource files */
SInt16 OpenResFile(ConstStr255Param fileName);
SInt16 FSpOpenResFile(const FSSpec* spec, SInt8 permission);
void FSpCreateResFile(const FSSpec* spec, OSType creator, OSType fileType, ScriptCode scriptTag);
void CloseResFile(SInt16 refNum);
SInt16 CurResFile(void);
SInt16 HomeResFile(Handle theResource);
void UseResFile(SInt16 refNum);
void UpdateResFile(SInt16 refNum);
SInt16 OpenResMemory(const unsigned char* data, UInt32 size);
void CloseResMemory(SInt16 refNum);

/* Adding and removing */
void AddResource(Handle theData, ResType theType, ResID theID, ConstStr255Param name);
void RemoveResource(Handle theResource);
void WriteResource(Handle theResource);
void SetResInfo(Handle theResource, ResID theID, ConstStr255Param name);
SInt16 GetResFileAttrs(SInt16 refNum);
void SetResFileAttrs(SInt16 refNum, SInt16 attrs);

/* Errors */
OSErr ResError(void);

/* String resources */
void GetString(StringPtr theString, SInt16 stringID);
void GetIndString(StringPtr theString, SInt16 strListID, SInt16 index);

#ifdef __cplusplus
}
#endif

#endif /* RESOURCE_MANAGER_H */
