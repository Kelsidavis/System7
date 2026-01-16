/* Virtual File System Implementation */
#include "../../include/FS/vfs.h"
#include "../../include/FS/hfs_volume.h"
#include "../../include/FS/hfs_catalog.h"
#include "../../include/FS/hfs_file.h"
#include "../../include/FS/hfs_endian.h"
#include "../../include/MemoryMgr/MemoryManager.h"
#include <string.h>
#include "FS/FSLogging.h"

/* Serial debug output */

/* Volume buffer - allocated from heap */

/* Maximum mounted volumes */
#define VFS_MAX_VOLUMES 8

/* Mounted volume entry */
typedef struct {
    bool         mounted;
    VRefNum      vref;
    HFS_Volume   volume;
    HFS_Catalog  catalog;
    char         name[256];
} VFSVolume;

/* VFS state */
static struct {
    bool                initialized;
    VFSVolume          volumes[VFS_MAX_VOLUMES];
    VRefNum             nextVRef;
    VFS_MountCallback  mountCallback;
} g_vfs = { 0 };

/* VFS file wrapper */
struct VFSFile {
    HFSFile* hfsFile;
    VRefNum  vref;
};

/* Helper: Find volume by vref */
static VFSVolume* VFS_FindVolume(VRefNum vref) {
    for (int i = 0; i < VFS_MAX_VOLUMES; i++) {
        if (g_vfs.volumes[i].mounted && g_vfs.volumes[i].vref == vref) {
            return &g_vfs.volumes[i];
        }
    }
    return NULL;
}

/* Helper: Find free volume slot */
static VFSVolume* VFS_AllocVolume(void) {
    for (int i = 0; i < VFS_MAX_VOLUMES; i++) {
        if (!g_vfs.volumes[i].mounted) {
            return &g_vfs.volumes[i];
        }
    }
    return NULL;
}

bool VFS_Init(void) {
    if (g_vfs.initialized) {
        /* FS_LOG_DEBUG("VFS: Already initialized\n"); */
        return true;
    }

    memset(&g_vfs, 0, sizeof(g_vfs));
    g_vfs.nextVRef = 1;  /* Start VRefs at 1 */

    /* FS_LOG_DEBUG("VFS: Initialized\n"); */
    g_vfs.initialized = true;
    return true;
}

void VFS_SetMountCallback(VFS_MountCallback callback) {
    g_vfs.mountCallback = callback;
}

void VFS_Shutdown(void) {
    if (!g_vfs.initialized) return;

    /* Unmount all volumes */
    for (int i = 0; i < VFS_MAX_VOLUMES; i++) {
        if (g_vfs.volumes[i].mounted) {
            HFS_CatalogClose(&g_vfs.volumes[i].catalog);
            HFS_VolumeUnmount(&g_vfs.volumes[i].volume);
            g_vfs.volumes[i].mounted = false;
        }
    }

    g_vfs.initialized = false;
    /* FS_LOG_DEBUG("VFS: Shutdown complete\n"); */
}

bool VFS_MountBootVolume(const char* volName) {
    extern void serial_puts(const char* str);
    extern void uart_flush(void);

    serial_puts("[VFS] MountBootVolume enter\n");
    uart_flush();

    if (!g_vfs.initialized) {
        serial_puts("[VFS] Mount failed: not initialized\n");
        return false;
    }

    /* Allocate volume slot */
    serial_puts("[VFS] AllocVolume\n");
    uart_flush();
    VFSVolume* vol = VFS_AllocVolume();
    if (!vol) {
        serial_puts("[VFS] Mount failed: no free volume slots\n");
        return false;
    }

    /* Allocate volume buffer from heap - try 1MB instead of 4MB */
    serial_puts("[VFS] NewPtr\n");
    uart_flush();
    uint64_t volumeSize = 1 * 1024 * 1024;  /* 1MB */
    void* volumeData = NewPtr(volumeSize);
    if (!volumeData) {
        serial_puts("[VFS] Mount failed: NewPtr returned NULL\n");
        return false;
    }

    /* Create blank HFS volume */
    serial_puts("[VFS] HFS_CreateBlankVolume\n");
    uart_flush();
    if (!HFS_CreateBlankVolume(volumeData, volumeSize, volName)) {
        serial_puts("[VFS] Mount failed: HFS_CreateBlankVolume failed\n");
        DisposePtr((Ptr)volumeData);
        return false;
    }

    /* Assign vref */
    vol->vref = g_vfs.nextVRef++;

    /* Mount the volume */
    serial_puts("[VFS] HFS_VolumeMountMemory\n");
    uart_flush();
    if (!HFS_VolumeMountMemory(&vol->volume, volumeData, volumeSize, vol->vref)) {
        serial_puts("[VFS] Mount failed: HFS_VolumeMountMemory failed\n");
        DisposePtr((Ptr)volumeData);
        return false;
    }

    /* Initialize catalog */
    serial_puts("[VFS] HFS_CatalogInit\n");
    uart_flush();
    if (!HFS_CatalogInit(&vol->catalog, &vol->volume)) {
        HFS_VolumeUnmount(&vol->volume);
        DisposePtr((Ptr)volumeData);
        serial_puts("[VFS] Mount failed: HFS_CatalogInit failed\n");
        return false;
    }

    /* Mark as mounted */
    vol->mounted = true;
    strncpy(vol->name, volName, sizeof(vol->name) - 1);
    vol->name[sizeof(vol->name) - 1] = '\0';

    serial_puts("[VFS] Boot volume mounted successfully (1MB)\n");
    uart_flush();

    /* Notify mount callback */
    if (g_vfs.mountCallback) {
        g_vfs.mountCallback(vol->vref, volName);
    }

    return true;
}

/* Format an ATA disk with HFS filesystem - REQUIRES EXPLICIT CALL */
bool VFS_FormatATA(int ata_device_index, const char* volName) {
    extern bool HFS_FormatVolume(HFS_BlockDev* bd, const char* volName);

    if (!g_vfs.initialized) {
        FS_LOG_DEBUG("VFS: Not initialized\n");
        return false;
    }

    /* Initialize temporary block device */
    HFS_BlockDev bd;
    if (!HFS_BD_InitATA(&bd, ata_device_index, false)) {
        FS_LOG_DEBUG("VFS: Failed to initialize ATA block device for formatting\n");
        return false;
    }

    /* Format the volume */
    FS_LOG_DEBUG("VFS: Formatting ATA device %d as '%s'...\n", ata_device_index, volName);
    bool result = HFS_FormatVolume(&bd, volName);

    /* Close block device */
    HFS_BD_Close(&bd);

    if (result) {
        FS_LOG_DEBUG("VFS: ATA device %d formatted successfully\n", ata_device_index);
    } else {
        FS_LOG_DEBUG("VFS: Failed to format ATA device %d\n", ata_device_index);
    }

    return result;
}

bool VFS_MountATA(int ata_device_index, const char* volName, VRefNum* vref) {
    if (!g_vfs.initialized) {
        FS_LOG_DEBUG("VFS: Not initialized\n");
        return false;
    }

    /* Allocate volume slot */
    VFSVolume* vol = VFS_AllocVolume();
    if (!vol) {
        FS_LOG_DEBUG("VFS: No free volume slots\n");
        return false;
    }

    /* Assign vref */
    vol->vref = g_vfs.nextVRef++;

    /* Initialize block device from ATA */
    if (!HFS_BD_InitATA(&vol->volume.bd, ata_device_index, false)) {
        FS_LOG_DEBUG("VFS: Failed to initialize ATA block device\n");
        return false;
    }

    /* Check if disk is formatted by reading MDB */
    uint8_t mdbSector[512];

    if (!HFS_BD_ReadSector(&vol->volume.bd, HFS_MDB_SECTOR, mdbSector)) {
        FS_LOG_DEBUG("VFS: Failed to read MDB sector\n");
        HFS_BD_Close(&vol->volume.bd);
        return false;
    }

    /* Check HFS signature */
    uint16_t sig = be16_read(&mdbSector[0]);

    if (sig != HFS_SIGNATURE) {
        FS_LOG_DEBUG("VFS: ERROR - Disk is not formatted with HFS (signature: 0x%04x)\n", sig);
        FS_LOG_DEBUG("VFS: Use VFS_FormatATA() to format this disk first\n");
        HFS_BD_Close(&vol->volume.bd);
        return false;
    }

    /* Disk is formatted, proceed with mounting */
    FS_LOG_DEBUG("VFS: Found valid HFS signature, mounting...\n");

    /* Parse MDB into volume structure */
    HFS_MDB* mdb = &vol->volume.mdb;

    mdb->drSigWord    = be16_read(&mdbSector[0]);
    mdb->drCrDate     = be32_read(&mdbSector[4]);
    mdb->drLsMod      = be32_read(&mdbSector[8]);
    mdb->drAtrb       = be16_read(&mdbSector[12]);
    mdb->drNmFls      = be16_read(&mdbSector[14]);
    mdb->drVBMSt      = be16_read(&mdbSector[16]);
    mdb->drAllocPtr   = be16_read(&mdbSector[18]);
    mdb->drNmAlBlks   = be16_read(&mdbSector[20]);
    mdb->drAlBlkSiz   = be32_read(&mdbSector[22]);
    mdb->drClpSiz     = be32_read(&mdbSector[26]);
    mdb->drAlBlSt     = be16_read(&mdbSector[30]);
    mdb->drNxtCNID    = be32_read(&mdbSector[32]);
    mdb->drFreeBks    = be16_read(&mdbSector[36]);

    /* Volume name */
    memcpy(mdb->drVN, &mdbSector[38], 28);

    /* Catalog file */
    mdb->drCTFlSize = be32_read(&mdbSector[142]);
    for (int i = 0; i < 3; i++) {
        mdb->drCTExtRec[i].startBlock = be16_read(&mdbSector[146 + i * 4]);
        mdb->drCTExtRec[i].blockCount = be16_read(&mdbSector[148 + i * 4]);
    }

    /* Extents file */
    mdb->drXTFlSize = be32_read(&mdbSector[126]);
    for (int i = 0; i < 3; i++) {
        mdb->drXTExtRec[i].startBlock = be16_read(&mdbSector[130 + i * 4]);
        mdb->drXTExtRec[i].blockCount = be16_read(&mdbSector[132 + i * 4]);
    }

    /* Cache volume parameters */
    vol->volume.alBlkSize = mdb->drAlBlkSiz;
    vol->volume.alBlSt = mdb->drAlBlSt;
    vol->volume.numAlBlks = mdb->drNmAlBlks;
    vol->volume.vbmStart = mdb->drVBMSt;
    vol->volume.catFileSize = mdb->drCTFlSize;
    memcpy(vol->volume.catExtents, mdb->drCTExtRec, sizeof(vol->volume.catExtents));
    vol->volume.extFileSize = mdb->drXTFlSize;
    memcpy(vol->volume.extExtents, mdb->drXTExtRec, sizeof(vol->volume.extExtents));
    vol->volume.nextCNID = mdb->drNxtCNID;
    vol->volume.rootDirID = 2;  /* HFS root is always 2 */

    /* Mark volume as mounted */
    vol->volume.vRefNum = vol->vref;
    vol->volume.mounted = true;

    /* Try to initialize catalog */
    if (!HFS_CatalogInit(&vol->catalog, &vol->volume)) {
        FS_LOG_DEBUG("VFS: Warning - Failed to initialize catalog for ATA volume\n");
        /* Continue anyway for empty formatted volumes */
    }

    /* Mark as mounted */
    vol->mounted = true;
    strncpy(vol->name, volName, sizeof(vol->name) - 1);
    vol->name[sizeof(vol->name) - 1] = '\0';

    FS_LOG_DEBUG("VFS: Mounted ATA volume '%s' as vRef %d\n", volName, vol->vref);

    /* Return vref */
    if (vref) {
        *vref = vol->vref;
    }

    /* Notify mount callback */
    if (g_vfs.mountCallback) {
        g_vfs.mountCallback(vol->vref, volName);
    }

    return true;
}

/* Format an SDHCI SD card with HFS filesystem - REQUIRES EXPLICIT CALL */
bool VFS_FormatSDHCI(int drive_index, const char* volName) {
    extern bool HFS_FormatVolume(HFS_BlockDev* bd, const char* volName);

    if (!g_vfs.initialized) {
        FS_LOG_DEBUG("VFS: Not initialized\n");
        return false;
    }

    #ifdef __ARM__
    /* Initialize temporary block device */
    HFS_BlockDev bd;
    if (!HFS_BD_InitSDHCI(&bd, drive_index, false)) {
        FS_LOG_DEBUG("VFS: Failed to initialize SDHCI block device for formatting\n");
        return false;
    }

    /* Format the volume */
    FS_LOG_DEBUG("VFS: Formatting SDHCI drive %d as '%s'...\n", drive_index, volName);
    bool result = HFS_FormatVolume(&bd, volName);

    /* Close block device */
    HFS_BD_Close(&bd);

    if (result) {
        FS_LOG_DEBUG("VFS: SDHCI drive %d formatted successfully\n", drive_index);
    } else {
        FS_LOG_DEBUG("VFS: Failed to format SDHCI drive %d\n", drive_index);
    }

    return result;
    #else
    FS_LOG_DEBUG("VFS: SDHCI not supported on this platform\n");
    return false;
    #endif
}

bool VFS_MountSDHCI(int drive_index, const char* volName, VRefNum* vref) {
    if (!g_vfs.initialized) {
        FS_LOG_DEBUG("VFS: Not initialized\n");
        return false;
    }

    #ifdef __ARM__
    /* Allocate volume slot */
    VFSVolume* vol = VFS_AllocVolume();
    if (!vol) {
        FS_LOG_DEBUG("VFS: No free volume slots\n");
        return false;
    }

    /* Assign vref */
    vol->vref = g_vfs.nextVRef++;

    /* Initialize block device from SDHCI */
    if (!HFS_BD_InitSDHCI(&vol->volume.bd, drive_index, false)) {
        FS_LOG_DEBUG("VFS: Failed to initialize SDHCI block device\n");
        return false;
    }

    /* Check if disk is formatted by reading MDB */
    uint8_t mdbSector[512];

    if (!HFS_BD_ReadSector(&vol->volume.bd, HFS_MDB_SECTOR, mdbSector)) {
        FS_LOG_DEBUG("VFS: Failed to read MDB sector from SDHCI\n");
        HFS_BD_Close(&vol->volume.bd);
        return false;
    }

    /* Check HFS signature */
    uint16_t sig = be16_read(&mdbSector[0]);

    if (sig != HFS_SIGNATURE) {
        FS_LOG_DEBUG("VFS: ERROR - SD card is not formatted with HFS (signature: 0x%04x)\n", sig);
        FS_LOG_DEBUG("VFS: Use VFS_FormatSDHCI() to format this SD card first\n");
        HFS_BD_Close(&vol->volume.bd);
        return false;
    }

    /* Disk is formatted, proceed with mounting */
    FS_LOG_DEBUG("VFS: Found valid HFS signature on SDHCI, mounting...\n");

    /* Parse MDB into volume structure */
    HFS_MDB* mdb = &vol->volume.mdb;

    mdb->drSigWord    = be16_read(&mdbSector[0]);
    mdb->drCrDate     = be32_read(&mdbSector[4]);
    mdb->drLsMod      = be32_read(&mdbSector[8]);
    mdb->drAtrb       = be16_read(&mdbSector[12]);
    mdb->drNmFls      = be16_read(&mdbSector[14]);
    mdb->drVBMSt      = be16_read(&mdbSector[16]);
    mdb->drAllocPtr   = be16_read(&mdbSector[18]);
    mdb->drNmAlBlks   = be16_read(&mdbSector[20]);
    mdb->drAlBlkSiz   = be32_read(&mdbSector[22]);
    mdb->drClpSiz     = be32_read(&mdbSector[26]);
    mdb->drAlBlSt     = be16_read(&mdbSector[30]);
    mdb->drNxtCNID    = be32_read(&mdbSector[32]);
    mdb->drFreeBks    = be16_read(&mdbSector[36]);

    /* Volume name */
    memcpy(mdb->drVN, &mdbSector[38], 28);

    /* Catalog file */
    mdb->drCTFlSize = be32_read(&mdbSector[142]);
    for (int i = 0; i < 3; i++) {
        mdb->drCTExtRec[i].startBlock = be16_read(&mdbSector[146 + i * 4]);
        mdb->drCTExtRec[i].blockCount = be16_read(&mdbSector[148 + i * 4]);
    }

    /* Extents file */
    mdb->drXTFlSize = be32_read(&mdbSector[126]);
    for (int i = 0; i < 3; i++) {
        mdb->drXTExtRec[i].startBlock = be16_read(&mdbSector[130 + i * 4]);
        mdb->drXTExtRec[i].blockCount = be16_read(&mdbSector[132 + i * 4]);
    }

    /* Cache volume parameters */
    vol->volume.alBlkSize = mdb->drAlBlkSiz;
    vol->volume.alBlSt = mdb->drAlBlSt;
    vol->volume.numAlBlks = mdb->drNmAlBlks;
    vol->volume.vbmStart = mdb->drVBMSt;
    vol->volume.catFileSize = mdb->drCTFlSize;
    memcpy(vol->volume.catExtents, mdb->drCTExtRec, sizeof(vol->volume.catExtents));
    vol->volume.extFileSize = mdb->drXTFlSize;
    memcpy(vol->volume.extExtents, mdb->drXTExtRec, sizeof(vol->volume.extExtents));
    vol->volume.nextCNID = mdb->drNxtCNID;
    vol->volume.rootDirID = 2;  /* HFS root is always 2 */

    /* Mark volume as mounted */
    vol->volume.vRefNum = vol->vref;
    vol->volume.mounted = true;

    /* Try to initialize catalog */
    if (!HFS_CatalogInit(&vol->catalog, &vol->volume)) {
        FS_LOG_DEBUG("VFS: Warning - Failed to initialize catalog for SDHCI volume\n");
        /* Continue anyway for empty formatted volumes */
    }

    /* Mark as mounted */
    vol->mounted = true;
    strncpy(vol->name, volName, sizeof(vol->name) - 1);
    vol->name[sizeof(vol->name) - 1] = '\0';

    FS_LOG_DEBUG("VFS: Mounted SDHCI volume '%s' as vRef %d\n", volName, vol->vref);

    /* Return vref */
    if (vref) {
        *vref = vol->vref;
    }

    /* Notify mount callback */
    if (g_vfs.mountCallback) {
        g_vfs.mountCallback(vol->vref, volName);
    }

    return true;
    #else
    FS_LOG_DEBUG("VFS: SDHCI not supported on this platform\n");
    return false;
    #endif
}

bool VFS_Unmount(VRefNum vref) {
    VFSVolume* vol = VFS_FindVolume(vref);
    if (!vol) {
        return false;
    }

    /* Close catalog */
    HFS_CatalogClose(&vol->catalog);

    /* Unmount volume */
    HFS_VolumeUnmount(&vol->volume);

    /* Mark as unmounted */
    vol->mounted = false;

    FS_LOG_DEBUG("VFS: Unmounted volume vRef %d\n", vref);
    return true;
}

/* Populate initial file system contents */
bool VFS_PopulateInitialFiles(void) {
    FS_LOG_DEBUG("VFS: Populating initial file system...\n");

    /* Find boot volume (vRef 1) */
    VFSVolume* vol = VFS_FindVolume(1);
    if (!vol || !vol->mounted) {
        FS_LOG_DEBUG("VFS: Cannot populate - boot volume not mounted\n");
        return false;
    }

    VRefNum vref = vol->vref;
    DirID rootDir = 2;  /* Root directory is always ID 2 in HFS */

    /* Create System Folder (if it doesn't exist) */
    DirID systemID = 0;
    CatEntry systemEntry;
    if (VFS_Lookup(vref, rootDir, "System Folder", &systemEntry)) {
        FS_LOG_DEBUG("VFS: System Folder already exists (ID=%d)\n", systemEntry.id);
        systemID = systemEntry.id;
    } else {
        if (!VFS_CreateFolder(vref, rootDir, "System Folder", &systemID)) {
            FS_LOG_DEBUG("VFS: Failed to create System Folder\n");
            return false;
        }
        FS_LOG_DEBUG("VFS: Created System Folder (ID=%d)\n", systemID);
    }

    /* Create Documents folder (if it doesn't exist) */
    DirID documentsID = 0;
    CatEntry documentsEntry;
    if (VFS_Lookup(vref, rootDir, "Documents", &documentsEntry)) {
        FS_LOG_DEBUG("VFS: Documents folder already exists (ID=%d)\n", documentsEntry.id);
        documentsID = documentsEntry.id;
    } else {
        if (!VFS_CreateFolder(vref, rootDir, "Documents", &documentsID)) {
            FS_LOG_DEBUG("VFS: Failed to create Documents folder\n");
            return false;
        }
        FS_LOG_DEBUG("VFS: Created Documents folder (ID=%d)\n", documentsID);
    }

    /* Create Applications folder (if it doesn't exist) */
    DirID appsID = 0;
    CatEntry appsEntry;
    if (VFS_Lookup(vref, rootDir, "Applications", &appsEntry)) {
        FS_LOG_DEBUG("VFS: Applications folder already exists (ID=%d)\n", appsEntry.id);
        appsID = appsEntry.id;
    } else {
        if (!VFS_CreateFolder(vref, rootDir, "Applications", &appsID)) {
            FS_LOG_DEBUG("VFS: Failed to create Applications folder\n");
            return false;
        }
        FS_LOG_DEBUG("VFS: Created Applications folder (ID=%d)\n", appsID);
    }

    /* Create README file in root */
    FileID readmeID = 0;
    if (!VFS_CreateFile(vref, rootDir, "Read Me", 'TEXT', 'ttxt', &readmeID)) {
        FS_LOG_DEBUG("VFS: Failed to create Read Me file\n");
        return false;
    }
    FS_LOG_DEBUG("VFS: Created Read Me file (ID=%u)\n", readmeID);

    /* Create About This Mac file */
    FileID aboutID = 0;
    if (!VFS_CreateFile(vref, rootDir, "About This Mac", 'TEXT', 'ttxt', &aboutID)) {
        FS_LOG_DEBUG("VFS: Failed to create About This Mac file\n");
        return false;
    }
    FS_LOG_DEBUG("VFS: Created About This Mac file (ID=%u)\n", aboutID);

    /* Create some sample documents */
    FileID doc1ID = 0;
    if (!VFS_CreateFile(vref, documentsID, "Sample Document", 'TEXT', 'ttxt', &doc1ID)) {
        FS_LOG_DEBUG("VFS: Failed to create Sample Document\n");
        return false;
    }
    FS_LOG_DEBUG("VFS: Created Sample Document (ID=%u)\n", doc1ID);

    FileID doc2ID = 0;
    if (!VFS_CreateFile(vref, documentsID, "Notes", 'TEXT', 'ttxt', &doc2ID)) {
        FS_LOG_DEBUG("VFS: Failed to create Notes file\n");
        return false;
    }
    FS_LOG_DEBUG("VFS: Created Notes file (ID=%u)\n", doc2ID);

    FS_LOG_DEBUG("VFS: Initial file system population complete\n");
    return true;
}

bool VFS_GetVolumeInfo(VRefNum vref, VolumeControlBlock* vcb) {
    if (!g_vfs.initialized || !vcb) return false;

    VFSVolume* vol = VFS_FindVolume(vref);
    if (!vol || !vol->mounted) {
        return false;
    }

    return HFS_GetVolumeInfo(&vol->volume, vcb);
}

VRefNum VFS_GetBootVRef(void) {
    /* Boot volume is always vRef 1 */
    return 1;
}

bool VFS_Enumerate(VRefNum vref, DirID dir, CatEntry* entries, int maxEntries, int* count) {

    FS_LOG_DEBUG("VFS_Enumerate: ENTRY vref=%d dir=%d maxEntries=%d\n", (int)vref, (int)dir, maxEntries);

    if (!g_vfs.initialized || !entries || !count) {
        FS_LOG_DEBUG("VFS_Enumerate: Invalid params - init=%d entries=%p count=%p\n",
                     g_vfs.initialized, (void*)entries, (void*)count);
        return false;
    }

    VFSVolume* vol = VFS_FindVolume(vref);
    if (!vol || !vol->mounted) {
        FS_LOG_DEBUG("VFS_Enumerate: vref %d not found or not mounted\n", (int)vref);
        return false;
    }

    /* Check if B-tree is initialized */
    if (!vol->catalog.bt.nodeBuffer) {
        FS_LOG_DEBUG("VFS_Enumerate: nodeBuffer is NULL - returning empty list\n");
        *count = 0;
        return true;
    }

    FS_LOG_DEBUG("VFS_Enumerate: Calling HFS_CatalogEnumerate, nodeBuffer=%p\n",
                 vol->catalog.bt.nodeBuffer);
    bool result = HFS_CatalogEnumerate(&vol->catalog, dir, entries, maxEntries, count);
    FS_LOG_DEBUG("VFS_Enumerate: HFS_CatalogEnumerate returned %d, count=%d\n", result, *count);
    return result;
}

bool VFS_Lookup(VRefNum vref, DirID dir, const char* name, CatEntry* entry) {
    if (!g_vfs.initialized || !name || !entry) return false;

    VFSVolume* vol = VFS_FindVolume(vref);
    if (!vol || !vol->mounted) {
        return false;
    }

    return HFS_CatalogLookup(&vol->catalog, dir, name, entry);
}

bool VFS_GetByID(VRefNum vref, FileID id, CatEntry* entry) {
    if (!g_vfs.initialized || !entry) return false;

    VFSVolume* vol = VFS_FindVolume(vref);
    if (!vol || !vol->mounted) {
        return false;
    }

    return HFS_CatalogGetByID(&vol->catalog, id, entry);
}

VFSFile* VFS_OpenFile(VRefNum vref, FileID id, bool resourceFork) {
    if (!g_vfs.initialized) return NULL;

    VFSVolume* vol = VFS_FindVolume(vref);
    if (!vol || !vol->mounted) {
        return NULL;
    }

    HFSFile* hfsFile = HFS_FileOpen(&vol->catalog, id, resourceFork);
    if (!hfsFile) return NULL;

    VFSFile* vfsFile = (VFSFile*)NewPtr(sizeof(VFSFile));
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

    VFSVolume* vol = VFS_FindVolume(vref);
    if (!vol || !vol->mounted) {
        return NULL;
    }

    HFSFile* hfsFile = HFS_FileOpenByPath(&vol->catalog, path, resourceFork);
    if (!hfsFile) return NULL;

    VFSFile* vfsFile = (VFSFile*)NewPtr(sizeof(VFSFile));
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

    DisposePtr((Ptr)file);
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
    FS_LOG_DEBUG("VFS_CreateFolder: Creating folder '%s' in parent %d\n", name, parent);

    /* Validate parameters */
    if (!name || !newID) {
        FS_LOG_DEBUG("VFS_CreateFolder: Invalid parameters\n");
        return false;
    }

    /* Check volume - for now just validate vref */
    if (vref != 0 && vref != -1) {
        FS_LOG_DEBUG("VFS_CreateFolder: Invalid volume %d\n", vref);
        return false;
    }

    /* Generate new folder ID */
    static DirID nextDirID = 1000;
    *newID = nextDirID++;

    /* Log success for now - full implementation would update HFS catalog */
    FS_LOG_DEBUG("VFS_CreateFolder: Created folder '%s' with ID %d\n", name, *newID);
    return true;
}

bool VFS_CreateFile(VRefNum vref, DirID parent, const char* name,
                   uint32_t type, uint32_t creator, FileID* newID) {
    FS_LOG_DEBUG("VFS_CreateFile: Creating file '%s' type='%.4s' creator='%.4s'\n",
                  name, (char*)&type, (char*)&creator);

    /* Validate parameters */
    if (!name || !newID) {
        FS_LOG_DEBUG("VFS_CreateFile: Invalid parameters\n");
        return false;
    }

    /* Check volume - for now just validate vref */
    if (vref != 0 && vref != -1) {
        FS_LOG_DEBUG("VFS_CreateFile: Invalid volume %d\n", vref);
        return false;
    }

    /* Generate new file ID */
    static FileID nextFileID = 2000;
    *newID = nextFileID++;

    /* Log success for now - full implementation would update HFS catalog */
    FS_LOG_DEBUG("VFS_CreateFile: Created file '%s' with ID %u\n", name, *newID);
    return true;
}

bool VFS_Rename(VRefNum vref, FileID id, const char* newName) {
    FS_LOG_DEBUG("VFS_Rename: Renaming file/folder %u to '%s'\n", id, newName);

    /* Validate parameters */
    if (!newName || strlen(newName) == 0 || strlen(newName) > 31) {
        FS_LOG_DEBUG("VFS_Rename: Invalid name\n");
        return false;
    }

    /* Check volume - for now just validate vref */
    if (vref != 0 && vref != -1) {
        FS_LOG_DEBUG("VFS_Rename: Invalid volume %d\n", vref);
        return false;
    }

    /* Log success for now - full implementation would update HFS catalog */
    FS_LOG_DEBUG("VFS_Rename: Successfully renamed ID %u to '%s'\n", id, newName);
    return true;
}

bool VFS_Delete(VRefNum vref, FileID id) {
    FS_LOG_DEBUG("VFS_Delete: Deleting file/folder ID %u\n", id);

    /* Check volume - for now just validate vref */
    if (vref != 0 && vref != -1) {
        FS_LOG_DEBUG("VFS_Delete: Invalid volume %d\n", vref);
        return false;
    }

    /* Check for special folders that shouldn't be deleted */
    if (id == 1 || id == 2) {  /* Root (1) or System folder (2) */
        FS_LOG_DEBUG("VFS_Delete: Cannot delete system folder\n");
        return false;
    }

    /* Log success for now - full implementation would update HFS catalog */
    FS_LOG_DEBUG("VFS_Delete: Successfully deleted ID %u\n", id);
    return true;
}