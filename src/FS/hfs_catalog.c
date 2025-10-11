/* HFS Catalog Operations Implementation */
#include "../../include/FS/hfs_catalog.h"
#include "../../include/FS/hfs_endian.h"
#include "../../include/MemoryMgr/MemoryManager.h"
#include <string.h>
#include "FS/FSLogging.h"

/* Serial debug output */

/* MacRoman to ASCII conversion table (simplified) */
static const uint8_t macRomanToASCII[128] = {
    /* 0x80-0xFF: Extended characters - map to ASCII approximations */
    'A', 'A', 'C', 'E', 'N', 'O', 'U', 'a', 'a', 'a', 'a', 'a', 'a', 'c', 'e', 'e',
    'e', 'e', 'i', 'i', 'i', 'i', 'n', 'o', 'o', 'o', 'o', 'o', 'u', 'u', 'u', 'u',
    ' ', ' ', ' ', ' ', ' ', '*', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '
};

void HFS_MacRomanToASCII(char* dst, const uint8_t* src, uint8_t len, size_t maxDst) {
    if (!dst || !src || maxDst == 0) return;

    size_t i;
    for (i = 0; i < len && i < maxDst - 1; i++) {
        uint8_t c = src[i];
        if (c < 0x80) {
            dst[i] = c;  /* Standard ASCII */
        } else {
            dst[i] = macRomanToASCII[c - 0x80];
        }
    }
    dst[i] = '\0';
}

bool HFS_ParseCatalogRecord(const HFS_CatKey* key, const void* data, uint16_t dataLen,
                           CatEntry* entry) {
    if (!key || !data || !entry) return false;

    memset(entry, 0, sizeof(CatEntry));

    /* Get parent ID and name from key */
    entry->parent = be32_read(&key->parentID);

    /* Debug: log key name data */
    FS_LOG_DEBUG("HFS_ParseCatalogRecord: key->nameLength=%d, key->name=[", key->nameLength);
    for (int i = 0; i < key->nameLength && i < 32; i++) {
        FS_LOG_DEBUG("%02x ", key->name[i]);
    }
    FS_LOG_DEBUG("]\n");

    HFS_MacRomanToASCII(entry->name, key->name, key->nameLength, sizeof(entry->name));

    /* Debug: log converted name */
    FS_LOG_DEBUG("HFS_ParseCatalogRecord: converted name='%s'\n", entry->name);

    /* Parse record type */
    uint16_t recordType = be16_read(data);

    switch (recordType) {
    case kHFS_FolderRecord: {
        const HFS_CatFolderRec* folder = (const HFS_CatFolderRec*)data;
        entry->kind = kNodeDir;
        entry->id = be32_read(&folder->folderID);
        entry->flags = be16_read(&folder->flags);
        entry->modTime = be32_read(&folder->modifyDate);
        entry->size = 0;  /* Folders don't have size */
        entry->type = make_ostype('f', 'l', 'd', 'r');
        entry->creator = make_ostype('M', 'A', 'C', 'S');
        return true;
    }

    case kHFS_FileRecord: {
        const HFS_CatFileRec* file = (const HFS_CatFileRec*)data;
        entry->kind = kNodeFile;
        entry->id = be32_read(&file->fileID);
        entry->flags = file->flags;
        entry->modTime = be32_read(&file->modifyDate);
        entry->size = be32_read(&file->dataLogicalSize);

        /* Get type and creator from Finder info */
        if (file->finderInfo[0] || file->finderInfo[1] ||
            file->finderInfo[2] || file->finderInfo[3]) {
            entry->type = be32_read(&file->finderInfo[0]);
            entry->creator = be32_read(&file->finderInfo[4]);
        } else {
            /* Default for files without Finder info */
            entry->type = make_ostype('?', '?', '?', '?');
            entry->creator = make_ostype('?', '?', '?', '?');
        }
        return true;
    }

    case kHFS_FolderThreadRecord:
    case kHFS_FileThreadRecord:
        /* Thread records - skip for enumeration */
        return false;

    default:
        /* FS_LOG_DEBUG("HFS Catalog: Unknown record type 0x%04x\n", recordType); */
        return false;
    }
}

/* Enumeration context */
typedef struct {
    DirID     parentID;
    CatEntry* entries;
    int       maxEntries;
    int       count;
} EnumContext;

/* Enumeration callback */
static bool enum_callback(void* keyPtr, uint16_t keyLen,
                         void* dataPtr, uint16_t dataLen,
                         void* context) {
    EnumContext* ctx = (EnumContext*)context;
    HFS_CatKey* key = (HFS_CatKey*)keyPtr;

    /* Debug: show first 8 bytes of key */
    uint8_t* bytes = (uint8_t*)keyPtr;
    FS_LOG_DEBUG("HFS enum_callback: keyBytes=[%02x %02x %02x %02x %02x %02x %02x %02x]\n",
                 bytes[0], bytes[1], bytes[2], bytes[3], bytes[4], bytes[5], bytes[6], bytes[7]);

    /* Check if this entry belongs to our parent */
    uint32_t entryParent = be32_read(&key->parentID);

    FS_LOG_DEBUG("HFS enum_callback: entryParent=%d target=%d nameLen=%d\n",
                 (int)entryParent, (int)ctx->parentID, key->nameLength);

    if (entryParent != ctx->parentID) {
        return true;  /* Continue iteration */
    }

    /* Check if we have room */
    if (ctx->count >= ctx->maxEntries) {
        FS_LOG_DEBUG("HFS enum_callback: out of room\n");
        return false;  /* Stop iteration */
    }

    /* Parse the record */
    CatEntry entry;
    if (HFS_ParseCatalogRecord(key, dataPtr, dataLen, &entry)) {
        FS_LOG_DEBUG("HFS enum_callback: matched! adding entry '%s'\n", entry.name);
        ctx->entries[ctx->count++] = entry;
    }

    return true;  /* Continue */
}

bool HFS_CatalogInit(HFS_Catalog* cat, HFS_Volume* vol) {
    /* FS_LOG_DEBUG("HFS_CatalogInit: ENTER (cat=%p, vol=%p)\n", cat, vol); */

    if (!cat || !vol || !vol->mounted) {
        FS_LOG_DEBUG("HFS_CatalogInit: Invalid params (cat=%p, vol=%p, mounted=%d)\n",
                     cat, vol, vol ? vol->mounted : 0);
        return false;
    }

    memset(cat, 0, sizeof(HFS_Catalog));
    cat->vol = vol;

    /* Initialize B-tree */
    FS_LOG_DEBUG("HFS_CatalogInit: About to initialize catalog B-tree (vol=%p, catFileSize=%u)\n",
                 vol, vol->catFileSize);
    if (!HFS_BT_Init(&cat->bt, vol, kBTreeCatalog)) {
        /* FS_LOG_DEBUG("HFS_CatalogInit: B-tree init failed\n"); */
        return false;
    }

    /* FS_LOG_DEBUG("HFS_CatalogInit: Success\n"); */
    return true;
}

void HFS_CatalogClose(HFS_Catalog* cat) {
    if (!cat) return;

    HFS_BT_Close(&cat->bt);
    memset(cat, 0, sizeof(HFS_Catalog));
}

bool HFS_CatalogEnumerate(HFS_Catalog* cat, DirID parentID,
                         CatEntry* entries, int maxEntries, int* count) {

    FS_LOG_DEBUG("HFS_CatalogEnumerate: ENTRY parentID=%d maxEntries=%d\n", (int)parentID, maxEntries);

    if (!cat || !entries || maxEntries <= 0 || !count) {
        FS_LOG_DEBUG("HFS_CatalogEnumerate: Invalid params\n");
        return false;
    }

    EnumContext ctx = {
        .parentID = parentID,
        .entries = entries,
        .maxEntries = maxEntries,
        .count = 0
    };

    FS_LOG_DEBUG("HFS_CatalogEnumerate: Calling HFS_BT_IterateLeaves, firstLeaf=%d\n",
                 (int)cat->bt.firstLeaf);

    /* Iterate through all leaf nodes */
    bool result = HFS_BT_IterateLeaves(&cat->bt, enum_callback, &ctx);

    FS_LOG_DEBUG("HFS_CatalogEnumerate: IterateLeaves returned %d, found %d entries\n",
                 result, ctx.count);

    *count = ctx.count;
    return result;
}

bool HFS_CatalogLookup(HFS_Catalog* cat, DirID parentID, const char* name,
                       CatEntry* entry) {
    if (!cat || !name || !entry) return false;

    /* Convert name to MacRoman Pascal string */
    uint8_t pname[32];
    size_t len = strlen(name);
    if (len > 31) len = 31;
    pname[0] = len;
    memcpy(pname + 1, name, len);

    /* Build catalog key */
    HFS_CatKey searchKey;
    searchKey.keyLength = 6 + len;  /* 1 + 1 + 4 + 1 + nameLen - 1 */
    searchKey.reserved = 0;
    be32_write(&searchKey.parentID, parentID);
    searchKey.nameLength = len;
    memcpy(searchKey.name, pname + 1, len);

    /* Linear search through leaves (simple implementation) */
    CatEntry entries[100];
    int count;
    if (!HFS_CatalogEnumerate(cat, parentID, entries, 100, &count)) {
        return false;
    }

    /* Find matching name (case-insensitive) */
    for (int i = 0; i < count; i++) {
        bool match = true;
        for (size_t j = 0; j < len; j++) {
            char c1 = entries[i].name[j];
            char c2 = name[j];
            if (c1 >= 'a' && c1 <= 'z') c1 -= 32;
            if (c2 >= 'a' && c2 <= 'z') c2 -= 32;
            if (c1 != c2) {
                match = false;
                break;
            }
        }
        if (match && entries[i].name[len] == '\0') {
            *entry = entries[i];
            return true;
        }
    }

    return false;
}

/* Get by ID context */
typedef struct {
    FileID    targetID;
    CatEntry* result;
    bool      found;
} GetByIDContext;

/* Get by ID callback */
static bool getbyid_callback(void* keyPtr, uint16_t keyLen,
                            void* dataPtr, uint16_t dataLen,
                            void* context) {
    GetByIDContext* ctx = (GetByIDContext*)context;
    HFS_CatKey* key = (HFS_CatKey*)keyPtr;

    /* Parse the record */
    CatEntry entry;
    if (!HFS_ParseCatalogRecord(key, dataPtr, dataLen, &entry)) {
        return true;  /* Continue - might be thread record */
    }

    /* Check if this is our target */
    if (entry.id == ctx->targetID) {
        *ctx->result = entry;
        ctx->found = true;
        return false;  /* Stop iteration */
    }

    return true;  /* Continue */
}

bool HFS_CatalogGetByID(HFS_Catalog* cat, FileID cnid, CatEntry* entry) {
    if (!cat || !entry || cnid < HFS_FIRST_CNID) return false;

    GetByIDContext ctx = {
        .targetID = cnid,
        .result = entry,
        .found = false
    };

    /* Iterate through all leaf nodes looking for our ID */
    HFS_BT_IterateLeaves(&cat->bt, getbyid_callback, &ctx);

    return ctx.found;
}

bool HFS_CatalogCreateFolder(HFS_Catalog* cat, DirID parentID, const char* name,
                             FileID* newFolderID) {
    if (!cat || !name || !newFolderID) {
        FS_LOG_DEBUG("HFS_CatalogCreateFolder: Invalid parameters\n");
        return false;
    }

    FS_LOG_DEBUG("HFS_CatalogCreateFolder: Creating folder '%s' in parent %d\n", name, parentID);

    /* Allocate a new CNID */
    FileID folderID = cat->vol->nextCNID++;

    FS_LOG_DEBUG("HFS_CatalogCreateFolder: Allocated CNID %d\n", folderID);

    /* Build catalog key */
    HFS_CatKey key;
    size_t nameLen = strlen(name);
    if (nameLen > 31) nameLen = 31;

    key.keyLength = 6 + nameLen;  /* reserved(1) + parentID(4) + nameLength(1) + name */
    key.reserved = 0;
    be32_write(&key.parentID, parentID);
    key.nameLength = nameLen;
    memcpy(key.name, name, nameLen);

    /* Build folder record */
    HFS_CatFolderRec folder;
    memset(&folder, 0, sizeof(folder));

    be16_write(&folder.recordType, kHFS_FolderRecord);
    be16_write(&folder.flags, 0);
    be16_write(&folder.valence, 0);  /* No items in folder yet */
    be32_write(&folder.folderID, folderID);
    be32_write(&folder.createDate, 0);  /* TODO: Get current time */
    be32_write(&folder.modifyDate, 0);
    be32_write(&folder.backupDate, 0);
    memset(folder.finderInfo, 0, sizeof(folder.finderInfo));
    memset(folder.reserved, 0, sizeof(folder.reserved));

    /* Insert into catalog B-tree */
    /* keyLen must include the keyLength byte itself + the rest of the key */
    bool success = HFS_BT_InsertRecord(&cat->bt, &key, 1 + key.keyLength,
                                       &folder, sizeof(HFS_CatFolderRec));

    if (success) {
        *newFolderID = folderID;
        FS_LOG_DEBUG("HFS_CatalogCreateFolder: Successfully created folder with ID %d\n", folderID);
    } else {
        FS_LOG_DEBUG("HFS_CatalogCreateFolder: Failed to insert catalog record\n");
    }

    return success;
}