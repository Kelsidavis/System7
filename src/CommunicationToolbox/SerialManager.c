/*
 * SerialManager.c
 * Serial Port Management Implementation
 *
 * Provides cross-platform serial port management with support for:
 * - Built-in serial ports
 * - USB-to-serial adapters
 * - Bluetooth serial connections
 * - Virtual serial ports
 * - Network serial connections
 *
 * Portable implementation for System 7.1 Communication Toolbox.
 */

#include "SerialManager.h"
#include "CommToolbox.h"
#include "System7.h"
#include "Memory.h"
#include <string.h>
#include <stdlib.h>

#ifdef _WIN32
#include <windows.h>
#elif defined(__unix__) || defined(__APPLE__)
#include <termios.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/select.h>
#include <dirent.h>
#endif

/* Internal structures */
typedef struct SerialPortRec {
    Str255 portName;            /* Port name */
    SerialConfig config;        /* Port configuration */
    short status;               /* Port status */
    short portType;             /* Port type */
    Boolean isOpen;             /* Port open status */
    long refCon;                /* Reference constant */

    /* Platform-specific handles */
#ifdef _WIN32
    HANDLE hSerial;             /* Windows serial handle */
    OVERLAPPED readOverlapped;  /* Overlapped I/O for reading */
    OVERLAPPED writeOverlapped; /* Overlapped I/O for writing */
#elif defined(__unix__) || defined(__APPLE__)
    int fd;                     /* Unix file descriptor */
    struct termios oldTermios;  /* Original terminal settings */
#endif

    /* Callbacks */
    SerialDataReceivedUPP dataCallback;
    SerialErrorUPP errorCallback;
    SerialSignalChangedUPP signalCallback;
    long callbackRefCon;

    /* Statistics */
    SerialStats stats;

    /* Buffer management */
    unsigned char *inputBuffer;
    unsigned char *outputBuffer;
    long inputBufferSize;
    long outputBufferSize;
    long inputCount;
    long outputCount;

    struct SerialPortRec *next; /* Linked list */
} SerialPortRec;

/* Global state */
static SerialPortPtr gPortList = NULL;
static Boolean gSerialManagerInitialized = false;

/* Forward declarations */
static OSErr PlatformOpenPort(SerialPortPtr port);
static OSErr PlatformClosePort(SerialPortPtr port);
static OSErr PlatformConfigurePort(SerialPortPtr port);
static OSErr PlatformRead(SerialPortPtr port, void *buffer, long *count);
static OSErr PlatformWrite(SerialPortPtr port, const void *buffer, long *count);
static OSErr PlatformSetSignals(SerialPortPtr port, long signals);
static OSErr PlatformGetSignals(SerialPortPtr port, long *signals);
static OSErr PlatformFlushBuffers(SerialPortPtr port, Boolean input, Boolean output);
static OSErr EnumeratePlatformPorts(SerialPortInfo ports[], short *count);
static SerialPortPtr FindPortByName(ConstStr255Param portName);
static SerialPortPtr FindPortByHandle(SerialPortHandle hPort);

/*
 * Initialization
 */

pascal OSErr InitSerialManager(void)
{
    if (gSerialManagerInitialized) {
        return noErr;
    }

    gPortList = NULL;
    gSerialManagerInitialized = true;

    return noErr;
}

/*
 * Port Enumeration
 */

pascal OSErr SerialEnumeratePorts(SerialPortInfo ports[], short *count)
{
    OSErr err;
    short platformCount = 0;

    if (ports == NULL || count == NULL) {
        return paramErr;
    }

    /* Get platform-specific ports */
    err = EnumeratePlatformPorts(ports, &platformCount);
    if (err != noErr) {
        *count = 0;
        return err;
    }

    *count = platformCount;
    return noErr;
}

pascal OSErr SerialGetPortInfo(ConstStr255Param portName, SerialPortInfo *info)
{
    SerialPortPtr port;

    if (portName == NULL || info == NULL) {
        return paramErr;
    }

    port = FindPortByName(portName);
    if (port != NULL) {
        /* Fill info from existing port */
        BlockMoveData(port->portName, info->portName, port->portName[0] + 1);
        PLstrcpy(info->description, "\pSerial Port");
        info->portType = port->portType;
        info->status = port->status;
        info->available = !port->isOpen;
        info->vendorID = 0;
        info->productID = 0;
        PLstrcpy(info->driverName, "\pBuilt-in");
        info->currentConfig = port->config;
    } else {
        /* Port not found - return basic info */
        BlockMoveData(portName, info->portName, portName[0] + 1);
        PLstrcpy(info->description, "\pSerial Port");
        info->portType = serialTypeBuiltIn;
        info->status = serialStatusClosed;
        info->available = true;
        info->vendorID = 0;
        info->productID = 0;
        PLstrcpy(info->driverName, "\pBuilt-in");

        /* Default configuration */
        info->currentConfig.baudRate = 9600;
        info->currentConfig.dataBits = dataBits8;
        info->currentConfig.stopBits = stopBits1;
        info->currentConfig.parity = parityNone;
        info->currentConfig.flowControl = flowControlNone;
        info->currentConfig.readTimeout = 5000;
        info->currentConfig.writeTimeout = 5000;
        info->currentConfig.DTR = true;
        info->currentConfig.RTS = true;
        info->currentConfig.breakSignal = false;
        info->currentConfig.bufferSize = 4096;
    }

    return noErr;
}

pascal OSErr SerialRefreshPortList(void)
{
    /* Force re-enumeration of ports */
    return noErr;
}

/*
 * Port Management
 */

pascal OSErr SerialOpenPort(ConstStr255Param portName, const SerialConfig *config,
                           SerialPortHandle *hPort)
{
    SerialPortPtr port;
    OSErr err;

    if (portName == NULL || config == NULL || hPort == NULL) {
        return paramErr;
    }

    /* Check if port is already open */
    port = FindPortByName(portName);
    if (port != NULL && port->isOpen) {
        return portInUse;
    }

    /* Create new port record */
    if (port == NULL) {
        port = (SerialPortPtr)NewPtr(sizeof(SerialPortRec));
        if (port == NULL) {
            return memFullErr;
        }

        /* Initialize port record */
        memset(port, 0, sizeof(SerialPortRec));
        BlockMoveData(portName, port->portName, portName[0] + 1);
        port->portType = serialTypeBuiltIn;
        port->next = gPortList;
        gPortList = port;
    }

    /* Set configuration */
    port->config = *config;
    port->status = serialStatusClosed;
    port->isOpen = false;

    /* Allocate buffers */
    port->inputBufferSize = config->bufferSize;
    port->outputBufferSize = config->bufferSize;
    port->inputBuffer = (unsigned char*)NewPtr(port->inputBufferSize);
    port->outputBuffer = (unsigned char*)NewPtr(port->outputBufferSize);
    if (port->inputBuffer == NULL || port->outputBuffer == NULL) {
        if (port->inputBuffer) DisposePtr((Ptr)port->inputBuffer);
        if (port->outputBuffer) DisposePtr((Ptr)port->outputBuffer);
        return memFullErr;
    }

    /* Open platform-specific port */
    err = PlatformOpenPort(port);
    if (err != noErr) {
        DisposePtr((Ptr)port->inputBuffer);
        DisposePtr((Ptr)port->outputBuffer);
        return err;
    }

    /* Configure the port */
    err = PlatformConfigurePort(port);
    if (err != noErr) {
        PlatformClosePort(port);
        DisposePtr((Ptr)port->inputBuffer);
        DisposePtr((Ptr)port->outputBuffer);
        return err;
    }

    port->isOpen = true;
    port->status = serialStatusOpen;
    *hPort = (SerialPortHandle)port;

    return noErr;
}

pascal OSErr SerialClosePort(SerialPortHandle hPort)
{
    SerialPortPtr port;
    OSErr err;

    port = FindPortByHandle(hPort);
    if (port == NULL) {
        return paramErr;
    }

    if (!port->isOpen) {
        return noErr;
    }

    /* Close platform-specific port */
    err = PlatformClosePort(port);

    /* Free buffers */
    if (port->inputBuffer) {
        DisposePtr((Ptr)port->inputBuffer);
        port->inputBuffer = NULL;
    }
    if (port->outputBuffer) {
        DisposePtr((Ptr)port->outputBuffer);
        port->outputBuffer = NULL;
    }

    port->isOpen = false;
    port->status = serialStatusClosed;

    return err;
}

pascal Boolean SerialIsPortOpen(SerialPortHandle hPort)
{
    SerialPortPtr port = FindPortByHandle(hPort);
    return (port != NULL) ? port->isOpen : false;
}

/*
 * Configuration
 */

pascal OSErr SerialSetConfig(SerialPortHandle hPort, const SerialConfig *config)
{
    SerialPortPtr port;

    port = FindPortByHandle(hPort);
    if (port == NULL || config == NULL) {
        return paramErr;
    }

    port->config = *config;

    if (port->isOpen) {
        return PlatformConfigurePort(port);
    }

    return noErr;
}

pascal OSErr SerialGetConfig(SerialPortHandle hPort, SerialConfig *config)
{
    SerialPortPtr port;

    port = FindPortByHandle(hPort);
    if (port == NULL || config == NULL) {
        return paramErr;
    }

    *config = port->config;
    return noErr;
}

pascal OSErr SerialSetBaudRate(SerialPortHandle hPort, long baudRate)
{
    SerialPortPtr port;

    port = FindPortByHandle(hPort);
    if (port == NULL) {
        return paramErr;
    }

    port->config.baudRate = baudRate;

    if (port->isOpen) {
        return PlatformConfigurePort(port);
    }

    return noErr;
}

pascal OSErr SerialSetDataFormat(SerialPortHandle hPort, short dataBits, short stopBits, short parity)
{
    SerialPortPtr port;

    port = FindPortByHandle(hPort);
    if (port == NULL) {
        return paramErr;
    }

    port->config.dataBits = dataBits;
    port->config.stopBits = stopBits;
    port->config.parity = parity;

    if (port->isOpen) {
        return PlatformConfigurePort(port);
    }

    return noErr;
}

pascal OSErr SerialSetFlowControl(SerialPortHandle hPort, short flowControl)
{
    SerialPortPtr port;

    port = FindPortByHandle(hPort);
    if (port == NULL) {
        return paramErr;
    }

    port->config.flowControl = flowControl;

    if (port->isOpen) {
        return PlatformConfigurePort(port);
    }

    return noErr;
}

pascal OSErr SerialSetTimeouts(SerialPortHandle hPort, long readTimeout, long writeTimeout)
{
    SerialPortPtr port;

    port = FindPortByHandle(hPort);
    if (port == NULL) {
        return paramErr;
    }

    port->config.readTimeout = readTimeout;
    port->config.writeTimeout = writeTimeout;

    return noErr;
}

/*
 * Data Transfer
 */

pascal OSErr SerialRead(SerialPortHandle hPort, void *buffer, long *count)
{
    SerialPortPtr port;

    port = FindPortByHandle(hPort);
    if (port == NULL || buffer == NULL || count == NULL) {
        return paramErr;
    }

    if (!port->isOpen) {
        return portClosedErr;
    }

    return PlatformRead(port, buffer, count);
}

pascal OSErr SerialWrite(SerialPortHandle hPort, const void *buffer, long *count)
{
    SerialPortPtr port;

    port = FindPortByHandle(hPort);
    if (port == NULL || buffer == NULL || count == NULL) {
        return paramErr;
    }

    if (!port->isOpen) {
        return portClosedErr;
    }

    return PlatformWrite(port, buffer, count);
}

pascal OSErr SerialReadAsync(SerialPortHandle hPort, void *buffer, long count,
                            SerialDataReceivedUPP callback, long refCon)
{
    /* Simplified async implementation - would use threads in full version */
    long actualCount = count;
    OSErr err = SerialRead(hPort, buffer, &actualCount);

    if (callback != NULL) {
        callback(hPort, buffer, actualCount, refCon);
    }

    return err;
}

pascal OSErr SerialWriteAsync(SerialPortHandle hPort, const void *buffer, long count,
                             SerialDataReceivedUPP callback, long refCon)
{
    /* Simplified async implementation - would use threads in full version */
    long actualCount = count;
    OSErr err = SerialWrite(hPort, buffer, &actualCount);

    if (callback != NULL) {
        callback(hPort, (void*)buffer, actualCount, refCon);
    }

    return err;
}

/*
 * Buffer Management
 */

pascal OSErr SerialFlushInput(SerialPortHandle hPort)
{
    SerialPortPtr port;

    port = FindPortByHandle(hPort);
    if (port == NULL) {
        return paramErr;
    }

    if (!port->isOpen) {
        return portClosedErr;
    }

    port->inputCount = 0;
    return PlatformFlushBuffers(port, true, false);
}

pascal OSErr SerialFlushOutput(SerialPortHandle hPort)
{
    SerialPortPtr port;

    port = FindPortByHandle(hPort);
    if (port == NULL) {
        return paramErr;
    }

    if (!port->isOpen) {
        return portClosedErr;
    }

    port->outputCount = 0;
    return PlatformFlushBuffers(port, false, true);
}

pascal OSErr SerialGetInputCount(SerialPortHandle hPort, long *count)
{
    SerialPortPtr port;

    port = FindPortByHandle(hPort);
    if (port == NULL || count == NULL) {
        return paramErr;
    }

    *count = port->inputCount;
    return noErr;
}

pascal OSErr SerialGetOutputCount(SerialPortHandle hPort, long *count)
{
    SerialPortPtr port;

    port = FindPortByHandle(hPort);
    if (port == NULL || count == NULL) {
        return paramErr;
    }

    *count = port->outputCount;
    return noErr;
}

pascal OSErr SerialSetBufferSizes(SerialPortHandle hPort, long inputSize, long outputSize)
{
    SerialPortPtr port;

    port = FindPortByHandle(hPort);
    if (port == NULL) {
        return paramErr;
    }

    /* Can only change buffer sizes when port is closed */
    if (port->isOpen) {
        return portInUse;
    }

    port->config.bufferSize = inputSize;  /* Store primary buffer size */
    return noErr;
}

/*
 * Signal Control
 */

pascal OSErr SerialSetDTR(SerialPortHandle hPort, Boolean state)
{
    SerialPortPtr port;
    long signals;
    OSErr err;

    port = FindPortByHandle(hPort);
    if (port == NULL) {
        return paramErr;
    }

    if (!port->isOpen) {
        return portClosedErr;
    }

    err = PlatformGetSignals(port, &signals);
    if (err != noErr) {
        return err;
    }

    if (state) {
        signals |= serialSignalDTR;
    } else {
        signals &= ~serialSignalDTR;
    }

    port->config.DTR = state;
    return PlatformSetSignals(port, signals);
}

pascal OSErr SerialSetRTS(SerialPortHandle hPort, Boolean state)
{
    SerialPortPtr port;
    long signals;
    OSErr err;

    port = FindPortByHandle(hPort);
    if (port == NULL) {
        return paramErr;
    }

    if (!port->isOpen) {
        return portClosedErr;
    }

    err = PlatformGetSignals(port, &signals);
    if (err != noErr) {
        return err;
    }

    if (state) {
        signals |= serialSignalRTS;
    } else {
        signals &= ~serialSignalRTS;
    }

    port->config.RTS = state;
    return PlatformSetSignals(port, signals);
}

pascal OSErr SerialSetBreak(SerialPortHandle hPort, Boolean state)
{
    SerialPortPtr port;

    port = FindPortByHandle(hPort);
    if (port == NULL) {
        return paramErr;
    }

    if (!port->isOpen) {
        return portClosedErr;
    }

    port->config.breakSignal = state;
    /* Platform-specific break signal implementation would go here */
    return noErr;
}

pascal OSErr SerialGetSignals(SerialPortHandle hPort, long *signals)
{
    SerialPortPtr port;

    port = FindPortByHandle(hPort);
    if (port == NULL || signals == NULL) {
        return paramErr;
    }

    if (!port->isOpen) {
        return portClosedErr;
    }

    return PlatformGetSignals(port, signals);
}

/*
 * Status and Statistics
 */

pascal OSErr SerialGetStatus(SerialPortHandle hPort, short *status)
{
    SerialPortPtr port;

    port = FindPortByHandle(hPort);
    if (port == NULL || status == NULL) {
        return paramErr;
    }

    *status = port->status;
    return noErr;
}

pascal OSErr SerialGetStats(SerialPortHandle hPort, SerialStats *stats)
{
    SerialPortPtr port;

    port = FindPortByHandle(hPort);
    if (port == NULL || stats == NULL) {
        return paramErr;
    }

    *stats = port->stats;
    return noErr;
}

pascal OSErr SerialResetStats(SerialPortHandle hPort)
{
    SerialPortPtr port;

    port = FindPortByHandle(hPort);
    if (port == NULL) {
        return paramErr;
    }

    memset(&port->stats, 0, sizeof(SerialStats));
    return noErr;
}

/*
 * Event Handling
 */

pascal OSErr SerialSetCallbacks(SerialPortHandle hPort, SerialDataReceivedUPP dataCallback,
                               SerialErrorUPP errorCallback, SerialSignalChangedUPP signalCallback,
                               long refCon)
{
    SerialPortPtr port;

    port = FindPortByHandle(hPort);
    if (port == NULL) {
        return paramErr;
    }

    port->dataCallback = dataCallback;
    port->errorCallback = errorCallback;
    port->signalCallback = signalCallback;
    port->callbackRefCon = refCon;

    return noErr;
}

pascal OSErr SerialProcessEvents(SerialPortHandle hPort)
{
    /* Process pending events - simplified implementation */
    return noErr;
}

/*
 * Reference Management
 */

pascal long SerialGetRefCon(SerialPortHandle hPort)
{
    SerialPortPtr port = FindPortByHandle(hPort);
    return (port != NULL) ? port->refCon : 0;
}

pascal void SerialSetRefCon(SerialPortHandle hPort, long refCon)
{
    SerialPortPtr port = FindPortByHandle(hPort);
    if (port != NULL) {
        port->refCon = refCon;
    }
}

/*
 * Internal Helper Functions
 */

static SerialPortPtr FindPortByName(ConstStr255Param portName)
{
    SerialPortPtr port = gPortList;

    while (port != NULL) {
        if (EqualString(port->portName, portName, false, true)) {
            return port;
        }
        port = port->next;
    }

    return NULL;
}

static SerialPortPtr FindPortByHandle(SerialPortHandle hPort)
{
    return (SerialPortPtr)hPort;
}

/*
 * Platform-Specific Implementations
 */

#ifdef _WIN32

static OSErr EnumeratePlatformPorts(SerialPortInfo ports[], short *count)
{
    short portCount = 0;
    short maxPorts = *count;

    /* Enumerate COM ports on Windows */
    for (int i = 1; i <= 32 && portCount < maxPorts; i++) {
        char portName[32];
        sprintf(portName, "COM%d", i);

        HANDLE hSerial = CreateFileA(portName, GENERIC_READ | GENERIC_WRITE,
                                    0, NULL, OPEN_EXISTING, 0, NULL);

        if (hSerial != INVALID_HANDLE_VALUE) {
            CloseHandle(hSerial);

            /* Convert to Pascal string */
            c2pstrcpy(ports[portCount].portName, portName);
            sprintf(portName, "Serial Port COM%d", i);
            c2pstrcpy(ports[portCount].description, portName);
            ports[portCount].portType = serialTypeBuiltIn;
            ports[portCount].status = serialStatusClosed;
            ports[portCount].available = true;
            ports[portCount].vendorID = 0;
            ports[portCount].productID = 0;
            c2pstrcpy(ports[portCount].driverName, "Built-in");

            portCount++;
        }
    }

    *count = portCount;
    return noErr;
}

static OSErr PlatformOpenPort(SerialPortPtr port)
{
    char portName[256];

    p2cstrcpy(portName, port->portName);

    port->hSerial = CreateFileA(portName, GENERIC_READ | GENERIC_WRITE,
                               0, NULL, OPEN_EXISTING,
                               FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, NULL);

    if (port->hSerial == INVALID_HANDLE_VALUE) {
        return ioErr;
    }

    /* Set up overlapped I/O */
    memset(&port->readOverlapped, 0, sizeof(OVERLAPPED));
    memset(&port->writeOverlapped, 0, sizeof(OVERLAPPED));

    port->readOverlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    port->writeOverlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

    return noErr;
}

static OSErr PlatformClosePort(SerialPortPtr port)
{
    if (port->hSerial != INVALID_HANDLE_VALUE) {
        CloseHandle(port->hSerial);
        port->hSerial = INVALID_HANDLE_VALUE;
    }

    if (port->readOverlapped.hEvent) {
        CloseHandle(port->readOverlapped.hEvent);
        port->readOverlapped.hEvent = NULL;
    }

    if (port->writeOverlapped.hEvent) {
        CloseHandle(port->writeOverlapped.hEvent);
        port->writeOverlapped.hEvent = NULL;
    }

    return noErr;
}

static OSErr PlatformConfigurePort(SerialPortPtr port)
{
    DCB dcb;
    COMMTIMEOUTS timeouts;

    if (!GetCommState(port->hSerial, &dcb)) {
        return ioErr;
    }

    dcb.BaudRate = port->config.baudRate;
    dcb.ByteSize = port->config.dataBits;
    dcb.StopBits = (port->config.stopBits == stopBits1) ? ONESTOPBIT : TWOSTOPBITS;

    switch (port->config.parity) {
        case parityNone:
            dcb.Parity = NOPARITY;
            break;
        case parityOdd:
            dcb.Parity = ODDPARITY;
            break;
        case parityEven:
            dcb.Parity = EVENPARITY;
            break;
    }

    switch (port->config.flowControl) {
        case flowControlNone:
            dcb.fOutxCtsFlow = FALSE;
            dcb.fRtsControl = RTS_CONTROL_ENABLE;
            break;
        case flowControlRtsCts:
            dcb.fOutxCtsFlow = TRUE;
            dcb.fRtsControl = RTS_CONTROL_HANDSHAKE;
            break;
    }

    if (!SetCommState(port->hSerial, &dcb)) {
        return ioErr;
    }

    /* Set timeouts */
    timeouts.ReadIntervalTimeout = MAXDWORD;
    timeouts.ReadTotalTimeoutMultiplier = 0;
    timeouts.ReadTotalTimeoutConstant = port->config.readTimeout;
    timeouts.WriteTotalTimeoutMultiplier = 0;
    timeouts.WriteTotalTimeoutConstant = port->config.writeTimeout;

    if (!SetCommTimeouts(port->hSerial, &timeouts)) {
        return ioErr;
    }

    return noErr;
}

static OSErr PlatformRead(SerialPortPtr port, void *buffer, long *count)
{
    DWORD bytesRead;

    if (!ReadFile(port->hSerial, buffer, *count, &bytesRead, &port->readOverlapped)) {
        if (GetLastError() == ERROR_IO_PENDING) {
            if (!GetOverlappedResult(port->hSerial, &port->readOverlapped, &bytesRead, TRUE)) {
                *count = 0;
                return ioErr;
            }
        } else {
            *count = 0;
            return ioErr;
        }
    }

    *count = bytesRead;
    port->stats.bytesRead += bytesRead;
    return noErr;
}

static OSErr PlatformWrite(SerialPortPtr port, const void *buffer, long *count)
{
    DWORD bytesWritten;

    if (!WriteFile(port->hSerial, buffer, *count, &bytesWritten, &port->writeOverlapped)) {
        if (GetLastError() == ERROR_IO_PENDING) {
            if (!GetOverlappedResult(port->hSerial, &port->writeOverlapped, &bytesWritten, TRUE)) {
                *count = 0;
                return ioErr;
            }
        } else {
            *count = 0;
            return ioErr;
        }
    }

    *count = bytesWritten;
    port->stats.bytesWritten += bytesWritten;
    return noErr;
}

static OSErr PlatformSetSignals(SerialPortPtr port, long signals)
{
    DWORD modemControl = 0;

    if (signals & serialSignalDTR) {
        modemControl |= SETDTR;
    } else {
        modemControl |= CLRDTR;
    }

    if (signals & serialSignalRTS) {
        modemControl |= SETRTS;
    } else {
        modemControl |= CLRRTS;
    }

    if (!EscapeCommFunction(port->hSerial, modemControl)) {
        return ioErr;
    }

    return noErr;
}

static OSErr PlatformGetSignals(SerialPortPtr port, long *signals)
{
    DWORD modemStatus;

    if (!GetCommModemStatus(port->hSerial, &modemStatus)) {
        return ioErr;
    }

    *signals = 0;
    if (modemStatus & MS_CTS_ON) *signals |= serialSignalCTS;
    if (modemStatus & MS_DSR_ON) *signals |= serialSignalDSR;
    if (modemStatus & MS_RLSD_ON) *signals |= serialSignalDCD;
    if (modemStatus & MS_RING_ON) *signals |= serialSignalRI;

    return noErr;
}

static OSErr PlatformFlushBuffers(SerialPortPtr port, Boolean input, Boolean output)
{
    DWORD flags = 0;

    if (input) flags |= PURGE_RXCLEAR;
    if (output) flags |= PURGE_TXCLEAR;

    if (!PurgeComm(port->hSerial, flags)) {
        return ioErr;
    }

    return noErr;
}

#elif defined(__unix__) || defined(__APPLE__)

static OSErr EnumeratePlatformPorts(SerialPortInfo ports[], short *count)
{
    short portCount = 0;
    short maxPorts = *count;
    DIR *dir;
    struct dirent *entry;

    /* Look for serial devices in /dev */
    dir = opendir("/dev");
    if (dir == NULL) {
        *count = 0;
        return ioErr;
    }

    while ((entry = readdir(dir)) != NULL && portCount < maxPorts) {
        if (strncmp(entry->d_name, "tty", 3) == 0 ||
            strncmp(entry->d_name, "cu.", 3) == 0) {

            char fullPath[512];
            sprintf(fullPath, "/dev/%s", entry->d_name);

            /* Try to open the device to see if it's accessible */
            int fd = open(fullPath, O_RDWR | O_NOCTTY | O_NONBLOCK);
            if (fd >= 0) {
                close(fd);

                c2pstrcpy(ports[portCount].portName, fullPath);
                c2pstrcpy(ports[portCount].description, entry->d_name);
                ports[portCount].portType = serialTypeBuiltIn;
                ports[portCount].status = serialStatusClosed;
                ports[portCount].available = true;
                ports[portCount].vendorID = 0;
                ports[portCount].productID = 0;
                c2pstrcpy(ports[portCount].driverName, "Built-in");

                portCount++;
            }
        }
    }

    closedir(dir);
    *count = portCount;
    return noErr;
}

static OSErr PlatformOpenPort(SerialPortPtr port)
{
    char portName[256];

    p2cstrcpy(portName, port->portName);

    port->fd = open(portName, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (port->fd < 0) {
        return ioErr;
    }

    /* Save original terminal settings */
    if (tcgetattr(port->fd, &port->oldTermios) != 0) {
        close(port->fd);
        port->fd = -1;
        return ioErr;
    }

    return noErr;
}

static OSErr PlatformClosePort(SerialPortPtr port)
{
    if (port->fd >= 0) {
        /* Restore original settings */
        tcsetattr(port->fd, TCSANOW, &port->oldTermios);
        close(port->fd);
        port->fd = -1;
    }

    return noErr;
}

static OSErr PlatformConfigurePort(SerialPortPtr port)
{
    struct termios tty;

    if (tcgetattr(port->fd, &tty) != 0) {
        return ioErr;
    }

    /* Configure baud rate */
    speed_t speed;
    switch (port->config.baudRate) {
        case 9600: speed = B9600; break;
        case 19200: speed = B19200; break;
        case 38400: speed = B38400; break;
        case 57600: speed = B57600; break;
        case 115200: speed = B115200; break;
        default: speed = B9600; break;
    }

    cfsetispeed(&tty, speed);
    cfsetospeed(&tty, speed);

    /* Configure data bits */
    tty.c_cflag &= ~CSIZE;
    switch (port->config.dataBits) {
        case 5: tty.c_cflag |= CS5; break;
        case 6: tty.c_cflag |= CS6; break;
        case 7: tty.c_cflag |= CS7; break;
        case 8: tty.c_cflag |= CS8; break;
    }

    /* Configure stop bits */
    if (port->config.stopBits == stopBits2) {
        tty.c_cflag |= CSTOPB;
    } else {
        tty.c_cflag &= ~CSTOPB;
    }

    /* Configure parity */
    switch (port->config.parity) {
        case parityNone:
            tty.c_cflag &= ~PARENB;
            break;
        case parityOdd:
            tty.c_cflag |= PARENB | PARODD;
            break;
        case parityEven:
            tty.c_cflag |= PARENB;
            tty.c_cflag &= ~PARODD;
            break;
    }

    /* Configure flow control */
    switch (port->config.flowControl) {
        case flowControlNone:
            tty.c_cflag &= ~CRTSCTS;
            tty.c_iflag &= ~(IXON | IXOFF | IXANY);
            break;
        case flowControlRtsCts:
            tty.c_cflag |= CRTSCTS;
            break;
        case flowControlXonXoff:
            tty.c_iflag |= (IXON | IXOFF | IXANY);
            break;
    }

    /* Raw mode */
    tty.c_cflag |= (CLOCAL | CREAD);
    tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    tty.c_iflag &= ~(INLCR | IGNCR | ICRNL);
    tty.c_oflag &= ~OPOST;

    /* Set timeouts */
    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = port->config.readTimeout / 100;  /* Convert ms to tenths of seconds */

    if (tcsetattr(port->fd, TCSANOW, &tty) != 0) {
        return ioErr;
    }

    return noErr;
}

static OSErr PlatformRead(SerialPortPtr port, void *buffer, long *count)
{
    ssize_t bytesRead = read(port->fd, buffer, *count);

    if (bytesRead < 0) {
        *count = 0;
        return ioErr;
    }

    *count = bytesRead;
    port->stats.bytesRead += bytesRead;
    return noErr;
}

static OSErr PlatformWrite(SerialPortPtr port, const void *buffer, long *count)
{
    ssize_t bytesWritten = write(port->fd, buffer, *count);

    if (bytesWritten < 0) {
        *count = 0;
        return ioErr;
    }

    *count = bytesWritten;
    port->stats.bytesWritten += bytesWritten;
    return noErr;
}

static OSErr PlatformSetSignals(SerialPortPtr port, long signals)
{
    int modemBits;

    if (ioctl(port->fd, TIOCMGET, &modemBits) != 0) {
        return ioErr;
    }

    if (signals & serialSignalDTR) {
        modemBits |= TIOCM_DTR;
    } else {
        modemBits &= ~TIOCM_DTR;
    }

    if (signals & serialSignalRTS) {
        modemBits |= TIOCM_RTS;
    } else {
        modemBits &= ~TIOCM_RTS;
    }

    if (ioctl(port->fd, TIOCMSET, &modemBits) != 0) {
        return ioErr;
    }

    return noErr;
}

static OSErr PlatformGetSignals(SerialPortPtr port, long *signals)
{
    int modemBits;

    if (ioctl(port->fd, TIOCMGET, &modemBits) != 0) {
        return ioErr;
    }

    *signals = 0;
    if (modemBits & TIOCM_DTR) *signals |= serialSignalDTR;
    if (modemBits & TIOCM_RTS) *signals |= serialSignalRTS;
    if (modemBits & TIOCM_CTS) *signals |= serialSignalCTS;
    if (modemBits & TIOCM_DSR) *signals |= serialSignalDSR;
    if (modemBits & TIOCM_CD) *signals |= serialSignalDCD;
    if (modemBits & TIOCM_RI) *signals |= serialSignalRI;

    return noErr;
}

static OSErr PlatformFlushBuffers(SerialPortPtr port, Boolean input, Boolean output)
{
    int queue = 0;

    if (input && output) {
        queue = TCIOFLUSH;
    } else if (input) {
        queue = TCIFLUSH;
    } else if (output) {
        queue = TCOFLUSH;
    }

    if (tcflush(port->fd, queue) != 0) {
        return ioErr;
    }

    return noErr;
}

#else

/* Placeholder implementations for unsupported platforms */
static OSErr EnumeratePlatformPorts(SerialPortInfo ports[], short *count)
{
    *count = 0;
    return noErr;
}

static OSErr PlatformOpenPort(SerialPortPtr port) { return noErr; }
static OSErr PlatformClosePort(SerialPortPtr port) { return noErr; }
static OSErr PlatformConfigurePort(SerialPortPtr port) { return noErr; }
static OSErr PlatformRead(SerialPortPtr port, void *buffer, long *count) { *count = 0; return noErr; }
static OSErr PlatformWrite(SerialPortPtr port, const void *buffer, long *count) { *count = 0; return noErr; }
static OSErr PlatformSetSignals(SerialPortPtr port, long signals) { return noErr; }
static OSErr PlatformGetSignals(SerialPortPtr port, long *signals) { *signals = 0; return noErr; }
static OSErr PlatformFlushBuffers(SerialPortPtr port, Boolean input, Boolean output) { return noErr; }

#endif