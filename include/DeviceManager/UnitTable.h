/*
 * UnitTable.h
 * System 7.1 Device Manager - Unit Table Management
 *
 * Manages the unit table that maps driver reference numbers to
 * Device Control Entries (DCEs).
 */

#ifndef UNIT_TABLE_H
#define UNIT_TABLE_H

#include "DeviceTypes.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Unit Table Constants
 * ============================================================================= */

#define UNIT_TABLE_INITIAL_SIZE     48      /* Initial size of unit table */
#define UNIT_TABLE_GROWTH_INCREMENT 8       /* Growth increment */
#define UNIT_TABLE_MAX_SIZE         256     /* Maximum size */

/* Reference number ranges */
#define MIN_DRIVER_REFNUM           -256    /* Minimum driver reference number */
#define MAX_DRIVER_REFNUM           -1      /* Maximum driver reference number */
#define MIN_FILE_REFNUM             1       /* Minimum file reference number */
#define MAX_FILE_REFNUM             32767   /* Maximum file reference number */

/* Special reference numbers */
#define INVALID_REFNUM              0       /* Invalid reference number */
#define SYSTEM_REFNUM_BASE          -32     /* Base for system drivers */

/* =============================================================================
 * Unit Table Entry Structure
 * ============================================================================= */

typedef struct UnitTableEntry {
    DCEHandle           dceHandle;          /* Handle to DCE */
    int16_t             refNum;             /* Reference number */
    uint32_t            flags;              /* Entry flags */
    uint32_t            lastAccess;         /* Last access time (ticks) */
    uint32_t            openCount;          /* Number of times opened */
    struct UnitTableEntry *next;           /* Next entry in hash chain */
} UnitTableEntry, *UnitTableEntryPtr;

/* Unit table entry flags */
#define kUTEntryInUse               0x0001  /* Entry is in use */
#define kUTEntryReserved            0x0002  /* Entry is reserved */
#define kUTEntrySystem              0x0004  /* System driver entry */
#define kUTEntryLocked              0x0008  /* Entry is locked */
#define kUTEntryTemporary           0x0010  /* Temporary entry */
#define kUTEntryModern              0x0020  /* Modern driver entry */

/* =============================================================================
 * Unit Table Structure
 * ============================================================================= */

typedef struct UnitTable {
    UnitTableEntryPtr  *entries;            /* Array of entry pointers */
    int16_t             size;                /* Current size of table */
    int16_t             count;               /* Number of entries in use */
    int16_t             maxSize;             /* Maximum allowed size */
    int16_t             nextFreeIndex;       /* Next available index */

    /* Hash table for fast lookups */
    UnitTableEntryPtr  *hashTable;          /* Hash table */
    int16_t             hashSize;            /* Size of hash table */

    /* Statistics */
    uint32_t            lookups;             /* Number of lookups performed */
    uint32_t            collisions;          /* Number of hash collisions */
    uint32_t            allocations;         /* Number of allocations */
    uint32_t            deallocations;       /* Number of deallocations */

    /* Synchronization */
    bool                isLocked;            /* Table lock flag */
    uint32_t            lockCount;           /* Lock nesting count */
} UnitTable, *UnitTablePtr;

/* =============================================================================
 * Unit Table Management Functions
 * ============================================================================= */

/**
 * Initialize the unit table
 *
 * @param initialSize Initial size of unit table
 * @return Error code
 */
int16_t UnitTable_Initialize(int16_t initialSize);

/**
 * Shutdown the unit table
 */
void UnitTable_Shutdown(void);

/**
 * Get the unit table instance
 *
 * @return Pointer to unit table
 */
UnitTablePtr UnitTable_GetInstance(void);

/**
 * Allocate a new unit table entry
 *
 * @param preferredRefNum Preferred reference number (or 0 for auto)
 * @return Reference number or error code
 */
int16_t UnitTable_AllocateEntry(int16_t preferredRefNum);

/**
 * Deallocate a unit table entry
 *
 * @param refNum Reference number to deallocate
 * @return Error code
 */
int16_t UnitTable_DeallocateEntry(int16_t refNum);

/**
 * Get unit table entry by reference number
 *
 * @param refNum Reference number
 * @return Unit table entry or NULL if not found
 */
UnitTableEntryPtr UnitTable_GetEntry(int16_t refNum);

/**
 * Set DCE handle for a reference number
 *
 * @param refNum Reference number
 * @param dceHandle DCE handle
 * @return Error code
 */
int16_t UnitTable_SetDCE(int16_t refNum, DCEHandle dceHandle);

/**
 * Get DCE handle for a reference number
 *
 * @param refNum Reference number
 * @return DCE handle or NULL if not found
 */
DCEHandle UnitTable_GetDCE(int16_t refNum);

/* =============================================================================
 * Unit Table Search and Enumeration
 * ============================================================================= */

/**
 * Find entry by driver name
 *
 * @param driverName Driver name to search for
 * @return Reference number or error code
 */
int16_t UnitTable_FindByName(const uint8_t *driverName);

/**
 * Find entry by DCE handle
 *
 * @param dceHandle DCE handle to search for
 * @return Reference number or error code
 */
int16_t UnitTable_FindByDCE(DCEHandle dceHandle);

/**
 * Enumerate all entries in the unit table
 *
 * @param callback Callback function for each entry
 * @param context User context data
 * @return Number of entries enumerated
 */
int16_t UnitTable_Enumerate(bool (*callback)(int16_t refNum, UnitTableEntryPtr entry, void *context),
                           void *context);

/**
 * Get list of all active reference numbers
 *
 * @param refNums Array to store reference numbers
 * @param maxCount Maximum number of entries to return
 * @return Number of reference numbers returned
 */
int16_t UnitTable_GetActiveRefNums(int16_t *refNums, int16_t maxCount);

/* =============================================================================
 * Unit Table Maintenance
 * ============================================================================= */

/**
 * Expand the unit table
 *
 * @param newSize New size for unit table
 * @return Error code
 */
int16_t UnitTable_Expand(int16_t newSize);

/**
 * Compact the unit table (remove unused entries)
 *
 * @return Number of entries removed
 */
int16_t UnitTable_Compact(void);

/**
 * Validate unit table integrity
 *
 * @return True if table is valid
 */
bool UnitTable_Validate(void);

/**
 * Rebuild hash table
 *
 * @return Error code
 */
int16_t UnitTable_RebuildHash(void);

/**
 * Lock the unit table for exclusive access
 */
void UnitTable_Lock(void);

/**
 * Unlock the unit table
 */
void UnitTable_Unlock(void);

/* =============================================================================
 * Unit Table Statistics and Information
 * ============================================================================= */

/**
 * Get unit table statistics
 *
 * @param stats Pointer to statistics structure
 */
void UnitTable_GetStatistics(void *stats);

/**
 * Get unit table size information
 *
 * @param size Current size
 * @param count Current count
 * @param maxSize Maximum size
 */
void UnitTable_GetSizeInfo(int16_t *size, int16_t *count, int16_t *maxSize);

/**
 * Check if reference number is valid
 *
 * @param refNum Reference number to check
 * @return True if valid driver reference number
 */
bool UnitTable_IsValidRefNum(int16_t refNum);

/**
 * Check if reference number is in use
 *
 * @param refNum Reference number to check
 * @return True if in use
 */
bool UnitTable_IsRefNumInUse(int16_t refNum);

/**
 * Get next available reference number
 *
 * @return Next available reference number or error code
 */
int16_t UnitTable_GetNextAvailableRefNum(void);

/* =============================================================================
 * Reference Number Utilities
 * ============================================================================= */

/**
 * Convert reference number to unit table index
 *
 * @param refNum Reference number
 * @return Unit table index
 */
int16_t RefNumToIndex(int16_t refNum);

/**
 * Convert unit table index to reference number
 *
 * @param index Unit table index
 * @return Reference number
 */
int16_t IndexToRefNum(int16_t index);

/**
 * Check if reference number is for a driver
 *
 * @param refNum Reference number
 * @return True if driver reference number
 */
bool IsDriverRefNum(int16_t refNum);

/**
 * Check if reference number is for a file
 *
 * @param refNum Reference number
 * @return True if file reference number
 */
bool IsFileRefNum(int16_t refNum);

/**
 * Check if reference number is a system driver
 *
 * @param refNum Reference number
 * @return True if system driver reference number
 */
bool IsSystemDriverRefNum(int16_t refNum);

/* =============================================================================
 * Hash Table Functions
 * ============================================================================= */

/**
 * Compute hash value for reference number
 *
 * @param refNum Reference number
 * @param tableSize Size of hash table
 * @return Hash value
 */
uint32_t ComputeRefNumHash(int16_t refNum, int16_t tableSize);

/**
 * Compute hash value for driver name
 *
 * @param driverName Driver name
 * @param tableSize Size of hash table
 * @return Hash value
 */
uint32_t ComputeNameHash(const uint8_t *driverName, int16_t tableSize);

/* =============================================================================
 * Debug and Diagnostics
 * ============================================================================= */

/**
 * Dump unit table contents for debugging
 *
 * @param includeEmpty Include empty entries in dump
 */
void UnitTable_Dump(bool includeEmpty);

/**
 * Verify unit table consistency
 *
 * @return Number of inconsistencies found
 */
int16_t UnitTable_VerifyConsistency(void);

/**
 * Get hash table load factor
 *
 * @return Load factor as percentage
 */
uint32_t UnitTable_GetLoadFactor(void);

/**
 * Get average hash chain length
 *
 * @return Average chain length
 */
uint32_t UnitTable_GetAvgChainLength(void);

#ifdef __cplusplus
}
#endif

#endif /* UNIT_TABLE_H */