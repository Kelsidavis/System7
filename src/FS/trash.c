/* System 7-style Trash implementation
 * Per-volume hidden Trash folders with desktop aggregation
 */

#include "FS/trash.h"
#include "FS/vfs_ops.h"
#include <string.h>
#include "FS/FSLogging.h"

#define TRASH_NAME "Trash"   /* classic 7.x name (hidden) */
static TrashInfo gTrash[8];  /* small mount table (grow if needed) */
static int gTrashCount = 0;

/* Find slot for volume */
static TrashInfo* find_slot(VRefNum v) {
    for (int i = 0; i < gTrashCount; i++) {
        if (gTrash[i].vref == v) return &gTrash[i];
    }
    return NULL;
}

/* Get or create slot for volume */
static TrashInfo* ensure_slot(VRefNum v) {
    TrashInfo* s = find_slot(v);
    if (s) return s;

    if (gTrashCount < (int)(sizeof(gTrash) / sizeof(gTrash[0]))) {
        TrashInfo* new_slot = &gTrash[gTrashCount++];
        new_slot->vref = v;
        new_slot->dirTrash = 0;
        new_slot->itemCount = 0;
        new_slot->exists = false;
        return new_slot;
    }
    return NULL;
}

/* Initialize trash system */
bool Trash_Init(void) {
    gTrashCount = 0;
    memset(gTrash, 0, sizeof(gTrash));
    return true;
}

/* Called when volume is mounted */
bool Trash_OnVolumeMount(VRefNum vref) {
    TrashInfo* ti = ensure_slot(vref);
    if (!ti) return false;

    DirID d = 0;
    if (!VFS_EnsureHiddenFolder(vref, TRASH_NAME, &d)) {
        return false;
    }

    ti->dirTrash = d;
    ti->exists = true;

    /* Get initial item count */
    VFS_GetDirItemCount(vref, d, &ti->itemCount, false);

    return true;
}

/* Called when volume is unmounted */
bool Trash_OnVolumeUnmount(VRefNum vref) {
    TrashInfo* ti = find_slot(vref);
    if (ti) {
        ti->exists = false;
        ti->itemCount = 0;
    }
    return true;
}

/* Get trash directory for volume */
bool Trash_GetDir(VRefNum vref, DirID* outDir) {
    TrashInfo* ti = find_slot(vref);
    if (!ti || !ti->exists) return false;
    *outDir = ti->dirTrash;
    return true;
}

/* Check if all trashes are empty */
bool Trash_IsEmptyAll(void) {
    return Trash_TotalItems() == 0;
}

/* Count total items across all volumes */
uint32_t Trash_TotalItems(void) {
    uint32_t sum = 0;
    for (int i = 0; i < gTrashCount; i++) {
        if (gTrash[i].exists) {
            sum += gTrash[i].itemCount;
        }
    }
    return sum;
}

/* Move a file/folder to trash */
bool Trash_MoveNode(VRefNum vref, DirID parent, FileID id) {
    /* Refuse if busy or locked */
    if (VFS_IsOpenByAnyProcess(id)) return false;
    if (VFS_IsLocked(id)) return false;

    /* Get or create trash directory */
    DirID trashDir = 0;
    if (!Trash_GetDir(vref, &trashDir)) {
        if (!Trash_OnVolumeMount(vref) || !Trash_GetDir(vref, &trashDir)) {
            return false;
        }
    }

    /* Get original name */
    const char* original = VFS_GetNameByID(vref, parent, id);
    if (!original) return false;

    /* Generate unique name in trash */
    char unique[32];
    if (!VFS_GenerateUniqueName(vref, trashDir, original, unique)) {
        return false;
    }

    /* Move to trash */
    if (!VFS_Move(vref, parent, id, trashDir, unique)) {
        return false;
    }

    /* Update cached count */
    TrashInfo* ti = find_slot(vref);
    if (ti) {
        uint32_t c = 0;
        VFS_GetDirItemCount(vref, trashDir, &c, false);
        ti->itemCount = c;
    }

    return true;
}

/* Empty all trash folders */
bool Trash_EmptyAll(bool force) {
    bool ok = true;

    for (int i = 0; i < gTrashCount; i++) {
        if (!gTrash[i].exists) continue;

        VRefNum v = gTrash[i].vref;
        DirID d = gTrash[i].dirTrash;

        /* Enumerate and delete contents */
        CatEntry ents[256];
        int n = 0;

        if (!VFS_Enumerate(v, d, ents, 256, &n)) {
            ok = false;
            continue;
        }

        for (int k = 0; k < n; k++) {
            if (!force && (VFS_IsLocked(ents[k].id) || VFS_IsOpenByAnyProcess(ents[k].id))) {
                ok = false;
                continue; /* skip locked/busy items */
            }

            if (!VFS_DeleteTree(v, d, ents[k].id)) {
                ok = false;
            }
        }

        /* Refresh count */
        uint32_t c = 0;
        VFS_GetDirItemCount(v, d, &c, false);
        gTrash[i].itemCount = c;
    }

    return ok;
}

/* Open trash window (boot volume's trash for now) */
bool Trash_OpenDesktopWindow(void) {
    /* For now, just return true - will integrate with window manager later */
    return true;
}

/* Get appropriate icon for current trash state */
__attribute__((unused))
static const void* Trash_CurrentIcon(void) {
    /* Will be linked to icon system */
    extern const void* IconSys_TrashEmpty(void);
    extern const void* IconSys_TrashFull(void);

    return Trash_IsEmptyAll() ? IconSys_TrashEmpty() : IconSys_TrashFull();
}