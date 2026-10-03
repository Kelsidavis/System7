/*
 * HFS constants retained for clients of the original FileMgr include path.
 * On-disk layouts are defined in FS/hfs_types.h; File Manager runtime
 * structures are defined in FileManagerTypes.h.
 */

#ifndef HFS_STRUCTS_H
#define HFS_STRUCTS_H

#include "SystemTypes.h"
#include "Errors/ErrorCodes.h"
#include "FS/hfs_constants.h"

/* HFS Constants */
#define MAX_BTREE_DEPTH         8           /* Maximum tree depth */
#define NUM_EXTENTS_PER_RECORD  3           /* Extents per extent record */
#define EXTENT_RECORD_SIZE      12          /* Size of extent record */
#define HFS_BLOCK_SIZE          512         /* Standard HFS block size */

/* Catalog Record Types */
#define kHFSFolderRecord        1           /* Directory record */
#define kHFSFileRecord          2           /* File record */
#define kHFSFolderThreadRecord  3           /* Directory thread record */
#define kHFSFileThreadRecord    4           /* File thread record */

/* B-Tree Node Types */
#define ndMapNode               2           /* Map node */
#define ndIndxNode              ndIndexNode /* Legacy spelling */

/* Fork Types */
#define dataFk                  0x00        /* Data fork */
#define rsrcFk                  0xFF        /* Resource fork */

#endif /* HFS_STRUCTS_H */
