/*
 * DeviceManagerCore.c
 * System 7.1 Device Manager - Core Implementation
 *
 * This module implements the core Device Manager functionality including
 * initialization, DCE management, and the main Device Manager API.
 *
 * Based on the original System 7.1 DeviceMgr.a assembly source.
 */

#include "DeviceManager/DeviceManager.h"
#include "DeviceManager/DeviceTypes.h"
#include "DeviceManager/UnitTable.h"
#include "DeviceManager/DriverInterface.h"
#include "MemoryManager.h"
#include "ResourceManager.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* =============================================================================
 * Global Variables
 * ============================================================================= */

static bool g_deviceManagerInitialized = false;
static bool g_chooserAlertEnabled = true;
static uint32_t g_tickCount = 0;

/* Statistics */
static struct {
    uint32_t openOperations;
    uint32_t closeOperations;
    uint32_t readOperations;
    uint32_t writeOperations;
    uint32_t controlOperations;
    uint32_t statusOperations;
    uint32_t killOperations;
    uint32_t errors;
} g_deviceManagerStats = {0};

/* =============================================================================
 * Internal Function Declarations
 * ============================================================================= */

static int16_t CreateDCE(DCEHandle *dceHandle);
static int16_t DisposeDCE(DCEHandle dceHandle);
static int16_t InitializeDCE(DCEPtr dce, int16_t refNum);
static bool ValidateDCE(DCEPtr dce);
static int16_t MapRefNumToUnit(int16_t refNum);
static bool IsDriverInstalled(int16_t refNum);
static int16_t AllocateDriverRefNum(void);

/* =============================================================================
 * Device Manager Initialization and Shutdown
 * ============================================================================= */

int16_t DeviceManager_Initialize(void)
{
    int16_t error;

    if (g_deviceManagerInitialized) {
        return noErr; /* Already initialized */
    }

    /* Initialize the unit table */
    error = UnitTable_Initialize(UNIT_TABLE_INITIAL_SIZE);
    if (error != noErr) {
        return error;
    }

    /* Reset statistics */
    memset(&g_deviceManagerStats, 0, sizeof(g_deviceManagerStats));

    /* Initialize tick counter */
    g_tickCount = 0;

    g_deviceManagerInitialized = true;
    return noErr;
}

void DeviceManager_Shutdown(void)
{
    if (!g_deviceManagerInitialized) {
        return;
    }

    /* Close all open drivers */
    int16_t refNums[MAX_UNIT_TABLE_SIZE];
    int16_t count = UnitTable_GetActiveRefNums(refNums, MAX_UNIT_TABLE_SIZE);

    for (int16_t i = 0; i < count; i++) {
        DCEHandle dceHandle = UnitTable_GetDCE(refNums[i]);
        if (dceHandle != NULL) {
            DCEPtr dce = *dceHandle;
            if (dce != NULL && (dce->dCtlFlags & Is_Open_Mask)) {
                /* Send goodbye message if driver needs it */
                if (dce->dCtlFlags & Needs_Goodbye_Mask) {
                    CntrlParam pb;
                    memset(&pb, 0, sizeof(pb));
                    pb.pb.ioResult = ioInProgress;
                    pb.ioCRefNum = refNums[i];
                    pb.csCode = goodBye;

                    CallDriverControl(&pb, dce);
                }

                /* Mark as closed */
                dce->dCtlFlags &= ~Is_Open_Mask;
            }

            DisposeDCE(dceHandle);
        }
    }

    /* Shutdown unit table */
    UnitTable_Shutdown();

    g_deviceManagerInitialized = false;
}

/* =============================================================================
 * DCE Management
 * ============================================================================= */

DCEHandle GetDCtlEntry(int16_t refNum)
{
    if (!g_deviceManagerInitialized || !UnitTable_IsValidRefNum(refNum)) {
        return NULL;
    }

    return UnitTable_GetDCE(refNum);
}

static int16_t CreateDCE(DCEHandle *dceHandle)
{
    Handle h = NewHandle(sizeof(DeviceControlEntry));
    if (h == NULL) {
        return memFullErr;
    }

    *dceHandle = (DCEHandle)h;

    /* Initialize DCE to zero */
    DCEPtr dce = **dceHandle;
    memset(dce, 0, sizeof(DeviceControlEntry));

    return noErr;
}

static int16_t DisposeDCE(DCEHandle dceHandle)
{
    if (dceHandle == NULL) {
        return paramErr;
    }

    DCEPtr dce = *dceHandle;
    if (dce != NULL) {
        /* Dispose of driver storage if allocated */
        if (dce->dCtlStorage != NULL) {
            DisposeHandle(dce->dCtlStorage);
            dce->dCtlStorage = NULL;
        }
    }

    DisposeHandle((Handle)dceHandle);
    return noErr;
}

static int16_t InitializeDCE(DCEPtr dce, int16_t refNum)
{
    if (dce == NULL) {
        return paramErr;
    }

    /* Initialize DCE fields */
    dce->dCtlDriver = NULL;
    dce->dCtlFlags = 0;
    dce->dCtlQHdr.qFlags = 0;
    dce->dCtlQHdr.qHead = NULL;
    dce->dCtlQHdr.qTail = NULL;
    dce->dCtlPosition = 0;
    dce->dCtlStorage = NULL;
    dce->dCtlRefNum = refNum;
    dce->dCtlCurTicks = g_tickCount;
    dce->dCtlWindow = NULL;
    dce->dCtlDelay = 0;
    dce->dCtlEMask = 0;
    dce->dCtlMenu = 0;

    return noErr;
}

static bool ValidateDCE(DCEPtr dce)
{
    if (dce == NULL) {
        return false;
    }

    /* Basic validation */
    if (dce->dCtlRefNum == 0 || !UnitTable_IsValidRefNum(dce->dCtlRefNum)) {
        return false;
    }

    return true;
}

/* =============================================================================
 * Driver Installation and Removal
 * ============================================================================= */

int16_t DrvrInstall(DriverHeaderPtr drvrPtr, int16_t refNum)
{
    if (!g_deviceManagerInitialized) {
        return dsIOCoreErr;
    }

    if (drvrPtr == NULL) {
        return paramErr;
    }

    /* Validate the driver */
    if (!ValidateDriver(drvrPtr, 0)) {
        return dInstErr;
    }

    /* Allocate reference number if needed */
    if (refNum == 0) {
        refNum = AllocateDriverRefNum();
        if (refNum < 0) {
            return refNum; /* Error code */
        }
    } else if (!UnitTable_IsValidRefNum(refNum)) {
        return badUnitErr;
    }

    /* Check if already installed */
    if (IsDriverInstalled(refNum)) {
        return unitEmptyErr; /* Actually means already in use */
    }

    /* Allocate unit table entry */
    int16_t error = UnitTable_AllocateEntry(refNum);
    if (error != noErr) {
        return error;
    }

    /* Create DCE */
    DCEHandle dceHandle;
    error = CreateDCE(&dceHandle);
    if (error != noErr) {
        UnitTable_DeallocateEntry(refNum);
        return error;
    }

    /* Initialize DCE */
    DCEPtr dce = *dceHandle;
    error = InitializeDCE(dce, refNum);
    if (error != noErr) {
        DisposeDCE(dceHandle);
        UnitTable_DeallocateEntry(refNum);
        return error;
    }

    /* Set driver information */
    dce->dCtlDriver = (void*)drvrPtr;
    dce->dCtlFlags = drvrPtr->drvrFlags;
    dce->dCtlDelay = drvrPtr->drvrDelay;
    dce->dCtlEMask = drvrPtr->drvrEMask;
    dce->dCtlMenu = drvrPtr->drvrMenu;

    /* Mark as RAM-based if it's a resource */
    dce->dCtlFlags |= Is_Ram_Based_Mask;

    /* Associate DCE with unit table entry */
    error = UnitTable_SetDCE(refNum, dceHandle);
    if (error != noErr) {
        DisposeDCE(dceHandle);
        UnitTable_DeallocateEntry(refNum);
        return error;
    }

    return noErr;
}

int16_t DrvrInstallResrvMem(DriverHeaderPtr drvrPtr, int16_t refNum)
{
    /* For now, same as regular DrvrInstall */
    /* In a full implementation, this would pre-allocate memory */
    return DrvrInstall(drvrPtr, refNum);
}

int16_t DrvrRemove(int16_t refNum)
{
    if (!g_deviceManagerInitialized) {
        return dsIOCoreErr;
    }

    if (!UnitTable_IsValidRefNum(refNum)) {
        return badUnitErr;
    }

    DCEHandle dceHandle = UnitTable_GetDCE(refNum);
    if (dceHandle == NULL) {
        return dRemovErr;
    }

    DCEPtr dce = *dceHandle;
    if (dce == NULL) {
        return dRemovErr;
    }

    /* Check if driver is open */
    if (dce->dCtlFlags & Is_Open_Mask) {
        return openErr;
    }

    /* Send goodbye message if needed */
    if (dce->dCtlFlags & Needs_Goodbye_Mask) {
        CntrlParam pb;
        memset(&pb, 0, sizeof(pb));
        pb.pb.ioResult = ioInProgress;
        pb.ioCRefNum = refNum;
        pb.csCode = goodBye;

        CallDriverControl(&pb, dce);
    }

    /* Remove from unit table */
    UnitTable_DeallocateEntry(refNum);

    /* Dispose DCE */
    DisposeDCE(dceHandle);

    return noErr;
}

/* =============================================================================
 * Device I/O Operations
 * ============================================================================= */

int16_t OpenDriver(const uint8_t *name, int16_t *drvrRefNum)
{
    if (!g_deviceManagerInitialized) {
        return dsIOCoreErr;
    }

    if (name == NULL || drvrRefNum == NULL) {
        return paramErr;
    }

    /* Try to find driver by name */
    int16_t refNum = UnitTable_FindByName(name);
    if (refNum < 0) {
        /* Driver not found, try to load it */
        Handle driverResource = LoadDriverResource(name, 0);
        if (driverResource == NULL) {
            return resNotFound;
        }

        /* Install the driver */
        DriverHeaderPtr drvrPtr = (DriverHeaderPtr)*driverResource;
        refNum = AllocateDriverRefNum();
        if (refNum < 0) {
            return refNum;
        }

        int16_t error = DrvrInstall(drvrPtr, refNum);
        if (error != noErr) {
            return error;
        }
    }

    /* Open the driver */
    IOParam pb;
    memset(&pb, 0, sizeof(pb));
    pb.pb.ioResult = ioInProgress;
    pb.ioRefNum = refNum;

    int16_t error = PBOpen(&pb, false);
    if (error == noErr) {
        *drvrRefNum = refNum;
        g_deviceManagerStats.openOperations++;
    } else {
        g_deviceManagerStats.errors++;
    }

    return error;
}

int16_t CloseDriver(int16_t refNum)
{
    if (!g_deviceManagerInitialized) {
        return dsIOCoreErr;
    }

    IOParam pb;
    memset(&pb, 0, sizeof(pb));
    pb.pb.ioResult = ioInProgress;
    pb.ioRefNum = refNum;

    int16_t error = PBClose(&pb, false);
    if (error == noErr) {
        g_deviceManagerStats.closeOperations++;
    } else {
        g_deviceManagerStats.errors++;
    }

    return error;
}

int16_t Control(int16_t refNum, int16_t csCode, const void *csParamPtr)
{
    if (!g_deviceManagerInitialized) {
        return dsIOCoreErr;
    }

    CntrlParam pb;
    memset(&pb, 0, sizeof(pb));
    pb.pb.ioResult = ioInProgress;
    pb.ioCRefNum = refNum;
    pb.csCode = csCode;

    if (csParamPtr != NULL) {
        /* Copy parameters (up to 22 bytes) */
        memcpy(pb.csParam, csParamPtr, sizeof(pb.csParam));
    }

    int16_t error = PBControl(&pb, false);
    if (error == noErr) {
        g_deviceManagerStats.controlOperations++;
    } else {
        g_deviceManagerStats.errors++;
    }

    return error;
}

int16_t Status(int16_t refNum, int16_t csCode, void *csParamPtr)
{
    if (!g_deviceManagerInitialized) {
        return dsIOCoreErr;
    }

    CntrlParam pb;
    memset(&pb, 0, sizeof(pb));
    pb.pb.ioResult = ioInProgress;
    pb.ioCRefNum = refNum;
    pb.csCode = csCode;

    int16_t error = PBStatus(&pb, false);

    if (error == noErr) {
        if (csParamPtr != NULL) {
            /* Copy results back */
            memcpy(csParamPtr, pb.csParam, sizeof(pb.csParam));
        }
        g_deviceManagerStats.statusOperations++;
    } else {
        g_deviceManagerStats.errors++;
    }

    return error;
}

int16_t KillIO(int16_t refNum)
{
    if (!g_deviceManagerInitialized) {
        return dsIOCoreErr;
    }

    IOParam pb;
    memset(&pb, 0, sizeof(pb));
    pb.pb.ioResult = ioInProgress;
    pb.ioRefNum = refNum;

    int16_t error = PBKillIO(&pb, false);
    if (error == noErr) {
        g_deviceManagerStats.killOperations++;
    } else {
        g_deviceManagerStats.errors++;
    }

    return error;
}

/* =============================================================================
 * Utility Functions
 * ============================================================================= */

static int16_t MapRefNumToUnit(int16_t refNum)
{
    /* Convert reference number to unit table index */
    if (refNum < 0) {
        return (-refNum) - 1;
    }
    return -1; /* Invalid for positive numbers */
}

static bool IsDriverInstalled(int16_t refNum)
{
    return UnitTable_IsRefNumInUse(refNum);
}

static int16_t AllocateDriverRefNum(void)
{
    return UnitTable_GetNextAvailableRefNum();
}

bool SetChooserAlert(bool alertState)
{
    bool oldState = g_chooserAlertEnabled;
    g_chooserAlertEnabled = alertState;
    return oldState;
}

void GetDeviceManagerStats(void *stats)
{
    if (stats != NULL) {
        memcpy(stats, &g_deviceManagerStats, sizeof(g_deviceManagerStats));
    }
}

/* =============================================================================
 * Driver Validation
 * ============================================================================= */

bool ValidateDriver(DriverHeaderPtr drvrPtr, uint32_t size)
{
    if (drvrPtr == NULL) {
        return false;
    }

    /* Check basic structure */
    if (size > 0 && size < sizeof(DriverHeader)) {
        return false;
    }

    /* Validate offsets */
    if (drvrPtr->drvrOpen < 0 || drvrPtr->drvrPrime < 0 ||
        drvrPtr->drvrCtl < 0 || drvrPtr->drvrStatus < 0 ||
        drvrPtr->drvrClose < 0) {
        return false;
    }

    /* Check driver name */
    if (drvrPtr->drvrName[0] == 0 || drvrPtr->drvrName[0] > 255) {
        return false;
    }

    return true;
}

bool IsValidRefNum(int16_t refNum)
{
    return UnitTable_IsValidRefNum(refNum);
}

bool IsDCEValid(DCEPtr dce)
{
    return ValidateDCE(dce) && (dce->dCtlFlags & Is_Open_Mask);
}

/* =============================================================================
 * Modern Platform Support
 * ============================================================================= */

int16_t RegisterModernDevice(const char *devicePath, uint32_t driverType, int16_t refNum)
{
    /* Placeholder for modern device registration */
    /* In a full implementation, this would interface with the host OS */
    return noErr;
}

int16_t UnregisterModernDevice(int16_t refNum)
{
    /* Placeholder for modern device unregistration */
    return noErr;
}

int16_t SimulateDeviceInterrupt(int16_t refNum, uint32_t interruptType)
{
    /* Placeholder for interrupt simulation */
    /* In a full implementation, this would trigger driver interrupt handlers */
    return noErr;
}

/* =============================================================================
 * Internal Utilities
 * ============================================================================= */

uint32_t GetCurrentTicks(void)
{
    return ++g_tickCount; /* Simple tick counter */
}