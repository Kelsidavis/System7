/*
 * SerialManager.h
 * Serial Port Management API - Portable System 7.1 Implementation
 *
 * Provides cross-platform serial port management with support for
 * modern USB-to-serial adapters, Bluetooth serial, and virtual serial ports.
 *
 * Extends Mac OS Communication Toolbox with modern serial functionality.
 */

#ifndef SERIALMANAGER_H
#define SERIALMANAGER_H

#include "CommToolbox.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Serial Manager Version */
#define SERIAL_MANAGER_VERSION 1

/* Serial Port Types */
enum {
    serialTypeBuiltIn = 0,      /* Built-in serial port */
    serialTypeUSB = 1,          /* USB-to-serial adapter */
    serialTypeBluetooth = 2,    /* Bluetooth serial */
    serialTypeVirtual = 3,      /* Virtual serial port */
    serialTypeNetwork = 4       /* Network serial (TCP/Telnet) */
};

/* Serial Port Status */
enum {
    serialStatusClosed = 0,
    serialStatusOpen = 1,
    serialStatusError = 2,
    serialStatusBusy = 3
};

/* Flow Control Types */
enum {
    flowControlNone = 0,
    flowControlXonXoff = 1,
    flowControlRtsCts = 2,
    flowControlDtrDsr = 3
};

/* Parity Types */
enum {
    parityNone = 0,
    parityOdd = 1,
    parityEven = 2,
    parityMark = 3,
    paritySpace = 4
};

/* Stop Bits */
enum {
    stopBits1 = 1,
    stopBits1_5 = 2,
    stopBits2 = 3
};

/* Data Bits */
enum {
    dataBits5 = 5,
    dataBits6 = 6,
    dataBits7 = 7,
    dataBits8 = 8
};

/* Serial Port Configuration */
typedef struct SerialConfig {
    long baudRate;              /* Baud rate */
    short dataBits;             /* Data bits */
    short stopBits;             /* Stop bits */
    short parity;               /* Parity */
    short flowControl;          /* Flow control */
    long readTimeout;           /* Read timeout (ms) */
    long writeTimeout;          /* Write timeout (ms) */
    Boolean DTR;                /* DTR signal */
    Boolean RTS;                /* RTS signal */
    Boolean breakSignal;        /* Break signal */
    long bufferSize;            /* Buffer size */
} SerialConfig;

/* Serial Port Information */
typedef struct SerialPortInfo {
    Str255 portName;            /* Port name */
    Str255 description;         /* Port description */
    short portType;             /* Port type */
    short status;               /* Port status */
    Boolean available;          /* Port available */
    long vendorID;              /* Vendor ID (for USB) */
    long productID;             /* Product ID (for USB) */
    Str255 driverName;          /* Driver name */
    SerialConfig currentConfig; /* Current configuration */
} SerialPortInfo;

/* Serial Port Statistics */
typedef struct SerialStats {
    long bytesRead;             /* Bytes read */
    long bytesWritten;          /* Bytes written */
    long readErrors;            /* Read errors */
    long writeErrors;           /* Write errors */
    long framing errors;         /* Framing errors */
    long parityErrors;          /* Parity errors */
    long overrunErrors;         /* Overrun errors */
    long bufferOverflows;       /* Buffer overflows */
    Boolean CTS;                /* CTS signal state */
    Boolean DSR;                /* DSR signal state */
    Boolean DCD;                /* DCD signal state */
    Boolean RI;                 /* RI signal state */
} SerialStats;

/* Serial Port Handle */
typedef struct SerialPortRec *SerialPortPtr, **SerialPortHandle;

/* Callback procedures */
typedef pascal void (*SerialDataReceivedUPP)(SerialPortHandle hPort, void *data, long size, long refCon);
typedef pascal void (*SerialErrorUPP)(SerialPortHandle hPort, OSErr error, long refCon);
typedef pascal void (*SerialSignalChangedUPP)(SerialPortHandle hPort, long signals, long refCon);

#if GENERATINGCFM
pascal SerialDataReceivedUPP NewSerialDataReceivedProc(SerialDataReceivedProcPtr userRoutine);
pascal SerialErrorUPP NewSerialErrorProc(SerialErrorProcPtr userRoutine);
pascal SerialSignalChangedUPP NewSerialSignalChangedProc(SerialSignalChangedProcPtr userRoutine);
pascal void DisposeSerialDataReceivedUPP(SerialDataReceivedUPP userUPP);
pascal void DisposeSerialErrorUPP(SerialErrorUPP userUPP);
pascal void DisposeSerialSignalChangedUPP(SerialSignalChangedUPP userUPP);
#else
#define NewSerialDataReceivedProc(userRoutine) (userRoutine)
#define NewSerialErrorProc(userRoutine) (userRoutine)
#define NewSerialSignalChangedProc(userRoutine) (userRoutine)
#define DisposeSerialDataReceivedUPP(userUPP)
#define DisposeSerialErrorUPP(userUPP)
#define DisposeSerialSignalChangedUPP(userUPP)
#endif

/* Serial Manager API */

/* Initialization */
pascal OSErr InitSerialManager(void);

/* Port Enumeration */
pascal OSErr SerialEnumeratePorts(SerialPortInfo ports[], short *count);
pascal OSErr SerialGetPortInfo(ConstStr255Param portName, SerialPortInfo *info);
pascal OSErr SerialRefreshPortList(void);

/* Port Management */
pascal OSErr SerialOpenPort(ConstStr255Param portName, const SerialConfig *config,
                           SerialPortHandle *hPort);
pascal OSErr SerialClosePort(SerialPortHandle hPort);
pascal Boolean SerialIsPortOpen(SerialPortHandle hPort);

/* Configuration */
pascal OSErr SerialSetConfig(SerialPortHandle hPort, const SerialConfig *config);
pascal OSErr SerialGetConfig(SerialPortHandle hPort, SerialConfig *config);
pascal OSErr SerialSetBaudRate(SerialPortHandle hPort, long baudRate);
pascal OSErr SerialSetDataFormat(SerialPortHandle hPort, short dataBits, short stopBits, short parity);
pascal OSErr SerialSetFlowControl(SerialPortHandle hPort, short flowControl);
pascal OSErr SerialSetTimeouts(SerialPortHandle hPort, long readTimeout, long writeTimeout);

/* Data Transfer */
pascal OSErr SerialRead(SerialPortHandle hPort, void *buffer, long *count);
pascal OSErr SerialWrite(SerialPortHandle hPort, const void *buffer, long *count);
pascal OSErr SerialReadAsync(SerialPortHandle hPort, void *buffer, long count,
                            SerialDataReceivedUPP callback, long refCon);
pascal OSErr SerialWriteAsync(SerialPortHandle hPort, const void *buffer, long count,
                             SerialDataReceivedUPP callback, long refCon);

/* Buffer Management */
pascal OSErr SerialFlushInput(SerialPortHandle hPort);
pascal OSErr SerialFlushOutput(SerialPortHandle hPort);
pascal OSErr SerialGetInputCount(SerialPortHandle hPort, long *count);
pascal OSErr SerialGetOutputCount(SerialPortHandle hPort, long *count);
pascal OSErr SerialSetBufferSizes(SerialPortHandle hPort, long inputSize, long outputSize);

/* Signal Control */
pascal OSErr SerialSetDTR(SerialPortHandle hPort, Boolean state);
pascal OSErr SerialSetRTS(SerialPortHandle hPort, Boolean state);
pascal OSErr SerialSetBreak(SerialPortHandle hPort, Boolean state);
pascal OSErr SerialGetSignals(SerialPortHandle hPort, long *signals);

/* Status and Statistics */
pascal OSErr SerialGetStatus(SerialPortHandle hPort, short *status);
pascal OSErr SerialGetStats(SerialPortHandle hPort, SerialStats *stats);
pascal OSErr SerialResetStats(SerialPortHandle hPort);

/* Event Handling */
pascal OSErr SerialSetCallbacks(SerialPortHandle hPort, SerialDataReceivedUPP dataCallback,
                               SerialErrorUPP errorCallback, SerialSignalChangedUPP signalCallback,
                               long refCon);
pascal OSErr SerialProcessEvents(SerialPortHandle hPort);

/* Modern Extensions */

/* USB Serial Support */
typedef struct USBSerialInfo {
    long vendorID;              /* USB Vendor ID */
    long productID;             /* USB Product ID */
    Str255 manufacturer;        /* Manufacturer name */
    Str255 product;             /* Product name */
    Str255 serialNumber;        /* Serial number */
    short interface;            /* Interface number */
} USBSerialInfo;

pascal OSErr SerialGetUSBInfo(SerialPortHandle hPort, USBSerialInfo *info);
pascal OSErr SerialEnumerateUSBPorts(USBSerialInfo ports[], short *count);

/* Bluetooth Serial Support */
typedef struct BluetoothSerialInfo {
    Str255 deviceName;          /* Bluetooth device name */
    unsigned char address[6];   /* Bluetooth address */
    short channel;              /* RFCOMM channel */
    Boolean paired;             /* Device paired */
    short signalStrength;       /* Signal strength */
} BluetoothSerialInfo;

pascal OSErr SerialGetBluetoothInfo(SerialPortHandle hPort, BluetoothSerialInfo *info);
pascal OSErr SerialEnumerateBluetoothPorts(BluetoothSerialInfo ports[], short *count);
pascal OSErr SerialPairBluetoothDevice(const BluetoothSerialInfo *info);

/* Virtual Serial Port Support */
typedef struct VirtualSerialConfig {
    Str255 peerName;            /* Peer port name */
    Boolean bidirectional;      /* Bidirectional connection */
    long bufferSize;            /* Buffer size */
} VirtualSerialConfig;

pascal OSErr SerialCreateVirtualPort(ConstStr255Param portName, const VirtualSerialConfig *config);
pascal OSErr SerialDestroyVirtualPort(ConstStr255Param portName);
pascal OSErr SerialConnectVirtualPorts(ConstStr255Param port1, ConstStr255Param port2);

/* Network Serial Support */
typedef struct NetworkSerialConfig {
    Str255 hostname;            /* Remote hostname */
    long port;                  /* Remote port */
    short protocol;             /* Protocol (TCP, Telnet, SSH) */
    Str255 username;            /* Username (for SSH) */
    Str255 password;            /* Password (for SSH) */
    Boolean useSSL;             /* Use SSL/TLS */
} NetworkSerialConfig;

enum {
    netSerialTCP = 1,
    netSerialTelnet = 2,
    netSerialSSH = 3
};

pascal OSErr SerialOpenNetworkPort(const NetworkSerialConfig *config, SerialPortHandle *hPort);

/* Advanced Features */
pascal OSErr SerialSetCustomBaudRate(SerialPortHandle hPort, long baudRate);
pascal OSErr SerialGetSupportedBaudRates(SerialPortHandle hPort, long rates[], short *count);
pascal OSErr SerialSetLineCoding(SerialPortHandle hPort, const SerialConfig *config);
pascal OSErr SerialGetLineCoding(SerialPortHandle hPort, SerialConfig *config);

/* Error Recovery */
pascal OSErr SerialRecoverFromError(SerialPortHandle hPort);
pascal OSErr SerialGetLastError(SerialPortHandle hPort, OSErr *error, Str255 description);

/* Power Management */
pascal OSErr SerialSetPowerState(SerialPortHandle hPort, Boolean active);
pascal OSErr SerialGetPowerState(SerialPortHandle hPort, Boolean *active);

/* Thread Safety */
pascal OSErr SerialLockPort(SerialPortHandle hPort);
pascal OSErr SerialUnlockPort(SerialPortHandle hPort);

/* Debugging and Diagnostics */
pascal OSErr SerialStartLogging(SerialPortHandle hPort, ConstStr255Param logFile);
pascal OSErr SerialStopLogging(SerialPortHandle hPort);
pascal OSErr SerialRunDiagnostics(SerialPortHandle hPort, short *result);

/* Port Reference Management */
pascal long SerialGetRefCon(SerialPortHandle hPort);
pascal void SerialSetRefCon(SerialPortHandle hPort, long refCon);

/* Signal definitions for GetSignals/SetSignals */
enum {
    serialSignalDTR = 1 << 0,
    serialSignalRTS = 1 << 1,
    serialSignalCTS = 1 << 2,
    serialSignalDSR = 1 << 3,
    serialSignalDCD = 1 << 4,
    serialSignalRI = 1 << 5,
    serialSignalBreak = 1 << 6
};

#ifdef __cplusplus
}
#endif

#endif /* SERIALMANAGER_H */