/*
 * FontCache.c - Font Cache Implementation
 *
 * Efficient font caching system to optimize font loading and access.
 * Provides LRU cache management with configurable size limits.
 */

#include "../include/FontManager/FontManager.h"
#include "../include/FontManager/BitmapFonts.h"
#include "../include/FontManager/TrueTypeFonts.h"
#include <Memory.h>
#include <Errors.h>
#include <string.h>
#include <stdlib.h>

/* Font cache global state */
static FontCache *gFontCache = NULL;
static unsigned long gCacheHitCount = 0;
static unsigned long gCacheMissCount = 0;

/* Cache entry hash table for fast lookup */
#define CACHE_HASH_SIZE 64
static FontCacheEntry *gCacheHashTable[CACHE_HASH_SIZE];

/* Internal helper functions */
static unsigned short CalculateCacheHash(short familyID, short fontSize, short faceStyle, short device);
static FontCacheEntry *FindCacheEntry(short familyID, short fontSize, short faceStyle, short device);
static OSErr AddCacheEntry(short familyID, short fontSize, short faceStyle, short device, Handle fontData);
static OSErr RemoveCacheEntry(FontCacheEntry *entry);
static OSErr RemoveLRUEntry(void);
static void UpdateAccessTime(FontCacheEntry *entry);
static unsigned long GetCurrentTime(void);
static OSErr ValidateCacheEntry(FontCacheEntry *entry);

/*
 * InitFontCache - Initialize the font cache system
 */
OSErr InitFontCache(short maxEntries, unsigned long maxSize)
{
    short i;

    if (gFontCache != NULL) {
        /* Already initialized - flush and reinitialize */
        FlushFontCache();
    }

    /* Allocate cache structure */
    gFontCache = (FontCache *)NewPtr(sizeof(FontCache));
    if (gFontCache == NULL) {
        return fontOutOfMemoryErr;
    }

    /* Initialize cache */
    gFontCache->entries = NULL;
    gFontCache->maxEntries = maxEntries;
    gFontCache->currentEntries = 0;
    gFontCache->cacheSize = 0;
    gFontCache->maxSize = maxSize;

    /* Initialize hash table */
    for (i = 0; i < CACHE_HASH_SIZE; i++) {
        gCacheHashTable[i] = NULL;
    }

    /* Reset statistics */
    gCacheHitCount = 0;
    gCacheMissCount = 0;

    return noErr;
}

/*
 * FlushFontCache - Clear all cached fonts
 */
OSErr FlushFontCache(void)
{
    FontCacheEntry *entry, *nextEntry;
    short i;

    if (gFontCache == NULL) {
        return noErr;
    }

    /* Remove all entries */
    entry = gFontCache->entries;
    while (entry != NULL) {
        nextEntry = entry->next;
        RemoveCacheEntry(entry);
        entry = nextEntry;
    }

    /* Clear hash table */
    for (i = 0; i < CACHE_HASH_SIZE; i++) {
        gCacheHashTable[i] = NULL;
    }

    /* Reset cache state */
    gFontCache->entries = NULL;
    gFontCache->currentEntries = 0;
    gFontCache->cacheSize = 0;

    return noErr;
}

/*
 * PurgeFontCache - Remove fonts for specific family from cache
 */
OSErr PurgeFontCache(short familyID)
{
    FontCacheEntry *entry, *nextEntry, *prevEntry;
    short i;

    if (gFontCache == NULL) {
        return noErr;
    }

    /* Scan cache for matching family */
    prevEntry = NULL;
    entry = gFontCache->entries;

    while (entry != NULL) {
        nextEntry = entry->next;

        if (entry->familyID == familyID) {
            /* Remove this entry */
            if (prevEntry != NULL) {
                prevEntry->next = nextEntry;
            } else {
                gFontCache->entries = nextEntry;
            }

            /* Remove from hash table */
            unsigned short hash = CalculateCacheHash(entry->familyID, entry->fontSize,
                                                   entry->faceStyle, entry->device);
            if (gCacheHashTable[hash] == entry) {
                gCacheHashTable[hash] = NULL; /* Simplified - should find next in chain */
            }

            /* Dispose entry */
            if (entry->fontData != NULL) {
                DisposeHandle(entry->fontData);
            }
            DisposePtr((Ptr)entry);

            gFontCache->currentEntries--;
        } else {
            prevEntry = entry;
        }

        entry = nextEntry;
    }

    return noErr;
}

/*
 * GetFontCacheStats - Get cache statistics
 */
OSErr GetFontCacheStats(short *entries, unsigned long *size)
{
    if (gFontCache == NULL) {
        if (entries) *entries = 0;
        if (size) *size = 0;
        return fontNotFoundErr;
    }

    if (entries != NULL) {
        *entries = gFontCache->currentEntries;
    }

    if (size != NULL) {
        *size = gFontCache->cacheSize;
    }

    return noErr;
}

/*
 * CacheBitmapFont - Add bitmap font to cache
 */
OSErr CacheBitmapFont(short familyID, short fontSize, short faceStyle, short device,
                      BitmapFontData *fontData)
{
    Handle fontHandle;
    OSErr error;
    long dataSize;

    if (gFontCache == NULL || fontData == NULL) {
        return paramErr;
    }

    /* Calculate data size */
    dataSize = sizeof(BitmapFontData) + fontData->bitmapSize + fontData->tableSize;

    /* Check if cache has space */
    if (gFontCache->cacheSize + dataSize > gFontCache->maxSize) {
        /* Try to make space */
        while (gFontCache->cacheSize + dataSize > gFontCache->maxSize && gFontCache->currentEntries > 0) {
            error = RemoveLRUEntry();
            if (error != noErr) {
                break;
            }
        }
    }

    /* Create handle for font data */
    fontHandle = NewHandle(dataSize);
    if (fontHandle == NULL) {
        return fontOutOfMemoryErr;
    }

    HLock(fontHandle);
    BlockMoveData(fontData, *fontHandle, sizeof(BitmapFontData));
    /* Copy bitmap and table data */
    BlockMoveData(fontData->bitmapData, *fontHandle + sizeof(BitmapFontData), fontData->bitmapSize);
    BlockMoveData(fontData->offsetWidthTable, *fontHandle + sizeof(BitmapFontData) + fontData->bitmapSize,
                 fontData->tableSize);
    HUnlock(fontHandle);

    /* Add to cache */
    error = AddCacheEntry(familyID, fontSize, faceStyle, device, fontHandle);
    if (error != noErr) {
        DisposeHandle(fontHandle);
        return error;
    }

    return noErr;
}

/*
 * CacheTrueTypeFont - Add TrueType font to cache
 */
OSErr CacheTrueTypeFont(short familyID, short fontSize, short faceStyle, short device,
                        TTFont *fontData)
{
    Handle fontHandle;
    OSErr error;
    long dataSize;

    if (gFontCache == NULL || fontData == NULL) {
        return paramErr;
    }

    /* Calculate data size */
    dataSize = sizeof(TTFont) + fontData->sfntSize;

    /* Check if cache has space */
    if (gFontCache->cacheSize + dataSize > gFontCache->maxSize) {
        /* Try to make space */
        while (gFontCache->cacheSize + dataSize > gFontCache->maxSize && gFontCache->currentEntries > 0) {
            error = RemoveLRUEntry();
            if (error != noErr) {
                break;
            }
        }
    }

    /* Create handle for font data */
    fontHandle = NewHandle(dataSize);
    if (fontHandle == NULL) {
        return fontOutOfMemoryErr;
    }

    HLock(fontHandle);
    BlockMoveData(fontData, *fontHandle, sizeof(TTFont));
    /* Copy SFNT data */
    HLock(fontData->sfntData);
    BlockMoveData(*fontData->sfntData, *fontHandle + sizeof(TTFont), fontData->sfntSize);
    HUnlock(fontData->sfntData);
    HUnlock(fontHandle);

    /* Add to cache */
    error = AddCacheEntry(familyID, fontSize, faceStyle, device, fontHandle);
    if (error != noErr) {
        DisposeHandle(fontHandle);
        return error;
    }

    return noErr;
}

/*
 * GetCachedBitmapFont - Retrieve bitmap font from cache
 */
OSErr GetCachedBitmapFont(short familyID, short fontSize, short faceStyle, short device,
                          BitmapFontData **fontData)
{
    FontCacheEntry *entry;

    if (gFontCache == NULL || fontData == NULL) {
        return paramErr;
    }

    *fontData = NULL;

    /* Look for font in cache */
    entry = FindCacheEntry(familyID, fontSize, faceStyle, device);
    if (entry == NULL) {
        gCacheMissCount++;
        return fontNotFoundErr;
    }

    /* Validate cache entry */
    if (ValidateCacheEntry(entry) != noErr) {
        RemoveCacheEntry(entry);
        gCacheMissCount++;
        return fontCorruptErr;
    }

    /* Update access time */
    UpdateAccessTime(entry);
    gCacheHitCount++;

    /* Return font data */
    HLock(entry->fontData);
    *fontData = (BitmapFontData *)*entry->fontData;
    /* Note: Caller should not dispose this data - it belongs to the cache */

    return noErr;
}

/*
 * GetCachedTrueTypeFont - Retrieve TrueType font from cache
 */
OSErr GetCachedTrueTypeFont(short familyID, short fontSize, short faceStyle, short device,
                            TTFont **fontData)
{
    FontCacheEntry *entry;

    if (gFontCache == NULL || fontData == NULL) {
        return paramErr;
    }

    *fontData = NULL;

    /* Look for font in cache */
    entry = FindCacheEntry(familyID, fontSize, faceStyle, device);
    if (entry == NULL) {
        gCacheMissCount++;
        return fontNotFoundErr;
    }

    /* Validate cache entry */
    if (ValidateCacheEntry(entry) != noErr) {
        RemoveCacheEntry(entry);
        gCacheMissCount++;
        return fontCorruptErr;
    }

    /* Update access time */
    UpdateAccessTime(entry);
    gCacheHitCount++;

    /* Return font data */
    HLock(entry->fontData);
    *fontData = (TTFont *)*entry->fontData;
    /* Note: Caller should not dispose this data - it belongs to the cache */

    return noErr;
}

/*
 * GetCacheHitRatio - Get cache performance statistics
 */
Fixed GetCacheHitRatio(void)
{
    unsigned long totalAccesses = gCacheHitCount + gCacheMissCount;

    if (totalAccesses == 0) {
        return 0;
    }

    return ((long)gCacheHitCount << 16) / totalAccesses;
}

/*
 * CompactFontCache - Compact cache by removing expired entries
 */
OSErr CompactFontCache(void)
{
    FontCacheEntry *entry, *nextEntry;
    unsigned long currentTime, expireTime;
    OSErr error = noErr;

    if (gFontCache == NULL) {
        return noErr;
    }

    currentTime = GetCurrentTime();
    expireTime = currentTime - (60 * 60 * 1000); /* Expire after 1 hour */

    entry = gFontCache->entries;
    while (entry != NULL) {
        nextEntry = entry->next;

        if (entry->lastUsed < expireTime) {
            /* Remove expired entry */
            RemoveCacheEntry(entry);
        }

        entry = nextEntry;
    }

    return error;
}

/* Internal helper function implementations */

static unsigned short CalculateCacheHash(short familyID, short fontSize, short faceStyle, short device)
{
    unsigned long hash = (familyID << 16) ^ (fontSize << 8) ^ faceStyle ^ device;
    return (unsigned short)(hash % CACHE_HASH_SIZE);
}

static FontCacheEntry *FindCacheEntry(short familyID, short fontSize, short faceStyle, short device)
{
    unsigned short hash;
    FontCacheEntry *entry;

    if (gFontCache == NULL) {
        return NULL;
    }

    /* Calculate hash */
    hash = CalculateCacheHash(familyID, fontSize, faceStyle, device);

    /* Search hash table */
    entry = gCacheHashTable[hash];
    while (entry != NULL) {
        if (entry->familyID == familyID &&
            entry->fontSize == fontSize &&
            entry->faceStyle == faceStyle &&
            entry->device == device) {
            return entry;
        }
        /* In a real hash table, we'd follow a chain here */
        break;
    }

    /* Linear search as fallback */
    entry = gFontCache->entries;
    while (entry != NULL) {
        if (entry->familyID == familyID &&
            entry->fontSize == fontSize &&
            entry->faceStyle == faceStyle &&
            entry->device == device) {
            return entry;
        }
        entry = entry->next;
    }

    return NULL;
}

static OSErr AddCacheEntry(short familyID, short fontSize, short faceStyle, short device, Handle fontData)
{
    FontCacheEntry *entry;
    unsigned short hash;
    long dataSize;

    if (gFontCache == NULL || fontData == NULL) {
        return paramErr;
    }

    /* Check if already at max entries */
    if (gFontCache->currentEntries >= gFontCache->maxEntries) {
        OSErr error = RemoveLRUEntry();
        if (error != noErr) {
            return error;
        }
    }

    /* Allocate new entry */
    entry = (FontCacheEntry *)NewPtr(sizeof(FontCacheEntry));
    if (entry == NULL) {
        return fontOutOfMemoryErr;
    }

    /* Initialize entry */
    entry->familyID = familyID;
    entry->fontSize = fontSize;
    entry->faceStyle = faceStyle;
    entry->device = device;
    entry->fontData = fontData;
    entry->lastUsed = GetCurrentTime();
    entry->next = NULL;

    /* Add to linked list */
    entry->next = gFontCache->entries;
    gFontCache->entries = entry;

    /* Add to hash table */
    hash = CalculateCacheHash(familyID, fontSize, faceStyle, device);
    if (gCacheHashTable[hash] == NULL) {
        gCacheHashTable[hash] = entry;
    }

    /* Update cache statistics */
    gFontCache->currentEntries++;
    dataSize = GetHandleSize(fontData);
    gFontCache->cacheSize += dataSize;

    return noErr;
}

static OSErr RemoveCacheEntry(FontCacheEntry *entry)
{
    FontCacheEntry *current, *prev;
    unsigned short hash;
    long dataSize;

    if (gFontCache == NULL || entry == NULL) {
        return paramErr;
    }

    /* Remove from linked list */
    prev = NULL;
    current = gFontCache->entries;

    while (current != NULL) {
        if (current == entry) {
            if (prev != NULL) {
                prev->next = current->next;
            } else {
                gFontCache->entries = current->next;
            }
            break;
        }
        prev = current;
        current = current->next;
    }

    /* Remove from hash table */
    hash = CalculateCacheHash(entry->familyID, entry->fontSize, entry->faceStyle, entry->device);
    if (gCacheHashTable[hash] == entry) {
        gCacheHashTable[hash] = NULL; /* Simplified */
    }

    /* Update cache statistics */
    if (entry->fontData != NULL) {
        dataSize = GetHandleSize(entry->fontData);
        gFontCache->cacheSize -= dataSize;
        DisposeHandle(entry->fontData);
    }

    gFontCache->currentEntries--;

    /* Dispose entry */
    DisposePtr((Ptr)entry);

    return noErr;
}

static OSErr RemoveLRUEntry(void)
{
    FontCacheEntry *entry, *lruEntry;
    unsigned long oldestTime;

    if (gFontCache == NULL || gFontCache->entries == NULL) {
        return fontNotFoundErr;
    }

    /* Find least recently used entry */
    lruEntry = gFontCache->entries;
    oldestTime = lruEntry->lastUsed;

    entry = gFontCache->entries->next;
    while (entry != NULL) {
        if (entry->lastUsed < oldestTime) {
            oldestTime = entry->lastUsed;
            lruEntry = entry;
        }
        entry = entry->next;
    }

    /* Remove the LRU entry */
    return RemoveCacheEntry(lruEntry);
}

static void UpdateAccessTime(FontCacheEntry *entry)
{
    if (entry != NULL) {
        entry->lastUsed = GetCurrentTime();
    }
}

static unsigned long GetCurrentTime(void)
{
    /* Return time in milliseconds since system startup */
    /* This is a simplified implementation */
    return TickCount() * 1000 / 60; /* Convert ticks to milliseconds */
}

static OSErr ValidateCacheEntry(FontCacheEntry *entry)
{
    if (entry == NULL) {
        return paramErr;
    }

    if (entry->fontData == NULL) {
        return fontCorruptErr;
    }

    /* Check handle validity */
    if (*entry->fontData == NULL) {
        return fontCorruptErr;
    }

    return noErr;
}