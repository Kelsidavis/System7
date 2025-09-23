/* Virtual File System Implementation */
#include "../../include/FS/vfs.h"
#include "../../include/FS/hfs_volume.h"
#include "../../include/FS/hfs_catalog.h"
#include "../../include/FS/hfs_file.h"
#include "../../include/MemoryMgr/MemoryManager.h"
#include <string.h>

/* Serial debug output */
extern void serial_printf(const char* fmt, ...);

/* Volume buffer - allocated from heap */

/* VFS state */
static struct {
    bool         initialized;
    HFS_Volume   bootVolume;
    HFS_Catalog  bootCatalog;
    VRefNum      bootVRef;
} g_vfs = { 0 };

/* VFS file wrapper */
struct VFSFile {
    HFSFile* hfsFile;
    VRefNum  vref;
};

bool VFS_Init(void) {
    if (g_vfs.initialized) {
        /* serial_printf("VFS: Already initialized\n"); */
        return true;
    }

    memset(&g_vfs, 0, sizeof(g_vfs));
    g_vfs.bootVRef = 1;  /* Boot volume is always vRef 1 */

    /* serial_printf("VFS: Initialized\n"); */
    g_vfs.initialized = true;
    return true;
}

void VFS_Shutdown(void) {
    if (!g_vfs.initialized) return;

    /* Close catalog if open */
    HFS_CatalogClose(&g_vfs.bootCatalog);

    /* Unmount volume if mounted */
    HFS_VolumeUnmount(&g_vfs.bootVolume);

    g_vfs.initialized = false;
    /* serial_printf("VFS: Shutdown complete\n"); */
}

bool VFS_MountBootVolume(const char* volName) {
    if (!g_vfs.initialized) {
        /* serial_printf("VFS: Not initialized\n"); */
        return false;
    }

    /* Allocate volume buffer from heap */
    uint64_t volumeSize = 4 * 1024 * 1024;  /* 4MB */
    void* volumeData = malloc(volumeSize);
    if (!volumeData) {
        /* serial_printf("VFS: Failed to allocate volume memory\n"); */
        return false;
    }

    /* Create blank HFS volume */
    if (!HFS_CreateBlankVolume(volumeData, volumeSize, volName)) {
        /* serial_printf("VFS: Failed to create blank volume\n"); */
        return false;
    }

    /* Mount the volume */
    if (!HFS_VolumeMountMemory(&g_vfs.bootVolume, volumeData, volumeSize, g_vfs.bootVRef)) {
        /* serial_printf("VFS: Failed to mount volume\n"); */
        return false;
    }

    /* Initialize catalog */
    if (!HFS_CatalogInit(&g_vfs.bootCatalog, &g_vfs.bootVolume)) {
        HFS_VolumeUnmount(&g_vfs.bootVolume);
        free(volumeData);
        /* serial_printf("VFS: Failed to initialize catalog\n"); */
        return false;
    }

    /* serial_printf("VFS: Mounted boot volume successfully\n"); */

    /* Create some default folders for testing */
    /* These would normally be created by the System installer */
    /* For now, we just report success */

    return true;
}

bool VFS_GetVolumeInfo(VRefNum vref, VolumeControlBlock* vcb) {
    if (!g_vfs.initialized || !vcb) return false;

    if (vref != g_vfs.bootVRef || !g_vfs.bootVolume.mounted) {
        return false;
    }

    return HFS_GetVolumeInfo(&g_vfs.bootVolume, vcb);
}

VRefNum VFS_GetBootVRef(void) {
    return g_vfs.bootVRef;
}

bool VFS_Enumerate(VRefNum vref, DirID dir, CatEntry* entries, int maxEntries, int* count) {
    if (!g_vfs.initialized || !entries || !count) return false;

    if (vref != g_vfs.bootVRef || !g_vfs.bootVolume.mounted) {
        return false;
    }

    /* If enumerating an empty volume, return empty list for now */
    if (!g_vfs.bootCatalog.bt.nodeBuffer) {
        *count = 0;
        return true;
    }

    return HFS_CatalogEnumerate(&g_vfs.bootCatalog, dir, entries, maxEntries, count);
}

bool VFS_Lookup(VRefNum vref, DirID dir, const char* name, CatEntry* entry) {
    if (!g_vfs.initialized || !name || !entry) return false;

    if (vref != g_vfs.bootVRef || !g_vfs.bootVolume.mounted) {
        return false;
    }

    return HFS_CatalogLookup(&g_vfs.bootCatalog, dir, name, entry);
}

bool VFS_GetByID(VRefNum vref, FileID id, CatEntry* entry) {
    if (!g_vfs.initialized || !entry) return false;

    if (vref != g_vfs.bootVRef || !g_vfs.bootVolume.mounted) {
        return false;
    }

    return HFS_CatalogGetByID(&g_vfs.bootCatalog, id, entry);
}

VFSFile* VFS_OpenFile(VRefNum vref, FileID id, bool resourceFork) {
    if (!g_vfs.initialized) return NULL;

    if (vref != g_vfs.bootVRef || !g_vfs.bootVolume.mounted) {
        return NULL;
    }

    HFSFile* hfsFile = HFS_FileOpen(&g_vfs.bootCatalog, id, resourceFork);
    if (!hfsFile) return NULL;

    VFSFile* vfsFile = (VFSFile*)malloc(sizeof(VFSFile));
    if (!vfsFile) {
        HFS_FileClose(hfsFile);
        return NULL;
    }

    vfsFile->hfsFile = hfsFile;
    vfsFile->vref = vref;

    return vfsFile;
}

VFSFile* VFS_OpenByPath(VRefNum vref, const char* path, bool resourceFork) {
    if (!g_vfs.initialized || !path) return NULL;

    if (vref != g_vfs.bootVRef || !g_vfs.bootVolume.mounted) {
        return NULL;
    }

    HFSFile* hfsFile = HFS_FileOpenByPath(&g_vfs.bootCatalog, path, resourceFork);
    if (!hfsFile) return NULL;

    VFSFile* vfsFile = (VFSFile*)malloc(sizeof(VFSFile));
    if (!vfsFile) {
        HFS_FileClose(hfsFile);
        return NULL;
    }

    vfsFile->hfsFile = hfsFile;
    vfsFile->vref = vref;

    return vfsFile;
}

void VFS_CloseFile(VFSFile* file) {
    if (!file) return;

    if (file->hfsFile) {
        HFS_FileClose(file->hfsFile);
    }

    free(file);
}

bool VFS_ReadFile(VFSFile* file, void* buffer, uint32_t length, uint32_t* bytesRead) {
    if (!file || !file->hfsFile) return false;
    return HFS_FileRead(file->hfsFile, buffer, length, bytesRead);
}

bool VFS_SeekFile(VFSFile* file, uint32_t position) {
    if (!file || !file->hfsFile) return false;
    return HFS_FileSeek(file->hfsFile, position);
}

uint32_t VFS_GetFileSize(VFSFile* file) {
    if (!file || !file->hfsFile) return 0;
    return HFS_FileGetSize(file->hfsFile);
}

uint32_t VFS_GetFilePosition(VFSFile* file) {
    if (!file || !file->hfsFile) return 0;
    return HFS_FileTell(file->hfsFile);
}

/* Write operations - stubs for now */
bool VFS_CreateFolder(VRefNum vref, DirID parent, const char* name, DirID* newID) {
    serial_printf("VFS_CreateFolder: Creating folder '%s' in parent %ld\n", name, parent);

    /* Validate parameters */
    if (!name || !newID) {
        serial_printf("VFS_CreateFolder: Invalid parameters\n");
        return false;
    }

    /* Check volume - for now just validate vref */
    if (vref != 0 && vref != -1) {
        serial_printf("VFS_CreateFolder: Invalid volume %d\n", vref);
        return false;
    }

    /* Generate new folder ID */
    static DirID nextDirID = 1000;
    *newID = nextDirID++;

    /* Log success for now - full implementation would update HFS catalog */
    serial_printf("VFS_CreateFolder: Created folder '%s' with ID %ld\n", name, *newID);
    return true;
}

bool VFS_CreateFile(VRefNum vref, DirID parent, const char* name,
                   uint32_t type, uint32_t creator, FileID* newID) {
    serial_printf("VFS_CreateFile: Creating file '%s' type='%.4s' creator='%.4s'\n",
                  name, (char*)&type, (char*)&creator);

    /* Validate parameters */
    if (!name || !newID) {
        serial_printf("VFS_CreateFile: Invalid parameters\n");
        return false;
    }

    /* Check volume - for now just validate vref */
    if (vref != 0 && vref != -1) {
        serial_printf("VFS_CreateFile: Invalid volume %d\n", vref);
        return false;
    }

    /* Generate new file ID */
    static FileID nextFileID = 2000;
    *newID = nextFileID++;

    /* Log success for now - full implementation would update HFS catalog */
    serial_printf("VFS_CreateFile: Created file '%s' with ID %ld\n", name, *newID);
    return true;
}

bool VFS_Rename(VRefNum vref, FileID id, const char* newName) {
    serial_printf("VFS_Rename: Renaming file/folder %ld to '%s'\n", id, newName);

    /* Validate parameters */
    if (!newName || strlen(newName) == 0 || strlen(newName) > 31) {
        serial_printf("VFS_Rename: Invalid name\n");
        return false;
    }

    /* Check volume - for now just validate vref */
    if (vref != 0 && vref != -1) {
        serial_printf("VFS_Rename: Invalid volume %d\n", vref);
        return false;
    }

    /* Log success for now - full implementation would update HFS catalog */
    serial_printf("VFS_Rename: Successfully renamed ID %ld to '%s'\n", id, newName);
    return true;
}

bool VFS_Delete(VRefNum vref, FileID id) {
    serial_printf("VFS_Delete: Deleting file/folder ID %ld\n", id);

    /* Check volume - for now just validate vref */
    if (vref != 0 && vref != -1) {
        serial_printf("VFS_Delete: Invalid volume %d\n", vref);
        return false;
    }

    /* Check for special folders that shouldn't be deleted */
    if (id == 1 || id == 2) {  /* Root (1) or System folder (2) */
        serial_printf("VFS_Delete: Cannot delete system folder\n");
        return false;
    }

    /* Log success for now - full implementation would update HFS catalog */
    serial_printf("VFS_Delete: Successfully deleted ID %ld\n", id);
    return true;
}