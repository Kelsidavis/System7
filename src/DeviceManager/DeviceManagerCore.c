#include "SuperCompat.h"
#include <string.h>
/*
 * DeviceManagerCore.c
 * System 7.1 Device Manager - Core Implementation
 *
 * This module implements the core Device Manager functionality including
 * initialization, DCE management, and the main Device Manager API.
 *
 */

#include "CompatibilityFix.h"
#include "SystemTypes.h"
#include "System71StdLib.h"

#include "DeviceManager/DeviceManager.h"
#include "DeviceManager/DeviceTypes.h"
#include "DeviceManager/UnitTable.h"
#include "DeviceManager/DriverInterface.h"
#include "MemoryMgr/memory_manager_types.h"
#include "ResourceManager.h"


/* =============================================================================
 * Global Variables
 * ============================================================================= */

static Boolean g_deviceManagerInitialized = false;
static Boolean g_chooserAlertEnabled = true;
static UInt32 g_tickCount = 0;

/* Statistics */
static struct {
    UInt32 openOperations;
    UInt32 closeOperations;
    UInt32 readOperations;
    UInt32 writeOperations;
    UInt32 controlOperations;
    UInt32 statusOperations;
    UInt32 killOperations;
    UInt32 errors;
} g_deviceManagerStats = {0};

/* =============================================================================
 * Internal Function Declarations
 * ============================================================================= */

static SInt16 CreateDCE(DCEHandle *dceHandle);
static SInt16 DisposeDCE(DCEHandle dceHandle);
static SInt16 InitializeDCE(DCEPtr dce, SInt16 refNum);
static Boolean ValidateDCE(DCEPtr dce);
static SInt16 MapRefNumToUnit(SInt16 refNum);
static Boolean IsDriverInstalled(SInt16 refNum);
static SInt16 AllocateDriverRefNum(void);

/* =============================================================================
 * Device Manager Initialization and Shutdown
 * ============================================================================= */

SInt16 DeviceManager_Initialize(void)
{
    SInt16 error;

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
    SInt16 refNums[MAX_UNIT_TABLE_SIZE];
    SInt16 count = UnitTable_GetActiveRefNums(refNums, MAX_UNIT_TABLE_SIZE);

    for (SInt16 i = 0; i < count; i++) {
        DCEHandle dceHandle = UnitTable_GetDCE(refNums[i]);
        if (dceHandle != NULL) {
            DCEPtr dce = *dceHandle;
            if (dce != NULL && (dce->dCtlFlags & Is_Open_Mask)) {
                /* Send goodbye message if driver needs it */
                if (dce->dCtlFlags & Needs_Goodbye_Mask) {
                    CntrlParam pb;
                    memset(&pb, 0, sizeof(pb));
                    pb.ioResult = ioInProgress;
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

DCEHandle GetDCtlEntry(SInt16 refNum)
{
    if (!g_deviceManagerInitialized || !UnitTable_IsValidRefNum(refNum)) {
        return NULL;
    }

    return UnitTable_GetDCE(refNum);
}

static SInt16 CreateDCE(DCEHandle *dceHandle)
{
    Handle h = NewHandle(sizeof(DCE));
    if (h == NULL) {
        return memFullErr;
    }

    *dceHandle = (DCEHandle)h;

    /* Initialize DCE to zero */
    DCEPtr dce = **dceHandle;
    memset(dce, 0, sizeof(DCE));

    return noErr;
}

static SInt16 DisposeDCE(DCEHandle dceHandle)
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

static SInt16 InitializeDCE(DCEPtr dce, SInt16 refNum)
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

static Boolean ValidateDCE(DCEPtr dce)
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

SInt16 DrvrInstall(DriverHeaderPtr drvrPtr, SInt16 refNum)
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
    SInt16 error = UnitTable_AllocateEntry(refNum);
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

SInt16 DrvrInstallResrvMem(DriverHeaderPtr drvrPtr, SInt16 refNum)
{
    /* For now, same as regular DrvrInstall */
    /* In a full implementation, this would pre-allocate memory */
    return DrvrInstall(drvrPtr, refNum);
}

SInt16 DrvrRemove(SInt16 refNum)
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
        pb.ioResult = ioInProgress;
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

SInt16 OpenDriver(const UInt8 *name, SInt16 *drvrRefNum)
{
    if (!g_deviceManagerInitialized) {
        return dsIOCoreErr;
    }

    if (name == NULL || drvrRefNum == NULL) {
        return paramErr;
    }

    /* Try to find driver by name */
    SInt16 refNum = UnitTable_FindByName(name);
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

        SInt16 error = DrvrInstall(drvrPtr, refNum);
        if (error != noErr) {
            return error;
        }
    }

    /* Open the driver */
    IOParam pb;
    memset(&pb, 0, sizeof(pb));
    pb.ioResult = ioInProgress;
    pb.ioRefNum = refNum;

    SInt16 error = PBOpen(&pb, false);
    if (error == noErr) {
        *drvrRefNum = refNum;
        g_deviceManagerStats.openOperations++;
    } else {
        g_deviceManagerStats.errors++;
    }

    return error;
}

SInt16 CloseDriver(SInt16 refNum)
{
    if (!g_deviceManagerInitialized) {
        return dsIOCoreErr;
    }

    IOParam pb;
    memset(&pb, 0, sizeof(pb));
    pb.ioResult = ioInProgress;
    pb.ioRefNum = refNum;

    SInt16 error = PBClose(&pb, false);
    if (error == noErr) {
        g_deviceManagerStats.closeOperations++;
    } else {
        g_deviceManagerStats.errors++;
    }

    return error;
}

SInt16 Control(SInt16 refNum, SInt16 csCode, const void *csParamPtr)
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
        /* Copy parameters - csParam is always 22 bytes per Mac OS API contract.
         * Callers must pass a buffer of at least sizeof(pb.csParam) bytes. */
        memcpy(pb.csParam, csParamPtr, sizeof(pb.csParam));
    }

    SInt16 error = PBControl(&pb, false);
    if (error == noErr) {
        g_deviceManagerStats.controlOperations++;
    } else {
        g_deviceManagerStats.errors++;
    }

    return error;
}

SInt16 Status(SInt16 refNum, SInt16 csCode, void *csParamPtr)
{
    if (!g_deviceManagerInitialized) {
        return dsIOCoreErr;
    }

    CntrlParam pb;
    memset(&pb, 0, sizeof(pb));
    pb.pb.ioResult = ioInProgress;
    pb.ioCRefNum = refNum;
    pb.csCode = csCode;

    SInt16 error = PBStatus(&pb, false);

    if (error == noErr) {
        if (csParamPtr != NULL) {
            /* Copy results back - caller must provide at least sizeof(pb.csParam) bytes */
            memcpy(csParamPtr, pb.csParam, sizeof(pb.csParam));
        }
        g_deviceManagerStats.statusOperations++;
    } else {
        g_deviceManagerStats.errors++;
    }

    return error;
}

SInt16 KillIO(SInt16 refNum)
{
    if (!g_deviceManagerInitialized) {
        return dsIOCoreErr;
    }

    IOParam pb;
    memset(&pb, 0, sizeof(pb));
    pb.ioResult = ioInProgress;
    pb.ioRefNum = refNum;

    SInt16 error = PBKillIO(&pb, false);
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

static SInt16 MapRefNumToUnit(SInt16 refNum)
{
    /* Convert reference number to unit table index */
    if (refNum < 0) {
        return (-refNum) - 1;
    }
    return -1; /* Invalid for positive numbers */
}

static Boolean IsDriverInstalled(SInt16 refNum)
{
    return UnitTable_IsRefNumInUse(refNum);
}

static SInt16 AllocateDriverRefNum(void)
{
    return UnitTable_GetNextAvailableRefNum();
}

Boolean SetChooserAlert(Boolean alertState)
{
    Boolean oldState = g_chooserAlertEnabled;
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

Boolean ValidateDriver(DriverHeaderPtr drvrPtr, UInt32 size)
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

Boolean IsValidRefNum(SInt16 refNum)
{
    return UnitTable_IsValidRefNum(refNum);
}

Boolean IsDCEValid(DCEPtr dce)
{
    return ValidateDCE(dce) && (dce->dCtlFlags & Is_Open_Mask);
}

/* =============================================================================
 * Modern Platform Support
 * ============================================================================= */

SInt16 RegisterModernDevice(const char *devicePath, UInt32 driverType, SInt16 refNum)
{
    /* Placeholder for modern device registration */
    /* In a full implementation, this would interface with the host OS */
    return noErr;
}

SInt16 UnregisterModernDevice(SInt16 refNum)
{
    /* Placeholder for modern device unregistration */
    return noErr;
}

SInt16 SimulateDeviceInterrupt(SInt16 refNum, UInt32 interruptType)
{
    /* Placeholder for interrupt simulation */
    /* In a full implementation, this would trigger driver interrupt handlers */
    return noErr;
}

/* =============================================================================
 * Internal Utilities
 * ============================================================================= */

UInt32 GetCurrentTicks(void)
{
    return ++g_tickCount; /* Simple tick counter */
}
