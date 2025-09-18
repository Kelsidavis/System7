/*
 * ModemManager.h
 * Modem Management API - Portable System 7.1 Implementation
 *
 * Provides modem control, AT command processing, dial-up networking,
 * and modern cellular modem support.
 *
 * Extends Mac OS Communication Toolbox with modern modem functionality.
 */

#ifndef MODEMMANAGER_H
#define MODEMMANAGER_H

#include "CommToolbox.h"
#include "SerialManager.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Modem Manager Version */
#define MODEM_MANAGER_VERSION 1

/* Modem Types */
enum {
    modemTypeAnalog = 0,        /* Analog dial-up modem */
    modemTypeISDN = 1,          /* ISDN modem */
    modemTypeCellular = 2,      /* Cellular data modem */
    modemTypeGSM = 3,           /* GSM modem */
    modemTypeCDMA = 4,          /* CDMA modem */
    modemTypeLTE = 5,           /* LTE modem */
    modemTypeSatellite = 6,     /* Satellite modem */
    modemTypeVirtual = 7        /* Virtual modem */
};

/* Modem Status */
enum {
    modemStatusDisconnected = 0,
    modemStatusConnecting = 1,
    modemStatusConnected = 2,
    modemStatusDisconnecting = 3,
    modemStatusError = 4,
    modemStatusBusy = 5,
    modemStatusNoCarrier = 6,
    modemStatusNoDialTone = 7
};

/* AT Command Response Types */
enum {
    atResponseOK = 0,
    atResponseError = 1,
    atResponseConnect = 2,
    atResponseBusy = 3,
    atResponseNoCarrier = 4,
    atResponseNoDialTone = 5,
    atResponseRing = 6,
    atResponseTimeout = 7
};

/* Connection Types */
enum {
    connectionTypeData = 0,
    connectionTypeFax = 1,
    connectionTypeVoice = 2
};

/* Modem Configuration */
typedef struct ModemConfig {
    Str255 initString;          /* Initialization string */
    Str255 dialPrefix;          /* Dial prefix */
    Str255 dialSuffix;          /* Dial suffix */
    Str255 hangupString;        /* Hangup string */
    Str255 escapeString;        /* Escape string */
    long commandTimeout;        /* Command timeout (ms) */
    long dialTimeout;           /* Dial timeout (ms) */
    long carrierTimeout;        /* Carrier timeout (ms) */
    Boolean pulseDialing;       /* Use pulse dialing */
    Boolean waitForDialTone;    /* Wait for dial tone */
    Boolean blindDial;          /* Blind dial */
    short speakerVolume;        /* Speaker volume (0-3) */
    Boolean speakerOnUntilCarrier; /* Speaker control */
    short autoAnswer;           /* Auto answer rings (0=off) */
    long baudRate;              /* Connection baud rate */
    short dataBits;             /* Data bits */
    short stopBits;             /* Stop bits */
    short parity;               /* Parity */
    short flowControl;          /* Flow control */
} ModemConfig;

/* Modem Information */
typedef struct ModemInfo {
    Str255 manufacturer;        /* Manufacturer name */
    Str255 model;               /* Model name */
    Str255 revision;            /* Firmware revision */
    Str255 serialNumber;        /* Serial number */
    short modemType;            /* Modem type */
    long maxBaudRate;           /* Maximum baud rate */
    Boolean supportsV90;        /* V.90 support */
    Boolean supportsV92;        /* V.92 support */
    Boolean supportsFax;        /* Fax support */
    Boolean supportsVoice;      /* Voice support */
    Boolean supportsDataCompression; /* Data compression */
    Boolean supportsErrorCorrection; /* Error correction */
    Str255 supportedCommands;   /* Supported AT commands */
} ModemInfo;

/* Connection Information */
typedef struct ConnectionInfo {
    short connectionType;       /* Connection type */
    long baudRate;              /* Connection speed */
    Str255 protocol;            /* Connection protocol */
    Boolean compressed;         /* Data compression active */
    Boolean errorCorrected;     /* Error correction active */
    short signalQuality;        /* Signal quality (0-100) */
    long bytesTransmitted;      /* Bytes transmitted */
    long bytesReceived;         /* Bytes received */
    unsigned long connectTime;  /* Connection time (seconds) */
} ConnectionInfo;

/* Cellular Modem Information */
typedef struct CellularInfo {
    Str255 carrier;             /* Carrier name */
    Str255 networkType;         /* Network type (GSM, CDMA, LTE) */
    short signalStrength;       /* Signal strength (0-100) */
    Str255 phoneNumber;         /* Phone number */
    Str255 IMEI;                /* IMEI number */
    Str255 IMSI;                /* IMSI number */
    Boolean roaming;            /* Roaming status */
    long dataUsage;             /* Data usage (bytes) */
} CellularInfo;

/* Modem Handle */
typedef struct ModemRec *ModemPtr, **ModemHandle;

/* Callback procedures */
typedef pascal void (*ModemConnectionUPP)(ModemHandle hModem, short status, long refCon);
typedef pascal void (*ModemDataReceivedUPP)(ModemHandle hModem, void *data, long size, long refCon);
typedef pascal void (*ModemATResponseUPP)(ModemHandle hModem, ConstStr255Param response, long refCon);

#if GENERATINGCFM
pascal ModemConnectionUPP NewModemConnectionProc(ModemConnectionProcPtr userRoutine);
pascal ModemDataReceivedUPP NewModemDataReceivedProc(ModemDataReceivedProcPtr userRoutine);
pascal ModemATResponseUPP NewModemATResponseProc(ModemATResponseProcPtr userRoutine);
pascal void DisposeModemConnectionUPP(ModemConnectionUPP userUPP);
pascal void DisposeModemDataReceivedUPP(ModemDataReceivedUPP userUPP);
pascal void DisposeModemATResponseUPP(ModemATResponseUPP userUPP);
#else
#define NewModemConnectionProc(userRoutine) (userRoutine)
#define NewModemDataReceivedProc(userRoutine) (userRoutine)
#define NewModemATResponseProc(userRoutine) (userRoutine)
#define DisposeModemConnectionUPP(userUPP)
#define DisposeModemDataReceivedUPP(userUPP)
#define DisposeModemATResponseUPP(userUPP)
#endif

/* Modem Manager API */

/* Initialization */
pascal OSErr InitModemManager(void);

/* Modem Management */
pascal OSErr ModemOpen(ConstStr255Param portName, const ModemConfig *config, ModemHandle *hModem);
pascal OSErr ModemClose(ModemHandle hModem);
pascal Boolean ModemIsOpen(ModemHandle hModem);

/* Configuration */
pascal OSErr ModemSetConfig(ModemHandle hModem, const ModemConfig *config);
pascal OSErr ModemGetConfig(ModemHandle hModem, ModemConfig *config);
pascal OSErr ModemGetInfo(ModemHandle hModem, ModemInfo *info);

/* AT Command Interface */
pascal OSErr ModemSendATCommand(ModemHandle hModem, ConstStr255Param command, Str255 response);
pascal OSErr ModemSendATCommandAsync(ModemHandle hModem, ConstStr255Param command,
                                    ModemATResponseUPP callback, long refCon);
pascal OSErr ModemWaitForResponse(ModemHandle hModem, Str255 response, long timeout);
pascal OSErr ModemFlushResponses(ModemHandle hModem);

/* Connection Management */
pascal OSErr ModemDial(ModemHandle hModem, ConstStr255Param phoneNumber,
                      ModemConnectionUPP callback, long refCon);
pascal OSErr ModemHangup(ModemHandle hModem);
pascal OSErr ModemAnswer(ModemHandle hModem, ModemConnectionUPP callback, long refCon);
pascal OSErr ModemGetConnectionInfo(ModemHandle hModem, ConnectionInfo *info);

/* Data Transfer */
pascal OSErr ModemRead(ModemHandle hModem, void *buffer, long *count);
pascal OSErr ModemWrite(ModemHandle hModem, const void *buffer, long *count);
pascal OSErr ModemSetDataCallback(ModemHandle hModem, ModemDataReceivedUPP callback, long refCon);

/* Status and Control */
pascal OSErr ModemGetStatus(ModemHandle hModem, short *status);
pascal OSErr ModemSetSpeakerVolume(ModemHandle hModem, short volume);
pascal OSErr ModemSetAutoAnswer(ModemHandle hModem, short rings);
pascal OSErr ModemGetSignalQuality(ModemHandle hModem, short *quality);

/* Modern Extensions */

/* Cellular Modem Support */
pascal OSErr ModemGetCellularInfo(ModemHandle hModem, CellularInfo *info);
pascal OSErr ModemSetAPN(ModemHandle hModem, ConstStr255Param apn, ConstStr255Param username,
                        ConstStr255Param password);
pascal OSErr ModemActivatePDPContext(ModemHandle hModem);
pascal OSErr ModemDeactivatePDPContext(ModemHandle hModem);

/* SMS Support */
typedef struct SMSMessage {
    Str255 sender;              /* Sender phone number */
    Str255 text;                /* Message text */
    unsigned long timestamp;    /* Timestamp */
    Boolean read;               /* Read status */
} SMSMessage;

pascal OSErr ModemSendSMS(ModemHandle hModem, ConstStr255Param recipient, ConstStr255Param text);
pascal OSErr ModemReceiveSMS(ModemHandle hModem, SMSMessage messages[], short *count);
pascal OSErr ModemDeleteSMS(ModemHandle hModem, short index);

/* Voice Call Support */
pascal OSErr ModemMakeVoiceCall(ModemHandle hModem, ConstStr255Param phoneNumber);
pascal OSErr ModemAnswerVoiceCall(ModemHandle hModem);
pascal OSErr ModemHangupVoiceCall(ModemHandle hModem);
pascal OSErr ModemSetVoiceVolume(ModemHandle hModem, short volume);

/* GPS Support (for cellular modems) */
typedef struct GPSInfo {
    double latitude;            /* Latitude */
    double longitude;           /* Longitude */
    double altitude;            /* Altitude */
    float accuracy;             /* Accuracy in meters */
    unsigned long timestamp;    /* GPS timestamp */
    Boolean valid;              /* Position valid */
} GPSInfo;

pascal OSErr ModemGetGPSPosition(ModemHandle hModem, GPSInfo *position);
pascal OSErr ModemEnableGPS(ModemHandle hModem, Boolean enable);

/* Network Registration */
typedef struct NetworkInfo {
    Str255 operatorName;        /* Network operator */
    Str255 countryCode;         /* Country code */
    Str255 networkCode;         /* Network code */
    short accessTechnology;     /* Access technology */
    short registrationStatus;   /* Registration status */
} NetworkInfo;

pascal OSErr ModemGetNetworkInfo(ModemHandle hModem, NetworkInfo *info);
pascal OSErr ModemScanNetworks(ModemHandle hModem, NetworkInfo networks[], short *count);
pascal OSErr ModemSelectNetwork(ModemHandle hModem, ConstStr255Param operatorCode);

/* SIM Card Support */
typedef struct SIMInfo {
    Str255 ICCID;               /* SIM card ID */
    Str255 phoneNumber;         /* Phone number */
    short status;               /* SIM status */
    Boolean pinRequired;        /* PIN required */
    short pinTriesLeft;         /* PIN tries remaining */
} SIMInfo;

pascal OSErr ModemGetSIMInfo(ModemHandle hModem, SIMInfo *info);
pascal OSErr ModemEnterPIN(ModemHandle hModem, ConstStr255Param pin);
pascal OSErr ModemChangePIN(ModemHandle hModem, ConstStr255Param oldPIN, ConstStr255Param newPIN);

/* Data Usage Monitoring */
typedef struct DataUsage {
    long bytesTransmitted;      /* Bytes transmitted */
    long bytesReceived;         /* Bytes received */
    unsigned long sessionTime;  /* Session time */
    long cost;                  /* Cost (if available) */
} DataUsage;

pascal OSErr ModemGetDataUsage(ModemHandle hModem, DataUsage *usage);
pascal OSErr ModemResetDataUsage(ModemHandle hModem);

/* Profile Management */
pascal OSErr ModemSaveProfile(ModemHandle hModem, ConstStr255Param profileName);
pascal OSErr ModemLoadProfile(ModemHandle hModem, ConstStr255Param profileName);
pascal OSErr ModemDeleteProfile(ModemHandle hModem, ConstStr255Param profileName);
pascal OSErr ModemGetProfileList(ModemHandle hModem, Str255 profiles[], short *count);

/* Diagnostics */
pascal OSErr ModemRunSelfTest(ModemHandle hModem, short *result);
pascal OSErr ModemGetLastError(ModemHandle hModem, OSErr *error, Str255 description);
pascal OSErr ModemStartLogging(ModemHandle hModem, ConstStr255Param logFile);
pascal OSErr ModemStopLogging(ModemHandle hModem);

/* Advanced AT Commands */
pascal OSErr ModemQueryCapabilities(ModemHandle hModem, Str255 capabilities);
pascal OSErr ModemSetEchoMode(ModemHandle hModem, Boolean echo);
pascal OSErr ModemSetVerboseMode(ModemHandle hModem, Boolean verbose);
pascal OSErr ModemFactoryReset(ModemHandle hModem);

/* Thread Safety */
pascal OSErr ModemLock(ModemHandle hModem);
pascal OSErr ModemUnlock(ModemHandle hModem);

/* Reference Management */
pascal long ModemGetRefCon(ModemHandle hModem);
pascal void ModemSetRefCon(ModemHandle hModem, long refCon);

/* Constants for various enumerations */

/* Speaker volume levels */
enum {
    speakerVolumeOff = 0,
    speakerVolumeLow = 1,
    speakerVolumeMedium = 2,
    speakerVolumeHigh = 3
};

/* Access technologies */
enum {
    accessTechGSM = 0,
    accessTechGPRS = 1,
    accessTechEDGE = 2,
    accessTechUMTS = 3,
    accessTechHSDPA = 4,
    accessTechLTE = 5
};

/* Registration status */
enum {
    regStatusNotRegistered = 0,
    regStatusRegistered = 1,
    regStatusSearching = 2,
    regStatusDenied = 3,
    regStatusUnknown = 4,
    regStatusRoaming = 5
};

/* SIM status */
enum {
    simStatusReady = 0,
    simStatusPINRequired = 1,
    simStatusPUKRequired = 2,
    simStatusNotInserted = 3,
    simStatusError = 4
};

#ifdef __cplusplus
}
#endif

#endif /* MODEMMANAGER_H */