/*
 * DriverLoader.c
 * System 7.1 Device Manager - Driver Loading and Resource Management
 *
 * Implements driver resource loading, validation, and installation
 * from both classic Mac OS 'DRVR' resources and modern driver formats.
 *
 * Based on the original System 7.1 DeviceMgr.a assembly source.
 */

#include "DeviceManager/DeviceManager.h"
#include "DeviceManager/DriverInterface.h"
#include "DeviceManager/UnitTable.h"
#include "ResourceManager.h"
#include "MemoryManager.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* =============================================================================
 * Constants
 * ============================================================================= */

#define DRVR_RESOURCE_TYPE      'DRVR'
#define DRIVER_SIGNATURE        0x4D41  /* 'MA' - Macintosh signature */
#define MAX_DRIVER_NAME_LENGTH  255
#define MAX_SEARCH_ATTEMPTS     16

/* =============================================================================
 * Global Variables
 * ============================================================================= */

static struct {
    uint32_t driversLoaded;
    uint32_t loadAttempts;
    uint32_t loadFailures;
    uint32_t validationFailures;
    uint32_t resourceNotFound;
    uint32_t memoryErrors;
} g_loaderStats = {0};

/* =============================================================================
 * Internal Function Declarations
 * ============================================================================= */

static int16_t LoadDriverFromResource(const uint8_t *driverName, int16_t resID, Handle *driverHandle);
static int16_t LoadDriverFromSystemFile(const uint8_t *driverName, Handle *driverHandle);
static int16_t LoadDriverFromSlotROM(const uint8_t *driverName, Handle *driverHandle);
static int16_t ValidateDriverResource(Handle driverHandle);
static int16_t ParseDriverName(const uint8_t *name, char *parsedName, int16_t maxLen);
static bool CompareDriverNames(const uint8_t *name1, const uint8_t *name2);
static int16_t GetDriverResourceID(const uint8_t *driverName);
static Handle SearchDriverResources(const uint8_t *driverName);
static int16_t CreateDriverFromTemplate(const char *templateName, Handle *driverHandle);

/* =============================================================================
 * Driver Loading Functions
 * ============================================================================= */

Handle LoadDriverResource(const uint8_t *driverName, int16_t resID)
{
    Handle driverHandle = NULL;
    int16_t error;

    if (driverName == NULL) {
        g_loaderStats.loadFailures++;
        return NULL;
    }

    g_loaderStats.loadAttempts++;

    /* First try to load by resource ID if specified */
    if (resID != 0) {
        error = LoadDriverFromResource(driverName, resID, &driverHandle);
        if (error == noErr && driverHandle != NULL) {
            g_loaderStats.driversLoaded++;
            return driverHandle;
        }
    }

    /* Search by name in system file */
    error = LoadDriverFromSystemFile(driverName, &driverHandle);
    if (error == noErr && driverHandle != NULL) {
        g_loaderStats.driversLoaded++;
        return driverHandle;
    }

    /* Search in slot ROM */
    error = LoadDriverFromSlotROM(driverName, &driverHandle);
    if (error == noErr && driverHandle != NULL) {
        g_loaderStats.driversLoaded++;
        return driverHandle;
    }

    /* Try resource search */
    driverHandle = SearchDriverResources(driverName);
    if (driverHandle != NULL) {
        g_loaderStats.driversLoaded++;
        return driverHandle;
    }

    /* Try creating from template */
    char parsedName[256];
    if (ParseDriverName(driverName, parsedName, sizeof(parsedName)) == noErr) {
        error = CreateDriverFromTemplate(parsedName, &driverHandle);
        if (error == noErr && driverHandle != NULL) {
            g_loaderStats.driversLoaded++;
            return driverHandle;
        }
    }

    g_loaderStats.resourceNotFound++;
    g_loaderStats.loadFailures++;
    return NULL;
}

int16_t FindDriverByName(const uint8_t *driverName)
{
    if (driverName == NULL) {
        return paramErr;
    }

    return UnitTable_FindByName(driverName);
}

int16_t AllocateUnitTableEntry(void)
{
    return UnitTable_GetNextAvailableRefNum();
}

int16_t DeallocateUnitTableEntry(int16_t refNum)
{
    return UnitTable_DeallocateEntry(refNum);
}

/* =============================================================================
 * Resource Loading Implementation
 * ============================================================================= */

static int16_t LoadDriverFromResource(const uint8_t *driverName, int16_t resID, Handle *driverHandle)
{
    if (driverHandle == NULL) {
        return paramErr;
    }

    *driverHandle = NULL;

    /* Try to get resource by ID */
    Handle resHandle = GetResource(DRVR_RESOURCE_TYPE, resID);
    if (resHandle == NULL) {
        return resNotFound;
    }

    /* Validate the resource */
    int16_t error = ValidateDriverResource(resHandle);
    if (error != noErr) {
        g_loaderStats.validationFailures++;
        ReleaseResource(resHandle);
        return error;
    }

    /* Detach resource so it won't be purged */
    DetachResource(resHandle);

    *driverHandle = resHandle;
    return noErr;
}

static int16_t LoadDriverFromSystemFile(const uint8_t *driverName, Handle *driverHandle)
{
    if (driverName == NULL || driverHandle == NULL) {
        return paramErr;
    }

    *driverHandle = NULL;

    /* Get resource ID for driver name */
    int16_t resID = GetDriverResourceID(driverName);
    if (resID <= 0) {
        return resNotFound;
    }

    /* Try to load from system file */
    Handle resHandle = GetResource(DRVR_RESOURCE_TYPE, resID);
    if (resHandle == NULL) {
        return resNotFound;
    }

    /* Validate driver */
    int16_t error = ValidateDriverResource(resHandle);
    if (error != noErr) {
        g_loaderStats.validationFailures++;
        ReleaseResource(resHandle);
        return error;
    }

    /* Check name matches */
    DriverHeaderPtr drvrPtr = (DriverHeaderPtr)*resHandle;
    if (!CompareDriverNames(driverName, drvrPtr->drvrName)) {
        ReleaseResource(resHandle);
        return resNotFound;
    }

    DetachResource(resHandle);
    *driverHandle = resHandle;
    return noErr;
}

static int16_t LoadDriverFromSlotROM(const uint8_t *driverName, Handle *driverHandle)
{
    if (driverName == NULL || driverHandle == NULL) {
        return paramErr;
    }

    *driverHandle = NULL;

    /* Placeholder for slot ROM driver loading */
    /* In the original Mac OS, this would search slot ROM for drivers */
    /* For our portable implementation, we'll simulate this */

    return resNotFound;
}

static Handle SearchDriverResources(const uint8_t *driverName)
{
    if (driverName == NULL) {
        return NULL;
    }

    /* Search through all DRVR resources */
    int16_t resCount = CountResources(DRVR_RESOURCE_TYPE);
    for (int16_t i = 1; i <= resCount; i++) {
        Handle resHandle = GetIndResource(DRVR_RESOURCE_TYPE, i);
        if (resHandle == NULL) {
            continue;
        }

        /* Validate and check name */
        if (ValidateDriverResource(resHandle) == noErr) {
            DriverHeaderPtr drvrPtr = (DriverHeaderPtr)*resHandle;
            if (CompareDriverNames(driverName, drvrPtr->drvrName)) {
                DetachResource(resHandle);
                return resHandle;
            }
        }

        ReleaseResource(resHandle);
    }

    return NULL;
}

static int16_t CreateDriverFromTemplate(const char *templateName, Handle *driverHandle)
{
    if (templateName == NULL || driverHandle == NULL) {
        return paramErr;
    }

    *driverHandle = NULL;

    /* Create a minimal driver template */
    /* This is used when no actual driver resource is found */

    size_t templateSize = sizeof(DriverHeader) + strlen(templateName) + 1;
    Handle h = NewHandle(templateSize);
    if (h == NULL) {
        g_loaderStats.memoryErrors++;
        return memFullErr;
    }

    DriverHeaderPtr drvrPtr = (DriverHeaderPtr)*h;
    memset(drvrPtr, 0, sizeof(DriverHeader));

    /* Set up minimal driver header */
    drvrPtr->drvrFlags = Read_Enable_Mask | Write_Enable_Mask |
                        Control_Enable_Mask | Status_Enable_Mask;
    drvrPtr->drvrDelay = 0;
    drvrPtr->drvrEMask = 0;
    drvrPtr->drvrMenu = 0;
    drvrPtr->drvrOpen = sizeof(DriverHeader);
    drvrPtr->drvrPrime = sizeof(DriverHeader) + 4;
    drvrPtr->drvrCtl = sizeof(DriverHeader) + 8;
    drvrPtr->drvrStatus = sizeof(DriverHeader) + 12;
    drvrPtr->drvrClose = sizeof(DriverHeader) + 16;

    /* Set driver name */
    uint8_t nameLen = strlen(templateName);
    if (nameLen > 255) nameLen = 255;
    drvrPtr->drvrName[0] = nameLen;
    memcpy(&drvrPtr->drvrName[1], templateName, nameLen);

    *driverHandle = h;
    return noErr;
}

/* =============================================================================
 * Driver Validation
 * ============================================================================= */

static int16_t ValidateDriverResource(Handle driverHandle)
{
    if (driverHandle == NULL) {
        return paramErr;
    }

    DriverHeaderPtr drvrPtr = (DriverHeaderPtr)*driverHandle;
    if (drvrPtr == NULL) {
        return memFullErr;
    }

    size_t handleSize = GetHandleSize(driverHandle);
    if (handleSize < sizeof(DriverHeader)) {
        return dInstErr;
    }

    /* Validate driver header */
    return ValidateDriver(drvrPtr, handleSize) ? noErr : dInstErr;
}

/* =============================================================================
 * Name Parsing and Comparison
 * ============================================================================= */

static int16_t ParseDriverName(const uint8_t *name, char *parsedName, int16_t maxLen)
{
    if (name == NULL || parsedName == NULL || maxLen <= 0) {
        return paramErr;
    }

    /* Handle Pascal string */
    uint8_t nameLen = name[0];
    if (nameLen == 0 || nameLen >= maxLen) {
        return paramErr;
    }

    memcpy(parsedName, &name[1], nameLen);
    parsedName[nameLen] = '\0';

    return noErr;
}

static bool CompareDriverNames(const uint8_t *name1, const uint8_t *name2)
{
    if (name1 == NULL || name2 == NULL) {
        return false;
    }

    /* Both are Pascal strings */
    uint8_t len1 = name1[0];
    uint8_t len2 = name2[0];

    if (len1 != len2) {
        return false;
    }

    return memcmp(&name1[1], &name2[1], len1) == 0;
}

static int16_t GetDriverResourceID(const uint8_t *driverName)
{
    if (driverName == NULL) {
        return 0;
    }

    /* Simple hash-based resource ID generation */
    /* In real Mac OS, this would be a more sophisticated lookup */
    uint32_t hash = 0;
    uint8_t nameLen = driverName[0];

    for (int i = 1; i <= nameLen; i++) {
        hash = hash * 31 + driverName[i];
    }

    /* Map to a reasonable resource ID range */
    return (int16_t)((hash % 1000) + 128);
}

/* =============================================================================
 * Driver Installation Helpers
 * ============================================================================= */

int16_t InstallDriverResource(Handle driverResource, const uint8_t *driverName)
{
    if (driverResource == NULL) {
        return paramErr;
    }

    /* Validate the driver */
    int16_t error = ValidateDriverResource(driverResource);
    if (error != noErr) {
        return error;
    }

    /* Get the driver header */
    DriverHeaderPtr drvrPtr = (DriverHeaderPtr)*driverResource;

    /* Allocate reference number */
    int16_t refNum = AllocateUnitTableEntry();
    if (refNum < 0) {
        return refNum;
    }

    /* Install the driver */
    error = DrvrInstall(drvrPtr, refNum);
    if (error != noErr) {
        DeallocateUnitTableEntry(refNum);
        return error;
    }

    return refNum;
}

int16_t RemoveDriverResource(int16_t refNum)
{
    return DrvrRemove(refNum);
}

/* =============================================================================
 * Modern Driver Support
 * ============================================================================= */

int16_t LoadModernDriver(const char *driverPath, ModernDriverInterfacePtr *driverInterface)
{
    if (driverPath == NULL || driverInterface == NULL) {
        return paramErr;
    }

    *driverInterface = NULL;

    /* Placeholder for loading modern drivers from shared libraries */
    /* In a full implementation, this would use dlopen/LoadLibrary */

    return fnfErr; /* Not implemented */
}

int16_t UnloadModernDriver(ModernDriverInterfacePtr driverInterface)
{
    if (driverInterface == NULL) {
        return paramErr;
    }

    /* Cleanup modern driver */
    if (driverInterface->cleanup != NULL) {
        driverInterface->cleanup(driverInterface->driverContext);
    }

    return noErr;
}

/* =============================================================================
 * Driver Statistics and Information
 * ============================================================================= */

void GetDriverLoaderStats(void *stats)
{
    if (stats != NULL) {
        memcpy(stats, &g_loaderStats, sizeof(g_loaderStats));
    }
}

int16_t GetLoadedDriverCount(void)
{
    return g_loaderStats.driversLoaded;
}

int16_t EnumerateLoadedDrivers(void (*callback)(int16_t refNum, const uint8_t *name, void *context), void *context)
{
    if (callback == NULL) {
        return paramErr;
    }

    /* Get all active reference numbers */
    int16_t refNums[MAX_UNIT_TABLE_SIZE];
    int16_t count = UnitTable_GetActiveRefNums(refNums, MAX_UNIT_TABLE_SIZE);

    for (int16_t i = 0; i < count; i++) {
        DCEHandle dceHandle = UnitTable_GetDCE(refNums[i]);
        if (dceHandle != NULL) {
            DCEPtr dce = *dceHandle;
            if (dce != NULL && dce->dCtlDriver != NULL) {
                uint8_t driverName[256] = {0};

                /* Get driver name */
                if (dce->dCtlFlags & FollowsNewRules_Mask) {
                    /* Modern driver */
                    ModernDriverInterfacePtr modernIf = (ModernDriverInterfacePtr)dce->dCtlDriver;
                    if (modernIf->driverName != NULL) {
                        size_t nameLen = strlen(modernIf->driverName);
                        if (nameLen > 255) nameLen = 255;
                        driverName[0] = (uint8_t)nameLen;
                        memcpy(&driverName[1], modernIf->driverName, nameLen);
                    }
                } else {
                    /* Classic driver */
                    DriverHeaderPtr drvrPtr = (DriverHeaderPtr)dce->dCtlDriver;
                    memcpy(driverName, drvrPtr->drvrName, 256);
                }

                callback(refNums[i], driverName, context);
            }
        }
    }

    return count;
}

/* =============================================================================
 * Resource Management Utilities
 * ============================================================================= */

bool IsDriverResourceValid(Handle driverResource)
{
    return ValidateDriverResource(driverResource) == noErr;
}

int16_t GetDriverResourceInfo(Handle driverResource, uint8_t *name, int16_t *version, uint32_t *flags)
{
    if (driverResource == NULL) {
        return paramErr;
    }

    int16_t error = ValidateDriverResource(driverResource);
    if (error != noErr) {
        return error;
    }

    DriverHeaderPtr drvrPtr = (DriverHeaderPtr)*driverResource;

    if (name != NULL) {
        memcpy(name, drvrPtr->drvrName, drvrPtr->drvrName[0] + 1);
    }

    if (version != NULL) {
        *version = 1; /* Default version */
    }

    if (flags != NULL) {
        *flags = drvrPtr->drvrFlags;
    }

    return noErr;
}

int16_t CloneDriverResource(Handle sourceDriver, Handle *clonedDriver)
{
    if (sourceDriver == NULL || clonedDriver == NULL) {
        return paramErr;
    }

    *clonedDriver = NULL;

    size_t size = GetHandleSize(sourceDriver);
    Handle clone = NewHandle(size);
    if (clone == NULL) {
        return memFullErr;
    }

    /* Copy driver data */
    memcpy(*clone, *sourceDriver, size);

    *clonedDriver = clone;
    return noErr;
}