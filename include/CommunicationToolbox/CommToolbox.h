/*
/* QElemPtr defined in MacTypes.h */
 * CommToolbox.h
 * Main Communication Toolbox API - Portable System 7.1 Implementation
 *
 * Provides complete Mac OS Communication Toolbox functionality including:
 * - Connection Manager for serial communication
 * - Terminal Manager for terminal emulation
 * - File Transfer Manager for file transfer protocols
 * - Communication Resource Manager for tool management
 *
 * Derived from ROM and public documentation.
 */

#ifndef COMMTOOLBOX_H
#define COMMTOOLBOX_H

#include "SystemTypes.h"

/* Forward declarations */

#include "SystemTypes.h"
#include "WindowManager/WindowTypes.h"

#include "System7.h"
#include "Dialogs.h"
#include "Files.h"
#include "Events.h"
#include "Resources.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Communication Toolbox Version */
#define CTB_VERSION 2

/* Tool Classes - also file types */
#define classCM 'cbnd'  /* Connection tools */
#define classFT 'fbnd'  /* File Transfer tools */
#define classTM 'tbnd'  /* Terminal tools */

/* Error Codes */

/* Forward declarations */
/* Handle is defined in MacTypes.h */
/* Handle is defined in MacTypes.h */
/* Handle is defined in MacTypes.h */
/* Handle is defined in MacTypes.h */

/* Communication Resource Manager types */
#define crmType 9
#define crmRecVersion 1

         /* queue link */
    short qType;            /* queue type = crmType */
    short crmVersion;       /* version of queue element */
    long crmPrivate;        /* reserved */
    short crmReserved;      /* reserved */
    long crmDeviceType;     /* device type assigned by DTS */
    long crmDeviceID;       /* device ID */
    long crmAttributes;     /* pointer to attribute block */
    long crmStatus;         /* status variable */
    long crmRefCon;         /* for device private use */
} CRMRec, *CRMRecPtr;

/* Buffer size structure */

/* Connection flags */

/* Terminal flags */

/* File Transfer flags */

/* Choice record for enhanced choose dialogs */

    long msg;               /* choice messages */
    ProcPtr idleProc;       /* idle procedure */
    ProcPtr filterProc;     /* filter procedure */
    Str63 newTool;          /* new tool name */
    Ptr newConfig;          /* new configuration */
    ProcPtr eventProc;      /* event procedure */
} ChooseRec, *ChooseRecPtr;

/* Core Communication Toolbox Functions */

/* Initialization */
pascal OSErr InitCTB(void);
pascal OSErr InitCRM(void);
pascal OSErr InitCM(void);
pascal OSErr InitTM(void);
pascal OSErr InitFT(void);

/* Communication Resource Manager */
pascal QHdrPtr CRMGetHeader(void);
pascal void CRMInstall(QElemPtr crmReqPtr);
pascal OSErr CRMRemove(QElemPtr crmReqPtr);
pascal QElemPtr CRMSearch(QElemPtr crmReqPtr);
pascal short CRMGetCRMVersion(void);

/* Resource Management */
pascal Handle CRMGetResource(ResType theType, short theID);
pascal Handle CRMGet1Resource(ResType theType, short theID);
pascal Handle CRMGetIndResource(ResType theType, short index);
pascal Handle CRMGet1IndResource(ResType theType, short index);
pascal Handle CRMGetNamedResource(ResType theType, ConstStr255Param name);
pascal Handle CRMGet1NamedResource(ResType theType, ConstStr255Param name);
pascal void CRMReleaseResource(Handle theHandle);
pascal Handle CRMGetToolResource(short procID, ResType theType, short theID);
pascal Handle CRMGetToolNamedResource(short procID, ResType theType, ConstStr255Param name);
pascal void CRMReleaseToolResource(short procID, Handle theHandle);
pascal long CRMGetIndex(Handle theHandle);

/* Tool Management */
pascal short CRMLocalToRealID(ResType bundleType, short toolID, ResType theType, short localID);
pascal short CRMRealToLocalID(ResType bundleType, short toolID, ResType theType, short realID);
pascal OSErr CRMGetIndToolName(OSType bundleType, short index, Str255 toolName);
pascal OSErr CRMFindCommunications(short *vRefNum, long *dirID);
pascal Boolean CRMIsDriverOpen(ConstStr255Param driverName);
pascal OSErr CRMReserveRF(short refNum);
pascal OSErr CRMReleaseRF(short refNum);

/* Common Tool Functions */
pascal short CTBGetProcID(ConstStr255Param name, short mgrSel);
pascal void CTBGetToolName(short procID, Str255 name, short mgrSel);
pascal Handle CTBGetVersion(CTBHandle hCTB, short mgrSel);
pascal Boolean CTBValidate(CTBHandle hCTB, short mgrSel);
pascal void CTBDefault(Ptr *config, short procID, Boolean allocate, short mgrSel);

/* Setup Functions */
pascal Handle CTBSetupPreflight(short procID, long *magicCookie, short mgrSel);
pascal void CTBSetupSetup(short procID, Ptr theConfig, short count,
                         DialogPtr theDialog, long *magicCookie, short mgrSel);
pascal void CTBSetupItem(short procID, Ptr theConfig, short count,
                        DialogPtr theDialog, short *theItem, long *magicCookie, short mgrSel);
pascal Boolean CTBSetupFilter(short procID, Ptr theConfig, short count,
                             DialogPtr theDialog, EventRecord *theEvent,
                             short *theItem, long *magicCookie, short mgrSel);
pascal void CTBSetupCleanup(short procID, Ptr theConfig, short count,
                           DialogPtr theDialog, long *magicCookie, short mgrSel);
pascal void CTBSetupXCleanup(short procID, Ptr theConfig, short count,
                            DialogPtr theDialog, Boolean OKed, long *magicCookie, short mgrSel);
pascal void CTBSetupPostflight(short procID, short mgrSel);

/* Configuration */
pascal Ptr CTBGetConfig(CTBHandle hCTB, short mgrSel);
pascal short CTBSetConfig(CTBHandle hCTB, Ptr thePtr, short mgrSel);

/* Localization */
pascal short CTBIntlToEnglish(CTBHandle hCTB, Ptr inputPtr, Ptr *outputPtr,
                             short language, short mgrSel);
pascal short CTBEnglishToIntl(CTBHandle hCTB, Ptr inputPtr, Ptr *outputPtr,
                             short language, short mgrSel);

/* Choose Functions */
pascal short CTBChoose(CTBHandle *hCTB, Point where, ProcPtr idleProc, short mgrSel);
pascal short CTBPChoose(CTBHandle *hCTB, Point where, ChooseRec *cRec, short mgrSel);

/* Utility Functions */
pascal Boolean CTBKeystrokeFilter(DialogPtr theDialog, EventRecord *theEvent, long flags);
pascal void CTBGetErrorMsg(CTBHandle hCTB, short id, Str255 errMsg, short mgrSel);

/* Modern Extensions */

/* Cross-platform serial port support */

/* Modern communication support */
pascal OSErr CTBEnumerateSerialPorts(SerialPortInfo ports[], short *count);
pascal OSErr CTBOpenModernSerial(ConstStr255Param portName, SerialPortInfo *config, short *refNum);
pascal OSErr CTBCloseModernSerial(short refNum);

/* Network protocol support */

pascal OSErr CTBOpenNetworkConnection(NetworkAddress *addr, short *refNum);
pascal OSErr CTBCloseNetworkConnection(short refNum);

/* Thread-safe operations */
pascal OSErr CTBLockHandle(Handle h);
pascal OSErr CTBUnlockHandle(Handle h);

/* Manager selectors for core functions */

#ifdef __cplusplus
}
#endif

#endif /* COMMTOOLBOX_H */