/*
 * ResourceMgr.h - Public Resource Manager API
 * Based on Inside Macintosh: More Macintosh Toolbox
 * Clean-room implementation for read-only resource access
 */

#ifndef RESOURCE_MGR_H
#define RESOURCE_MGR_H

#include "SystemTypes.h"

/* Resource Manager Error Codes
 * Most are already defined in SystemTypes.h, only add missing ones
 */
enum {
    noMemForRsrc    = -188,  /* Not enough memory for resource */
    badRefNum       = -1000, /* Bad reference number */
    resFileNotOpen  = -1001  /* Resource file not open */
};

/* Resource Attributes are already defined in SystemTypes.h */

/* Core Resource Manager Functions */
/* Note: InitResourceManager returns void to match existing declaration */
void InitResourceManager(void);
void ShutdownResourceManager(void);

/* Resource Loading Functions */
Handle GetResource(ResType theType, ResID theID);
Handle Get1Resource(ResType theType, ResID theID);
Handle GetNamedResource(ResType theType, ConstStr255Param name);
Handle Get1NamedResource(ResType theType, ConstStr255Param name);

/* Resource Information */
/* Use char* for name to match existing declaration */
void GetResInfo(Handle theResource, ResID *theID, ResType *theType, char* name);
Size GetResourceSizeOnDisk(Handle theResource);
Size GetMaxResourceSize(Handle theResource);

/* Resource Release */
void ReleaseResource(Handle theResource);
void DetachResource(Handle theResource);
void LoadResource(Handle theResource);

/* Resource File Management */
SInt16 CurResFile(void);
SInt16 HomeResFile(Handle theResource);
void UseResFile(SInt16 refNum);
/* Note: OpenResFile declared in ResourceMgr/resource_manager.h */
// SInt16 OpenResFile(const unsigned char* fileName);
void CloseResFile(SInt16 refNum);
SInt16 FSpOpenResFile(const FSSpec* spec, SInt8 permission);
void FSpCreateResFile(const FSSpec* spec, OSType creator, OSType fileType, ScriptCode scriptTag);

/* Error Handling */
OSErr ResError(void);
void SetResLoad(Boolean load);

/* Resource Attributes (stubs for read-only) */
SInt16 GetResAttrs(Handle theResource);
void SetResAttrs(Handle theResource, SInt16 attrs);
void ChangedResource(Handle theResource);

/* Write operations (stubs returning errors for read-only) */
void AddResource(Handle theData, ResType theType, ResID theID, ConstStr255Param name);
void RemoveResource(Handle theResource);
void WriteResource(Handle theResource);
void UpdateResFile(SInt16 refNum);

/* Additional utilities */
SInt16 CountResources(ResType theType);
SInt16 Count1Resources(ResType theType);
Handle GetIndResource(ResType theType, SInt16 index);
Handle Get1IndResource(ResType theType, SInt16 index);
void GetIndType(ResType *theType, SInt16 index);
void Get1IndType(ResType *theType, SInt16 index);
SInt16 CountTypes(void);
SInt16 Count1Types(void);
SInt16 UniqueID(ResType theType);
SInt16 Unique1ID(ResType theType);

#endif /* RESOURCE_MGR_H */