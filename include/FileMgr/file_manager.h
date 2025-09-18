/*
 * RE-AGENT-BANNER
 * HFS File Manager Core Interface
 * Reverse-engineered from Mac OS System 7.1 source code
 * Evidence: TFS.a, TFSVOL.a, BTSVCS.a, CMSVCS.a, FXM.a
 * Preserving original HFS File Manager semantics and API conventions
 */

#ifndef FILE_MANAGER_H
#define FILE_MANAGER_H

#include "hfs_structs.h"
#include <stdint.h>
#include <stdbool.h>

/* File Manager Function Prototypes - Evidence from TFS.a */

/*
 * Core File Manager Operations
 * Evidence: TFS.a TFSDispatch, VolumeCall, RefNumCall, OpenCall
 */
OSErr TFSDispatch(uint16_t trap_index, uint16_t trap_word, void* param_block);
OSErr VolumeCall(void* param_block);
OSErr RefNumCall(void* param_block);
OSErr OpenCall(void* param_block);

/*
 * Volume Management Operations
 * Evidence: TFSVOL.a MountVol, CheckRemount, MtCheck, FindDrive, OffLine
 */
OSErr MountVol(void* param_block);
OSErr CheckRemount(VCB* vcb);
OSErr MtCheck(VCB* vcb);
void* FindDrive(int16_t drive_num);
OSErr OffLine(VCB* vcb);
OSErr FlushVol(VCB* vcb);

/*
 * B-Tree Services for Catalog and Extent Management
 * Evidence: BTSVCS.a BTOpen, BTClose, BTSearch, BTInsert, BTDelete, BTGetRecord
 */
OSErr BTOpen(FCB* fcb, void* btcb);
OSErr BTClose(void* btcb);
OSErr BTSearch(void* btcb, void* key, BTNode** node, uint16_t* record_index);
OSErr BTInsert(void* btcb, void* key, void* data, uint16_t data_len);
OSErr BTDelete(void* btcb, void* key);
OSErr BTGetRecord(void* btcb, int16_t selection_mode, void** key, void** data, uint16_t* data_len);
OSErr BTFlush(void* btcb);

/*
 * Catalog Manager Services
 * Evidence: CMSVCS.a catalog operations for file/folder metadata
 */
OSErr CMGetCatalogHint(VCB* vcb, uint32_t dir_id, ConstStringPtr name, void* hint);
OSErr CMLocateCatalogHint(VCB* vcb, void* hint, void* catalog_record);
OSErr CMAddCatalogRecord(VCB* vcb, void* key, void* catalog_record);
OSErr CMDeleteCatalogRecord(VCB* vcb, void* key);
OSErr CMUpdateCatalogRecord(VCB* vcb, void* key, void* catalog_record);

/*
 * File Extent Management (FXM)
 * Evidence: FXM.a extent allocation and mapping functions
 */
OSErr FXMFindExtent(FCB* fcb, uint32_t file_block, ExtentRecord* extent_rec);
OSErr FXMAllocateExtent(VCB* vcb, FCB* fcb, uint32_t bytes_needed, ExtentRecord* extent_rec);
OSErr FXMDeallocateExtent(VCB* vcb, ExtentRecord* extent_rec);
OSErr FXMExtendFile(FCB* fcb, uint32_t bytes_to_add);
OSErr FXMTruncateFile(FCB* fcb, uint32_t new_length);

/*
 * File System Queue Operations
 * Evidence: TFS.a FSQueue, FSQueueSync for async/sync operation support
 */
OSErr FSQueue(void* param_block);
OSErr FSQueueSync(void);

/*
 * Internal Support Functions
 * Evidence: Various assembly files for command completion and hooks
 */
void CmdDone(void);
OSErr DSHook(void);  /* Disk switch hook */

/*
 * Utility Functions for HFS Implementation
 */
bool IsValidHFSSignature(uint16_t signature);
uint32_t CalculateAllocationBlocks(uint32_t file_size, uint32_t block_size);
OSErr ValidateVCB(const VCB* vcb);
OSErr ValidateFCB(const FCB* fcb);
OSErr ConvertPascalStringToC(const uint8_t* pascal_str, char* c_str, size_t max_len);
OSErr ConvertCStringToPascal(const char* c_str, uint8_t* pascal_str, size_t max_len);

/*
 * File Manager Constants and Macros
 */
#define HFS_DEFAULT_CLUMP_SIZE  4096
#define HFS_ROOT_DIR_ID         2
#define HFS_ROOT_PARENT_ID      1
#define MAX_HFS_FILENAME        31
#define MAX_HFS_VOLUME_NAME     27

/* File Manager Trap Numbers (Evidence: TFS.a trap dispatch table) */
#define kHFSDispatch            0xA060
#define kMountVol               0xA00F
#define kUnmountVol             0xA00E
#define kFlushVol               0xA013
#define kGetVol                 0xA014
#define kSetVol                 0xA015

/* Volume Attributes */
#define kHFSVolumeHardwareLockBit   7
#define kHFSVolumeSoftwareLockBit   15

/* File Attributes */
#define kHFSFileLockedBit           0
#define kHFSFileInvisibleBit        1
#define kHFSFileHasBundleBit        5

/*
 * Error codes specific to HFS operations
 * Evidence: Various assembly files error handling patterns
 */
#define badMDBErr               -60     /* Bad master directory block */
#define wrPermErr               -61     /* Write permissions error */
#define memFullErr              -108    /* Not enough memory */
#define nsDrvErr                -56     /* No such drive */
#define offLinErr               -53     /* Volume is offline */
#define volOnLinErr             -55     /* Volume already online */

#endif /* FILE_MANAGER_H */