/*
 * DeviceIO.c
 * System 7.1 Device Manager - Device I/O Operations Implementation
 *
 * Implements the core I/O operations including Open, Close, Read, Write,
 * Control, Status, and KillIO operations using parameter blocks.
 *
 * Based on the original System 7.1 DeviceMgr.a assembly source.
 */

#include "DeviceManager/DeviceIO.h"
#include "DeviceManager/DeviceManager.h"
#include "DeviceManager/DriverInterface.h"
#include "DeviceManager/UnitTable.h"
#include "MemoryManager.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* =============================================================================
 * Global Variables
 * ============================================================================= */

static uint32_t g_ioRequestID = 1;
static IOStatistics g_ioStatistics = {0};

/* =============================================================================
 * Internal Function Declarations
 * ============================================================================= */

static int16_t ValidateIOParam(IOParamPtr pb);
static int16_t ValidateCntrlParam(CntrlParamPtr pb);
static int16_t ProcessAsyncIO(void *pb, DCEPtr dce, bool isAsync);
static void CompleteIOOperation(IOParamPtr pb, int16_t result);
static int16_t HandleFileSystemRedirect(IOParamPtr pb);
static bool IsFileRefNum(int16_t refNum);

/* =============================================================================
 * Parameter Block I/O Operations
 * ============================================================================= */

int16_t PBOpen(IOParamPtr paramBlock, bool async)
{
    if (paramBlock == NULL) {
        return paramErr;
    }

    int16_t error = ValidateIOParam(paramBlock);
    if (error != noErr) {
        return error;
    }

    /* Check if this is for a file (positive refNum) */
    if (IsFileRefNum(paramBlock->ioRefNum)) {
        return HandleFileSystemRedirect(paramBlock);
    }

    /* Get DCE for driver */
    DCEHandle dceHandle = GetDCtlEntry(paramBlock->ioRefNum);
    if (dceHandle == NULL) {
        return badUnitErr;
    }

    DCEPtr dce = *dceHandle;
    if (dce == NULL) {
        return unitEmptyErr;
    }

    /* Set up parameter block for processing */
    paramBlock->pb.ioResult = ioInProgress;
    paramBlock->ioActCount = 0;

    /* Process the operation */
    int16_t result;
    if (async) {
        result = ProcessAsyncIO(paramBlock, dce, true);
    } else {
        result = CallDriverOpen(paramBlock, dce);
        CompleteIOOperation(paramBlock, result);
    }

    /* Update statistics */
    g_ioStatistics.readOperations++; /* Open counts as read operation */

    return result;
}

int16_t PBClose(IOParamPtr paramBlock, bool async)
{
    if (paramBlock == NULL) {
        return paramErr;
    }

    int16_t error = ValidateIOParam(paramBlock);
    if (error != noErr) {
        return error;
    }

    /* Check if this is for a file */
    if (IsFileRefNum(paramBlock->ioRefNum)) {
        return HandleFileSystemRedirect(paramBlock);
    }

    /* Get DCE for driver */
    DCEHandle dceHandle = GetDCtlEntry(paramBlock->ioRefNum);
    if (dceHandle == NULL) {
        return badUnitErr;
    }

    DCEPtr dce = *dceHandle;
    if (dce == NULL) {
        return unitEmptyErr;
    }

    /* Set up parameter block */
    paramBlock->pb.ioResult = ioInProgress;

    /* Process the operation */
    int16_t result;
    if (async) {
        result = ProcessAsyncIO(paramBlock, dce, true);
    } else {
        result = CallDriverClose(paramBlock, dce);
        CompleteIOOperation(paramBlock, result);
    }

    return result;
}

int16_t PBRead(IOParamPtr paramBlock, bool async)
{
    if (paramBlock == NULL) {
        return paramErr;
    }

    int16_t error = ValidateIOParam(paramBlock);
    if (error != noErr) {
        return error;
    }

    /* Check if this is for a file */
    if (IsFileRefNum(paramBlock->ioRefNum)) {
        return HandleFileSystemRedirect(paramBlock);
    }

    /* Get DCE for driver */
    DCEHandle dceHandle = GetDCtlEntry(paramBlock->ioRefNum);
    if (dceHandle == NULL) {
        return badUnitErr;
    }

    DCEPtr dce = *dceHandle;
    if (dce == NULL) {
        return unitEmptyErr;
    }

    /* Check if driver supports read */
    if (!(dce->dCtlFlags & Read_Enable_Mask)) {
        return readErr;
    }

    /* Set up parameter block */
    paramBlock->pb.ioResult = ioInProgress;
    paramBlock->pb.ioTrap = (paramBlock->pb.ioTrap & 0xFF00) | aRdCmd;
    paramBlock->ioActCount = 0;

    /* Process the operation */
    int16_t result;
    if (async) {
        result = ProcessAsyncIO(paramBlock, dce, true);
    } else {
        result = CallDriverPrime(paramBlock, dce);
        CompleteIOOperation(paramBlock, result);
    }

    /* Update statistics */
    g_ioStatistics.readOperations++;
    if (result == noErr) {
        g_ioStatistics.bytesRead += paramBlock->ioActCount;
    } else {
        g_ioStatistics.errors++;
    }

    return result;
}

int16_t PBWrite(IOParamPtr paramBlock, bool async)
{
    if (paramBlock == NULL) {
        return paramErr;
    }

    int16_t error = ValidateIOParam(paramBlock);
    if (error != noErr) {
        return error;
    }

    /* Check if this is for a file */
    if (IsFileRefNum(paramBlock->ioRefNum)) {
        return HandleFileSystemRedirect(paramBlock);
    }

    /* Get DCE for driver */
    DCEHandle dceHandle = GetDCtlEntry(paramBlock->ioRefNum);
    if (dceHandle == NULL) {
        return badUnitErr;
    }

    DCEPtr dce = *dceHandle;
    if (dce == NULL) {
        return unitEmptyErr;
    }

    /* Check if driver supports write */
    if (!(dce->dCtlFlags & Write_Enable_Mask)) {
        return writErr;
    }

    /* Set up parameter block */
    paramBlock->pb.ioResult = ioInProgress;
    paramBlock->pb.ioTrap = (paramBlock->pb.ioTrap & 0xFF00) | aWrCmd;
    paramBlock->ioActCount = 0;

    /* Process the operation */
    int16_t result;
    if (async) {
        result = ProcessAsyncIO(paramBlock, dce, true);
    } else {
        result = CallDriverPrime(paramBlock, dce);
        CompleteIOOperation(paramBlock, result);
    }

    /* Update statistics */
    g_ioStatistics.writeOperations++;
    if (result == noErr) {
        g_ioStatistics.bytesWritten += paramBlock->ioActCount;
    } else {
        g_ioStatistics.errors++;
    }

    return result;
}

int16_t PBControl(CntrlParamPtr paramBlock, bool async)
{
    if (paramBlock == NULL) {
        return paramErr;
    }

    int16_t error = ValidateCntrlParam(paramBlock);
    if (error != noErr) {
        return error;
    }

    /* Get DCE for driver */
    DCEHandle dceHandle = GetDCtlEntry(paramBlock->ioCRefNum);
    if (dceHandle == NULL) {
        return badUnitErr;
    }

    DCEPtr dce = *dceHandle;
    if (dce == NULL) {
        return unitEmptyErr;
    }

    /* Check if driver supports control */
    if (!(dce->dCtlFlags & Control_Enable_Mask)) {
        return controlErr;
    }

    /* Set up parameter block */
    paramBlock->pb.ioResult = ioInProgress;

    /* Process the operation */
    int16_t result;
    if (async) {
        result = ProcessAsyncIO(paramBlock, dce, true);
    } else {
        result = CallDriverControl(paramBlock, dce);
        paramBlock->pb.ioResult = result;
    }

    /* Update statistics */
    g_ioStatistics.controlOperations++;
    if (result != noErr) {
        g_ioStatistics.errors++;
    }

    return result;
}

int16_t PBStatus(CntrlParamPtr paramBlock, bool async)
{
    if (paramBlock == NULL) {
        return paramErr;
    }

    int16_t error = ValidateCntrlParam(paramBlock);
    if (error != noErr) {
        return error;
    }

    /* Get DCE for driver */
    DCEHandle dceHandle = GetDCtlEntry(paramBlock->ioCRefNum);
    if (dceHandle == NULL) {
        return badUnitErr;
    }

    DCEPtr dce = *dceHandle;
    if (dce == NULL) {
        return unitEmptyErr;
    }

    /* Check if driver supports status */
    if (!(dce->dCtlFlags & Status_Enable_Mask)) {
        return statusErr;
    }

    /* Set up parameter block */
    paramBlock->pb.ioResult = ioInProgress;

    /* Process the operation */
    int16_t result;
    if (async) {
        result = ProcessAsyncIO(paramBlock, dce, true);
    } else {
        result = CallDriverStatus(paramBlock, dce);
        paramBlock->pb.ioResult = result;
    }

    /* Update statistics */
    g_ioStatistics.statusOperations++;
    if (result != noErr) {
        g_ioStatistics.errors++;
    }

    return result;
}

int16_t PBKillIO(IOParamPtr paramBlock, bool async)
{
    if (paramBlock == NULL) {
        return paramErr;
    }

    int16_t error = ValidateIOParam(paramBlock);
    if (error != noErr) {
        return error;
    }

    /* Get DCE for driver */
    DCEHandle dceHandle = GetDCtlEntry(paramBlock->ioRefNum);
    if (dceHandle == NULL) {
        return badUnitErr;
    }

    DCEPtr dce = *dceHandle;
    if (dce == NULL) {
        return unitEmptyErr;
    }

    /* Set up parameter block */
    paramBlock->pb.ioResult = ioInProgress;

    /* Kill pending I/O operations */
    int16_t result = CallDriverKill(paramBlock, dce);
    paramBlock->pb.ioResult = result;

    /* Update statistics */
    g_ioStatistics.killOperations++;

    return result;
}

/* =============================================================================
 * I/O Parameter Block Management
 * ============================================================================= */

void InitIOParamBlock(IOParamPtr pb, IOOperationType operation, int16_t refNum)
{
    if (pb == NULL) {
        return;
    }

    memset(pb, 0, sizeof(IOParam));

    /* Set up header */
    pb->pb.qLink = NULL;
    pb->pb.qType = 0;
    pb->pb.ioTrap = 0;
    pb->pb.ioCmdAddr = NULL;
    pb->pb.ioCompletion = NULL;
    pb->pb.ioResult = 0;
    pb->pb.ioNamePtr = NULL;
    pb->pb.ioVRefNum = 0;

    /* Set operation-specific fields */
    pb->ioRefNum = refNum;
    pb->ioVersNum = 0;
    pb->ioPermssn = fsCurPerm;

    switch (operation) {
        case kIOOperationRead:
            pb->pb.ioTrap |= aRdCmd;
            pb->ioPosMode = fsAtMark;
            break;

        case kIOOperationWrite:
            pb->pb.ioTrap |= aWrCmd;
            pb->ioPosMode = fsAtMark;
            break;

        default:
            break;
    }
}

void SetIOBuffer(IOParamPtr pb, void *buffer, int32_t count)
{
    if (pb == NULL) {
        return;
    }

    pb->ioBuffer = buffer;
    pb->ioReqCount = count;
    pb->ioActCount = 0;
}

void SetIOPosition(IOParamPtr pb, int16_t mode, int32_t offset)
{
    if (pb == NULL) {
        return;
    }

    pb->ioPosMode = mode;
    pb->ioPosOffset = offset;
}

void SetIOCompletion(IOParamPtr pb, IOCompletionProc completion)
{
    if (pb == NULL) {
        return;
    }

    pb->pb.ioCompletion = (void*)completion;
}

bool IsIOComplete(IOParamPtr pb)
{
    if (pb == NULL) {
        return true;
    }

    return pb->pb.ioResult != ioInProgress;
}

bool IsIOInProgress(IOParamPtr pb)
{
    if (pb == NULL) {
        return false;
    }

    return pb->pb.ioResult == ioInProgress;
}

int16_t GetIOResult(IOParamPtr pb)
{
    if (pb == NULL) {
        return paramErr;
    }

    return pb->pb.ioResult;
}

/* =============================================================================
 * Asynchronous I/O Management
 * ============================================================================= */

AsyncIORequestPtr CreateAsyncIORequest(IOParamPtr pb, uint32_t priority,
                                       AsyncIOCompletionProc completion)
{
    if (pb == NULL) {
        return NULL;
    }

    AsyncIORequestPtr request = (AsyncIORequestPtr)malloc(sizeof(AsyncIORequest));
    if (request == NULL) {
        return NULL;
    }

    memset(request, 0, sizeof(AsyncIORequest));

    /* Copy parameter block */
    memcpy(&request->param, pb, sizeof(IOParam));

    /* Set request properties */
    request->requestID = g_ioRequestID++;
    request->priority = priority;
    request->isCancelled = false;
    request->isCompleted = false;
    request->context = NULL;

    return request;
}

int16_t CancelAsyncIORequest(AsyncIORequestPtr request)
{
    if (request == NULL) {
        return paramErr;
    }

    if (request->isCompleted) {
        return noErr; /* Already completed */
    }

    request->isCancelled = true;
    request->param.pb.ioResult = abortErr;

    return noErr;
}

int16_t WaitForAsyncIO(AsyncIORequestPtr request, uint32_t timeout)
{
    if (request == NULL) {
        return paramErr;
    }

    /* In a real implementation, this would wait for completion */
    /* For now, we simulate immediate completion */
    request->isCompleted = true;

    return request->param.pb.ioResult;
}

void DestroyAsyncIORequest(AsyncIORequestPtr request)
{
    if (request != NULL) {
        free(request);
    }
}

/* =============================================================================
 * Internal Helper Functions
 * ============================================================================= */

static int16_t ValidateIOParam(IOParamPtr pb)
{
    if (pb == NULL) {
        return paramErr;
    }

    /* Check reference number */
    if (!IsValidRefNum(pb->ioRefNum) && !IsFileRefNum(pb->ioRefNum)) {
        return badUnitErr;
    }

    return noErr;
}

static int16_t ValidateCntrlParam(CntrlParamPtr pb)
{
    if (pb == NULL) {
        return paramErr;
    }

    /* Check reference number */
    if (!IsValidRefNum(pb->ioCRefNum)) {
        return badUnitErr;
    }

    return noErr;
}

static int16_t ProcessAsyncIO(void *pb, DCEPtr dce, bool isAsync)
{
    if (!isAsync) {
        return noErr; /* Should not be called for sync operations */
    }

    /* Mark driver as active */
    dce->dCtlFlags |= Is_Active_Mask;

    /* Add to driver's I/O queue */
    EnqueueIORequest(dce, (IOParamPtr)pb);

    /* In a real implementation, this would trigger driver processing */
    /* For now, we simulate immediate completion */
    IOParamPtr nextPB = DequeueIORequest(dce);
    if (nextPB != NULL) {
        int16_t result;

        /* Determine operation type and call appropriate driver routine */
        if ((nextPB->pb.ioTrap & 0xFF) == aRdCmd) {
            result = CallDriverPrime(nextPB, dce);
        } else if ((nextPB->pb.ioTrap & 0xFF) == aWrCmd) {
            result = CallDriverPrime(nextPB, dce);
        } else {
            result = CallDriverOpen(nextPB, dce); /* Default to open */
        }

        CompleteIOOperation(nextPB, result);
    }

    /* Check if more operations are pending */
    if (!DriverHasPendingIO(dce)) {
        dce->dCtlFlags &= ~Is_Active_Mask;
    }

    return noErr;
}

static void CompleteIOOperation(IOParamPtr pb, int16_t result)
{
    if (pb == NULL) {
        return;
    }

    /* Set result */
    pb->pb.ioResult = result;

    /* Call completion routine if provided */
    if (pb->pb.ioCompletion != NULL) {
        IOCompletionProc completion = (IOCompletionProc)pb->pb.ioCompletion;
        completion(pb);
    }
}

static int16_t HandleFileSystemRedirect(IOParamPtr pb)
{
    /* In the original Mac OS, this would redirect to the File System */
    /* For our implementation, we return an error since we don't implement files */
    return fnfErr; /* File not found */
}

static bool IsFileRefNum(int16_t refNum)
{
    return refNum > 0;
}

void IODone(IOParamPtr paramBlock)
{
    if (paramBlock == NULL) {
        return;
    }

    /* Mark operation as complete */
    if (paramBlock->pb.ioResult == ioInProgress) {
        paramBlock->pb.ioResult = noErr;
    }

    /* Call completion routine */
    CompleteIOOperation(paramBlock, paramBlock->pb.ioResult);
}

void EnqueueIORequest(DCEPtr dce, IOParamPtr paramBlock)
{
    if (dce == NULL || paramBlock == NULL) {
        return;
    }

    /* Simple queue implementation - add to tail */
    paramBlock->pb.qLink = NULL;

    if (dce->dCtlQHdr.qTail == NULL) {
        /* Empty queue */
        dce->dCtlQHdr.qHead = paramBlock;
        dce->dCtlQHdr.qTail = paramBlock;
    } else {
        /* Add to tail */
        ((IOParamPtr)dce->dCtlQHdr.qTail)->pb.qLink = paramBlock;
        dce->dCtlQHdr.qTail = paramBlock;
    }
}

IOParamPtr DequeueIORequest(DCEPtr dce)
{
    if (dce == NULL || dce->dCtlQHdr.qHead == NULL) {
        return NULL;
    }

    /* Remove from head */
    IOParamPtr pb = (IOParamPtr)dce->dCtlQHdr.qHead;
    dce->dCtlQHdr.qHead = pb->pb.qLink;

    if (dce->dCtlQHdr.qHead == NULL) {
        /* Queue is now empty */
        dce->dCtlQHdr.qTail = NULL;
    }

    pb->pb.qLink = NULL;
    return pb;
}

bool DriverHasPendingIO(DCEPtr dce)
{
    if (dce == NULL) {
        return false;
    }

    return dce->dCtlQHdr.qHead != NULL;
}