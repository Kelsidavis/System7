/*
 * DefLoader.h - Definition Resource Loader (WDEF, LDEF, MDEF)
 *
 * System 7.1 Portable Definition Resource Loader
 * Loads and registers window, list, and menu definition resources
 */

#ifndef DEF_LOADER_H
#define DEF_LOADER_H

#include "SystemTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * DEFINITION RESOURCE TYPES
 * ======================================================================== */

#define WDEF_TYPE                   FOURCC('W', 'D', 'E', 'F') /* Window definition */
#define LDEF_TYPE                   FOURCC('L', 'D', 'E', 'F') /* List definition */
#define MDEF_TYPE                   FOURCC('M', 'D', 'E', 'F') /* Menu definition */

#define MAX_DEFS                    32      /* Maximum definitions */
#define MAX_DEF_NAME                64      /* Max definition name */

/* ========================================================================
 * DEFINITION ENTRY
 * ======================================================================== */

typedef struct DefEntry {
    ResType         defType;                /* Definition type (WDEF/LDEF/MDEF) */
    SInt16          resourceID;             /* Resource ID in System file */
    SInt16          defID;                  /* Definition ID (for WDEF: procID) */
    Handle          resourceHandle;         /* Loaded resource handle */
    SInt32          resourceSize;           /* Size in bytes */

    /* Entry point */
    void*           defProc;                /* Loaded entry point */

    /* Metadata */
    char            name[MAX_DEF_NAME];    /* Definition name */
    SInt16          version;                /* Definition version */

    /* State */
    Boolean         loaded;                 /* Currently loaded */
    Boolean         registered;             /* Registered with manager */

    /* Linked list for multiple definitions */
    struct DefEntry *next;
    struct DefEntry *prev;
} DefEntry;

/* ========================================================================
 * DEFINITION LOADER API
 * ======================================================================== */

/**
 * Initialize Definition Loader subsystem
 * @return extNoErr on success, error code on failure
 */
OSErr DefLoader_Initialize(void);

/**
 * Shutdown Definition Loader (unload all definitions)
 */
void DefLoader_Shutdown(void);

/**
 * Load a definition resource (WDEF, LDEF, or MDEF)
 * @param defType Definition type (WDEF_TYPE, LDEF_TYPE, or MDEF_TYPE)
 * @param resourceID Resource ID to load
 * @return extNoErr on success, error code on failure
 */
OSErr DefLoader_LoadDefinition(ResType defType, SInt16 resourceID);

/**
 * Load all definition resources of a specific type
 * @param defType Definition type (WDEF_TYPE, LDEF_TYPE, or MDEF_TYPE)
 * @return Number of definitions successfully loaded
 */
SInt16 DefLoader_LoadAllDefinitions(ResType defType);

/**
 * Get definition entry by type and resource ID
 * @param defType Definition type
 * @param resourceID Resource ID
 * @return Pointer to definition entry or NULL if not found
 */
DefEntry* DefLoader_GetByResourceID(ResType defType, SInt16 resourceID);

/**
 * Unload a definition
 * @param defType Definition type
 * @param resourceID Resource ID to unload
 * @return extNoErr on success, error code on failure
 */
OSErr DefLoader_UnloadDefinition(ResType defType, SInt16 resourceID);

/**
 * Get count of loaded definitions by type
 * @param defType Definition type
 * @return Number of loaded definitions of that type
 */
SInt16 DefLoader_GetLoadedCount(ResType defType);

/**
 * Get total count of loaded definitions
 * @return Total number of loaded definitions
 */
SInt16 DefLoader_GetTotalCount(void);

/**
 * Dump definition registry (debug)
 */
void DefLoader_DumpRegistry(void);

/**
 * Check if Definition Loader is initialized
 * @return true if initialized, false otherwise
 */
Boolean DefLoader_IsInitialized(void);

#ifdef __cplusplus
}
#endif

#endif /* DEF_LOADER_H */
