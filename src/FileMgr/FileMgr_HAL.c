/*
 * FileMgr_HAL.c - Hardware Abstraction Layer for File Manager
 *
 * Bridges the HFS File Manager with modern filesystems on x86_64 and ARM64,
 * integrating with Memory Manager and Resource Manager for complete I/O.
 */

#include "file_manager.h"
#include "hfs_structs.h"
#include "../MemoryMgr/MemoryMgr_HAL.h"
#include "../ResourceMgr/resource_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>

#ifdef __APPLE__
    #include <sys/xattr.h>
    #define HAS_RESOURCE_FORK 1
#endif

/* File Manager global state */
typedef struct OpenFile {
    int16_t         refNum;        /* File reference number */
    char            path[1024];    /* Full file path */
    int             fd;            /* Unix file descriptor */
    FCB*            fcb;           /* File control block */
    Handle          buffer;        /* I/O buffer (Memory Manager handle) */
    uint32_t        position;      /* Current file position */
    uint32_t        eof;           /* End of file position */
    uint8_t         permission;    /* Read/write permission */
    struct OpenFile* next;         /* Next in chain */
} OpenFile;

typedef struct MountedVolume {
    VCB*            vcb;           /* Volume control block */
    char            mountPath[1024]; /* Mount point path */
    uint32_t        totalBlocks;   /* Total blocks on volume */
    uint32_t        freeBlocks;    /* Free blocks available */
    Handle          btreeCache;    /* B-Tree cache (Memory Manager handle) */
    struct MountedVolume* next;    /* Next in chain */
} MountedVolume;

static OpenFile* gOpenFiles = NULL;
static MountedVolume* gMountedVolumes = NULL;
static int16_t gNextRefNum = 1;
static int16_t gNextVRefNum = -1;
static pthread_mutex_t gFileLock = PTHREAD_MUTEX_INITIALIZER;

/* B-Tree cache for performance */
typedef struct BTNodeCache {
    uint32_t    nodeNumber;
    BTNode*     node;           /* Memory Manager allocated */
    uint32_t    accessTime;
    bool        dirty;
} BTNodeCache;

#define MAX_BTREE_CACHE 64
static BTNodeCache gBTreeCache[MAX_BTREE_CACHE];
static int gCacheIndex = 0;

/* Initialize File Manager HAL */
OSErr FileMgr_HAL_Initialize(void)
{
    pthread_mutex_lock(&gFileLock);

    /* Initialize file lists */
    gOpenFiles = NULL;
    gMountedVolumes = NULL;
    gNextRefNum = 1;
    gNextVRefNum = -1;

    /* Initialize B-Tree cache */
    memset(gBTreeCache, 0, sizeof(gBTreeCache));
    gCacheIndex = 0;

    /* Mount default volume (current directory) */
    OSErr err = FileMgr_HAL_MountVolume(".", "System");

    pthread_mutex_unlock(&gFileLock);
    return err;
}

/* Mount a volume (directory as HFS volume) */
OSErr FileMgr_HAL_MountVolume(const char* path, const char* volumeName)
{
    pthread_mutex_lock(&gFileLock);

    /* Check if already mounted */
    MountedVolume* vol = gMountedVolumes;
    while (vol) {
        if (strcmp(vol->mountPath, path) == 0) {
            pthread_mutex_unlock(&gFileLock);
            return dupFNErr;  /* Already mounted */
        }
        vol = vol->next;
    }

    /* Create volume control block using Memory Manager */
    VCB* vcb = (VCB*)NewPtr(sizeof(VCB));
    if (!vcb) {
        pthread_mutex_unlock(&gFileLock);
        return memFullErr;
    }

    /* Initialize VCB */
    memset(vcb, 0, sizeof(VCB));
    vcb->vcbSigWord = 0x4244;  /* 'BD' signature */
    vcb->vcbVRefNum = gNextVRefNum--;
    strncpy((char*)vcb->vcbVN, volumeName, 27);
    vcb->vcbVN[0] = strlen(volumeName);  /* Pascal string */

    /* Get volume statistics */
    struct stat st;
    if (stat(path, &st) == 0) {
        vcb->vcbNmAlBlks = st.st_blocks;
        vcb->vcbFreeBks = st.st_blocks / 4;  /* Estimate free space */
        vcb->vcbAlBlkSiz = 4096;  /* 4KB allocation blocks */
    }

    /* Create mounted volume entry */
    vol = (MountedVolume*)malloc(sizeof(MountedVolume));
    if (!vol) {
        DisposePtr((Ptr)vcb);
        pthread_mutex_unlock(&gFileLock);
        return memFullErr;
    }

    vol->vcb = vcb;
    strncpy(vol->mountPath, path, sizeof(vol->mountPath) - 1);
    vol->totalBlocks = vcb->vcbNmAlBlks;
    vol->freeBlocks = vcb->vcbFreeBks;

    /* Allocate B-Tree cache using Memory Manager */
    vol->btreeCache = NewHandle(16 * 1024);  /* 16KB cache */

    /* Add to mounted volumes list */
    vol->next = gMountedVolumes;
    gMountedVolumes = vol;

    pthread_mutex_unlock(&gFileLock);
    return noErr;
}

/* Open a file */
OSErr FileMgr_HAL_OpenFile(const char* fileName, int16_t vRefNum, uint8_t permission, int16_t* refNum)
{
    pthread_mutex_lock(&gFileLock);

    /* Find volume */
    MountedVolume* vol = gMountedVolumes;
    while (vol && vol->vcb->vcbVRefNum != vRefNum) {
        vol = vol->next;
    }

    if (!vol) {
        pthread_mutex_unlock(&gFileLock);
        return nsvErr;  /* No such volume */
    }

    /* Build full path */
    char fullPath[2048];
    snprintf(fullPath, sizeof(fullPath), "%s/%s", vol->mountPath, fileName);

    /* Open the file */
    int flags = 0;
    switch (permission) {
        case fsRdPerm: flags = O_RDONLY; break;
        case fsWrPerm: flags = O_WRONLY | O_CREAT; break;
        case fsRdWrPerm: flags = O_RDWR | O_CREAT; break;
        default: flags = O_RDONLY; break;
    }

    int fd = open(fullPath, flags, 0644);
    if (fd < 0) {
        pthread_mutex_unlock(&gFileLock);
        return fnfErr;  /* File not found */
    }

    /* Create FCB using Memory Manager */
    FCB* fcb = (FCB*)NewPtr(sizeof(FCB));
    if (!fcb) {
        close(fd);
        pthread_mutex_unlock(&gFileLock);
        return memFullErr;
    }

    /* Initialize FCB */
    memset(fcb, 0, sizeof(FCB));
    fcb->fcbFlNum = gNextRefNum;
    fcb->fcbVPtr = (Ptr)vol->vcb;
    fcb->fcbBfAdr = 0;  /* Will allocate buffer on first I/O */

    /* Get file info */
    struct stat st;
    if (fstat(fd, &st) == 0) {
        fcb->fcbEOF = st.st_size;
        fcb->fcbPLen = st.st_size;
    }

    /* Create open file entry */
    OpenFile* of = (OpenFile*)malloc(sizeof(OpenFile));
    if (!of) {
        DisposePtr((Ptr)fcb);
        close(fd);
        pthread_mutex_unlock(&gFileLock);
        return memFullErr;
    }

    of->refNum = gNextRefNum++;
    strncpy(of->path, fullPath, sizeof(of->path) - 1);
    of->fd = fd;
    of->fcb = fcb;
    of->buffer = NULL;  /* Allocate on first I/O */
    of->position = 0;
    of->eof = fcb->fcbEOF;
    of->permission = permission;

    /* Add to open files list */
    of->next = gOpenFiles;
    gOpenFiles = of;

    *refNum = of->refNum;

    pthread_mutex_unlock(&gFileLock);
    return noErr;
}

/* Close a file */
OSErr FileMgr_HAL_CloseFile(int16_t refNum)
{
    pthread_mutex_lock(&gFileLock);

    OpenFile* of = gOpenFiles;
    OpenFile* prev = NULL;

    while (of && of->refNum != refNum) {
        prev = of;
        of = of->next;
    }

    if (!of) {
        pthread_mutex_unlock(&gFileLock);
        return fnOpnErr;  /* File not open */
    }

    /* Flush any pending writes */
    if (of->buffer) {
        fsync(of->fd);
    }

    /* Remove from list */
    if (prev) {
        prev->next = of->next;
    } else {
        gOpenFiles = of->next;
    }

    /* Clean up resources using Memory Manager */
    if (of->buffer) {
        DisposeHandle(of->buffer);
    }
    if (of->fcb) {
        DisposePtr((Ptr)of->fcb);
    }

    /* Close file */
    close(of->fd);
    free(of);

    pthread_mutex_unlock(&gFileLock);
    return noErr;
}

/* Read from file */
OSErr FileMgr_HAL_ReadFile(int16_t refNum, void* buffer, uint32_t* count)
{
    pthread_mutex_lock(&gFileLock);

    OpenFile* of = gOpenFiles;
    while (of && of->refNum != refNum) {
        of = of->next;
    }

    if (!of) {
        pthread_mutex_unlock(&gFileLock);
        return fnOpnErr;
    }

    /* Allocate I/O buffer if needed using Memory Manager */
    if (!of->buffer) {
        of->buffer = NewHandle(32 * 1024);  /* 32KB buffer */
        if (!of->buffer) {
            pthread_mutex_unlock(&gFileLock);
            return memFullErr;
        }
    }

    /* Read data */
    ssize_t bytesRead = read(of->fd, buffer, *count);
    if (bytesRead < 0) {
        pthread_mutex_unlock(&gFileLock);
        return ioErr;
    }

    *count = bytesRead;
    of->position += bytesRead;

    pthread_mutex_unlock(&gFileLock);
    return (bytesRead == 0) ? eofErr : noErr;
}

/* Write to file */
OSErr FileMgr_HAL_WriteFile(int16_t refNum, const void* buffer, uint32_t* count)
{
    pthread_mutex_lock(&gFileLock);

    OpenFile* of = gOpenFiles;
    while (of && of->refNum != refNum) {
        of = of->next;
    }

    if (!of) {
        pthread_mutex_unlock(&gFileLock);
        return fnOpnErr;
    }

    /* Check write permission */
    if (of->permission == fsRdPerm) {
        pthread_mutex_unlock(&gFileLock);
        return wrPermErr;
    }

    /* Allocate I/O buffer if needed using Memory Manager */
    if (!of->buffer) {
        of->buffer = NewHandle(32 * 1024);  /* 32KB buffer */
        if (!of->buffer) {
            pthread_mutex_unlock(&gFileLock);
            return memFullErr;
        }
    }

    /* Write data */
    ssize_t bytesWritten = write(of->fd, buffer, *count);
    if (bytesWritten < 0) {
        pthread_mutex_unlock(&gFileLock);
        return ioErr;
    }

    *count = bytesWritten;
    of->position += bytesWritten;
    if (of->position > of->eof) {
        of->eof = of->position;
        of->fcb->fcbEOF = of->eof;
    }

    pthread_mutex_unlock(&gFileLock);
    return noErr;
}

/* Set file position */
OSErr FileMgr_HAL_SetFilePos(int16_t refNum, uint16_t posMode, int32_t posOffset)
{
    pthread_mutex_lock(&gFileLock);

    OpenFile* of = gOpenFiles;
    while (of && of->refNum != refNum) {
        of = of->next;
    }

    if (!of) {
        pthread_mutex_unlock(&gFileLock);
        return fnOpnErr;
    }

    int whence;
    switch (posMode) {
        case fsFromStart: whence = SEEK_SET; break;
        case fsFromLEOF: whence = SEEK_END; break;
        case fsFromMark: whence = SEEK_CUR; break;
        default: whence = SEEK_SET; break;
    }

    off_t newPos = lseek(of->fd, posOffset, whence);
    if (newPos < 0) {
        pthread_mutex_unlock(&gFileLock);
        return posErr;
    }

    of->position = newPos;

    pthread_mutex_unlock(&gFileLock);
    return noErr;
}

/* Get file info */
OSErr FileMgr_HAL_GetFileInfo(const char* fileName, int16_t vRefNum, HFileInfo* fileInfo)
{
    pthread_mutex_lock(&gFileLock);

    /* Find volume */
    MountedVolume* vol = gMountedVolumes;
    while (vol && vol->vcb->vcbVRefNum != vRefNum) {
        vol = vol->next;
    }

    if (!vol) {
        pthread_mutex_unlock(&gFileLock);
        return nsvErr;
    }

    /* Build full path */
    char fullPath[2048];
    snprintf(fullPath, sizeof(fullPath), "%s/%s", vol->mountPath, fileName);

    /* Get file stats */
    struct stat st;
    if (stat(fullPath, &st) != 0) {
        pthread_mutex_unlock(&gFileLock);
        return fnfErr;
    }

    /* Fill file info structure */
    memset(fileInfo, 0, sizeof(HFileInfo));
    fileInfo->ioVRefNum = vRefNum;
    fileInfo->ioFlAttrib = S_ISDIR(st.st_mode) ? 0x10 : 0;
    fileInfo->ioFlLgLen = st.st_size;
    fileInfo->ioFlPyLen = st.st_blocks * 512;
    fileInfo->ioFlCrDat = st.st_ctime;
    fileInfo->ioFlMdDat = st.st_mtime;

    /* Set file type and creator (default values) */
    fileInfo->ioFlFndrInfo.fdType = 'TEXT';
    fileInfo->ioFlFndrInfo.fdCreator = 'MACS';

    #ifdef HAS_RESOURCE_FORK
    /* Check for resource fork on macOS */
    ssize_t rfSize = getxattr(fullPath, "com.apple.ResourceFork", NULL, 0, 0, 0);
    if (rfSize > 0) {
        fileInfo->ioFlRLgLen = rfSize;
        fileInfo->ioFlRPyLen = ((rfSize + 511) / 512) * 512;
    }
    #endif

    pthread_mutex_unlock(&gFileLock);
    return noErr;
}

/* Create file */
OSErr FileMgr_HAL_CreateFile(const char* fileName, int16_t vRefNum, OSType creator, OSType fileType)
{
    pthread_mutex_lock(&gFileLock);

    /* Find volume */
    MountedVolume* vol = gMountedVolumes;
    while (vol && vol->vcb->vcbVRefNum != vRefNum) {
        vol = vol->next;
    }

    if (!vol) {
        pthread_mutex_unlock(&gFileLock);
        return nsvErr;
    }

    /* Build full path */
    char fullPath[2048];
    snprintf(fullPath, sizeof(fullPath), "%s/%s", vol->mountPath, fileName);

    /* Create file */
    int fd = open(fullPath, O_CREAT | O_EXCL | O_WRONLY, 0644);
    if (fd < 0) {
        pthread_mutex_unlock(&gFileLock);
        return (errno == EEXIST) ? dupFNErr : ioErr;
    }

    close(fd);

    #ifdef HAS_RESOURCE_FORK
    /* Set file type and creator as extended attributes on macOS */
    char typeCreator[8];
    memcpy(typeCreator, &fileType, 4);
    memcpy(typeCreator + 4, &creator, 4);
    setxattr(fullPath, "com.apple.FinderInfo", typeCreator, 8, 0, 0);
    #endif

    pthread_mutex_unlock(&gFileLock);
    return noErr;
}

/* Delete file */
OSErr FileMgr_HAL_DeleteFile(const char* fileName, int16_t vRefNum)
{
    pthread_mutex_lock(&gFileLock);

    /* Find volume */
    MountedVolume* vol = gMountedVolumes;
    while (vol && vol->vcb->vcbVRefNum != vRefNum) {
        vol = vol->next;
    }

    if (!vol) {
        pthread_mutex_unlock(&gFileLock);
        return nsvErr;
    }

    /* Build full path */
    char fullPath[2048];
    snprintf(fullPath, sizeof(fullPath), "%s/%s", vol->mountPath, fileName);

    /* Delete file */
    if (unlink(fullPath) != 0) {
        pthread_mutex_unlock(&gFileLock);
        return (errno == ENOENT) ? fnfErr : ioErr;
    }

    pthread_mutex_unlock(&gFileLock);
    return noErr;
}

/* B-Tree cache operations for performance */
BTNode* FileMgr_HAL_GetBTreeNode(uint32_t nodeNumber)
{
    /* Check cache */
    for (int i = 0; i < MAX_BTREE_CACHE; i++) {
        if (gBTreeCache[i].node && gBTreeCache[i].nodeNumber == nodeNumber) {
            gBTreeCache[i].accessTime = time(NULL);
            return gBTreeCache[i].node;
        }
    }

    /* Allocate new node using Memory Manager */
    BTNode* node = (BTNode*)NewPtr(512);  /* Standard B-Tree node size */
    if (!node) return NULL;

    /* Add to cache (simple LRU) */
    int oldestIdx = 0;
    uint32_t oldestTime = UINT32_MAX;
    for (int i = 0; i < MAX_BTREE_CACHE; i++) {
        if (!gBTreeCache[i].node) {
            oldestIdx = i;
            break;
        }
        if (gBTreeCache[i].accessTime < oldestTime) {
            oldestTime = gBTreeCache[i].accessTime;
            oldestIdx = i;
        }
    }

    /* Replace oldest entry */
    if (gBTreeCache[oldestIdx].node && gBTreeCache[oldestIdx].dirty) {
        /* Would flush to disk here */
        DisposePtr((Ptr)gBTreeCache[oldestIdx].node);
    }

    gBTreeCache[oldestIdx].nodeNumber = nodeNumber;
    gBTreeCache[oldestIdx].node = node;
    gBTreeCache[oldestIdx].accessTime = time(NULL);
    gBTreeCache[oldestIdx].dirty = false;

    return node;
}

/* Flush File Manager buffers */
OSErr FileMgr_HAL_FlushVolume(int16_t vRefNum)
{
    pthread_mutex_lock(&gFileLock);

    /* Flush all open files on volume */
    OpenFile* of = gOpenFiles;
    while (of) {
        if (of->fcb && ((VCB*)of->fcb->fcbVPtr)->vcbVRefNum == vRefNum) {
            fsync(of->fd);
        }
        of = of->next;
    }

    /* Flush B-Tree cache */
    for (int i = 0; i < MAX_BTREE_CACHE; i++) {
        if (gBTreeCache[i].node && gBTreeCache[i].dirty) {
            /* Would write to disk here */
            gBTreeCache[i].dirty = false;
        }
    }

    pthread_mutex_unlock(&gFileLock);
    return noErr;
}

/* Shutdown File Manager HAL */
void FileMgr_HAL_Shutdown(void)
{
    pthread_mutex_lock(&gFileLock);

    /* Close all open files */
    while (gOpenFiles) {
        OpenFile* of = gOpenFiles;
        gOpenFiles = of->next;

        if (of->buffer) {
            DisposeHandle(of->buffer);
        }
        if (of->fcb) {
            DisposePtr((Ptr)of->fcb);
        }

        close(of->fd);
        free(of);
    }

    /* Unmount all volumes */
    while (gMountedVolumes) {
        MountedVolume* vol = gMountedVolumes;
        gMountedVolumes = vol->next;

        if (vol->btreeCache) {
            DisposeHandle(vol->btreeCache);
        }
        if (vol->vcb) {
            DisposePtr((Ptr)vol->vcb);
        }

        free(vol);
    }

    /* Clear B-Tree cache */
    for (int i = 0; i < MAX_BTREE_CACHE; i++) {
        if (gBTreeCache[i].node) {
            DisposePtr((Ptr)gBTreeCache[i].node);
            gBTreeCache[i].node = NULL;
        }
    }

    pthread_mutex_unlock(&gFileLock);
}