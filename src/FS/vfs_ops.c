/* VFS operations for Trash support
 * Bridges HFS catalog operations with trash requirements
 */

#include "FS/vfs_ops.h"
#include "FS/vfs.h"
#include "FS/hfs_types.h"
#include <string.h>
#include "FS/FSLogging.h"

bool VFS_EnsureHiddenFolder(VRefNum vref, const char* name, DirID* outDir) {
    if (!outDir) return false;

    /* Check if folder already exists at volume root (dirID=2) */
    CatEntry entry;
    if (VFS_Lookup(vref, 2, name, &entry) && entry.kind == kNodeDir) {
        *outDir = entry.id;
        return true;
    }

    /* Create it via VFS (uses overlay) */
    return VFS_CreateFolder(vref, 2, name, outDir);
}

bool VFS_Move(VRefNum vref, DirID fromDir, FileID id, DirID toDir, const char* newName) {
    FS_LOG_DEBUG("VFS_Move: id=%u from dir=%u to dir=%u, newName=%s\n",
                 id, fromDir, toDir, newName ? newName : "(null)");

    /* Get current entry data */
    CatEntry entry;
    if (!VFS_GetByID(vref, id, &entry)) {
        FS_LOG_DEBUG("VFS_Move: entry %u not found\n", id);
        return false;
    }

    /* Use internal overlay access — VFS_MoveOverlay updates parent */
    extern bool VFS_MoveOverlay(VRefNum vref, FileID id, DirID newParent,
                                const char* newName, const CatEntry* current);
    return VFS_MoveOverlay(vref, id, toDir, newName, &entry);
}

bool VFS_Copy(VRefNum vref, DirID fromDir, FileID id, DirID toDir,
              const char* newName, FileID* newID) {
    FS_LOG_DEBUG("VFS_Copy: id=%u from dir=%u to dir=%u\n", id, fromDir, toDir);

    /* Read source entry */
    CatEntry src;
    if (!VFS_GetByID(vref, id, &src)) return false;

    /* Create new entry in destination */
    char copyName[32];
    if (newName) {
        strncpy(copyName, newName, 31);
    } else {
        strncpy(copyName, src.name, 31);
    }
    copyName[31] = '\0';

    if (src.kind == kNodeDir) {
        DirID dirID;
        if (!VFS_CreateFolder(vref, toDir, copyName, &dirID)) return false;
        if (newID) *newID = dirID;
    } else {
        FileID fid;
        if (!VFS_CreateFile(vref, toDir, copyName, src.type, src.creator, &fid)) return false;
        if (newID) *newID = fid;
    }

    return true;
}

bool VFS_DeleteTree(VRefNum vref, DirID parent, FileID id) {
    FS_LOG_DEBUG("VFS_DeleteTree: id=%u parent=%u\n", id, parent);

    /* Get entry info */
    CatEntry entry;
    if (!VFS_GetByID(vref, id, &entry)) return true;  /* Already gone */

    /* If directory, recursively delete contents first */
    if (entry.kind == kNodeDir) {
        CatEntry children[64];
        int count = 0;
        if (VFS_Enumerate(vref, id, children, 64, &count)) {
            for (int i = 0; i < count; i++) {
                VFS_DeleteTree(vref, id, children[i].id);
            }
        }
    }

    /* Delete this entry */
    return VFS_Delete(vref, id);
}

bool VFS_GetDirItemCount(VRefNum vref, DirID dir, uint32_t* outCount, bool recursive) {
    if (!outCount) return false;

    CatEntry entries[128];
    int count = 0;
    if (!VFS_Enumerate(vref, dir, entries, 128, &count)) {
        *outCount = 0;
        return false;
    }

    uint32_t total = (uint32_t)count;
    if (recursive) {
        for (int i = 0; i < count; i++) {
            if (entries[i].kind == kNodeDir) {
                uint32_t sub = 0;
                VFS_GetDirItemCount(vref, entries[i].id, &sub, true);
                total += sub;
            }
        }
    }

    *outCount = total;
    return true;
}

bool VFS_IsOpenByAnyProcess(FileID id) {
    /* No process tracking yet */
    return false;
}

bool VFS_IsLocked(FileID id) {
    /* Check Finder info locked flag - no lock tracking yet */
    return false;
}

bool VFS_SetFinderFlags(FileID id, uint16_t setMask, uint16_t clearMask) {
    /* Update Finder info flags - no metadata tracking yet */
    return true;
}

bool VFS_GenerateUniqueName(VRefNum vref, DirID dir, const char* base, char* out) {
    /* Generate "Name", "Name 2", "Name 3", etc. */
    if (!VFS_Exists(vref, dir, base)) {
        strncpy(out, base, 31);
        out[31] = 0;
        return true;
    }

    for (int i = 2; i < 1000; i++) {
        char tmp[40];
        int base_len = strlen(base);
        if (base_len > 25) base_len = 25;
        memcpy(tmp, base, base_len);
        tmp[base_len] = ' ';

        int num = i;
        int digits = 0;
        char numbuf[10];
        do {
            numbuf[digits++] = '0' + (num % 10);
            num /= 10;
        } while (num > 0);

        for (int d = 0; d < digits; d++) {
            tmp[base_len + 1 + d] = numbuf[digits - 1 - d];
        }
        tmp[base_len + 1 + digits] = 0;

        if (!VFS_Exists(vref, dir, tmp)) {
            strncpy(out, tmp, 31);
            out[31] = 0;
            return true;
        }
    }

    return false;
}

bool VFS_Exists(VRefNum vref, DirID dir, const char* name) {
    CatEntry entry;
    return VFS_Lookup(vref, dir, name, &entry);
}

const char* VFS_GetNameByID(VRefNum vref, DirID parent, FileID id) {
    static char nameBuf[32];
    CatEntry entry;
    if (VFS_GetByID(vref, id, &entry)) {
        strncpy(nameBuf, entry.name, 31);
        nameBuf[31] = '\0';
        return nameBuf;
    }
    strncpy(nameBuf, "Item", 5);
    return nameBuf;
}

VRefNum VFS_GetVRefByID(FileID id) {
    extern VRefNum VFS_GetBootVRef(void);
    return VFS_GetBootVRef();
}

bool VFS_GetParentDir(VRefNum vref, FileID id, DirID* parentDir) {
    if (!parentDir) return false;

    CatEntry entry;
    if (VFS_GetByID(vref, id, &entry)) {
        *parentDir = entry.parent;
        return true;
    }

    *parentDir = 2;  /* Fallback to root */
    return true;
}
