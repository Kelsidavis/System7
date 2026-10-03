/*
 * file_manager.h - Compatibility include for the original File Manager path
 *
 * Keep this path for existing clients, but keep API declarations in the
 * canonical FileManager.h. Maintaining separate prototypes here allowed the
 * two headers to disagree about parameter types and made including both
 * headers ill-formed.
 */

#ifndef FILEMGR_FILE_MANAGER_H
#define FILEMGR_FILE_MANAGER_H

#include "FileManager.h"
#include "hfs_structs.h"

/* Constants retained for clients of the original HFS-specific header. */
#define HFS_DEFAULT_CLUMP_SIZE  4096
#define HFS_ROOT_PARENT_ID      1
#define MAX_HFS_FILENAME        31
#define MAX_HFS_VOLUME_NAME     27

#define kHFSDispatch            0xA060
#define kMountVol               0xA00F
#define kUnmountVol             0xA00E
#define kFlushVol               0xA013
#define kGetVol                 0xA014
#define kSetVol                 0xA015

#define kHFSVolumeHardwareLockBit   7
#define kHFSVolumeSoftwareLockBit   15
#define kHFSFileLockedBit           0
#define kHFSFileInvisibleBit        1
#define kHFSFileHasBundleBit        5

#ifndef badMDBErr
#define badMDBErr               -60
#endif
#ifndef nsDrvErr
#define nsDrvErr                -56
#endif
#ifndef offLinErr
#define offLinErr               -53
#endif
#ifndef volOnLinErr
#define volOnLinErr             -55
#endif

typedef const unsigned char* ConstStringPtr;

#endif /* FILEMGR_FILE_MANAGER_H */
