/*
 * ConnectionManager.h
 * Connection Manager API - Portable System 7.1 Implementation
 *
 * Provides connection management for serial communication, modem control,
 * and network connections with modern protocol support.
 *
 * Based on Apple's Connection Manager 7.1 specification.
 */

#ifndef CONNECTIONMANAGER_H
#define CONNECTIONMANAGER_H

#include "CommToolbox.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Connection Manager Version */
#define curCMVersion 2

/* Connection Manager Environment Record Version */
#define curConnEnvRecVers 0

/* Connection errors */
enum {
    cmGenericError = -1,
    cmNoErr = 0,
    cmRejected = 1,
    cmFailed = 2,
    cmTimeOut = 3,
    cmNotOpen = 4,
    cmNotClosed = 5,
    cmNoRequestPending = 6,
    cmNotSupported = 7,
    cmNoTools = 8,
    cmUserCancel = 9,
    cmUnknownError = 11
};

/* Connection status flags */
enum {
    cmStatusOpening = 1 << 0,
    cmStatusOpen = 1 << 1,
    cmStatusClosing = 1 << 2,
    cmStatusDataAvail = 1 << 3,
    cmStatusCntlAvail = 1 << 4,
    cmStatusAttnAvail = 1 << 5,
    cmStatusDRPend = 1 << 6,
    cmStatusDWPend = 1 << 7,
    cmStatusCRPend = 1 << 8,
    cmStatusCWPend = 1 << 9,
    cmStatusARPend = 1 << 10,
    cmStatusAWPend = 1 << 11,
    cmStatusBreakPend = 1 << 12,
    cmStatusListenPend = 1 << 13,
    cmStatusIncomingCallPresent = 1 << 14,
    cmStatusReserved0 = 1 << 15
};

/* Connection channel types */
enum {
    cmData = 1 << 0,
    cmCntl = 1 << 1,
    cmAttn = 1 << 2,
    cmDataNoTimeout = 1 << 4,
    cmCntlNoTimeout = 1 << 5,
    cmAttnNoTimeout = 1 << 6
};

/* Connection search flags */
enum {
    cmSearchSevenBit = 1 << 0
};

/* Connection record */
typedef struct ConnRecord {
    short procID;                   /* tool procedure ID */
    long flags;                     /* connection flags */
    OSErr errCode;                  /* last error */
    long refCon;                    /* reference constant */
    long userData;                  /* user data */
    ProcPtr defProc;                /* default procedure */
    Ptr config;                     /* configuration data */
    Ptr oldConfig;                  /* old configuration */
    CMBufferSizes bufferSizes;      /* buffer sizes */
    long cmPrivate;                 /* private data */
    CMBufferSizes requestedSizes;   /* requested buffer sizes */
    long reserved1;                 /* reserved */
    long reserved2;                 /* reserved */
} ConnRecord, *ConnPtr, **ConnHandle;

/* Connection environment record */
typedef struct ConnEnvironRec {
    short version;                  /* record version */
    long baudRate;                  /* baud rate */
    short dataBits;                 /* data bits */
    short stopBits;                 /* stop bits */
    short parity;                   /* parity */
    Boolean useDTR;                 /* use DTR */
    Boolean useRTS;                 /* use RTS */
    Boolean useCTS;                 /* use CTS */
    Boolean useDCD;                 /* use DCD */
    short threshold;                /* threshold */
    long inputTimeout;              /* input timeout */
    long outputTimeout;             /* output timeout */
    Str255 name;                    /* connection name */
    long reserved;                  /* reserved */
} ConnEnvironRec, *ConnEnvironRecPtr;

/* Completion procedures */
typedef pascal void (*ConnectionCompletionUPP)(ConnHandle hConn);
typedef pascal long (*ConnectionSearchCallBackUPP)(ConnHandle hConn, Ptr matchData, long refCon);
typedef pascal void (*ConnectionChooseIdleUPP)(void);

#if GENERATINGCFM
/* UPP creation/disposal for CFM */
pascal ConnectionCompletionUPP NewConnectionCompletionProc(ConnectionCompletionProcPtr userRoutine);
pascal ConnectionSearchCallBackUPP NewConnectionSearchCallBackProc(ConnectionSearchCallBackProcPtr userRoutine);
pascal ConnectionChooseIdleUPP NewConnectionChooseIdleProc(ConnectionChooseIdleProcPtr userRoutine);
pascal void DisposeConnectionCompletionUPP(ConnectionCompletionUPP userUPP);
pascal void DisposeConnectionSearchCallBackUPP(ConnectionSearchCallBackUPP userUPP);
pascal void DisposeConnectionChooseIdleUPP(ConnectionChooseIdleUPP userUPP);
#else
/* Direct procedure pointers for 68K */
#define NewConnectionCompletionProc(userRoutine) (userRoutine)
#define NewConnectionSearchCallBackProc(userRoutine) (userRoutine)
#define NewConnectionChooseIdleProc(userRoutine) (userRoutine)
#define DisposeConnectionCompletionUPP(userUPP)
#define DisposeConnectionSearchCallBackUPP(userUPP)
#define DisposeConnectionChooseIdleUPP(userUPP)
#endif

/* Connection Manager API */

/* Initialization and Management */
pascal CMErr InitCM(void);
pascal short CMGetCMVersion(void);

/* Tool Management */
pascal void CMGetToolName(short procID, Str255 name);
pascal short CMGetProcID(ConstStr255Param name);

/* Connection Creation and Disposal */
pascal ConnHandle CMNew(short procID, long flags, const CMBufferSizes *desiredSizes,
                        long refCon, long userData);
pascal void CMDispose(ConnHandle hConn);

/* Connection State Management */
pascal void CMSetRefCon(ConnHandle hConn, long refCon);
pascal long CMGetRefCon(ConnHandle hConn);
pascal void CMSetUserData(ConnHandle hConn, long userData);
pascal long CMGetUserData(ConnHandle hConn);

/* Connection Operations */
pascal CMErr CMOpen(ConnHandle hConn, Boolean async, ConnectionCompletionUPP completor,
                   long timeout);
pascal CMErr CMListen(ConnHandle hConn, Boolean async, ConnectionCompletionUPP completor,
                     long timeout);
pascal CMErr CMAccept(ConnHandle hConn, Boolean accept);
pascal CMErr CMClose(ConnHandle hConn, Boolean async, ConnectionCompletionUPP completor,
                    long timeout, Boolean now);
pascal CMErr CMAbort(ConnHandle hConn);

/* Data Transfer */
pascal CMErr CMRead(ConnHandle hConn, void *theBuffer, long *count, short theChannel,
                   Boolean async, ConnectionCompletionUPP completor, long timeout, short *flags);
pascal CMErr CMWrite(ConnHandle hConn, const void *theBuffer, long *count, short theChannel,
                    Boolean async, ConnectionCompletionUPP completor, long timeout, short flags);

/* Status and Control */
pascal CMErr CMStatus(ConnHandle hConn, CMBufferSizes *sizes, long *flags);
pascal void CMIdle(ConnHandle hConn);
pascal void CMReset(ConnHandle hConn);
pascal void CMBreak(ConnHandle hConn, long duration, Boolean async, ConnectionCompletionUPP completor);
pascal CMErr CMIOKill(ConnHandle hConn, short which);

/* Event Handling */
pascal void CMActivate(ConnHandle hConn, Boolean activate);
pascal void CMResume(ConnHandle hConn, Boolean resume);
pascal Boolean CMMenu(ConnHandle hConn, short menuID, short item);
pascal void CMEvent(ConnHandle hConn, const EventRecord *theEvent);

/* Search Support */
pascal long CMAddSearch(ConnHandle hConn, ConstStr255Param theString, short flags,
                       ConnectionSearchCallBackUPP callBack);
pascal void CMRemoveSearch(ConnHandle hConn, long refNum);
pascal void CMClearSearch(ConnHandle hConn);

/* Configuration */
pascal Boolean CMValidate(ConnHandle hConn);
pascal void CMDefault(Ptr *config, short procID, Boolean allocate);
pascal Ptr CMGetConfig(ConnHandle hConn);
pascal short CMSetConfig(ConnHandle hConn, Ptr thePtr);

/* Setup and Configuration */
pascal Handle CMSetupPreflight(short procID, long *magicCookie);
pascal void CMSetupSetup(short procID, Ptr theConfig, short count, DialogPtr theDialog,
                        long *magicCookie);
pascal void CMSetupItem(short procID, Ptr theConfig, short count, DialogPtr theDialog,
                       short *theItem, long *magicCookie);
pascal Boolean CMSetupFilter(short procID, Ptr theConfig, short count, DialogPtr theDialog,
                            EventRecord *theEvent, short *theItem, long *magicCookie);
pascal void CMSetupCleanup(short procID, Ptr theConfig, short count, DialogPtr theDialog,
                          long *magicCookie);
pascal void CMSetupXCleanup(short procID, Ptr theConfig, short count, DialogPtr theDialog,
                           Boolean OKed, long *magicCookie);
pascal void CMSetupPostflight(short procID);

/* Localization */
pascal short CMIntlToEnglish(ConnHandle hConn, Ptr inputPtr, Ptr *outputPtr, short language);
pascal short CMEnglishToIntl(ConnHandle hConn, Ptr inputPtr, Ptr *outputPtr, short language);

/* Tool Information */
pascal Handle CMGetVersion(ConnHandle hConn);
pascal CMErr CMGetConnEnvirons(ConnHandle hConn, ConnEnvironRec *theEnvirons);

/* Choose Support */
pascal short CMChoose(ConnHandle *hConn, Point where, ConnectionChooseIdleUPP idleProc);
pascal short CMPChoose(ConnHandle *hConn, Point where, ChooseRec *cRec);

/* Error Handling */
pascal void CMGetErrorMsg(ConnHandle hConn, short id, Str255 errMsg);

/* Modern Extensions */

/* Serial Port Management */
typedef struct ModernSerialConfig {
    Str255 portName;        /* Port name */
    long baudRate;          /* Baud rate */
    short dataBits;         /* Data bits */
    short stopBits;         /* Stop bits */
    short parity;           /* Parity */
    short flowControl;      /* Flow control */
    long readTimeout;       /* Read timeout in ms */
    long writeTimeout;      /* Write timeout in ms */
    Boolean DTR;            /* DTR control */
    Boolean RTS;            /* RTS control */
} ModernSerialConfig;

/* Network Configuration */
typedef struct NetworkConfig {
    short protocol;         /* Protocol type */
    Str255 hostname;        /* Hostname/IP */
    long port;              /* Port number */
    Str255 username;        /* Username */
    Str255 password;        /* Password */
    long timeout;           /* Connection timeout */
    Boolean useSSL;         /* Use SSL/TLS */
} NetworkConfig;

/* Modern Connection API */
pascal CMErr CMOpenModernSerial(ConnHandle hConn, const ModernSerialConfig *config);
pascal CMErr CMOpenNetworkConnection(ConnHandle hConn, const NetworkConfig *config);
pascal CMErr CMSetModernConfig(ConnHandle hConn, const void *config, short configType);
pascal CMErr CMGetModernConfig(ConnHandle hConn, void *config, short configType);

/* Thread-safe operations */
pascal CMErr CMLockConnection(ConnHandle hConn);
pascal CMErr CMUnlockConnection(ConnHandle hConn);

/* Configuration types for modern config */
enum {
    cmConfigSerial = 1,
    cmConfigNetwork = 2,
    cmConfigModem = 3
};

#ifdef __cplusplus
}
#endif

#endif /* CONNECTIONMANAGER_H */