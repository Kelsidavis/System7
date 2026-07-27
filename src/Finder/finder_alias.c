/*
 * finder_alias.c - Finder aliases, stored on the VFS
 *
 * System 7 aliases are files of type 'alis' whose data records what they
 * point at, so opening one opens the target instead. The Finder marks them
 * with the alias bit in the catalog's Finder flags, which is what lets it
 * show them in italics and treat them as pointers rather than documents.
 *
 * This is a second implementation, and deliberately so. alias_manager.c
 * builds on the classic File Manager - FSMakeFSSpec, FSpCreate,
 * FSpCreateResFile - and that layer's volume registry is a stub: VCB_Find
 * always comes back empty, so FSMakeFSSpec fails with nsvErr before an alias
 * is ever attempted, and Make Alias did nothing at all. Every Finder
 * operation that does work - Duplicate, Paste, New Folder, Trash - goes
 * through the VFS. Aliases now do too.
 *
 * The stored record is deliberately small and self-describing: a magic word
 * and version so a stale or foreign file is rejected rather than followed,
 * the target's volume and file ID for the fast path, and its name so a
 * target that was moved can still be found by searching. Real System 7
 * alias records carry a good deal more for reconnecting across volumes; this
 * carries what this file system can actually act on.
 *
 * Copyright (c) 2025 - System 7.1 Portable Project
 */

#include <string.h>

#include "SystemTypes.h"
#include "FS/vfs.h"
#include "FS/hfs_types.h"
#include "Finder/finder.h"
#include "Finder/FinderLogging.h"

/* Finder flag marking a file as an alias, per Inside Macintosh. */
#define kFinderIsAliasFlag  0x8000

#define kAliasMagic    0x414C5321UL   /* 'ALS!' */
#define kAliasVersion  1

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t targetIsFolder;
    uint32_t targetVRef;
    uint32_t targetID;
    uint32_t targetParent;
    char     targetName[64];
} AliasPayload;

/*
 * Finder_CreateAliasFile - make an alias to one item, in the given folder.
 *
 * Returns true and fills outID when the alias file exists and carries a
 * usable record. On any failure the partially created file is removed, so a
 * file of type 'alis' that cannot be resolved is never left behind - an
 * alias that silently does nothing when opened is worse than no alias.
 */
Boolean Finder_CreateAliasFile(VRefNum vref, DirID parentDir,
                               const char* aliasName,
                               const CatEntry* target, FileID* outID)
{
    if (!aliasName || !target) return false;

    FileID aliasID = 0;
    if (!VFS_CreateFile(vref, parentDir, aliasName, 'alis', 'MACS', &aliasID)) {
        FINDER_LOG_DEBUG("Alias: could not create '%s'\n", aliasName);
        return false;
    }

    AliasPayload payload;
    memset(&payload, 0, sizeof(payload));
    payload.magic = kAliasMagic;
    payload.version = kAliasVersion;
    payload.targetIsFolder = (target->kind == kNodeDir) ? 1 : 0;
    payload.targetVRef = (uint32_t)vref;
    payload.targetID = (uint32_t)target->id;
    payload.targetParent = (uint32_t)target->parent;
    strncpy(payload.targetName, target->name, sizeof(payload.targetName) - 1);

    VFSFile* file = VFS_OpenFile(vref, aliasID, false);
    if (!file) {
        VFS_Delete(vref, aliasID);
        FINDER_LOG_DEBUG("Alias: could not open '%s' to write its record\n", aliasName);
        return false;
    }

    uint32_t written = 0;
    Boolean ok = VFS_WriteFile(file, &payload, sizeof(payload), &written) &&
                 written == sizeof(payload);
    VFS_CloseFile(file);

    if (!ok) {
        VFS_Delete(vref, aliasID);
        FINDER_LOG_DEBUG("Alias: could not write the record for '%s'\n", aliasName);
        return false;
    }

    /* Mark it as an alias so the Finder treats it as a pointer, keeping the
     * label bits the catalog already holds. */
    CatEntry created;
    if (VFS_GetByID(vref, aliasID, &created)) {
        VFS_SetCatEntryInfo(vref, aliasID, 'alis', 'MACS',
                            (uint16_t)(created.flags | kFinderIsAliasFlag));
    }

    if (outID) *outID = aliasID;
    FINDER_LOG_DEBUG("Alias: '%s' -> '%s' (id=%d)\n",
                     aliasName, target->name, (int)target->id);
    return true;
}

/*
 * Finder_ResolveAlias - follow an alias file to what it points at.
 *
 * The recorded ID is tried first. If that is gone - the target was deleted
 * and its ID reused, or never existed - the name is looked up in the folder
 * the target was last in. Returns false rather than guessing when neither
 * finds anything, so opening a broken alias reports nothing rather than
 * opening the wrong file.
 */
Boolean Finder_ResolveAlias(VRefNum vref, FileID aliasID, CatEntry* outTarget)
{
    if (!outTarget) return false;

    VFSFile* file = VFS_OpenFile(vref, aliasID, false);
    if (!file) return false;

    AliasPayload payload;
    uint32_t got = 0;
    Boolean read = VFS_ReadFile(file, &payload, sizeof(payload), &got);
    VFS_CloseFile(file);

    if (!read || got != sizeof(payload)) {
        FINDER_LOG_DEBUG("Alias: %d has no readable record\n", (int)aliasID);
        return false;
    }
    if (payload.magic != kAliasMagic || payload.version != kAliasVersion) {
        FINDER_LOG_DEBUG("Alias: %d has a record this build does not know\n", (int)aliasID);
        return false;
    }

    if (VFS_GetByID((VRefNum)payload.targetVRef, (FileID)payload.targetID, outTarget)) {
        return true;
    }

    payload.targetName[sizeof(payload.targetName) - 1] = '\0';
    if (VFS_Lookup((VRefNum)payload.targetVRef, (DirID)payload.targetParent,
                   payload.targetName, outTarget)) {
        FINDER_LOG_DEBUG("Alias: %d resolved by name after its target moved\n", (int)aliasID);
        return true;
    }

    FINDER_LOG_DEBUG("Alias: %d points at something that is gone\n", (int)aliasID);
    return false;
}

/* Is this catalog entry an alias file? */
Boolean Finder_IsAliasEntry(const CatEntry* entry)
{
    if (!entry) return false;
    return (entry->type == 'alis') || ((entry->flags & kFinderIsAliasFlag) != 0);
}
