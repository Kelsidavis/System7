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

/* In-memory overlay for filesystem mutations */
#define VFS_MAX_OVERLAY 256

typedef struct {
    FileID  id;
    bool    active;         /* Slot in use */
    bool    deleted;        /* Entry was deleted */
    bool    created;        /* Entry was created (not from catalog) */
    bool    moved;          /* Parent directory changed */
    bool    renamed;        /* Name changed */
    DirID   newParent;      /* New parent dir if moved */
    CatEntry entry;         /* Full entry (for created entries, or modified state) */
    /* File data storage for overlay-created files */
    uint8_t* fileData;      /* Persisted file content */
    uint32_t fileDataSize;  /* Size of file content */
} VFSOverlayEntry;

/* Mounted volume entry */
typedef struct {
    bool         mounted;
    VRefNum      vref;
    HFS_Volume   volume;
    HFS_Catalog  catalog;
    char         name[256];
    /* In-memory overlay for create/delete/move/rename */
    VFSOverlayEntry overlay[VFS_MAX_OVERLAY];
    int             overlayCount;
    FileID          nextCNID;      /* Next catalog node ID for new entries */
} VFSVolume;

/* VFS state */
static struct {
    bool                initialized;
    VFSVolume          volumes[VFS_MAX_VOLUMES];
    VRefNum             nextVRef;
    VFS_MountCallback  mountCallback;
} g_vfs = { 0 };

/* VFS file wrapper — supports both HFS-backed and overlay-backed files */
struct VFSFile {
    HFSFile* hfsFile;      /* HFS backing (NULL for overlay files) */
    VRefNum  vref;
    FileID   fileID;        /* For overlay files: ID to persist on close */
    /* In-memory data for overlay-created files */
    uint8_t* memData;       /* Malloc'd buffer (NULL if HFS-backed) */
    uint32_t memSize;       /* Current data size */
    uint32_t memCapacity;   /* Buffer capacity */
    uint32_t memPosition;   /* Read/write position */
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

/* Helper: Find overlay entry by ID */
static VFSOverlayEntry* VFS_FindOverlay(VFSVolume* vol, FileID id) {
    for (int i = 0; i < VFS_MAX_OVERLAY; i++) {
        if (vol->overlay[i].active && vol->overlay[i].id == id) {
            return &vol->overlay[i];
        }
    }
    return NULL;
}

/* Helper: Allocate overlay entry */
static VFSOverlayEntry* VFS_AllocOverlay(VFSVolume* vol) {
    for (int i = 0; i < VFS_MAX_OVERLAY; i++) {
        if (!vol->overlay[i].active) {
            memset(&vol->overlay[i], 0, sizeof(VFSOverlayEntry));
            vol->overlay[i].active = true;
            vol->overlayCount++;
            return &vol->overlay[i];
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

    /* Mark as mounted and initialize overlay */
    vol->mounted = true;
    memset(vol->overlay, 0, sizeof(vol->overlay));
    vol->overlayCount = 0;
    vol->nextCNID = 5000;  /* Start above typical HFS CNIDs */
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

    /* Mark as mounted and initialize overlay */
    vol->mounted = true;
    memset(vol->overlay, 0, sizeof(vol->overlay));
    vol->overlayCount = 0;
    vol->nextCNID = 5000;
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

    /* Mark as mounted and initialize overlay */
    vol->mounted = true;
    memset(vol->overlay, 0, sizeof(vol->overlay));
    vol->overlayCount = 0;
    vol->nextCNID = 5000;
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
        FS_LOG_DEBUG("VFS_Enumerate: Invalid params\n");
        return false;
    }

    VFSVolume* vol = VFS_FindVolume(vref);
    if (!vol || !vol->mounted) {
        FS_LOG_DEBUG("VFS_Enumerate: vref %d not found or not mounted\n", (int)vref);
        return false;
    }

    int n = 0;

    /* First: get catalog entries (if B-tree available) */
    if (vol->catalog.bt.nodeBuffer) {
        int catCount = 0;
        HFS_CatalogEnumerate(&vol->catalog, dir, entries, maxEntries, &catCount);

        /* Filter out deleted and moved-away entries, apply renames */
        for (int i = 0; i < catCount && n < maxEntries; i++) {
            FileID eid = entries[i].id;
            VFSOverlayEntry* oe = VFS_FindOverlay(vol, eid);
            if (oe) {
                if (oe->deleted) continue;  /* Skip deleted */
                if (oe->moved && oe->newParent != dir) continue;  /* Moved away */
                /* Apply renames */
                if (oe->renamed) {
                    strncpy(entries[n].name, oe->entry.name, 31);
                    entries[n].name[31] = '\0';
                }
                if (i != n) entries[n] = entries[i];
            } else {
                if (i != n) entries[n] = entries[i];
            }
            n++;
        }
    }

    /* Second: add entries moved INTO this directory from elsewhere */
    for (int i = 0; i < VFS_MAX_OVERLAY && n < maxEntries; i++) {
        VFSOverlayEntry* oe = &vol->overlay[i];
        if (!oe->active || oe->deleted) continue;
        if (oe->moved && !oe->created && oe->newParent == dir) {
            /* Check it wasn't already in catalog results for this dir */
            bool alreadyListed = false;
            for (int j = 0; j < n; j++) {
                if (entries[j].id == oe->id) { alreadyListed = true; break; }
            }
            if (!alreadyListed) {
                entries[n++] = oe->entry;
            }
        }
    }

    /* Third: add overlay-created entries in this directory */
    for (int i = 0; i < VFS_MAX_OVERLAY && n < maxEntries; i++) {
        VFSOverlayEntry* oe = &vol->overlay[i];
        if (!oe->active || oe->deleted || !oe->created) continue;
        if (oe->entry.parent == dir) {
            entries[n++] = oe->entry;
        }
    }

    *count = n;
    FS_LOG_DEBUG("VFS_Enumerate: returned %d entries\n", n);
    return true;
}

bool VFS_Lookup(VRefNum vref, DirID dir, const char* name, CatEntry* entry) {
    if (!g_vfs.initialized || !name || !entry) return false;

    VFSVolume* vol = VFS_FindVolume(vref);
    if (!vol || !vol->mounted) return false;

    /* Check overlay first — created entries and renamed entries */
    for (int i = 0; i < VFS_MAX_OVERLAY; i++) {
        VFSOverlayEntry* oe = &vol->overlay[i];
        if (!oe->active || oe->deleted) continue;

        DirID effectiveParent = oe->moved ? oe->newParent : oe->entry.parent;
        if (effectiveParent == dir && strcmp(oe->entry.name, name) == 0) {
            *entry = oe->entry;
            return true;
        }
    }

    /* Fall through to catalog */
    if (!HFS_CatalogLookup(&vol->catalog, dir, name, entry)) return false;

    /* Check if catalog result was deleted or moved away */
    VFSOverlayEntry* oe = VFS_FindOverlay(vol, entry->id);
    if (oe) {
        if (oe->deleted) return false;
        if (oe->moved && oe->newParent != dir) return false;
        if (oe->renamed) {
            strncpy(entry->name, oe->entry.name, 31);
            entry->name[31] = '\0';
        }
    }

    return true;
}

bool VFS_GetByID(VRefNum vref, FileID id, CatEntry* entry) {
    if (!g_vfs.initialized || !entry) return false;

    VFSVolume* vol = VFS_FindVolume(vref);
    if (!vol || !vol->mounted) return false;

    /* Check overlay first */
    VFSOverlayEntry* oe = VFS_FindOverlay(vol, id);
    if (oe) {
        if (oe->deleted) return false;
        *entry = oe->entry;
        return true;
    }

    return HFS_CatalogGetByID(&vol->catalog, id, entry);
}

VFSFile* VFS_OpenFile(VRefNum vref, FileID id, bool resourceFork) {
    if (!g_vfs.initialized) return NULL;

    VFSVolume* vol = VFS_FindVolume(vref);
    if (!vol || !vol->mounted) return NULL;

    /* Check if this is an overlay-created file (no HFS backing) */
    VFSOverlayEntry* oe = VFS_FindOverlay(vol, id);
    if (oe && oe->created && !oe->deleted) {
        /* Overlay-backed file — use in-memory buffer */
        VFSFile* vfsFile = (VFSFile*)NewPtr(sizeof(VFSFile));
        if (!vfsFile) return NULL;
        memset(vfsFile, 0, sizeof(VFSFile));
        vfsFile->vref = vref;
        vfsFile->fileID = id;

        /* Load any previously persisted data */
        if (oe->fileData && oe->fileDataSize > 0) {
            uint32_t cap = (oe->fileDataSize + 4095) & ~4095u;
            vfsFile->memData = (uint8_t*)NewPtr(cap);
            if (vfsFile->memData) {
                memcpy(vfsFile->memData, oe->fileData, oe->fileDataSize);
                vfsFile->memSize = oe->fileDataSize;
                vfsFile->memCapacity = cap;
            }
        }
        return vfsFile;
    }

    /* HFS-backed file */
    HFSFile* hfsFile = HFS_FileOpen(&vol->catalog, id, resourceFork);
    if (!hfsFile) return NULL;

    VFSFile* vfsFile = (VFSFile*)NewPtr(sizeof(VFSFile));
    if (!vfsFile) {
        HFS_FileClose(hfsFile);
        return NULL;
    }
    memset(vfsFile, 0, sizeof(VFSFile));
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

    /* Persist in-memory data to overlay entry on close */
    if (file->memData && file->memSize > 0 && file->fileID != 0) {
        VFSVolume* vol = VFS_FindVolume(file->vref);
        if (vol) {
            VFSOverlayEntry* oe = VFS_FindOverlay(vol, file->fileID);
            if (oe && oe->created) {
                /* Free old persisted data */
                if (oe->fileData) {
                    DisposePtr((Ptr)oe->fileData);
                }
                /* Copy current buffer to overlay */
                oe->fileData = (uint8_t*)NewPtr(file->memSize);
                if (oe->fileData) {
                    memcpy(oe->fileData, file->memData, file->memSize);
                    oe->fileDataSize = file->memSize;
                    /* Update CatEntry size and modification time */
                    oe->entry.size = file->memSize;
                    extern void GetDateTime(uint32_t* secs);
                    uint32_t now = 0;
                    GetDateTime(&now);
                    oe->entry.modTime = now;
                }
            }
        }
    }

    if (file->memData) {
        DisposePtr((Ptr)file->memData);
    }

    DisposePtr((Ptr)file);
}

bool VFS_ReadFile(VFSFile* file, void* buffer, uint32_t length, uint32_t* bytesRead) {
    if (!file || !buffer) return false;

    /* In-memory file */
    if (file->memData) {
        uint32_t avail = (file->memPosition < file->memSize) ?
                          file->memSize - file->memPosition : 0;
        uint32_t toRead = (length < avail) ? length : avail;
        if (toRead > 0) {
            memcpy(buffer, file->memData + file->memPosition, toRead);
            file->memPosition += toRead;
        }
        if (bytesRead) *bytesRead = toRead;
        return true;
    }

    /* HFS-backed file */
    if (!file->hfsFile) return false;
    return HFS_FileRead(file->hfsFile, buffer, length, bytesRead);
}

bool VFS_WriteFile(VFSFile* file, const void* buffer, uint32_t length, uint32_t* bytesWritten) {
    if (!file || !buffer) return false;

    /* Ensure we have an in-memory buffer */
    uint32_t endPos = file->memPosition + length;

    if (endPos > file->memCapacity) {
        /* Grow buffer — round up to 4KB blocks */
        uint32_t newCap = (endPos + 4095) & ~4095u;
        uint8_t* newBuf = (uint8_t*)NewPtr(newCap);
        if (!newBuf) return false;
        memset(newBuf, 0, newCap);
        if (file->memData && file->memSize > 0) {
            memcpy(newBuf, file->memData, file->memSize);
            DisposePtr((Ptr)file->memData);
        }
        file->memData = newBuf;
        file->memCapacity = newCap;
    }

    memcpy(file->memData + file->memPosition, buffer, length);
    file->memPosition += length;
    if (file->memPosition > file->memSize) {
        file->memSize = file->memPosition;
    }

    if (bytesWritten) *bytesWritten = length;
    return true;
}

bool VFS_SeekFile(VFSFile* file, uint32_t position) {
    if (!file) return false;
    if (file->memData || !file->hfsFile) {
        file->memPosition = position;
        return true;
    }
    return HFS_FileSeek(file->hfsFile, position);
}

uint32_t VFS_GetFileSize(VFSFile* file) {
    if (!file) return 0;
    if (file->memData || !file->hfsFile) return file->memSize;
    return HFS_FileGetSize(file->hfsFile);
}

uint32_t VFS_GetFilePosition(VFSFile* file) {
    if (!file) return 0;
    if (file->memData || !file->hfsFile) return file->memPosition;
    return HFS_FileTell(file->hfsFile);
}

/* Move entry to a new parent directory (overlay-based) */
bool VFS_MoveOverlay(VRefNum vref, FileID id, DirID newParent,
                     const char* newName, const CatEntry* current) {
    VFSVolume* vol = VFS_FindVolume(vref);
    if (!vol || !vol->mounted || !current) return false;

    /* Check if already in overlay */
    VFSOverlayEntry* oe = VFS_FindOverlay(vol, id);
    if (oe) {
        oe->moved = true;
        oe->newParent = newParent;
        oe->entry.parent = newParent;
        if (newName) {
            strncpy(oe->entry.name, newName, 31);
            oe->entry.name[31] = '\0';
            oe->renamed = true;
        }
        return true;
    }

    /* Create new overlay entry */
    oe = VFS_AllocOverlay(vol);
    if (!oe) return false;

    oe->id = id;
    oe->moved = true;
    oe->newParent = newParent;
    oe->entry = *current;
    oe->entry.parent = newParent;
    if (newName) {
        strncpy(oe->entry.name, newName, 31);
        oe->entry.name[31] = '\0';
        oe->renamed = true;
    }

    FS_LOG_DEBUG("VFS_MoveOverlay: Moved ID %u to parent %u\n", id, newParent);
    return true;
}

/* Write operations */
bool VFS_CreateFolder(VRefNum vref, DirID parent, const char* name, DirID* newID) {
    FS_LOG_DEBUG("VFS_CreateFolder: Creating folder '%s' in parent %d\n", name, parent);

    if (!name || !newID) return false;

    VFSVolume* vol = VFS_FindVolume(vref);
    if (!vol || !vol->mounted) return false;

    VFSOverlayEntry* oe = VFS_AllocOverlay(vol);
    if (!oe) return false;

    /* Get current time for timestamps */
    extern void GetDateTime(uint32_t* secs);
    uint32_t now = 0;
    GetDateTime(&now);

    FileID id = vol->nextCNID++;
    oe->id = id;
    oe->created = true;
    strncpy(oe->entry.name, name, 31);
    oe->entry.name[31] = '\0';
    oe->entry.kind = kNodeDir;
    oe->entry.parent = parent;
    oe->entry.id = id;
    oe->entry.createTime = now;
    oe->entry.modTime = now;

    *newID = id;
    FS_LOG_DEBUG("VFS_CreateFolder: Created folder '%s' with ID %u\n", name, id);
    return true;
}

bool VFS_CreateFile(VRefNum vref, DirID parent, const char* name,
                   uint32_t type, uint32_t creator, FileID* newID) {
    FS_LOG_DEBUG("VFS_CreateFile: Creating file '%s'\n", name);

    if (!name || !newID) return false;

    VFSVolume* vol = VFS_FindVolume(vref);
    if (!vol || !vol->mounted) return false;

    VFSOverlayEntry* oe = VFS_AllocOverlay(vol);
    if (!oe) return false;

    /* Get current time for timestamps */
    extern void GetDateTime(uint32_t* secs);
    uint32_t now = 0;
    GetDateTime(&now);

    FileID id = vol->nextCNID++;
    oe->id = id;
    oe->created = true;
    strncpy(oe->entry.name, name, 31);
    oe->entry.name[31] = '\0';
    oe->entry.kind = kNodeFile;
    oe->entry.type = type;
    oe->entry.creator = creator;
    oe->entry.parent = parent;
    oe->entry.id = id;
    oe->entry.createTime = now;
    oe->entry.modTime = now;

    *newID = id;
    FS_LOG_DEBUG("VFS_CreateFile: Created file '%s' with ID %u\n", name, id);
    return true;
}

bool VFS_Rename(VRefNum vref, FileID id, const char* newName) {
    FS_LOG_DEBUG("VFS_Rename: Renaming file/folder %u to '%s'\n", id, newName);

    if (!newName || strlen(newName) == 0 || strlen(newName) > 31) return false;

    VFSVolume* vol = VFS_FindVolume(vref);
    if (!vol || !vol->mounted) return false;

    /* Check if already in overlay */
    VFSOverlayEntry* oe = VFS_FindOverlay(vol, id);
    if (oe) {
        /* Update existing overlay entry */
        strncpy(oe->entry.name, newName, 31);
        oe->entry.name[31] = '\0';
        oe->renamed = true;
        return true;
    }

    /* Create new overlay entry from catalog data */
    CatEntry catEntry;
    if (!HFS_CatalogGetByID(&vol->catalog, id, &catEntry)) {
        return false;
    }

    oe = VFS_AllocOverlay(vol);
    if (!oe) return false;

    oe->id = id;
    oe->renamed = true;
    oe->entry = catEntry;
    strncpy(oe->entry.name, newName, 31);
    oe->entry.name[31] = '\0';

    FS_LOG_DEBUG("VFS_Rename: Successfully renamed ID %u to '%s'\n", id, newName);
    return true;
}

bool VFS_Delete(VRefNum vref, FileID id) {
    FS_LOG_DEBUG("VFS_Delete: Deleting file/folder ID %u\n", id);

    /* Protect root and system folders */
    if (id <= 2) return false;

    VFSVolume* vol = VFS_FindVolume(vref);
    if (!vol || !vol->mounted) return false;

    /* Check if it's an overlay-created entry */
    VFSOverlayEntry* oe = VFS_FindOverlay(vol, id);
    if (oe) {
        if (oe->created) {
            /* Was created in overlay — free data and remove the slot */
            if (oe->fileData) {
                DisposePtr((Ptr)oe->fileData);
                oe->fileData = NULL;
            }
            oe->active = false;
            vol->overlayCount--;
        } else {
            /* Mark catalog entry as deleted */
            oe->deleted = true;
        }
        return true;
    }

    /* Mark catalog entry as deleted via new overlay slot */
    oe = VFS_AllocOverlay(vol);
    if (!oe) return false;

    oe->id = id;
    oe->deleted = true;

    FS_LOG_DEBUG("VFS_Delete: Marked ID %u as deleted\n", id);
    return true;
}

bool VFS_SetCatEntryInfo(VRefNum vref, FileID id,
                         uint32_t type, uint32_t creator, uint16_t flags) {
    VFSVolume* vol = VFS_FindVolume(vref);
    if (!vol || !vol->mounted) return false;

    /* Check if already in overlay */
    VFSOverlayEntry* oe = VFS_FindOverlay(vol, id);
    if (oe) {
        oe->entry.type = type;
        oe->entry.creator = creator;
        oe->entry.flags = flags;
        return true;
    }

    /* Create overlay entry from catalog */
    CatEntry catEntry;
    if (!HFS_CatalogGetByID(&vol->catalog, id, &catEntry)) return false;

    oe = VFS_AllocOverlay(vol);
    if (!oe) return false;

    oe->id = id;
    oe->entry = catEntry;
    oe->entry.type = type;
    oe->entry.creator = creator;
    oe->entry.flags = flags;
    return true;
}