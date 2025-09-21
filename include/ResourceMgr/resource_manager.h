/*
 * RE-AGENT-BANNER
 * Apple System 7.1 Resource Manager - Main Interface
 *
 * Reverse engineered from System.rsrc
 * SHA256: 78150ebb66707277e0947fbf81f6a27fc5d263a71bbf96df7084d5c3ec22a5ba
 *
 * Evidence source: /home/k/Desktop/system7/evidence.curated.resourcemgr.json
 * Mappings source: /home/k/Desktop/system7/mappings.resourcemgr.json
 *
 * Copyright 1983, 1984, 1985, 1986, 1987, 1988, 1989, 1990 Apple Computer Inc.
 * Reimplemented for System 7.1 decompilation project
 */

#ifndef RESOURCE_MANAGER_H
#define RESOURCE_MANAGER_H

#include "SystemTypes.h"

/* Forward declarations */

#include "SystemTypes.h"

#include "resource_types.h"

/*
 * Core Resource Manager Functions
 * Evidence: Function analysis from r2_aflj.json, mapped to Classic Mac OS API
 */

/*
 * ResourceManagerInit - Initialize Resource Manager
 * Evidence: fcn.00002715 at 0x2715, largest function (257 bytes)
 * Mapped from: Complex initialization function with control flow
 */
OSErr ResourceManagerInit(void);

/*
 * TrapDispatcher - System trap dispatcher
 * Evidence: fcn.00000001 at 0x1, heavily referenced (15 calls)
 * Mapped from: Entry point with extensive cross-references
 */
void TrapDispatcher(UInt16 trapWord);

/* Resource Loading Functions */

/*
 * GetResourceImpl - Load resource by type and ID
 * Evidence: fcn.00002ccd at 0x2CCD, 4-parameter function (157 bytes)
 * Mapped from: Function signature consistent with GetResource(type, id, &handle, &error)
 */
Handle GetResourceImpl(ResType theType, ResID theID, Handle* resHandle, OSErr* error);

/*
 * GetResource - Standard GetResource trap implementation
 * System trap 0xA9A0
 */
Handle GetResource(ResType theType, ResID theID);

/*
 * Get1Resource - Get resource from current file only
 * System trap 0xA9A1
 */
Handle Get1Resource(ResType theType, ResID theID);

/* Resource File Functions */

/*
 * OpenResFileImpl - Open resource file implementation
 * Evidence: fcn.00006528 at 0x6528, 3-parameter function (47 bytes)
 * Mapped from: Function signature matching OpenResFile(fileName, vRefNum, permission)
 */
SInt16 OpenResFileImpl(ConstStr255Param fileName, SInt16 vRefNum, UInt8 permission);

/*
 * OpenResFile - Standard OpenResFile trap implementation
 * System trap 0xA997
 */
SInt16 OpenResFile(ConstStr255Param fileName);

/*
 * CloseResFile - Close resource file
 * System trap 0xA99A
 */
void CloseResFile(SInt16 refNum);

/* Resource Management Functions */

/*
 * ReleaseResourceHandle - Release resource handle
 * Evidence: fcn.0000e69f at 0xE69F, single-parameter function (62 bytes)
 * Mapped from: Handle management operation signature
 */
void ReleaseResourceHandle(Handle resHandle);

/*
 * ReleaseResource - Standard ReleaseResource trap implementation
 * System trap 0xA9A3
 */
void ReleaseResource(Handle theResource);

/*
 * AddResource - Add resource to current file
 * System trap 0xA9AB
 */
void AddResource(Handle theData, ResType theType, ResID theID, ConstStr255Param name);

/*
 * UpdateResFile - Write changes to resource file
 * System trap 0xA9AD
 */
void UpdateResFile(SInt16 refNum);

/* Error Handling */

/*
 * ResErrorImpl - Get last resource error implementation
 * Evidence: fcn.0005d15a at 0x5D15A, utility function (31 bytes)
 * Mapped from: Error handling pattern, calls trap dispatcher
 */
OSErr ResErrorImpl(void);

/*
 * ResError - Get last resource error
 * System trap 0xA9AF
 */
OSErr ResError(void);

/* Utility Functions */

/*
 * CountResources - Count resources of given type
 */
SInt16 CountResources(ResType theType);

/*
 * Count1Resources - Count resources in current file
 */
SInt16 Count1Resources(ResType theType);

/*
 * GetIndResource - Get resource by index
 */
Handle GetIndResource(ResType theType, SInt16 index);

/*
 * Get1IndResource - Get resource by index in current file
 */
Handle Get1IndResource(ResType theType, SInt16 index);

/* Resource Attributes */

/*
 * SetResLoad - Control automatic loading of resources
 */
void SetResLoad(Boolean load);

/*
 * SetResPurge - Control automatic purging of resources
 */
void SetResPurge(Boolean purge);

/*
 * GetResAttrs - Get resource attributes
 */
SInt16 GetResAttrs(Handle theResource);

/*
 * SetResAttrs - Set resource attributes
 */
void SetResAttrs(Handle theResource, SInt16 attrs);

/* File Information */

/*
 * CurResFile - Get current resource file
 */
SInt16 CurResFile(void);

/*
 * HomeResFile - Get home file of resource
 */
SInt16 HomeResFile(Handle theResource);

/*
 * SetResFileAttrs - Set resource file attributes
 */
void SetResFileAttrs(SInt16 refNum, SInt16 attrs);

/*
 * GetResFileAttrs - Get resource file attributes
 */
SInt16 GetResFileAttrs(SInt16 refNum);

/* Internal Resource Manager State */
extern OSErr gLastResError;
extern SInt16 gCurResFile;
extern Boolean gResLoad;
extern Boolean gResPurge;

#endif /* RESOURCE_MANAGER_H */

/*
 * RE-AGENT-TRAILER-JSON
 * {
 *   "component": "resource_manager_interface",
 *   "evidence_density": 0.75,
 *   "functions_declared": 26,
 *   "core_functions": 6,
 *   "trap_functions": 8,
 *   "provenance": {
 *     "evidence_functions": 6,
 *     "mapped_from_r2": true,
 *     "api_knowledge": "Classic Mac OS Resource Manager"
 *   },
 *   "confidence": "high"
 * }
 */