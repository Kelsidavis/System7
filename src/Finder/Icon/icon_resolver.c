/* Icon Resolution with Priority System
 * Faithful to System 7 icon lookup order
 */

#include "Finder/Icon/icon_types.h"
#include "Finder/Icon/icon_resources.h"
#include "Finder/Icon/icon_system.h"
#include <stddef.h>
#include <string.h>

/* Icon cache for performance */
#define ICON_CACHE_SIZE 32

typedef struct IconCacheEntry {
    uint32_t type;
    uint32_t creator;
    int16_t rsrcID;
    IconFamily family;
    bool valid;
    uint32_t lastAccess;  /* For LRU eviction */
} IconCacheEntry;

static IconCacheEntry gIconCache[ICON_CACHE_SIZE];
static uint32_t gCacheAccessCounter = 0;
static bool gIconSystemInitialized = false;

/* System icon resource IDs (standard System 7 icon IDs) */
#define ICON_ID_GENERIC_FOLDER    128
#define ICON_ID_GENERIC_DOCUMENT  129
#define ICON_ID_GENERIC_APP       130
#define ICON_ID_SYSTEM_FOLDER     131
#define ICON_ID_TRASH_EMPTY       132
#define ICON_ID_TRASH_FULL        133
#define ICON_ID_HARD_DISK         134

/* Initialize icon system */
bool Icon_Init(void) {
    if (gIconSystemInitialized) {
        return true;  /* Already initialized */
    }

    /* Clear icon cache */
    memset(gIconCache, 0, sizeof(gIconCache));
    gCacheAccessCounter = 0;

    /* Pre-load common system icons from resources if available */
    /* This improves performance by caching frequently used icons */

    /* Try to load generic folder icon */
    IconFamily tempFamily;
    if (IconRes_LoadFamilyByID(ICON_ID_GENERIC_FOLDER, &tempFamily)) {
        /* Cache the folder icon for quick access */
        gIconCache[0].type = 'fold';
        gIconCache[0].creator = 0;
        gIconCache[0].rsrcID = ICON_ID_GENERIC_FOLDER;
        gIconCache[0].family = tempFamily;
        gIconCache[0].valid = true;
        gIconCache[0].lastAccess = ++gCacheAccessCounter;
    }

    /* Try to load generic document icon */
    if (IconRes_LoadFamilyByID(ICON_ID_GENERIC_DOCUMENT, &tempFamily)) {
        gIconCache[1].type = 'TEXT';
        gIconCache[1].creator = 0;
        gIconCache[1].rsrcID = ICON_ID_GENERIC_DOCUMENT;
        gIconCache[1].family = tempFamily;
        gIconCache[1].valid = true;
        gIconCache[1].lastAccess = ++gCacheAccessCounter;
    }

    /* Try to load generic application icon */
    if (IconRes_LoadFamilyByID(ICON_ID_GENERIC_APP, &tempFamily)) {
        gIconCache[2].type = 'APPL';
        gIconCache[2].creator = 0;
        gIconCache[2].rsrcID = ICON_ID_GENERIC_APP;
        gIconCache[2].family = tempFamily;
        gIconCache[2].valid = true;
        gIconCache[2].lastAccess = ++gCacheAccessCounter;
    }

    /* Try to load trash icons */
    if (IconRes_LoadFamilyByID(ICON_ID_TRASH_EMPTY, &tempFamily)) {
        gIconCache[3].type = 'trsh';
        gIconCache[3].creator = 'emty';
        gIconCache[3].rsrcID = ICON_ID_TRASH_EMPTY;
        gIconCache[3].family = tempFamily;
        gIconCache[3].valid = true;
        gIconCache[3].lastAccess = ++gCacheAccessCounter;
    }

    if (IconRes_LoadFamilyByID(ICON_ID_TRASH_FULL, &tempFamily)) {
        gIconCache[4].type = 'trsh';
        gIconCache[4].creator = 'full';
        gIconCache[4].rsrcID = ICON_ID_TRASH_FULL;
        gIconCache[4].family = tempFamily;
        gIconCache[4].valid = true;
        gIconCache[4].lastAccess = ++gCacheAccessCounter;
    }

    gIconSystemInitialized = true;
    return true;
}

/* Look up icon in cache */
static IconCacheEntry* Icon_FindInCache(uint32_t type, uint32_t creator, int16_t rsrcID) {
    for (int i = 0; i < ICON_CACHE_SIZE; i++) {
        if (gIconCache[i].valid &&
            gIconCache[i].type == type &&
            gIconCache[i].creator == creator &&
            gIconCache[i].rsrcID == rsrcID) {
            gIconCache[i].lastAccess = ++gCacheAccessCounter;
            return &gIconCache[i];
        }
    }
    return NULL;
}

/* Add icon to cache (LRU eviction) */
static IconCacheEntry* Icon_AddToCache(uint32_t type, uint32_t creator, int16_t rsrcID, const IconFamily* family) {
    IconCacheEntry* entry = NULL;

    /* Find empty slot */
    for (int i = 0; i < ICON_CACHE_SIZE; i++) {
        if (!gIconCache[i].valid) {
            entry = &gIconCache[i];
            break;
        }
    }

    /* If no empty slot, evict LRU entry */
    if (!entry) {
        uint32_t minAccess = gCacheAccessCounter;
        int lruIndex = 0;

        for (int i = 0; i < ICON_CACHE_SIZE; i++) {
            if (gIconCache[i].lastAccess < minAccess) {
                minAccess = gIconCache[i].lastAccess;
                lruIndex = i;
            }
        }

        entry = &gIconCache[lruIndex];
    }

    /* Fill cache entry */
    entry->type = type;
    entry->creator = creator;
    entry->rsrcID = rsrcID;
    entry->family = *family;
    entry->valid = true;
    entry->lastAccess = ++gCacheAccessCounter;

    return entry;
}

/* Resolve icon for file/folder */
bool Icon_ResolveForNode(const FileKind* fk, IconHandle* out) {
    static IconFamily tempFam;

    if (!fk || !out) return false;

    /* 1) Custom icon (folder/file flag + "Icon\r") */
    if (fk->hasCustomIcon && fk->path) {
        if (IconRes_LoadCustomIconForPath(fk->path, &tempFam)) {
            out->fam = &tempFam;
            out->selected = false;
            return true;
        }
    }

    /*
     * 2) What the node is, before what it claims to contain.
     *
     * This used to come after the type/creator lookup below, which is the
     * wrong way round: a folder's icon follows from its being a folder, not
     * from a type and creator it has no real use for. Every folder here
     * carries creator 'MACS', that lookup maps 'MACS' to the Finder's own
     * icon, and so every folder on screen was drawn as a small Macintosh.
     *
     * A custom icon still wins - it is checked above - which is right: a
     * folder someone has given an icon to should show it.
     */
    if (fk->isTrash) {
        out->fam = fk->isTrashFull ? IconSys_TrashFull() : IconSys_TrashEmpty();
        out->selected = false;
        return true;
    }

    if (fk->isVolume) {
        out->fam = IconSys_DefaultVolume();
        out->selected = false;
        return true;
    }

    if (fk->isFolder) {
        out->fam = IconSys_DefaultFolder();
        out->selected = false;
        return true;
    }

    /* 3) Bundles: BNDL/FREF by creator/type. Only files reach this now. */
    int16_t iconID = 0;
    if (IconRes_MapTypeCreatorToIcon(fk->type, fk->creator, &iconID)) {
        /* Check cache first */
        IconCacheEntry* cached = Icon_FindInCache(fk->type, fk->creator, iconID);
        if (cached) {
            out->fam = &cached->family;
            out->selected = false;
            return true;
        }

        /* Not in cache - load from resources */
        if (IconRes_LoadFamilyByID(iconID, &tempFam)) {
            /* Add to cache for future lookups */
            IconCacheEntry* entry = Icon_AddToCache(fk->type, fk->creator, iconID, &tempFam);
            if (entry) {
                out->fam = &entry->family;
            } else {
                out->fam = &tempFam;
            }
            out->selected = false;
            return true;
        }

        /*
         * The table named an icon that is not there.
         *
         * IconRes_MapTypeCreatorToIcon hands back 128 for APPL, 129 and 130
         * for TeachText and SimpleText documents, and 131 for plain text.
         * None of those IDs exist: the extracted icon set is keyed by the
         * resource numbers the icons actually had - all negative, stored here
         * as their absolute values - and the only one of the table's numbers
         * that is present is 999, the Finder's own. So every file fell through
         * to the generic document, which is why an application and a text file
         * look identical.
         *
         * Falling through is the right thing to do; doing it without a word is
         * not. Said once, because the alternative is a line per icon per
         * redraw.
         */
        extern void serial_puts(const char* s);
        static bool toldAboutMissingIcon = false;
        if (!toldAboutMissingIcon) {
            toldAboutMissingIcon = true;
            serial_puts("[ICON] type/creator maps to an icon ID that does not exist;\n");
            serial_puts("[ICON] falling back to the generic document for every file.\n");
        }
    }


    /* Default document */
    out->fam = IconSys_DefaultDoc();
    out->selected = false;
    return true;
}

/* Hit testing implementation */
int Icon_HitTest(const IconSlot* slots, int count, int x, int y) {
    for (int i = count - 1; i >= 0; --i) {
        const IconSlot* s = &slots[i];

        /* Check icon bounds */
        if (x >= s->iconR.left && x < s->iconR.right &&
            y >= s->iconR.top && y < s->iconR.bottom) {
            return s->objectId;
        }

        /* Check label bounds */
        if (x >= s->labelR.left && x < s->labelR.right &&
            y >= s->labelR.top && y < s->labelR.bottom) {
            return s->objectId;
        }
    }
    return -1;
}
