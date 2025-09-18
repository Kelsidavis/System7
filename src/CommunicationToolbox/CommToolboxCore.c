/*
 * CommToolboxCore.c
 * Communication Toolbox Core Implementation
 *
 * Implements the core Communication Toolbox functionality including:
 * - Tool management and registration
 * - Resource management
 * - Configuration and setup dialogs
 * - Tool selection and validation
 * - Cross-platform abstraction layer
 *
 * Based on Apple's Communication Toolbox 7.1 specification.
 */

#include "CommToolbox.h"
#include "ConnectionManager.h"
#include "TerminalManager.h"
#include "FileTransfer.h"
#include "SerialManager.h"
#include "ModemManager.h"
#include "ProtocolStack.h"
#include "System7.h"
#include "Memory.h"
#include "Resources.h"
#include "Dialogs.h"
#include <string.h>
#include <stdlib.h>

/* Internal structures */
typedef struct CTBGlobals {
    QHdr deviceQueue;           /* Queue of registered devices */
    short initialized;          /* Initialization flags */
    short cmInitialized;        /* Connection Manager initialized */
    short tmInitialized;        /* Terminal Manager initialized */
    short ftInitialized;        /* File Transfer Manager initialized */
    short version;              /* CTB version */
    Handle resourceMap;         /* Resource map */
    long reserved[8];           /* Reserved for future use */
} CTBGlobals, *CTBGlobalsPtr, **CTBGlobalsHandle;

/* Global state */
static CTBGlobalsHandle gCTBGlobals = NULL;
static Boolean gCTBInitialized = false;

/* Tool registration table */
typedef struct ToolEntry {
    short procID;
    OSType toolType;
    Str255 name;
    ProcPtr toolProc;
    long refCon;
    struct ToolEntry *next;
} ToolEntry, *ToolEntryPtr;

static ToolEntryPtr gToolList = NULL;

/* Forward declarations */
static OSErr InitializeCTBGlobals(void);
static OSErr RegisterBuiltinTools(void);
static ToolEntryPtr FindTool(short procID, OSType toolType);
static short GetNextProcID(OSType toolType);

/*
 * Core Initialization
 */

pascal OSErr InitCTB(void)
{
    OSErr err;

    if (gCTBInitialized) {
        return noErr;
    }

    /* Initialize global state */
    err = InitializeCTBGlobals();
    if (err != noErr) {
        return err;
    }

    /* Initialize sub-managers */
    err = InitCRM();
    if (err != noErr) {
        return err;
    }

    err = InitSerialManager();
    if (err != noErr) {
        return err;
    }

    err = InitModemManager();
    if (err != noErr) {
        return err;
    }

    err = InitProtocolStack();
    if (err != noErr) {
        return err;
    }

    /* Register built-in tools */
    err = RegisterBuiltinTools();
    if (err != noErr) {
        return err;
    }

    gCTBInitialized = true;
    return noErr;
}

pascal OSErr InitCRM(void)
{
    if (gCTBGlobals == NULL) {
        return memFullErr;
    }

    /* Initialize device queue */
    (**gCTBGlobals).deviceQueue.qFlags = 0;
    (**gCTBGlobals).deviceQueue.qHead = NULL;
    (**gCTBGlobals).deviceQueue.qTail = NULL;

    return noErr;
}

static OSErr InitializeCTBGlobals(void)
{
    if (gCTBGlobals != NULL) {
        return noErr;
    }

    gCTBGlobals = (CTBGlobalsHandle)NewHandle(sizeof(CTBGlobals));
    if (gCTBGlobals == NULL) {
        return memFullErr;
    }

    HLock((Handle)gCTBGlobals);

    /* Initialize globals */
    (**gCTBGlobals).deviceQueue.qFlags = 0;
    (**gCTBGlobals).deviceQueue.qHead = NULL;
    (**gCTBGlobals).deviceQueue.qTail = NULL;
    (**gCTBGlobals).initialized = 0;
    (**gCTBGlobals).cmInitialized = 0;
    (**gCTBGlobals).tmInitialized = 0;
    (**gCTBGlobals).ftInitialized = 0;
    (**gCTBGlobals).version = CTB_VERSION;
    (**gCTBGlobals).resourceMap = NULL;

    HUnlock((Handle)gCTBGlobals);

    return noErr;
}

/*
 * Communication Resource Manager
 */

pascal QHdrPtr CRMGetHeader(void)
{
    if (gCTBGlobals == NULL) {
        return NULL;
    }

    return &(**gCTBGlobals).deviceQueue;
}

pascal void CRMInstall(QElemPtr crmReqPtr)
{
    CRMRecPtr crmRec;

    if (crmReqPtr == NULL || gCTBGlobals == NULL) {
        return;
    }

    crmRec = (CRMRecPtr)crmReqPtr;

    /* Set up queue element */
    crmRec->qType = crmType;
    crmRec->crmVersion = crmRecVersion;

    /* Add to device queue */
    Enqueue(crmReqPtr, &(**gCTBGlobals).deviceQueue);
}

pascal OSErr CRMRemove(QElemPtr crmReqPtr)
{
    OSErr err;

    if (crmReqPtr == NULL || gCTBGlobals == NULL) {
        return paramErr;
    }

    err = Dequeue(crmReqPtr, &(**gCTBGlobals).deviceQueue);
    return err;
}

pascal QElemPtr CRMSearch(QElemPtr crmReqPtr)
{
    CRMRecPtr searchRec, currentRec;
    QElemPtr currentElem;

    if (crmReqPtr == NULL || gCTBGlobals == NULL) {
        return NULL;
    }

    searchRec = (CRMRecPtr)crmReqPtr;
    currentElem = (**gCTBGlobals).deviceQueue.qHead;

    while (currentElem != NULL) {
        currentRec = (CRMRecPtr)currentElem;

        /* Match device type if specified */
        if (searchRec->crmDeviceType != 0 &&
            currentRec->crmDeviceType == searchRec->crmDeviceType) {
            return currentElem;
        }

        currentElem = currentElem->qLink;
    }

    return NULL;
}

pascal short CRMGetCRMVersion(void)
{
    return CTB_VERSION;
}

/*
 * Resource Management
 */

pascal Handle CRMGetResource(ResType theType, short theID)
{
    return GetResource(theType, theID);
}

pascal Handle CRMGet1Resource(ResType theType, short theID)
{
    return Get1Resource(theType, theID);
}

pascal Handle CRMGetIndResource(ResType theType, short index)
{
    return GetIndResource(theType, index);
}

pascal Handle CRMGet1IndResource(ResType theType, short index)
{
    return Get1IndResource(theType, index);
}

pascal Handle CRMGetNamedResource(ResType theType, ConstStr255Param name)
{
    return GetNamedResource(theType, name);
}

pascal Handle CRMGet1NamedResource(ResType theType, ConstStr255Param name)
{
    return Get1NamedResource(theType, name);
}

pascal void CRMReleaseResource(Handle theHandle)
{
    ReleaseResource(theHandle);
}

pascal Handle CRMGetToolResource(short procID, ResType theType, short theID)
{
    /* For now, delegate to regular resource manager */
    /* In a full implementation, this would search tool-specific resources */
    return GetResource(theType, theID);
}

pascal Handle CRMGetToolNamedResource(short procID, ResType theType, ConstStr255Param name)
{
    /* For now, delegate to regular resource manager */
    return GetNamedResource(theType, name);
}

pascal void CRMReleaseToolResource(short procID, Handle theHandle)
{
    ReleaseResource(theHandle);
}

pascal long CRMGetIndex(Handle theHandle)
{
    /* Return a pseudo-index based on handle address */
    return (long)theHandle;
}

/*
 * Tool Management
 */

pascal short CRMLocalToRealID(ResType bundleType, short toolID, ResType theType, short localID)
{
    /* Simple mapping for now - in full implementation, would consult bundle resources */
    return localID + (toolID * 1000);
}

pascal short CRMRealToLocalID(ResType bundleType, short toolID, ResType theType, short realID)
{
    /* Reverse of above mapping */
    return realID % 1000;
}

pascal OSErr CRMGetIndToolName(OSType bundleType, short index, Str255 toolName)
{
    ToolEntryPtr tool;
    short count = 0;

    tool = gToolList;
    while (tool != NULL) {
        if (tool->toolType == bundleType) {
            count++;
            if (count == index) {
                BlockMoveData(tool->name, toolName, tool->name[0] + 1);
                return noErr;
            }
        }
        tool = tool->next;
    }

    return resNotFound;
}

pascal OSErr CRMFindCommunications(short *vRefNum, long *dirID)
{
    /* Return system directory for now */
    *vRefNum = 0;
    *dirID = 0;
    return noErr;
}

pascal Boolean CRMIsDriverOpen(ConstStr255Param driverName)
{
    /* Check if a driver is open - simplified implementation */
    return false;
}

pascal OSErr CRMReserveRF(short refNum)
{
    /* Reserve resource file - placeholder */
    return noErr;
}

pascal OSErr CRMReleaseRF(short refNum)
{
    /* Release resource file - placeholder */
    return noErr;
}

/*
 * Common Tool Functions
 */

pascal short CTBGetProcID(ConstStr255Param name, short mgrSel)
{
    ToolEntryPtr tool;
    OSType toolType;

    /* Map manager selector to tool type */
    switch (mgrSel) {
        case cmSel:
            toolType = classCM;
            break;
        case tmSel:
            toolType = classTM;
            break;
        case ftSel:
            toolType = classFT;
            break;
        default:
            return 0;
    }

    tool = gToolList;
    while (tool != NULL) {
        if (tool->toolType == toolType &&
            EqualString(tool->name, name, false, true)) {
            return tool->procID;
        }
        tool = tool->next;
    }

    return 0;
}

pascal void CTBGetToolName(short procID, Str255 name, short mgrSel)
{
    OSType toolType;
    ToolEntryPtr tool;

    /* Map manager selector to tool type */
    switch (mgrSel) {
        case cmSel:
            toolType = classCM;
            break;
        case tmSel:
            toolType = classTM;
            break;
        case ftSel:
            toolType = classFT;
            break;
        default:
            name[0] = 0;
            return;
    }

    tool = FindTool(procID, toolType);
    if (tool != NULL) {
        BlockMoveData(tool->name, name, tool->name[0] + 1);
    } else {
        name[0] = 0;
    }
}

pascal Handle CTBGetVersion(CTBHandle hCTB, short mgrSel)
{
    Handle versionHandle;
    Str255 versionStr = "\pCommunication Toolbox 2.0 (Portable)";

    versionHandle = NewHandle(versionStr[0] + 1);
    if (versionHandle != NULL) {
        HLock(versionHandle);
        BlockMoveData(versionStr, *versionHandle, versionStr[0] + 1);
        HUnlock(versionHandle);
    }

    return versionHandle;
}

pascal Boolean CTBValidate(CTBHandle hCTB, short mgrSel)
{
    /* Basic validation - check handle validity */
    if (hCTB == NULL) {
        return false;
    }

    return true;
}

pascal void CTBDefault(Ptr *config, short procID, Boolean allocate, short mgrSel)
{
    if (allocate) {
        *config = NewPtr(256);  /* Default config size */
        if (*config != NULL) {
            /* Initialize with default values */
            memset(*config, 0, 256);
        }
    }
}

/*
 * Setup Functions (Simplified)
 */

pascal Handle CTBSetupPreflight(short procID, long *magicCookie, short mgrSel)
{
    *magicCookie = 0x12345678;  /* Magic value for validation */
    return NewHandle(0);        /* Empty handle for now */
}

pascal void CTBSetupSetup(short procID, Ptr theConfig, short count,
                         DialogPtr theDialog, long *magicCookie, short mgrSel)
{
    /* Setup dialog handling - placeholder */
}

pascal void CTBSetupItem(short procID, Ptr theConfig, short count,
                        DialogPtr theDialog, short *theItem, long *magicCookie, short mgrSel)
{
    /* Item handling - placeholder */
}

pascal Boolean CTBSetupFilter(short procID, Ptr theConfig, short count,
                             DialogPtr theDialog, EventRecord *theEvent,
                             short *theItem, long *magicCookie, short mgrSel)
{
    /* Event filtering - placeholder */
    return false;
}

pascal void CTBSetupCleanup(short procID, Ptr theConfig, short count,
                           DialogPtr theDialog, long *magicCookie, short mgrSel)
{
    /* Cleanup - placeholder */
}

pascal void CTBSetupXCleanup(short procID, Ptr theConfig, short count,
                            DialogPtr theDialog, Boolean OKed, long *magicCookie, short mgrSel)
{
    /* Extended cleanup - placeholder */
}

pascal void CTBSetupPostflight(short procID, short mgrSel)
{
    /* Postflight - placeholder */
}

/*
 * Configuration
 */

pascal Ptr CTBGetConfig(CTBHandle hCTB, short mgrSel)
{
    if (hCTB == NULL) {
        return NULL;
    }

    /* Return configuration pointer from handle */
    return (**hCTB).config;
}

pascal short CTBSetConfig(CTBHandle hCTB, Ptr thePtr, short mgrSel)
{
    if (hCTB == NULL) {
        return paramErr;
    }

    (**hCTB).config = thePtr;
    return noErr;
}

/*
 * Localization (Simplified)
 */

pascal short CTBIntlToEnglish(CTBHandle hCTB, Ptr inputPtr, Ptr *outputPtr,
                             short language, short mgrSel)
{
    /* Simple pass-through for now */
    *outputPtr = inputPtr;
    return noErr;
}

pascal short CTBEnglishToIntl(CTBHandle hCTB, Ptr inputPtr, Ptr *outputPtr,
                             short language, short mgrSel)
{
    /* Simple pass-through for now */
    *outputPtr = inputPtr;
    return noErr;
}

/*
 * Choose Functions (Simplified)
 */

pascal short CTBChoose(CTBHandle *hCTB, Point where, ProcPtr idleProc, short mgrSel)
{
    /* Simple implementation - would show tool selection dialog */
    return noErr;
}

pascal short CTBPChoose(CTBHandle *hCTB, Point where, ChooseRec *cRec, short mgrSel)
{
    /* Extended choose with custom record */
    return noErr;
}

/*
 * Utility Functions
 */

pascal Boolean CTBKeystrokeFilter(DialogPtr theDialog, EventRecord *theEvent, long flags)
{
    /* Handle special keystrokes in CTB dialogs */
    if (theEvent->what == keyDown) {
        char key = theEvent->message & charCodeMask;
        if (key == 0x1B) {  /* Escape key */
            return true;
        }
    }
    return false;
}

pascal void CTBGetErrorMsg(CTBHandle hCTB, short id, Str255 errMsg, short mgrSel)
{
    /* Map error ID to error message */
    switch (id) {
        case ctbNoErr:
            PLstrcpy(errMsg, "\pNo error");
            break;
        case ctbGenericError:
            PLstrcpy(errMsg, "\pGeneric error");
            break;
        case ctbNoTools:
            PLstrcpy(errMsg, "\pNo tools available");
            break;
        case ctbUserCancel:
            PLstrcpy(errMsg, "\pUser cancelled");
            break;
        default:
            PLstrcpy(errMsg, "\pUnknown error");
            break;
    }
}

/*
 * Modern Extensions
 */

pascal OSErr CTBEnumerateSerialPorts(SerialPortInfo ports[], short *count)
{
    return SerialEnumeratePorts(ports, count);
}

pascal OSErr CTBOpenModernSerial(ConstStr255Param portName, SerialPortInfo *config, short *refNum)
{
    SerialPortHandle hPort;
    OSErr err;

    err = SerialOpenPort(portName, (SerialConfig*)config, &hPort);
    if (err == noErr) {
        *refNum = (short)((long)hPort & 0xFFFF);
    }
    return err;
}

pascal OSErr CTBCloseModernSerial(short refNum)
{
    SerialPortHandle hPort = (SerialPortHandle)refNum;
    return SerialClosePort(hPort);
}

pascal OSErr CTBOpenNetworkConnection(NetworkAddress *addr, short *refNum)
{
    ProtocolHandle hProtocol;
    ProtocolConfig config;
    OSErr err;

    /* Set up protocol configuration */
    config.protocolType = protocolTypeTCP;
    config.remoteAddr = *addr;
    config.timeout = 30000;  /* 30 seconds */

    err = ProtocolCreate(protocolTypeTCP, &config, &hProtocol);
    if (err == noErr) {
        *refNum = (short)((long)hProtocol & 0xFFFF);
    }
    return err;
}

pascal OSErr CTBCloseNetworkConnection(short refNum)
{
    ProtocolHandle hProtocol = (ProtocolHandle)refNum;
    return ProtocolDispose(hProtocol);
}

pascal OSErr CTBLockHandle(Handle h)
{
    if (h == NULL) {
        return paramErr;
    }
    HLock(h);
    return noErr;
}

pascal OSErr CTBUnlockHandle(Handle h)
{
    if (h == NULL) {
        return paramErr;
    }
    HUnlock(h);
    return noErr;
}

/*
 * Internal Helper Functions
 */

static OSErr RegisterBuiltinTools(void)
{
    ToolEntryPtr tool;

    /* Register built-in Connection Manager tools */
    tool = (ToolEntryPtr)NewPtr(sizeof(ToolEntry));
    if (tool != NULL) {
        tool->procID = 1;
        tool->toolType = classCM;
        PLstrcpy(tool->name, "\pSerial Connection");
        tool->toolProc = NULL;
        tool->refCon = 0;
        tool->next = gToolList;
        gToolList = tool;
    }

    /* Register built-in Terminal Manager tools */
    tool = (ToolEntryPtr)NewPtr(sizeof(ToolEntry));
    if (tool != NULL) {
        tool->procID = 1;
        tool->toolType = classTM;
        PLstrcpy(tool->name, "\pVT100 Terminal");
        tool->toolProc = NULL;
        tool->refCon = 0;
        tool->next = gToolList;
        gToolList = tool;
    }

    /* Register built-in File Transfer Manager tools */
    tool = (ToolEntryPtr)NewPtr(sizeof(ToolEntry));
    if (tool != NULL) {
        tool->procID = 1;
        tool->toolType = classFT;
        PLstrcpy(tool->name, "\pXModem File Transfer");
        tool->toolProc = NULL;
        tool->refCon = 0;
        tool->next = gToolList;
        gToolList = tool;
    }

    return noErr;
}

static ToolEntryPtr FindTool(short procID, OSType toolType)
{
    ToolEntryPtr tool = gToolList;

    while (tool != NULL) {
        if (tool->procID == procID && tool->toolType == toolType) {
            return tool;
        }
        tool = tool->next;
    }

    return NULL;
}

static short GetNextProcID(OSType toolType)
{
    ToolEntryPtr tool = gToolList;
    short maxID = 0;

    while (tool != NULL) {
        if (tool->toolType == toolType && tool->procID > maxID) {
            maxID = tool->procID;
        }
        tool = tool->next;
    }

    return maxID + 1;
}