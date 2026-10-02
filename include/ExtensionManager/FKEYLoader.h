/*
 * FKEYLoader.h - FKEY (Function Key) Resource Loader
 *
 * System 7.1 Portable Function Key loader
 * Loads and manages FKEY resources that extend system functionality
 */

#ifndef FKEY_LOADER_H
#define FKEY_LOADER_H

#include "SystemTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * FUNCTION KEY CONSTANTS
 * ======================================================================== */

#define MAX_FKEYS                   32      /* Maximum function keys */
#define MAX_FKEY_NAME               64      /* Max FKEY name length */

/* FKEY resource type */
#define FKEY_TYPE                   FOURCC('F', 'K', 'E', 'Y') /* Function key resource */

/* ========================================================================
 * FUNCTION KEY ENTRY
 * ======================================================================== */

typedef struct FKEYEntry {
    SInt16          resourceID;             /* Resource ID in System file */
    Handle          resourceHandle;         /* Loaded resource handle */
    SInt32          resourceSize;           /* Size in bytes */

    /* Entry point */
    void*           fkeyProc;               /* Loaded entry point */

    /* Metadata */
    char            name[MAX_FKEY_NAME];   /* FKEY name */
    SInt16          version;                /* FKEY version */

    /* State */
    Boolean         loaded;                 /* Currently loaded */
    Boolean         active;                 /* Currently active */

    /* Linked list for multiple FKEYs */
    struct FKEYEntry *next;
    struct FKEYEntry *prev;
} FKEYEntry;

/* ========================================================================
 * FUNCTION KEY LOADER API
 * ======================================================================== */

/**
 * Initialize FKEY Loader subsystem
 * @return extNoErr on success, error code on failure
 */
OSErr FKEYLoader_Initialize(void);

/**
 * Shutdown FKEY Loader (unload all FKEYs)
 */
void FKEYLoader_Shutdown(void);

/**
 * Load an FKEY resource
 * @param resourceID Resource ID of FKEY to load
 * @return extNoErr on success, error code on failure
 */
OSErr FKEYLoader_LoadFKEY(SInt16 resourceID);

/**
 * Load all FKEY resources from System file
 * @return Number of FKEYs successfully loaded
 */
SInt16 FKEYLoader_LoadAllFKEYs(void);

/**
 * Get FKEY entry by resource ID
 * @param resourceID Resource ID in System file
 * @return Pointer to FKEY entry or NULL if not found
 */
FKEYEntry* FKEYLoader_GetByResourceID(SInt16 resourceID);

/**
 * Unload an FKEY
 * @param resourceID Resource ID of FKEY to unload
 * @return extNoErr on success, error code on failure
 */
OSErr FKEYLoader_UnloadFKEY(SInt16 resourceID);

/**
 * Get count of loaded FKEYs
 * @return Number of loaded function key resources
 */
SInt16 FKEYLoader_GetLoadedCount(void);

/**
 * Dump FKEY registry (debug)
 */
void FKEYLoader_DumpRegistry(void);

/**
 * Check if FKEY Loader is initialized
 * @return true if initialized, false otherwise
 */
Boolean FKEYLoader_IsInitialized(void);

#ifdef __cplusplus
}
#endif

#endif /* FKEY_LOADER_H */
