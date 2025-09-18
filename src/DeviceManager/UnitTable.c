/*
 * UnitTable.c
 * System 7.1 Device Manager - Unit Table Management Implementation
 *
 * Implements the unit table that maps driver reference numbers to
 * Device Control Entries (DCEs). This is the core data structure
 * for device driver management in System 7.1.
 *
 * Based on the original System 7.1 DeviceMgr.a assembly source.
 */

#include "DeviceManager/UnitTable.h"
#include "DeviceManager/DeviceTypes.h"
#include "MemoryManager.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* =============================================================================
 * Global Variables
 * ============================================================================= */

static UnitTable *g_unitTable = NULL;
static bool g_unitTableInitialized = false;

/* =============================================================================
 * Internal Function Declarations
 * ============================================================================= */

static int16_t ExpandTable(int16_t newSize);
static int16_t RebuildHashTable(void);
static UnitTableEntryPtr FindEntryByRefNum(int16_t refNum);
static UnitTableEntryPtr FindEntryByName(const uint8_t *driverName);
static uint32_t HashRefNum(int16_t refNum);
static uint32_t HashDriverName(const uint8_t *driverName);
static int16_t AllocateEntry(int16_t preferredRefNum);
static void DeallocateEntry(UnitTableEntryPtr entry);
static bool ValidateEntry(UnitTableEntryPtr entry);
static void UpdateAccessTime(UnitTableEntryPtr entry);

/* =============================================================================
 * Unit Table Initialization and Shutdown
 * ============================================================================= */

int16_t UnitTable_Initialize(int16_t initialSize)
{
    if (g_unitTableInitialized) {
        return noErr; /* Already initialized */
    }

    if (initialSize <= 0 || initialSize > UNIT_TABLE_MAX_SIZE) {
        initialSize = UNIT_TABLE_INITIAL_SIZE;
    }

    /* Allocate unit table structure */
    g_unitTable = (UnitTable*)malloc(sizeof(UnitTable));
    if (g_unitTable == NULL) {
        return memFullErr;
    }

    memset(g_unitTable, 0, sizeof(UnitTable));

    /* Allocate entry array */
    g_unitTable->entries = (UnitTableEntryPtr*)malloc(sizeof(UnitTableEntryPtr) * initialSize);
    if (g_unitTable->entries == NULL) {
        free(g_unitTable);
        g_unitTable = NULL;
        return memFullErr;
    }

    memset(g_unitTable->entries, 0, sizeof(UnitTableEntryPtr) * initialSize);

    /* Initialize hash table */
    g_unitTable->hashSize = initialSize * 2; /* Hash table twice the size for better distribution */
    g_unitTable->hashTable = (UnitTableEntryPtr*)malloc(sizeof(UnitTableEntryPtr) * g_unitTable->hashSize);
    if (g_unitTable->hashTable == NULL) {
        free(g_unitTable->entries);
        free(g_unitTable);
        g_unitTable = NULL;
        return memFullErr;
    }

    memset(g_unitTable->hashTable, 0, sizeof(UnitTableEntryPtr) * g_unitTable->hashSize);

    /* Initialize table fields */
    g_unitTable->size = initialSize;
    g_unitTable->count = 0;
    g_unitTable->maxSize = UNIT_TABLE_MAX_SIZE;
    g_unitTable->nextFreeIndex = 0;
    g_unitTable->isLocked = false;
    g_unitTable->lockCount = 0;

    /* Reset statistics */
    g_unitTable->lookups = 0;
    g_unitTable->collisions = 0;
    g_unitTable->allocations = 0;
    g_unitTable->deallocations = 0;

    g_unitTableInitialized = true;
    return noErr;
}

void UnitTable_Shutdown(void)
{
    if (!g_unitTableInitialized || g_unitTable == NULL) {
        return;
    }

    UnitTable_Lock();

    /* Deallocate all entries */
    for (int16_t i = 0; i < g_unitTable->size; i++) {
        if (g_unitTable->entries[i] != NULL) {
            DeallocateEntry(g_unitTable->entries[i]);
            g_unitTable->entries[i] = NULL;
        }
    }

    /* Free table structures */
    if (g_unitTable->entries != NULL) {
        free(g_unitTable->entries);
        g_unitTable->entries = NULL;
    }

    if (g_unitTable->hashTable != NULL) {
        free(g_unitTable->hashTable);
        g_unitTable->hashTable = NULL;
    }

    free(g_unitTable);
    g_unitTable = NULL;

    g_unitTableInitialized = false;
}

UnitTablePtr UnitTable_GetInstance(void)
{
    return g_unitTable;
}

/* =============================================================================
 * Entry Allocation and Deallocation
 * ============================================================================= */

int16_t UnitTable_AllocateEntry(int16_t preferredRefNum)
{
    if (!g_unitTableInitialized || g_unitTable == NULL) {
        return dsIOCoreErr;
    }

    UnitTable_Lock();

    int16_t refNum = AllocateEntry(preferredRefNum);

    UnitTable_Unlock();

    return refNum;
}

int16_t UnitTable_DeallocateEntry(int16_t refNum)
{
    if (!g_unitTableInitialized || g_unitTable == NULL) {
        return dsIOCoreErr;
    }

    if (!UnitTable_IsValidRefNum(refNum)) {
        return badUnitErr;
    }

    UnitTable_Lock();

    UnitTableEntryPtr entry = FindEntryByRefNum(refNum);
    if (entry == NULL) {
        UnitTable_Unlock();
        return badUnitErr;
    }

    /* Remove from hash table */
    uint32_t hashIndex = HashRefNum(refNum) % g_unitTable->hashSize;
    UnitTableEntryPtr *hashChain = &g_unitTable->hashTable[hashIndex];

    while (*hashChain != NULL) {
        if (*hashChain == entry) {
            *hashChain = entry->next;
            break;
        }
        hashChain = &(*hashChain)->next;
    }

    /* Remove from main table */
    int16_t tableIndex = RefNumToIndex(refNum);
    if (tableIndex >= 0 && tableIndex < g_unitTable->size) {
        g_unitTable->entries[tableIndex] = NULL;
    }

    DeallocateEntry(entry);
    g_unitTable->count--;
    g_unitTable->deallocations++;

    UnitTable_Unlock();

    return noErr;
}

static int16_t AllocateEntry(int16_t preferredRefNum)
{
    int16_t refNum;

    if (preferredRefNum != 0 && UnitTable_IsValidRefNum(preferredRefNum)) {
        /* Check if preferred ref num is available */
        if (!UnitTable_IsRefNumInUse(preferredRefNum)) {
            refNum = preferredRefNum;
        } else {
            return unitEmptyErr; /* Already in use */
        }
    } else {
        /* Find next available reference number */
        refNum = UnitTable_GetNextAvailableRefNum();
        if (refNum < 0) {
            return refNum; /* Error code */
        }
    }

    /* Check if we need to expand the table */
    int16_t tableIndex = RefNumToIndex(refNum);
    if (tableIndex >= g_unitTable->size) {
        int16_t error = ExpandTable(tableIndex + UNIT_TABLE_GROWTH_INCREMENT);
        if (error != noErr) {
            return error;
        }
    }

    /* Allocate entry structure */
    UnitTableEntryPtr entry = (UnitTableEntryPtr)malloc(sizeof(UnitTableEntry));
    if (entry == NULL) {
        return memFullErr;
    }

    memset(entry, 0, sizeof(UnitTableEntry));

    /* Initialize entry */
    entry->refNum = refNum;
    entry->dceHandle = NULL;
    entry->flags = kUTEntryInUse;
    entry->lastAccess = 0; /* GetCurrentTicks() in real implementation */
    entry->openCount = 0;
    entry->next = NULL;

    /* Add to main table */
    g_unitTable->entries[tableIndex] = entry;

    /* Add to hash table */
    uint32_t hashIndex = HashRefNum(refNum) % g_unitTable->hashSize;
    entry->next = g_unitTable->hashTable[hashIndex];
    g_unitTable->hashTable[hashIndex] = entry;

    g_unitTable->count++;
    g_unitTable->allocations++;

    return refNum;
}

static void DeallocateEntry(UnitTableEntryPtr entry)
{
    if (entry != NULL) {
        /* Note: DCE handle should be disposed by caller */
        free(entry);
    }
}

/* =============================================================================
 * Entry Access Functions
 * ============================================================================= */

UnitTableEntryPtr UnitTable_GetEntry(int16_t refNum)
{
    if (!g_unitTableInitialized || g_unitTable == NULL) {
        return NULL;
    }

    if (!UnitTable_IsValidRefNum(refNum)) {
        return NULL;
    }

    UnitTable_Lock();

    UnitTableEntryPtr entry = FindEntryByRefNum(refNum);
    if (entry != NULL) {
        UpdateAccessTime(entry);
        g_unitTable->lookups++;
    }

    UnitTable_Unlock();

    return entry;
}

int16_t UnitTable_SetDCE(int16_t refNum, DCEHandle dceHandle)
{
    if (!g_unitTableInitialized || g_unitTable == NULL) {
        return dsIOCoreErr;
    }

    UnitTable_Lock();

    UnitTableEntryPtr entry = FindEntryByRefNum(refNum);
    if (entry == NULL) {
        UnitTable_Unlock();
        return badUnitErr;
    }

    entry->dceHandle = dceHandle;
    UpdateAccessTime(entry);

    UnitTable_Unlock();

    return noErr;
}

DCEHandle UnitTable_GetDCE(int16_t refNum)
{
    if (!g_unitTableInitialized || g_unitTable == NULL) {
        return NULL;
    }

    UnitTable_Lock();

    UnitTableEntryPtr entry = FindEntryByRefNum(refNum);
    DCEHandle dceHandle = NULL;

    if (entry != NULL) {
        dceHandle = entry->dceHandle;
        UpdateAccessTime(entry);
        g_unitTable->lookups++;
    }

    UnitTable_Unlock();

    return dceHandle;
}

/* =============================================================================
 * Search Functions
 * ============================================================================= */

int16_t UnitTable_FindByName(const uint8_t *driverName)
{
    if (!g_unitTableInitialized || g_unitTable == NULL || driverName == NULL) {
        return badUnitErr;
    }

    UnitTable_Lock();

    UnitTableEntryPtr entry = FindEntryByName(driverName);
    int16_t refNum = badUnitErr;

    if (entry != NULL) {
        refNum = entry->refNum;
        UpdateAccessTime(entry);
        g_unitTable->lookups++;
    }

    UnitTable_Unlock();

    return refNum;
}

int16_t UnitTable_FindByDCE(DCEHandle dceHandle)
{
    if (!g_unitTableInitialized || g_unitTable == NULL || dceHandle == NULL) {
        return badUnitErr;
    }

    UnitTable_Lock();

    int16_t refNum = badUnitErr;

    /* Linear search through all entries */
    for (int16_t i = 0; i < g_unitTable->size; i++) {
        UnitTableEntryPtr entry = g_unitTable->entries[i];
        if (entry != NULL && entry->dceHandle == dceHandle) {
            refNum = entry->refNum;
            UpdateAccessTime(entry);
            g_unitTable->lookups++;
            break;
        }
    }

    UnitTable_Unlock();

    return refNum;
}

int16_t UnitTable_Enumerate(bool (*callback)(int16_t refNum, UnitTableEntryPtr entry, void *context),
                           void *context)
{
    if (!g_unitTableInitialized || g_unitTable == NULL || callback == NULL) {
        return 0;
    }

    UnitTable_Lock();

    int16_t count = 0;

    for (int16_t i = 0; i < g_unitTable->size; i++) {
        UnitTableEntryPtr entry = g_unitTable->entries[i];
        if (entry != NULL && (entry->flags & kUTEntryInUse)) {
            bool shouldContinue = callback(entry->refNum, entry, context);
            count++;
            if (!shouldContinue) {
                break;
            }
        }
    }

    UnitTable_Unlock();

    return count;
}

int16_t UnitTable_GetActiveRefNums(int16_t *refNums, int16_t maxCount)
{
    if (!g_unitTableInitialized || g_unitTable == NULL || refNums == NULL || maxCount <= 0) {
        return 0;
    }

    UnitTable_Lock();

    int16_t count = 0;

    for (int16_t i = 0; i < g_unitTable->size && count < maxCount; i++) {
        UnitTableEntryPtr entry = g_unitTable->entries[i];
        if (entry != NULL && (entry->flags & kUTEntryInUse)) {
            refNums[count++] = entry->refNum;
        }
    }

    UnitTable_Unlock();

    return count;
}

/* =============================================================================
 * Table Maintenance
 * ============================================================================= */

int16_t UnitTable_Expand(int16_t newSize)
{
    if (!g_unitTableInitialized || g_unitTable == NULL) {
        return dsIOCoreErr;
    }

    if (newSize <= g_unitTable->size || newSize > g_unitTable->maxSize) {
        return paramErr;
    }

    return ExpandTable(newSize);
}

static int16_t ExpandTable(int16_t newSize)
{
    /* Reallocate entry array */
    UnitTableEntryPtr *newEntries = (UnitTableEntryPtr*)realloc(g_unitTable->entries,
                                                                sizeof(UnitTableEntryPtr) * newSize);
    if (newEntries == NULL) {
        return memFullErr;
    }

    /* Clear new entries */
    for (int16_t i = g_unitTable->size; i < newSize; i++) {
        newEntries[i] = NULL;
    }

    g_unitTable->entries = newEntries;
    g_unitTable->size = newSize;

    /* Rebuild hash table if needed */
    if (g_unitTable->count > g_unitTable->hashSize / 2) {
        int16_t error = RebuildHashTable();
        if (error != noErr) {
            return error;
        }
    }

    return noErr;
}

int16_t UnitTable_Compact(void)
{
    if (!g_unitTableInitialized || g_unitTable == NULL) {
        return 0;
    }

    UnitTable_Lock();

    int16_t removed = 0;

    /* Remove unused entries */
    for (int16_t i = 0; i < g_unitTable->size; i++) {
        UnitTableEntryPtr entry = g_unitTable->entries[i];
        if (entry != NULL && !(entry->flags & kUTEntryInUse)) {
            g_unitTable->entries[i] = NULL;
            DeallocateEntry(entry);
            removed++;
        }
    }

    /* Rebuild hash table to remove dead references */
    RebuildHashTable();

    UnitTable_Unlock();

    return removed;
}

bool UnitTable_Validate(void)
{
    if (!g_unitTableInitialized || g_unitTable == NULL) {
        return false;
    }

    UnitTable_Lock();

    bool isValid = true;

    /* Check table structure */
    if (g_unitTable->size <= 0 || g_unitTable->count < 0 ||
        g_unitTable->count > g_unitTable->size) {
        isValid = false;
    }

    /* Validate entries */
    if (isValid) {
        for (int16_t i = 0; i < g_unitTable->size; i++) {
            UnitTableEntryPtr entry = g_unitTable->entries[i];
            if (entry != NULL && !ValidateEntry(entry)) {
                isValid = false;
                break;
            }
        }
    }

    UnitTable_Unlock();

    return isValid;
}

static int16_t RebuildHashTable(void)
{
    /* Clear hash table */
    memset(g_unitTable->hashTable, 0, sizeof(UnitTableEntryPtr) * g_unitTable->hashSize);

    /* Rebuild hash chains */
    for (int16_t i = 0; i < g_unitTable->size; i++) {
        UnitTableEntryPtr entry = g_unitTable->entries[i];
        if (entry != NULL && (entry->flags & kUTEntryInUse)) {
            uint32_t hashIndex = HashRefNum(entry->refNum) % g_unitTable->hashSize;
            entry->next = g_unitTable->hashTable[hashIndex];
            g_unitTable->hashTable[hashIndex] = entry;
        }
    }

    return noErr;
}

int16_t UnitTable_RebuildHash(void)
{
    if (!g_unitTableInitialized || g_unitTable == NULL) {
        return dsIOCoreErr;
    }

    UnitTable_Lock();
    int16_t result = RebuildHashTable();
    UnitTable_Unlock();

    return result;
}

/* =============================================================================
 * Locking Functions
 * ============================================================================= */

void UnitTable_Lock(void)
{
    if (g_unitTable != NULL) {
        g_unitTable->isLocked = true;
        g_unitTable->lockCount++;
    }
}

void UnitTable_Unlock(void)
{
    if (g_unitTable != NULL && g_unitTable->lockCount > 0) {
        g_unitTable->lockCount--;
        if (g_unitTable->lockCount == 0) {
            g_unitTable->isLocked = false;
        }
    }
}

/* =============================================================================
 * Information and Statistics
 * ============================================================================= */

void UnitTable_GetStatistics(void *stats)
{
    if (!g_unitTableInitialized || g_unitTable == NULL || stats == NULL) {
        return;
    }

    UnitTable_Lock();
    memcpy(stats, &g_unitTable->lookups, sizeof(uint32_t) * 4); /* Copy statistics */
    UnitTable_Unlock();
}

void UnitTable_GetSizeInfo(int16_t *size, int16_t *count, int16_t *maxSize)
{
    if (!g_unitTableInitialized || g_unitTable == NULL) {
        if (size) *size = 0;
        if (count) *count = 0;
        if (maxSize) *maxSize = 0;
        return;
    }

    UnitTable_Lock();

    if (size) *size = g_unitTable->size;
    if (count) *count = g_unitTable->count;
    if (maxSize) *maxSize = g_unitTable->maxSize;

    UnitTable_Unlock();
}

bool UnitTable_IsValidRefNum(int16_t refNum)
{
    return (refNum >= MIN_DRIVER_REFNUM && refNum <= MAX_DRIVER_REFNUM);
}

bool UnitTable_IsRefNumInUse(int16_t refNum)
{
    if (!g_unitTableInitialized || g_unitTable == NULL) {
        return false;
    }

    UnitTable_Lock();

    UnitTableEntryPtr entry = FindEntryByRefNum(refNum);
    bool inUse = (entry != NULL && (entry->flags & kUTEntryInUse));

    UnitTable_Unlock();

    return inUse;
}

int16_t UnitTable_GetNextAvailableRefNum(void)
{
    if (!g_unitTableInitialized || g_unitTable == NULL) {
        return dsIOCoreErr;
    }

    UnitTable_Lock();

    /* Start from next free index and search for available refnum */
    for (int16_t i = 1; i <= MAX_UNIT_TABLE_SIZE; i++) {
        int16_t refNum = -i; /* Driver refnums are negative */
        if (!UnitTable_IsRefNumInUse(refNum)) {
            UnitTable_Unlock();
            return refNum;
        }
    }

    UnitTable_Unlock();

    return unitEmptyErr; /* No available reference numbers */
}

/* =============================================================================
 * Internal Helper Functions
 * ============================================================================= */

static UnitTableEntryPtr FindEntryByRefNum(int16_t refNum)
{
    uint32_t hashIndex = HashRefNum(refNum) % g_unitTable->hashSize;
    UnitTableEntryPtr entry = g_unitTable->hashTable[hashIndex];

    while (entry != NULL) {
        if (entry->refNum == refNum) {
            return entry;
        }
        entry = entry->next;
        g_unitTable->collisions++;
    }

    return NULL;
}

static UnitTableEntryPtr FindEntryByName(const uint8_t *driverName)
{
    /* Linear search through all entries */
    for (int16_t i = 0; i < g_unitTable->size; i++) {
        UnitTableEntryPtr entry = g_unitTable->entries[i];
        if (entry != NULL && entry->dceHandle != NULL) {
            DCEPtr dce = *(entry->dceHandle);
            if (dce != NULL && dce->dCtlDriver != NULL) {
                /* Compare driver names based on driver type */
                if (dce->dCtlFlags & FollowsNewRules_Mask) {
                    /* Modern driver - compare with interface name */
                    /* This would need implementation for modern drivers */
                } else {
                    /* Classic driver - compare Pascal strings */
                    DriverHeaderPtr drvrPtr = (DriverHeaderPtr)dce->dCtlDriver;
                    if (drvrPtr != NULL) {
                        uint8_t nameLen = driverName[0];
                        if (nameLen == drvrPtr->drvrName[0] &&
                            memcmp(&driverName[1], &drvrPtr->drvrName[1], nameLen) == 0) {
                            return entry;
                        }
                    }
                }
            }
        }
    }

    return NULL;
}

static uint32_t HashRefNum(int16_t refNum)
{
    /* Simple hash function for reference numbers */
    return (uint32_t)(refNum * 31 + 17);
}

static uint32_t HashDriverName(const uint8_t *driverName)
{
    if (driverName == NULL) {
        return 0;
    }

    uint32_t hash = 0;
    uint8_t nameLen = driverName[0];

    for (int i = 1; i <= nameLen; i++) {
        hash = hash * 31 + driverName[i];
    }

    return hash;
}

static bool ValidateEntry(UnitTableEntryPtr entry)
{
    if (entry == NULL) {
        return false;
    }

    /* Check basic fields */
    if (!UnitTable_IsValidRefNum(entry->refNum)) {
        return false;
    }

    if (!(entry->flags & kUTEntryInUse)) {
        return false;
    }

    return true;
}

static void UpdateAccessTime(UnitTableEntryPtr entry)
{
    if (entry != NULL) {
        entry->lastAccess = 0; /* GetCurrentTicks() in real implementation */
    }
}

/* =============================================================================
 * Reference Number Utilities
 * ============================================================================= */

int16_t RefNumToIndex(int16_t refNum)
{
    if (refNum < 0) {
        return (-refNum) - 1;
    }
    return -1; /* Invalid for positive numbers (file refnums) */
}

int16_t IndexToRefNum(int16_t index)
{
    if (index >= 0) {
        return -(index + 1);
    }
    return 0; /* Invalid */
}

bool IsDriverRefNum(int16_t refNum)
{
    return UnitTable_IsValidRefNum(refNum);
}

bool IsFileRefNum(int16_t refNum)
{
    return (refNum >= MIN_FILE_REFNUM && refNum <= MAX_FILE_REFNUM);
}

bool IsSystemDriverRefNum(int16_t refNum)
{
    return (refNum >= SYSTEM_REFNUM_BASE && refNum <= MAX_DRIVER_REFNUM);
}

/* =============================================================================
 * Hash Utilities
 * ============================================================================= */

uint32_t ComputeRefNumHash(int16_t refNum, int16_t tableSize)
{
    return HashRefNum(refNum) % tableSize;
}

uint32_t ComputeNameHash(const uint8_t *driverName, int16_t tableSize)
{
    return HashDriverName(driverName) % tableSize;
}

/* =============================================================================
 * Debug Functions
 * ============================================================================= */

void UnitTable_Dump(bool includeEmpty)
{
    if (!g_unitTableInitialized || g_unitTable == NULL) {
        printf("Unit table not initialized\n");
        return;
    }

    printf("Unit Table Dump:\n");
    printf("  Size: %d, Count: %d, Max: %d\n",
           g_unitTable->size, g_unitTable->count, g_unitTable->maxSize);
    printf("  Hash Size: %d\n", g_unitTable->hashSize);
    printf("  Statistics: Lookups=%u, Collisions=%u, Allocs=%u, Deallocs=%u\n",
           g_unitTable->lookups, g_unitTable->collisions,
           g_unitTable->allocations, g_unitTable->deallocations);

    for (int16_t i = 0; i < g_unitTable->size; i++) {
        UnitTableEntryPtr entry = g_unitTable->entries[i];
        if (entry != NULL || includeEmpty) {
            printf("  [%d]: ", i);
            if (entry != NULL) {
                printf("RefNum=%d, Flags=0x%X, DCE=%p\n",
                       entry->refNum, entry->flags, entry->dceHandle);
            } else {
                printf("(empty)\n");
            }
        }
    }
}

int16_t UnitTable_VerifyConsistency(void)
{
    if (!g_unitTableInitialized || g_unitTable == NULL) {
        return 1;
    }

    int16_t inconsistencies = 0;

    UnitTable_Lock();

    /* Check entry count */
    int16_t actualCount = 0;
    for (int16_t i = 0; i < g_unitTable->size; i++) {
        if (g_unitTable->entries[i] != NULL) {
            actualCount++;
        }
    }

    if (actualCount != g_unitTable->count) {
        inconsistencies++;
    }

    /* Check hash table consistency */
    for (int16_t i = 0; i < g_unitTable->hashSize; i++) {
        UnitTableEntryPtr entry = g_unitTable->hashTable[i];
        while (entry != NULL) {
            /* Check if entry exists in main table */
            int16_t tableIndex = RefNumToIndex(entry->refNum);
            if (tableIndex < 0 || tableIndex >= g_unitTable->size ||
                g_unitTable->entries[tableIndex] != entry) {
                inconsistencies++;
            }
            entry = entry->next;
        }
    }

    UnitTable_Unlock();

    return inconsistencies;
}

uint32_t UnitTable_GetLoadFactor(void)
{
    if (!g_unitTableInitialized || g_unitTable == NULL || g_unitTable->hashSize == 0) {
        return 0;
    }

    return (g_unitTable->count * 100) / g_unitTable->hashSize;
}

uint32_t UnitTable_GetAvgChainLength(void)
{
    if (!g_unitTableInitialized || g_unitTable == NULL || g_unitTable->count == 0) {
        return 0;
    }

    uint32_t totalChainLength = 0;
    uint32_t usedSlots = 0;

    for (int16_t i = 0; i < g_unitTable->hashSize; i++) {
        UnitTableEntryPtr entry = g_unitTable->hashTable[i];
        if (entry != NULL) {
            usedSlots++;
            uint32_t chainLength = 0;
            while (entry != NULL) {
                chainLength++;
                entry = entry->next;
            }
            totalChainLength += chainLength;
        }
    }

    return usedSlots > 0 ? totalChainLength / usedSlots : 0;
}