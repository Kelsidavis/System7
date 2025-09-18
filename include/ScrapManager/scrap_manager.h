/*
 * Macintosh Scrap Manager
 * Based on System 6.0.7 implementation
 *
 * This is a reimplementation based on Inside Macintosh documentation
 * and analysis of System 6.0.7 resources.
 */

#ifndef SCRAP_MANAGER_H
#define SCRAP_MANAGER_H

#include <stdint.h>

/* Standard Mac types */
typedef int32_t OSErr;
typedef uint32_t OSType;
typedef void** Handle;
typedef char* Ptr;

/* Scrap Manager error codes */
#define noErr           0
#define noScrapErr      -100
#define noTypeErr       -102

/* Scrap state flags */
#define scrapNotLoaded  0
#define scrapLoaded     1
#define scrapDirty      2

/* Low memory globals offsets */
#define ScrapSize       0x960
#define ScrapHandle     0x964
#define ScrapCount      0x968
#define ScrapState      0x96A
#define ScrapName       0x96C

/* Scrap information structure */
typedef struct ScrapStuff {
    int32_t     scrapSize;
    Handle      scrapHandle;
    int16_t     scrapCount;
    int16_t     scrapState;
    char*       scrapName;
} ScrapStuff;

/* Function prototypes matching trap handlers */

/* A9FC - ZeroScrap: Clear the scrap */
OSErr ZeroScrap(void);

/* A9F9 - InfoScrap: Get scrap information */
ScrapStuff* InfoScrap(void);

/* A9FE - PutScrap: Put data into scrap */
OSErr PutScrap(int32_t length, OSType theType, Ptr source);

/* A9FD - GetScrap: Get data from scrap */
int32_t GetScrap(Handle hDest, OSType theType, int32_t *offset);

/* A9FB - LoadScrap: Load scrap from disk */
OSErr LoadScrap(void);

/* A9FA - UnloadScrap: Unload scrap to disk */
OSErr UnloadScrap(void);

#endif /* SCRAP_MANAGER_H */
