/*
 * FileManager.h - System 7.1 File Manager Portable C Implementation
 *
 * This header defines the public API for the Macintosh System 7.1 File Manager,
 * providing HFS (Hierarchical File System) support for modern platforms.
 *
 * Copyright (c) 2024 - Implementation for System 7.1 Portable
 * Derived from System 7 ROM analysis (Ghidra) architecture
 */

#ifndef __FILEMANAGER_H__
#define __FILEMANAGER_H__

#include "SystemTypes.h"

#include "SystemTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Platform configuration */
#define FM_PLATFORM_X86 1

/* Thread safety - we don't need pthread for single-threaded kernel */
/* pthread types are already defined in SystemTypes.h if needed */

/* Forward declarations */

/* File Manager specific types */

/* DirID is defined in SystemTypes.h as long */

/* Mac OS date/time (seconds since Jan 1, 1904) */

/* Additional File System Error Codes beyond MacTypes.h */

/* File attributes */

/* Volume attributes */

/* Fork types */

/* FInfo and FXInfo are defined in SystemTypes.h */

/* FSSpec - File System Specification */
/* FSSpec is defined in SystemTypes.h */

/* HFS Extent descriptor */

/* HFS Extent record (3 extents) */

/* ParamBlockRec, CInfoPBRec, WDPBRec, and FCBPBRec are defined in SystemTypes.h */

/* Volume mount info */

/* ============================================================================
 * File Manager Public API
 * ============================================================================ */

/* File System Initialization */
OSErr FM_Initialize(void);
OSErr FM_Shutdown(void);

/* File Operations - Basic */
OSErr FSOpen(ConstStr255Param fileName, VolumeRefNum vRefNum, FileRefNum* refNum);
OSErr FSClose(FileRefNum refNum);
OSErr FSRead(FileRefNum refNum, UInt32* count, void* buffer);
OSErr FSWrite(FileRefNum refNum, UInt32* count, const void* buffer);

/* File Operations - Extended */
OSErr FSOpenDF(ConstStr255Param fileName, VolumeRefNum vRefNum, FileRefNum* refNum);
OSErr FSOpenRF(ConstStr255Param fileName, VolumeRefNum vRefNum, FileRefNum* refNum);
OSErr FSCreate(ConstStr255Param fileName, VolumeRefNum vRefNum, UInt32 creator, UInt32 fileType);
OSErr FSDelete(ConstStr255Param fileName, VolumeRefNum vRefNum);
OSErr FSRename(ConstStr255Param oldName, ConstStr255Param newName, VolumeRefNum vRefNum);

/* File Position and Size */
OSErr FSGetFPos(FileRefNum refNum, UInt32* position);
OSErr FSSetFPos(FileRefNum refNum, UInt16 posMode, SInt32 posOffset);
OSErr FSGetEOF(FileRefNum refNum, UInt32* eof);
OSErr FSSetEOF(FileRefNum refNum, UInt32 eof);
OSErr FSAllocate(FileRefNum refNum, UInt32* count);

/* File Information */
OSErr FSGetFInfo(ConstStr255Param fileName, VolumeRefNum vRefNum, FInfo* fndrInfo);
OSErr FSSetFInfo(ConstStr255Param fileName, VolumeRefNum vRefNum, const FInfo* fndrInfo);
OSErr FSGetCatInfo(CInfoPBPtr paramBlock);
OSErr FSSetCatInfo(CInfoPBPtr paramBlock);

/* Directory Operations */
OSErr FSMakeFSSpec(VolumeRefNum vRefNum, DirID dirID, ConstStr255Param fileName, FSSpec* spec);
OSErr FSCreateDir(ConstStr255Param dirName, VolumeRefNum vRefNum, DirID* createdDirID);
OSErr FSDeleteDir(ConstStr255Param dirName, VolumeRefNum vRefNum);
OSErr FSGetWDInfo(WDRefNum wdRefNum, VolumeRefNum* vRefNum, DirID* dirID, UInt32* procID);
OSErr FSOpenWD(VolumeRefNum vRefNum, DirID dirID, UInt32 procID, WDRefNum* wdRefNum);
OSErr FSCloseWD(WDRefNum wdRefNum);

/* Volume Operations */
OSErr FSMount(UInt16 drvNum, void* buffer);
OSErr FSUnmount(VolumeRefNum vRefNum);
OSErr FSEject(VolumeRefNum vRefNum);
OSErr FSFlushVol(ConstStr255Param volName, VolumeRefNum vRefNum);
OSErr FSGetVInfo(VolumeRefNum vRefNum, StringPtr volName, UInt16* actualVRefNum, UInt32* freeBytes);
OSErr FSSetVol(ConstStr255Param volName, VolumeRefNum vRefNum);
OSErr FSGetVol(StringPtr volName, VolumeRefNum* vRefNum);

/* File ID Operations */
OSErr FSCreateFileIDRef(ConstStr255Param fileName, VolumeRefNum vRefNum, DirID dirID, FileID* fileID);
OSErr FSDeleteFileIDRef(ConstStr255Param fileName, VolumeRefNum vRefNum);
OSErr FSResolveFileIDRef(ConstStr255Param volName, VolumeRefNum vRefNum, FileID fileID, FSSpec* spec);
OSErr FSExchangeFiles(const FSSpec* file1, const FSSpec* file2);

/* High-level wrappers */
OSErr HGetFInfo(short vRefNum, long dirID, ConstStr255Param fileName, FInfo *fndrInfo);
OSErr DirCreate(short vRefNum, long parentDirID, ConstStr255Param directoryName, long *createdDirID);

/* Alias and Path Resolution */
OSErr FSResolveAliasFile(FSSpec* theSpec, Boolean resolveAliasChains, Boolean* targetIsFolder, Boolean* wasAliased);
OSErr FSMakeAlias(const FSSpec* fromFile, const FSSpec* target, void** alias);

/* Parameter Block Operations (Low-level) */
OSErr PBOpenSync(ParmBlkPtr paramBlock);
OSErr PBOpenAsync(ParmBlkPtr paramBlock);
OSErr PBCloseSync(ParmBlkPtr paramBlock);
OSErr PBCloseAsync(ParmBlkPtr paramBlock);
OSErr PBReadSync(ParmBlkPtr paramBlock);
OSErr PBReadAsync(ParmBlkPtr paramBlock);
OSErr PBWriteSync(ParmBlkPtr paramBlock);
OSErr PBWriteAsync(ParmBlkPtr paramBlock);
OSErr PBGetCatInfoSync(CInfoPBPtr paramBlock);
OSErr PBGetCatInfoAsync(CInfoPBPtr paramBlock);
OSErr PBSetCatInfoSync(CInfoPBPtr paramBlock);
OSErr PBSetCatInfoAsync(CInfoPBPtr paramBlock);

/* HFS-specific Parameter Block Operations */
OSErr PBHOpenDFSync(ParmBlkPtr paramBlock);
OSErr PBHOpenDFAsync(ParmBlkPtr paramBlock);
OSErr PBHOpenRFSync(ParmBlkPtr paramBlock);
OSErr PBHOpenRFAsync(ParmBlkPtr paramBlock);
OSErr PBHCreateSync(ParmBlkPtr paramBlock);
OSErr PBHCreateAsync(ParmBlkPtr paramBlock);
OSErr PBHDeleteSync(ParmBlkPtr paramBlock);
OSErr PBHDeleteAsync(ParmBlkPtr paramBlock);
OSErr PBHRenameSync(ParmBlkPtr paramBlock);
OSErr PBHRenameAsync(ParmBlkPtr paramBlock);

/* Working Directory Parameter Block Operations */
OSErr PBOpenWDSync(WDPBPtr paramBlock);
OSErr PBOpenWDAsync(WDPBPtr paramBlock);
OSErr PBCloseWDSync(WDPBPtr paramBlock);
OSErr PBCloseWDAsync(WDPBPtr paramBlock);
OSErr PBGetWDInfoSync(WDPBPtr paramBlock);
OSErr PBGetWDInfoAsync(WDPBPtr paramBlock);

/* File Control Block Operations */
OSErr PBGetFCBInfoSync(FCBPBPtr paramBlock);
OSErr PBGetFCBInfoAsync(FCBPBPtr paramBlock);

/* Utility Functions */
OSErr FM_GetVolumeFromRefNum(VolumeRefNum vRefNum, VCB** vcb);
OSErr FM_GetFCBFromRefNum(FileRefNum refNum, FCB** fcb);
Boolean FM_IsDirectory(const FSSpec* spec);
OSErr FM_ConvertPath(const char* unixPath, FSSpec* spec);
OSErr FM_ConvertToUnixPath(const FSSpec* spec, char* unixPath, size_t maxLen);

/* Process Manager Integration */
OSErr FM_SetProcessOwner(FileRefNum refNum, UInt32 processID);
OSErr FM_ReleaseProcessFiles(UInt32 processID);
OSErr FM_YieldToProcess(void);

/* Cache Management */
OSErr FM_FlushCache(void);
OSErr FM_SetCacheSize(UInt32 cacheSize);

/* Compatibility Functions */
OSErr FM_RegisterExternalFS(UInt16 fsID, void* dispatcher);
OSErr FM_UnregisterExternalFS(UInt16 fsID);

/* Debug and Statistics */
void FM_GetStatistics(void* stats);
void FM_DumpVolumeInfo(VolumeRefNum vRefNum);
void FM_DumpOpenFiles(void);

#ifdef __cplusplus
}
#endif

#endif /* __FILEMANAGER_H__ */

/* Register a mounted VFS volume with the classic File Manager, so VCB_Find -
 * and therefore PBGetCatInfoSync, FSMakeFSSpec and everything built on them -
 * can resolve it. Called by the VFS as each volume mounts. */
void FM_RegisterVFSVolume(SInt16 vref, const char* name);
