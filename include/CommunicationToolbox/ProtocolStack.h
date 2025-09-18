/*
 * ProtocolStack.h
 * Protocol Stack API - Portable System 7.1 Implementation
 *
 * Provides communication protocol implementation including TCP/IP,
 * SSH, Telnet, and other network protocols for modern connectivity.
 *
 * Extends Mac OS Communication Toolbox with modern protocol support.
 */

#ifndef PROTOCOLSTACK_H
#define PROTOCOLSTACK_H

#include "CommToolbox.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Protocol Stack Version */
#define PROTOCOL_STACK_VERSION 1

/* Protocol Types */
enum {
    protocolTypeSerial = 0,     /* Raw serial protocol */
    protocolTypeTCP = 1,        /* TCP/IP protocol */
    protocolTypeUDP = 2,        /* UDP protocol */
    protocolTypeTelnet = 3,     /* Telnet protocol */
    protocolTypeSSH = 4,        /* SSH protocol */
    protocolTypeHTTP = 5,       /* HTTP protocol */
    protocolTypeHTTPS = 6,      /* HTTPS protocol */
    protocolTypeFTP = 7,        /* FTP protocol */
    protocolTypeSFTP = 8,       /* SFTP protocol */
    protocolTypeSCP = 9,        /* SCP protocol */
    protocolTypeModbus = 10,    /* Modbus protocol */
    protocolTypeMQTT = 11,      /* MQTT protocol */
    protocolTypeCustom = 99     /* Custom protocol */
};

/* Protocol Status */
enum {
    protocolStatusDisconnected = 0,
    protocolStatusConnecting = 1,
    protocolStatusConnected = 2,
    protocolStatusDisconnecting = 3,
    protocolStatusError = 4,
    protocolStatusListening = 5
};

/* Protocol Errors */
enum {
    protocolNoErr = 0,
    protocolGenericError = -1,
    protocolTimeout = -2,
    protocolConnectionRefused = -3,
    protocolHostUnreachable = -4,
    protocolAuthenticationFailed = -5,
    protocolProtocolError = -6,
    protocolNotSupported = -7,
    protocolBufferFull = -8,
    protocolInvalidParameter = -9
};

/* Address Family Types */
enum {
    addressFamilyIPv4 = 1,
    addressFamilyIPv6 = 2,
    addressFamilySerial = 3,
    addressFamilyLocal = 4
};

/* Socket Types */
enum {
    socketTypeStream = 1,       /* TCP */
    socketTypeDatagram = 2,     /* UDP */
    socketTypeRaw = 3           /* Raw socket */
};

/* Network Address Structure */
typedef struct NetworkAddress {
    short family;               /* Address family */
    union {
        struct {
            unsigned char addr[4]; /* IPv4 address */
            unsigned short port;   /* Port number */
        } ipv4;
        struct {
            unsigned char addr[16]; /* IPv6 address */
            unsigned short port;    /* Port number */
            long flowInfo;          /* Flow information */
            long scopeID;           /* Scope ID */
        } ipv6;
        struct {
            Str255 portName;        /* Serial port name */
            long baudRate;          /* Baud rate */
        } serial;
        struct {
            Str255 path;            /* Local socket path */
        } local;
    } address;
} NetworkAddress;

/* Protocol Configuration */
typedef struct ProtocolConfig {
    short protocolType;         /* Protocol type */
    NetworkAddress localAddr;   /* Local address */
    NetworkAddress remoteAddr;  /* Remote address */
    long timeout;               /* Connection timeout */
    long keepAlive;             /* Keep-alive interval */
    Boolean noDelay;            /* Disable Nagle algorithm */
    Boolean reuseAddr;          /* Reuse address */
    long bufferSize;            /* Buffer size */
    short priority;             /* Priority */
    Str255 username;            /* Username (for auth protocols) */
    Str255 password;            /* Password (for auth protocols) */
    Handle privateKey;          /* Private key (for SSH) */
    Handle certificate;         /* Certificate (for SSL/TLS) */
} ProtocolConfig;

/* Protocol Statistics */
typedef struct ProtocolStats {
    long bytesTransmitted;      /* Bytes transmitted */
    long bytesReceived;         /* Bytes received */
    long packetsTransmitted;    /* Packets transmitted */
    long packetsReceived;       /* Packets received */
    long errors;                /* Error count */
    long retransmissions;       /* Retransmission count */
    unsigned long connectTime;  /* Connection time */
    float roundTripTime;        /* Round trip time (ms) */
    short signalStrength;       /* Signal strength */
} ProtocolStats;

/* Protocol Handle */
typedef struct ProtocolRec *ProtocolPtr, **ProtocolHandle;

/* Callback procedures */
typedef pascal void (*ProtocolConnectUPP)(ProtocolHandle hProtocol, OSErr result, long refCon);
typedef pascal void (*ProtocolDataUPP)(ProtocolHandle hProtocol, void *data, long size, long refCon);
typedef pascal void (*ProtocolErrorUPP)(ProtocolHandle hProtocol, OSErr error, long refCon);
typedef pascal void (*ProtocolStatusUPP)(ProtocolHandle hProtocol, short status, long refCon);

#if GENERATINGCFM
pascal ProtocolConnectUPP NewProtocolConnectProc(ProtocolConnectProcPtr userRoutine);
pascal ProtocolDataUPP NewProtocolDataProc(ProtocolDataProcPtr userRoutine);
pascal ProtocolErrorUPP NewProtocolErrorProc(ProtocolErrorProcPtr userRoutine);
pascal ProtocolStatusUPP NewProtocolStatusProc(ProtocolStatusProcPtr userRoutine);
pascal void DisposeProtocolConnectUPP(ProtocolConnectUPP userUPP);
pascal void DisposeProtocolDataUPP(ProtocolDataUPP userUPP);
pascal void DisposeProtocolErrorUPP(ProtocolErrorUPP userUPP);
pascal void DisposeProtocolStatusUPP(ProtocolStatusUPP userUPP);
#else
#define NewProtocolConnectProc(userRoutine) (userRoutine)
#define NewProtocolDataProc(userRoutine) (userRoutine)
#define NewProtocolErrorProc(userRoutine) (userRoutine)
#define NewProtocolStatusProc(userRoutine) (userRoutine)
#define DisposeProtocolConnectUPP(userUPP)
#define DisposeProtocolDataUPP(userUPP)
#define DisposeProtocolErrorUPP(userUPP)
#define DisposeProtocolStatusUPP(userUPP)
#endif

/* Protocol Stack API */

/* Initialization */
pascal OSErr InitProtocolStack(void);

/* Protocol Management */
pascal OSErr ProtocolCreate(short protocolType, const ProtocolConfig *config, ProtocolHandle *hProtocol);
pascal OSErr ProtocolDispose(ProtocolHandle hProtocol);

/* Connection Management */
pascal OSErr ProtocolConnect(ProtocolHandle hProtocol, Boolean async, ProtocolConnectUPP callback, long refCon);
pascal OSErr ProtocolListen(ProtocolHandle hProtocol, short backlog, ProtocolConnectUPP callback, long refCon);
pascal OSErr ProtocolAccept(ProtocolHandle hProtocol, ProtocolHandle *newProtocol);
pascal OSErr ProtocolDisconnect(ProtocolHandle hProtocol, Boolean graceful);

/* Data Transfer */
pascal OSErr ProtocolSend(ProtocolHandle hProtocol, const void *data, long *size);
pascal OSErr ProtocolReceive(ProtocolHandle hProtocol, void *buffer, long *size);
pascal OSErr ProtocolSendTo(ProtocolHandle hProtocol, const void *data, long size, const NetworkAddress *addr);
pascal OSErr ProtocolReceiveFrom(ProtocolHandle hProtocol, void *buffer, long *size, NetworkAddress *addr);

/* Asynchronous Operations */
pascal OSErr ProtocolSendAsync(ProtocolHandle hProtocol, const void *data, long size,
                              ProtocolDataUPP callback, long refCon);
pascal OSErr ProtocolReceiveAsync(ProtocolHandle hProtocol, void *buffer, long size,
                                 ProtocolDataUPP callback, long refCon);

/* Status and Control */
pascal OSErr ProtocolGetStatus(ProtocolHandle hProtocol, short *status);
pascal OSErr ProtocolGetStats(ProtocolHandle hProtocol, ProtocolStats *stats);
pascal OSErr ProtocolSetConfig(ProtocolHandle hProtocol, const ProtocolConfig *config);
pascal OSErr ProtocolGetConfig(ProtocolHandle hProtocol, ProtocolConfig *config);

/* Event Handling */
pascal OSErr ProtocolSetCallbacks(ProtocolHandle hProtocol, ProtocolDataUPP dataCallback,
                                 ProtocolErrorUPP errorCallback, ProtocolStatusUPP statusCallback,
                                 long refCon);
pascal OSErr ProtocolProcessEvents(ProtocolHandle hProtocol);

/* TCP/IP Specific Functions */

/* TCP Socket Options */
pascal OSErr TCPSetNoDelay(ProtocolHandle hProtocol, Boolean enable);
pascal OSErr TCPSetKeepAlive(ProtocolHandle hProtocol, Boolean enable, long interval);
pascal OSErr TCPSetLinger(ProtocolHandle hProtocol, Boolean enable, short timeout);
pascal OSErr TCPGetPeerAddress(ProtocolHandle hProtocol, NetworkAddress *addr);
pascal OSErr TCPGetLocalAddress(ProtocolHandle hProtocol, NetworkAddress *addr);

/* UDP Socket Functions */
pascal OSErr UDPSetBroadcast(ProtocolHandle hProtocol, Boolean enable);
pascal OSErr UDPJoinMulticast(ProtocolHandle hProtocol, const NetworkAddress *group);
pascal OSErr UDPLeaveMulticast(ProtocolHandle hProtocol, const NetworkAddress *group);

/* SSL/TLS Support */
typedef struct SSLConfig {
    short version;              /* SSL/TLS version */
    Handle certificate;         /* Client certificate */
    Handle privateKey;          /* Private key */
    Handle caCertificates;      /* CA certificates */
    Boolean verifyPeer;         /* Verify peer certificate */
    Str255 serverName;          /* Server name for SNI */
} SSLConfig;

pascal OSErr ProtocolSetSSLConfig(ProtocolHandle hProtocol, const SSLConfig *config);
pascal OSErr ProtocolStartTLS(ProtocolHandle hProtocol);
pascal OSErr ProtocolGetSSLInfo(ProtocolHandle hProtocol, Str255 cipher, short *strength);

/* SSH Support */
typedef struct SSHConfig {
    short version;              /* SSH version */
    Str255 username;            /* Username */
    Str255 password;            /* Password */
    Handle publicKey;           /* Public key */
    Handle privateKey;          /* Private key */
    Str255 hostKey;             /* Expected host key */
    Boolean compression;        /* Enable compression */
} SSHConfig;

pascal OSErr ProtocolSetSSHConfig(ProtocolHandle hProtocol, const SSHConfig *config);
pascal OSErr SSHExecuteCommand(ProtocolHandle hProtocol, ConstStr255Param command,
                              void *output, long *outputSize);
pascal OSErr SSHStartShell(ProtocolHandle hProtocol);
pascal OSErr SSHForwardPort(ProtocolHandle hProtocol, short localPort, ConstStr255Param remoteHost, short remotePort);

/* HTTP/HTTPS Support */
typedef struct HTTPRequest {
    Str255 method;              /* HTTP method */
    Str255 path;                /* Request path */
    Str255 version;             /* HTTP version */
    Handle headers;             /* HTTP headers */
    Handle body;                /* Request body */
} HTTPRequest;

typedef struct HTTPResponse {
    short statusCode;           /* HTTP status code */
    Str255 statusText;          /* Status text */
    Handle headers;             /* Response headers */
    Handle body;                /* Response body */
} HTTPResponse;

pascal OSErr HTTPSendRequest(ProtocolHandle hProtocol, const HTTPRequest *request);
pascal OSErr HTTPReceiveResponse(ProtocolHandle hProtocol, HTTPResponse *response);
pascal OSErr HTTPSetHeader(Handle headers, ConstStr255Param name, ConstStr255Param value);
pascal OSErr HTTPGetHeader(Handle headers, ConstStr255Param name, Str255 value);

/* FTP Support */
pascal OSErr FTPLogin(ProtocolHandle hProtocol, ConstStr255Param username, ConstStr255Param password);
pascal OSErr FTPChangeDirectory(ProtocolHandle hProtocol, ConstStr255Param path);
pascal OSErr FTPListDirectory(ProtocolHandle hProtocol, Handle *listing);
pascal OSErr FTPUploadFile(ProtocolHandle hProtocol, ConstStr255Param localFile, ConstStr255Param remoteName);
pascal OSErr FTPDownloadFile(ProtocolHandle hProtocol, ConstStr255Param remoteName, ConstStr255Param localFile);
pascal OSErr FTPDeleteFile(ProtocolHandle hProtocol, ConstStr255Param fileName);

/* Telnet Support */
typedef struct TelnetOptions {
    Boolean echo;               /* Echo option */
    Boolean suppressGoAhead;    /* Suppress go ahead */
    Boolean terminalType;       /* Terminal type negotiation */
    Boolean windowSize;         /* Window size negotiation */
    Str255 termType;            /* Terminal type */
    short rows;                 /* Terminal rows */
    short cols;                 /* Terminal columns */
} TelnetOptions;

pascal OSErr TelnetSetOptions(ProtocolHandle hProtocol, const TelnetOptions *options);
pascal OSErr TelnetSendCommand(ProtocolHandle hProtocol, unsigned char command, unsigned char option);
pascal OSErr TelnetSendSubnegotiation(ProtocolHandle hProtocol, unsigned char option,
                                     const void *data, short length);

/* Protocol Resolution */
pascal OSErr ResolveHostname(ConstStr255Param hostname, NetworkAddress addresses[], short *count);
pascal OSErr GetHostname(ConstStr255Param address, Str255 hostname);
pascal OSErr GetServicePort(ConstStr255Param service, ConstStr255Param protocol, short *port);

/* Network Interface Information */
typedef struct NetworkInterface {
    Str255 name;                /* Interface name */
    NetworkAddress address;     /* Interface address */
    NetworkAddress netmask;     /* Network mask */
    NetworkAddress broadcast;   /* Broadcast address */
    Boolean up;                 /* Interface up */
    Boolean running;            /* Interface running */
    Boolean loopback;           /* Loopback interface */
    long mtu;                   /* Maximum transmission unit */
} NetworkInterface;

pascal OSErr GetNetworkInterfaces(NetworkInterface interfaces[], short *count);
pascal OSErr GetDefaultGateway(NetworkAddress *gateway);

/* Advanced Features */
pascal OSErr ProtocolSetTimeout(ProtocolHandle hProtocol, long timeout);
pascal OSErr ProtocolGetTimeout(ProtocolHandle hProtocol, long *timeout);
pascal OSErr ProtocolSetBufferSize(ProtocolHandle hProtocol, long sendSize, long receiveSize);
pascal OSErr ProtocolFlushBuffers(ProtocolHandle hProtocol);

/* Quality of Service */
typedef struct QoSConfig {
    short priority;             /* Traffic priority */
    long bandwidth;             /* Required bandwidth */
    long latency;               /* Maximum latency */
    float jitter;               /* Maximum jitter */
    float packetLoss;           /* Maximum packet loss */
} QoSConfig;

pascal OSErr ProtocolSetQoS(ProtocolHandle hProtocol, const QoSConfig *qos);

/* Thread Safety */
pascal OSErr ProtocolLock(ProtocolHandle hProtocol);
pascal OSErr ProtocolUnlock(ProtocolHandle hProtocol);

/* Reference Management */
pascal long ProtocolGetRefCon(ProtocolHandle hProtocol);
pascal void ProtocolSetRefCon(ProtocolHandle hProtocol, long refCon);

/* Debugging and Logging */
pascal OSErr ProtocolStartPacketCapture(ProtocolHandle hProtocol, ConstStr255Param fileName);
pascal OSErr ProtocolStopPacketCapture(ProtocolHandle hProtocol);
pascal OSErr ProtocolSetLogLevel(short level);

/* Log levels */
enum {
    logLevelNone = 0,
    logLevelError = 1,
    logLevelWarning = 2,
    logLevelInfo = 3,
    logLevelDebug = 4,
    logLevelTrace = 5
};

/* SSL/TLS Versions */
enum {
    sslVersionSSL2 = 2,
    sslVersionSSL3 = 3,
    sslVersionTLS10 = 10,
    sslVersionTLS11 = 11,
    sslVersionTLS12 = 12,
    sslVersionTLS13 = 13
};

/* SSH Versions */
enum {
    sshVersion1 = 1,
    sshVersion2 = 2
};

#ifdef __cplusplus
}
#endif

#endif /* PROTOCOLSTACK_H */