/*
 * FileTransfer.h
 * File Transfer Manager API - Portable System 7.1 Implementation
 *
 * Provides file transfer protocols (XMODEM, YMODEM, ZMODEM), error recovery,
 * and modern secure file transfer support (SFTP, SCP, HTTP).
 *
 * Based on Apple's File Transfer Manager 7.1 specification.
 */

#ifndef FILETRANSFER_H
#define FILETRANSFER_H

#include "CommToolbox.h"
#include "ConnectionManager.h"
#include "Files.h"

#ifdef __cplusplus
extern "C" {
#endif

/* File Transfer Manager Version */
#define curFTVersion 2

/* File Transfer errors */
enum {
    ftGenericError = -1,
    ftNoErr = 0,
    ftRejected = 1,
    ftFailed = 2,
    ftTimeOut = 3,
    ftTooManyRetry = 4,
    ftNotEnoughDSpace = 5,
    ftRemoteCancel = 6,
    ftWrongFormat = 7,
    ftNoTools = 8,
    ftUserCancel = 9,
    ftNotSupported = 10
};

/* File Transfer flags */
typedef unsigned long FTFlags;
enum {
    ftIsFTMode = 1 << 0,
    ftNoMenus = 1 << 1,
    ftQuiet = 1 << 2,
    ftConfigChanged = 1 << 4,
    ftSucc = 1 << 7
};

/* File Transfer attributes */
typedef unsigned short FTAttributes;
enum {
    ftSameCircuit = 1 << 0,
    ftSendDisable = 1 << 1,
    ftReceiveDisable = 1 << 2,
    ftTextOnly = 1 << 3,
    ftNoStdFile = 1 << 4,
    ftMultipleFileSend = 1 << 5
};

/* File Transfer directions */
enum {
    ftSending = 0,
    ftReceiving = 1
};

/* File Transfer Callback Messages */
enum {
    ftInitMsg = 0,
    ftDisposeMsg = 1,
    ftSuspendMsg = 2,
    ftResumeMsg = 3,
    ftMenuMsg = 4,
    ftEventMsg = 5,
    ftActivateMsg = 6,
    ftDeactivateMsg = 7,
    ftGetErrorStringMsg = 8,
    ftAbortMsg = 50,
    ftStartMsg = 51,
    ftExecMsg = 52,
    ftFinishMsg = 53
};

/* File operation constants */
enum {
    ftReadProc = 0,
    ftWriteProc = 1,
    ftSeekProc = 2,
    ftCloseProc = 3
};

/* File Transfer record */
typedef struct FTRecord {
    short procID;                   /* tool procedure ID */
    FTFlags flags;                  /* file transfer flags */
    OSErr errCode;                  /* last error */
    long refCon;                    /* reference constant */
    long userData;                  /* user data */
    ProcPtr defProc;                /* default procedure */
    Ptr config;                     /* configuration data */
    Ptr oldConfig;                  /* old configuration */
    FTAttributes attributes;        /* file transfer attributes */
    WindowPtr owner;                /* owning window */
    long ftPrivate;                 /* private data */
    ProcPtr sendProc;               /* send procedure */
    ProcPtr recvProc;               /* receive procedure */
    ProcPtr writeProc;              /* write procedure */
    ProcPtr readProc;               /* read procedure */
    ProcPtr environsProc;           /* environment procedure */
    long reserved1;                 /* reserved */
    long reserved2;                 /* reserved */
} FTRecord, *FTPtr, **FTHandle;

/* File Transfer callback procedures */
typedef pascal OSErr (*FileTransferSendProcUPP)(unsigned char *bufferPtr, Size *count, long refCon);
typedef pascal OSErr (*FileTransferReceiveProcUPP)(unsigned char *bufferPtr, Size *count, long refCon);
typedef pascal OSErr (*FileTransferReadProcUPP)(short refNum, long *count, void *bufferPtr, long refCon);
typedef pascal OSErr (*FileTransferWriteProcUPP)(short refNum, long *count, const void *bufferPtr, long refCon);
typedef pascal void (*FileTransferEnvironsProcUPP)(FTHandle hFT, long msg, long param, long refCon);
typedef pascal void (*FileTransferNotificationUPP)(FTHandle hFT, FSSpec *theFile);
typedef pascal void (*FileTransferChooseIdleUPP)(void);

#if GENERATINGCFM
/* UPP creation/disposal for CFM */
pascal FileTransferSendProcUPP NewFileTransferSendProc(FileTransferSendProcPtr userRoutine);
pascal FileTransferReceiveProcUPP NewFileTransferReceiveProc(FileTransferReceiveProcPtr userRoutine);
pascal FileTransferReadProcUPP NewFileTransferReadProc(FileTransferReadProcPtr userRoutine);
pascal FileTransferWriteProcUPP NewFileTransferWriteProc(FileTransferWriteProcPtr userRoutine);
pascal FileTransferEnvironsProcUPP NewFileTransferEnvironsProc(FileTransferEnvironsProcPtr userRoutine);
pascal FileTransferNotificationUPP NewFileTransferNotificationProc(FileTransferNotificationProcPtr userRoutine);
pascal FileTransferChooseIdleUPP NewFileTransferChooseIdleProc(FileTransferChooseIdleProcPtr userRoutine);
pascal void DisposeFileTransferSendProcUPP(FileTransferSendProcUPP userUPP);
pascal void DisposeFileTransferReceiveProcUPP(FileTransferReceiveProcUPP userUPP);
pascal void DisposeFileTransferReadProcUPP(FileTransferReadProcUPP userUPP);
pascal void DisposeFileTransferWriteProcUPP(FileTransferWriteProcUPP userUPP);
pascal void DisposeFileTransferEnvironsProcUPP(FileTransferEnvironsProcUPP userUPP);
pascal void DisposeFileTransferNotificationUPP(FileTransferNotificationUPP userUPP);
pascal void DisposeFileTransferChooseIdleUPP(FileTransferChooseIdleUPP userUPP);
#else
/* Direct procedure pointers for 68K */
#define NewFileTransferSendProc(userRoutine) (userRoutine)
#define NewFileTransferReceiveProc(userRoutine) (userRoutine)
#define NewFileTransferReadProc(userRoutine) (userRoutine)
#define NewFileTransferWriteProc(userRoutine) (userRoutine)
#define NewFileTransferEnvironsProc(userRoutine) (userRoutine)
#define NewFileTransferNotificationProc(userRoutine) (userRoutine)
#define NewFileTransferChooseIdleProc(userRoutine) (userRoutine)
#define DisposeFileTransferSendProcUPP(userUPP)
#define DisposeFileTransferReceiveProcUPP(userUPP)
#define DisposeFileTransferReadProcUPP(userUPP)
#define DisposeFileTransferWriteProcUPP(userUPP)
#define DisposeFileTransferEnvironsProcUPP(userUPP)
#define DisposeFileTransferNotificationUPP(userUPP)
#define DisposeFileTransferChooseIdleUPP(userUPP)
#endif

/* File Transfer Manager API */

/* Initialization and Management */
pascal FTErr InitFT(void);
pascal short FTGetFTVersion(void);

/* Tool Management */
pascal void FTGetToolName(short procID, Str255 name);
pascal short FTGetProcID(ConstStr255Param name);

/* File Transfer Creation and Disposal */
pascal FTHandle FTNew(short procID, FTFlags theFlags, FileTransferSendProcUPP theSendProc,
                     FileTransferReceiveProcUPP theRecvProc, FileTransferReadProcUPP theReadProc,
                     FileTransferWriteProcUPP theWriteProc, FileTransferEnvironsProcUPP theEnvironsProc,
                     WindowPtr owner, long theRefCon, long theUserData);
pascal void FTDispose(FTHandle hFT);

/* File Transfer State Management */
pascal void FTSetRefCon(FTHandle hFT, long refCon);
pascal long FTGetRefCon(FTHandle hFT);
pascal void FTSetUserData(FTHandle hFT, long userData);
pascal long FTGetUserData(FTHandle hFT);

/* File Transfer Operations */
pascal FTErr FTStart(FTHandle hFT, short direction, const SFReply *fileInfo);
pascal void FTExec(FTHandle hFT);
pascal FTErr FTCleanup(FTHandle hFT, Boolean now);
pascal FTErr FTAbort(FTHandle hFT);

/* Event Handling */
pascal void FTActivate(FTHandle hFT, Boolean activate);
pascal void FTResume(FTHandle hFT, Boolean resume);
pascal Boolean FTMenu(FTHandle hFT, short menuID, short item);
pascal void FTEvent(FTHandle hFT, const EventRecord *theEvent);

/* Configuration */
pascal Boolean FTValidate(FTHandle hFT);
pascal void FTDefault(Ptr *config, short procID, Boolean allocate);
pascal Ptr FTGetConfig(FTHandle hFT);
pascal short FTSetConfig(FTHandle hFT, Ptr thePtr);

/* Setup and Configuration */
pascal Handle FTSetupPreflight(short procID, long *magicCookie);
pascal void FTSetupSetup(short procID, Ptr theConfig, short count, DialogPtr theDialog,
                        long *magicCookie);
pascal void FTSetupItem(short procID, Ptr theConfig, short count, DialogPtr theDialog,
                       short *theItem, long *magicCookie);
pascal Boolean FTSetupFilter(short procID, Ptr theConfig, short count, DialogPtr theDialog,
                            EventRecord *theEvent, short *theItem, long *magicCookie);
pascal void FTSetupCleanup(short procID, Ptr theConfig, short count, DialogPtr theDialog,
                          long *magicCookie);
pascal void FTSetupXCleanup(short procID, Ptr theConfig, short count, DialogPtr theDialog,
                           Boolean OKed, long *magicCookie);
pascal void FTSetupPostflight(short procID);

/* Localization */
pascal short FTIntlToEnglish(FTHandle hFT, Ptr inputPtr, Ptr *outputPtr, short language);
pascal short FTEnglishToIntl(FTHandle hFT, Ptr inputPtr, Ptr *outputPtr, short language);

/* Tool Information */
pascal Handle FTGetVersion(FTHandle hFT);

/* Choose Support */
pascal short FTChoose(FTHandle *hFT, Point where, FileTransferChooseIdleUPP idleProc);
pascal short FTPChoose(FTHandle *hFT, Point where, ChooseRec *cRec);

/* Send/Receive Operations */
pascal OSErr FTSend(FTHandle hFT, unsigned char *bufferPtr, long bufferSize,
                   FileTransferNotificationUPP notifyProc, long refCon);
pascal OSErr FTReceive(FTHandle hFT, unsigned char *bufferPtr, long bufferSize,
                      FileTransferNotificationUPP notifyProc, long refCon);

/* Modern Extensions */

/* Protocol Types */
enum {
    ftProtocolXModem = 1,
    ftProtocolYModem = 2,
    ftProtocolZModem = 3,
    ftProtocolKermit = 4,
    ftProtocolSFTP = 10,
    ftProtocolSCP = 11,
    ftProtocolHTTP = 12,
    ftProtocolHTTPS = 13,
    ftProtocolFTP = 14,
    ftProtocolFTPS = 15
};

/* Encryption Types */
enum {
    ftEncryptionNone = 0,
    ftEncryptionSSL = 1,
    ftEncryptionTLS = 2,
    ftEncryptionSSH = 3
};

/* Modern File Transfer Configuration */
typedef struct ModernFTConfig {
    short protocol;         /* Transfer protocol */
    short encryption;       /* Encryption type */
    Str255 hostname;        /* Remote hostname */
    long port;              /* Remote port */
    Str255 username;        /* Username */
    Str255 password;        /* Password */
    Str255 remotePath;      /* Remote path */
    Boolean resume;         /* Resume capability */
    Boolean compression;    /* Use compression */
    long blockSize;         /* Transfer block size */
    long timeout;           /* Transfer timeout */
} ModernFTConfig;

/* Transfer Statistics */
typedef struct FTStats {
    long bytesTransferred;  /* Bytes transferred */
    long totalBytes;        /* Total bytes */
    unsigned long startTime; /* Start time */
    unsigned long elapsedTime; /* Elapsed time */
    long bytesPerSecond;    /* Transfer rate */
    short errors;           /* Error count */
    short retries;          /* Retry count */
    Boolean completed;      /* Transfer completed */
} FTStats;

/* Progress Information */
typedef struct FTProgress {
    Str255 currentFile;     /* Current file name */
    long fileSize;          /* Current file size */
    long fileTransferred;   /* Bytes transferred for current file */
    short fileNumber;       /* Current file number */
    short totalFiles;       /* Total files */
    long overallSize;       /* Overall transfer size */
    long overallTransferred; /* Overall bytes transferred */
    short percentComplete;  /* Percentage complete */
} FTProgress;

/* Modern File Transfer API */
pascal FTErr FTSetModernConfig(FTHandle hFT, const ModernFTConfig *config);
pascal FTErr FTGetModernConfig(FTHandle hFT, ModernFTConfig *config);
pascal FTErr FTStartModernTransfer(FTHandle hFT, const FSSpec *files, short numFiles,
                                  const ModernFTConfig *config);
pascal FTErr FTGetTransferStats(FTHandle hFT, FTStats *stats);
pascal FTErr FTGetProgress(FTHandle hFT, FTProgress *progress);

/* Secure Transfer Support */
pascal FTErr FTSetSSHKeys(FTHandle hFT, ConstStr255Param publicKey, ConstStr255Param privateKey);
pascal FTErr FTSetSSLCertificate(FTHandle hFT, Handle certificate);
pascal FTErr FTVerifyRemoteHost(FTHandle hFT, ConstStr255Param hostKey);

/* Batch Operations */
pascal FTErr FTAddFileToQueue(FTHandle hFT, const FSSpec *fileSpec, ConstStr255Param remoteName);
pascal FTErr FTRemoveFileFromQueue(FTHandle hFT, short index);
pascal FTErr FTGetQueueSize(FTHandle hFT, short *count);
pascal FTErr FTStartBatchTransfer(FTHandle hFT);

/* Error Recovery */
pascal FTErr FTSetRetryPolicy(FTHandle hFT, short maxRetries, long retryDelay);
pascal FTErr FTResumeTransfer(FTHandle hFT, long resumePosition);
pascal FTErr FTGetErrorDetails(FTHandle hFT, Str255 errorMsg, long *errorCode);

/* Compression Support */
pascal FTErr FTSetCompressionLevel(FTHandle hFT, short level);
pascal FTErr FTGetCompressionRatio(FTHandle hFT, float *ratio);

/* Directory Operations */
pascal FTErr FTCreateRemoteDirectory(FTHandle hFT, ConstStr255Param dirName);
pascal FTErr FTListRemoteDirectory(FTHandle hFT, ConstStr255Param dirPath, Handle *dirList);
pascal FTErr FTChangeRemoteDirectory(FTHandle hFT, ConstStr255Param dirPath);
pascal FTErr FTDeleteRemoteFile(FTHandle hFT, ConstStr255Param fileName);

/* Thread-safe operations */
pascal FTErr FTLockFileTransfer(FTHandle hFT);
pascal FTErr FTUnlockFileTransfer(FTHandle hFT);

/* Progress callback for modern transfers */
typedef pascal void (*ModernFTProgressUPP)(FTHandle hFT, const FTProgress *progress, long refCon);

#if GENERATINGCFM
pascal ModernFTProgressUPP NewModernFTProgressProc(ModernFTProgressProcPtr userRoutine);
pascal void DisposeModernFTProgressUPP(ModernFTProgressUPP userUPP);
#else
#define NewModernFTProgressProc(userRoutine) (userRoutine)
#define DisposeModernFTProgressUPP(userUPP)
#endif

pascal FTErr FTSetProgressCallback(FTHandle hFT, ModernFTProgressUPP progressProc, long refCon);

#ifdef __cplusplus
}
#endif

#endif /* FILETRANSFER_H */