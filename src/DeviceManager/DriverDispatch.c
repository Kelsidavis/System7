#include "SuperCompat.h"
#include <stdlib.h>
#include <string.h>
/*
 * DriverDispatch.c
 * System 7.1 Device Manager - Driver Dispatch Implementation
 *
 * Implements the driver dispatch mechanism that calls device driver
 * entry points and manages the calling conventions.
 *
 * Based on the original System 7.1 DeviceMgr.a assembly source.
 */

#include "CompatibilityFix.h"
#include "SystemTypes.h"
#include "System71StdLib.h"

#include "DeviceManager/DriverInterface.h"
#include "DeviceManager/DeviceManager.h"
#include "DeviceManager/DeviceTypes.h"
#include "DeviceManager/UnitTable.h"


/* =============================================================================
 * Global Variables
 * ============================================================================= */

static UInt32 g_dispatchCount = 0;
static UInt32 g_dispatchErrors = 0;

/* =============================================================================
 * Internal Function Declarations
 * ============================================================================= */

static SInt16 ValidateDispatchParameters(void *pb, DCEPtr dce);
static SInt16 ExecuteClassicDriver(DriverSelector selector, void *pb, DCEPtr dce);
static SInt16 ExecuteModernDriver(DriverSelector selector, void *pb, DCEPtr dce);
static void UpdateDriverStatistics(DCEPtr dce, DriverSelector selector, SInt16 result);
static Boolean ShouldBypassDriver(DCEPtr dce, DriverSelector selector);

/* =============================================================================
 * Driver Registration Functions
 * ============================================================================= */

SInt16 RegisterClassicDriver(Handle driverResource, SInt16 refNum)
{
    if (driverResource == NULL) {
        return paramErr;
    }

    DriverHeaderPtr drvrPtr = (DriverHeaderPtr)*driverResource;
    if (!ValidateClassicDriver(driverResource)) {
        return dInstErr;
    }

    return DrvrInstall(drvrPtr, refNum);
}

SInt16 RegisterModernDriver(ModernDriverInterfacePtr driverInterface, SInt16 refNum)
{
    if (driverInterface == NULL) {
        return paramErr;
    }

    if (!ValidateModernDriver(driverInterface)) {
        return dInstErr;
    }

    /* Allocate reference number if needed */
    if (refNum == 0) {
        refNum = UnitTable_GetNextAvailableRefNum();
        if (refNum < 0) {
            return refNum;
        }
    }

    /* Allocate unit table entry */
    SInt16 error = UnitTable_AllocateEntry(refNum);
    if (error != noErr) {
        return error;
    }

    /* Create DCE for modern driver */
    Handle dceHandle = NewHandle(sizeof(DCE));
    if (dceHandle == NULL) {
        UnitTable_DeallocateEntry(refNum);
        return memFullErr;
    }

    DCEPtr dce = (DCEPtr)*dceHandle;
    memset(dce, 0, sizeof(DCE));

    /* Initialize DCE */
    dce->dCtlRefNum = refNum;
    dce->dCtlFlags = Is_Ram_Based_Mask | FollowsNewRules_Mask;
    dce->dCtlDriver = (void*)driverInterface;

    /* Set capabilities based on modern interface */
    if (driverInterface->dispatch.drvPrime != NULL) {
        dce->dCtlFlags |= Read_Enable_Mask | Write_Enable_Mask;
    }
    if (driverInterface->dispatch.drvControl != NULL) {
        dce->dCtlFlags |= Control_Enable_Mask;
    }
    if (driverInterface->dispatch.drvStatus != NULL) {
        dce->dCtlFlags |= Status_Enable_Mask;
    }

    /* Associate with unit table */
    error = UnitTable_SetDCE(refNum, (DCEHandle)dceHandle);
    if (error != noErr) {
        DisposeHandle(dceHandle);
        UnitTable_DeallocateEntry(refNum);
        return error;
    }

    /* Initialize modern driver if it has an init routine */
    if (driverInterface->init != NULL) {
        int result = driverInterface->init(driverInterface->driverContext);
        if (result != 0) {
            UnregisterDriver(refNum);
            return dInstErr;
        }
    }

    return noErr;
}

SInt16 UnregisterDriver(SInt16 refNum)
{
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

    /* Check if it's a modern driver */
    if (dce->dCtlFlags & FollowsNewRules_Mask) {
        ModernDriverInterfacePtr modernIf = (ModernDriverInterfacePtr)dce->dCtlDriver;
        if (modernIf != NULL && modernIf->cleanup != NULL) {
            modernIf->cleanup(modernIf->driverContext);
        }
    }

    return DrvrRemove(refNum);
}

/* =============================================================================
 * Driver Dispatch Functions
 * ============================================================================= */

SInt16 DispatchDriverCall(DriverSelector selector, void *pb, DCEPtr dce)
{
    SInt16 result;

    g_dispatchCount++;

    /* Validate parameters */
    result = ValidateDispatchParameters(pb, dce);
    if (result != noErr) {
        g_dispatchErrors++;
        return result;
    }

    /* Check if we should bypass the driver */
    if (ShouldBypassDriver(dce, selector)) {
        return noErr;
    }

    /* Dispatch based on driver type */
    if (dce->dCtlFlags & FollowsNewRules_Mask) {
        result = ExecuteModernDriver(selector, pb, dce);
    } else {
        result = ExecuteClassicDriver(selector, pb, dce);
    }

    /* Update statistics */
    UpdateDriverStatistics(dce, selector, result);

    if (result != noErr) {
        g_dispatchErrors++;
    }

    return result;
}

SInt16 CallDriverOpen(IOParamPtr pb, DCEPtr dce)
{
    if (pb == NULL || dce == NULL) {
        return paramErr;
    }

    /* Check if already open */
    if (dce->dCtlFlags & Is_Open_Mask) {
        return noErr; /* Already open */
    }

    SInt16 result = DispatchDriverCall(kDriverOpen, pb, dce);
    if (result == noErr) {
        dce->dCtlFlags |= Is_Open_Mask;
        dce->dCtlPosition = 0;
    }

    return result;
}

SInt16 CallDriverPrime(IOParamPtr pb, DCEPtr dce)
{
    if (pb == NULL || dce == NULL) {
        return paramErr;
    }

    /* Check if driver is open */
    if (!(dce->dCtlFlags & Is_Open_Mask)) {
        return notOpenErr;
    }

    /* Check capabilities */
    Boolean isRead = (pb->ioTrap & 0xFF) == aRdCmd;
    Boolean isWrite = (pb->ioTrap & 0xFF) == aWrCmd;

    if (isRead && !(dce->dCtlFlags & Read_Enable_Mask)) {
        return readErr;
    }
    if (isWrite && !(dce->dCtlFlags & Write_Enable_Mask)) {
        return writErr;
    }

    return DispatchDriverCall(kDriverPrime, pb, dce);
}

SInt16 CallDriverControl(CntrlParamPtr pb, DCEPtr dce)
{
    if (pb == NULL || dce == NULL) {
        return paramErr;
    }

    /* Check if driver supports control */
    if (!(dce->dCtlFlags & Control_Enable_Mask)) {
        return controlErr;
    }

    /* Handle special control codes */
    switch (pb->csCode) {
        case killCode:
            return CallDriverKill((IOParamPtr)pb, dce);

        case kControlDriverGestalt:
            /* Handle driver gestalt requests */
            return noErr; /* Placeholder */

        default:
            break;
    }

    return DispatchDriverCall(kDriverControl, pb, dce);
}

SInt16 CallDriverStatus(CntrlParamPtr pb, DCEPtr dce)
{
    if (pb == NULL || dce == NULL) {
        return paramErr;
    }

    /* Check if driver supports status */
    if (!(dce->dCtlFlags & Status_Enable_Mask)) {
        return statusErr;
    }

    /* Handle special status codes */
    switch (pb->csCode) {
        case kStatusGetDCE:
            /* Return DCE handle */
            *(DCEHandle*)pb->csParam = (DCEHandle)((char*)dce - sizeof(Handle*));
            return noErr;

        case kStatusDriverGestalt:
            /* Handle driver gestalt requests */
            return noErr; /* Placeholder */

        default:
            break;
    }

    return DispatchDriverCall(kDriverStatus, pb, dce);
}

SInt16 CallDriverClose(IOParamPtr pb, DCEPtr dce)
{
    if (pb == NULL || dce == NULL) {
        return paramErr;
    }

    /* Check if driver is open */
    if (!(dce->dCtlFlags & Is_Open_Mask)) {
        return notOpenErr;
    }

    SInt16 result = DispatchDriverCall(kDriverClose, pb, dce);
    if (result == noErr) {
        dce->dCtlFlags &= ~Is_Open_Mask;
    }

    return result;
}

SInt16 CallDriverKill(IOParamPtr pb, DCEPtr dce)
{
    if (pb == NULL || dce == NULL) {
        return paramErr;
    }

    /* Mark driver as inactive */
    dce->dCtlFlags &= ~Is_Active_Mask;

    /* Clear I/O queue */
    dce->dCtlQHdr.qHead = NULL;
    dce->dCtlQHdr.qTail = NULL;

    return DispatchDriverCall(kDriverKill, pb, dce);
}

/* =============================================================================
 * Driver Execution Functions
 * ============================================================================= */

static SInt16 ExecuteClassicDriver(DriverSelector selector, void *pb, DCEPtr dce)
{
    DriverHeaderPtr drvrHeader = (DriverHeaderPtr)dce->dCtlDriver;
    if (drvrHeader == NULL) {
        return dInstErr;
    }

    SInt16 offset;
    switch (selector) {
        case kDriverOpen:
            offset = drvrHeader->drvrOpen;
            break;
        case kDriverPrime:
            offset = drvrHeader->drvrPrime;
            break;
        case kDriverControl:
            offset = drvrHeader->drvrCtl;
            break;
        case kDriverStatus:
            offset = drvrHeader->drvrStatus;
            break;
        case kDriverClose:
            offset = drvrHeader->drvrClose;
            break;
        default:
            return paramErr;
    }

    if (offset <= 0) {
        return badReqErr;
    }

    /* In the original Mac OS, this would jump to the driver code at the offset.
     * For our portable implementation, we simulate successful execution. */

    /* Placeholder: In a real implementation, you would either:
     * 1. Use a 68k emulator to execute the original driver code
     * 2. Have pre-converted drivers with C implementations
     * 3. Use a driver translation layer
     */

    return noErr;
}

static SInt16 ExecuteModernDriver(DriverSelector selector, void *pb, DCEPtr dce)
{
    ModernDriverInterfacePtr modernIf = (ModernDriverInterfacePtr)dce->dCtlDriver;
    if (modernIf == NULL) {
        return dInstErr;
    }

    DriverDispatchTablePtr dispatch = &modernIf->dispatch;

    switch (selector) {
        case kDriverOpen:
            if (dispatch->drvOpen != NULL) {
                return dispatch->drvOpen((IOParamPtr)pb, dce);
            }
            break;

        case kDriverPrime:
            if (dispatch->drvPrime != NULL) {
                return dispatch->drvPrime((IOParamPtr)pb, dce);
            }
            break;

        case kDriverControl:
            if (dispatch->drvControl != NULL) {
                return dispatch->drvControl((CntrlParamPtr)pb, dce);
            }
            break;

        case kDriverStatus:
            if (dispatch->drvStatus != NULL) {
                return dispatch->drvStatus((CntrlParamPtr)pb, dce);
            }
            break;

        case kDriverClose:
            if (dispatch->drvClose != NULL) {
                return dispatch->drvClose((IOParamPtr)pb, dce);
            }
            break;

        case kDriverKill:
            if (dispatch->drvKill != NULL) {
                return dispatch->drvKill((IOParamPtr)pb, dce);
            }
            break;

        default:
            return paramErr;
    }

    return badReqErr;
}

/* =============================================================================
 * Validation Functions
 * ============================================================================= */

Boolean ValidateClassicDriver(Handle driverResource)
{
    if (driverResource == NULL) {
        return false;
    }

    DriverHeaderPtr drvrPtr = (DriverHeaderPtr)*driverResource;
    return ValidateDriver(drvrPtr, GetHandleSize(driverResource));
}

Boolean ValidateModernDriver(ModernDriverInterfacePtr driverInterface)
{
    if (driverInterface == NULL) {
        return false;
    }

    /* Check required fields */
    if (driverInterface->driverName == NULL ||
        strlen(driverInterface->driverName) == 0) {
        return false;
    }

    /* At least one entry point must be provided */
    DriverDispatchTablePtr dispatch = &driverInterface->dispatch;
    if (dispatch->drvOpen == NULL && dispatch->drvPrime == NULL &&
        dispatch->drvControl == NULL && dispatch->drvStatus == NULL &&
        dispatch->drvClose == NULL) {
        return false;
    }

    return true;
}

UInt16 GetDriverCapabilities(DCEPtr dce)
{
    if (dce == NULL) {
        return 0;
    }

    return dce->dCtlFlags;
}

Boolean DriverSupportsOperation(DCEPtr dce, DriverSelector operation)
{
    if (dce == NULL) {
        return false;
    }

    switch (operation) {
        case kDriverPrime:
            return (dce->dCtlFlags & (Read_Enable_Mask | Write_Enable_Mask)) != 0;
        case kDriverControl:
            return (dce->dCtlFlags & Control_Enable_Mask) != 0;
        case kDriverStatus:
            return (dce->dCtlFlags & Status_Enable_Mask) != 0;
        case kDriverOpen:
        case kDriverClose:
        case kDriverKill:
            return true; /* Always supported */
        default:
            return false;
    }
}

/* =============================================================================
 * Helper Functions
 * ============================================================================= */

static SInt16 ValidateDispatchParameters(void *pb, DCEPtr dce)
{
    if (pb == NULL || dce == NULL) {
        return paramErr;
    }

    /* Check if DCE is valid */
    if (!IsValidRefNum(dce->dCtlRefNum)) {
        return badUnitErr;
    }

    return noErr;
}

static void UpdateDriverStatistics(DCEPtr dce, DriverSelector selector, SInt16 result)
{
    /* Update driver usage statistics */
    dce->dCtlCurTicks = GetCurrentTicks();

    /* In a full implementation, this could track per-driver statistics */
}

static Boolean ShouldBypassDriver(DCEPtr dce, DriverSelector selector)
{
    /* Check if driver should be bypassed for certain operations */
    /* This could be used for debugging or testing purposes */
    return false;
}

SInt16 GetDriverName(DCEPtr dce, char *name, SInt16 maxLen)
{
    if (dce == NULL || name == NULL || maxLen <= 0) {
        return paramErr;
    }

    if (dce->dCtlFlags & FollowsNewRules_Mask) {
        /* Modern driver */
        ModernDriverInterfacePtr modernIf = (ModernDriverInterfacePtr)dce->dCtlDriver;
        if (modernIf != NULL && modernIf->driverName != NULL) {
            strncpy(name, modernIf->driverName, maxLen - 1);
            name[maxLen - 1] = '\0';
            return strlen(name);
        }
    } else {
        /* Classic driver */
        DriverHeaderPtr drvrHeader = (DriverHeaderPtr)dce->dCtlDriver;
        if (drvrHeader != NULL) {
            UInt8 nameLen = drvrHeader->drvrName[0];
            if (nameLen > 0 && nameLen < maxLen) {
                memcpy(name, &drvrHeader->drvrName[1], nameLen);
                name[nameLen] = '\0';
                return nameLen;
            }
        }
    }

    return 0;
}

DriverContextPtr CreateDriverContext(DCEPtr dce, Boolean isModern)
{
    DriverContextPtr context = (DriverContextPtr)malloc(sizeof(DriverContext));
    if (context == NULL) {
        return NULL;
    }

    memset(context, 0, sizeof(DriverContext));
    context->dce = dce;
    context->isModern = isModern;

    if (isModern && dce != NULL) {
        context->modernIf = (ModernDriverInterfacePtr)dce->dCtlDriver;
    }

    return context;
}

void DestroyDriverContext(DriverContextPtr context)
{
    if (context != NULL) {
        free(context);
    }
}

DriverContextPtr GetDriverContext(DCEPtr dce)
{
    /* In a full implementation, this would maintain a table of contexts */
    /* For now, create a temporary context */
    return CreateDriverContext(dce, (dce->dCtlFlags & FollowsNewRules_Mask) != 0);
}
