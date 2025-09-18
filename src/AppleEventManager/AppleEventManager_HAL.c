/*
 * Copyright (c) 2024 System7 Project
 * MIT License - See LICENSE file
 */
/*
 * AppleEventManager_HAL.c
 *
 * Hardware Abstraction Layer for Apple Event Manager
 * Provides platform-specific inter-process communication
 *
 * Based on Mac OS 7.1 Apple Event Manager
 */

#include "AppleEventManager/AppleEventManager.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#ifdef __APPLE__
#include <mach/mach.h>
#include <mach/message.h>
#include <servers/bootstrap.h>
#include <CoreFoundation/CoreFoundation.h>
#elif defined(__linux__)
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <fcntl.h>
#elif defined(_WIN32)
#include <windows.h>
#endif

/* ========================================================================
 * Platform-Specific IPC Structures
 * ======================================================================== */

typedef struct IPCConnection {
#ifdef __APPLE__
    mach_port_t sendPort;
    mach_port_t receivePort;
    CFMessagePortRef messagePort;
#elif defined(__linux__)
    int socketFd;
    struct sockaddr_un address;
#elif defined(_WIN32)
    HANDLE pipeHandle;
    char pipeName[256];
#endif
    ProcessSerialNumber targetPSN;
    Boolean isConnected;
} IPCConnection;

/* Connection pool */
#define MAX_IPC_CONNECTIONS 32
static IPCConnection g_connections[MAX_IPC_CONNECTIONS];
static Boolean g_halInitialized = false;

/* ========================================================================
 * macOS Implementation (Mach Ports / CFMessagePort)
 * ======================================================================== */

#ifdef __APPLE__

static CFDataRef MessagePortCallback(CFMessagePortRef local, SInt32 msgid, CFDataRef data, void* info) {
    (void)local;
    (void)info;

    if (!data) return NULL;

    /* Convert CFData to Apple Event */
    AppleEvent event;
    Size dataSize = CFDataGetLength(data);
    const void* dataPtr = CFDataGetBytePtr(data);

    OSErr err = AECreateDesc(typeAppleEvent, dataPtr, dataSize, &event);
    if (err != noErr) return NULL;

    /* Process the event */
    AppleEvent reply;
    err = AEProcessAppleEvent(&event, &reply, kAEWaitReply, kAEDefaultTimeout);

    /* Convert reply to CFData */
    CFDataRef replyData = NULL;
    if (err == noErr && reply.dataHandle) {
        Size replySize = AEGetHandleSize(reply.dataHandle);
        void* replyPtr = AEGetHandleData(reply.dataHandle);
        replyData = CFDataCreate(NULL, replyPtr, replySize);
        AEDisposeDesc(&reply);
    }

    AEDisposeDesc(&event);
    return replyData;
}

OSErr HAL_InitializeIPC(void) {
    if (g_halInitialized) return noErr;

    /* Create a message port for receiving events */
    CFStringRef portName = CFStringCreateWithFormat(NULL, NULL,
                                                   CFSTR("com.system7.ae.%d"), getpid());

    CFMessagePortRef localPort = CFMessagePortCreateLocal(NULL, portName,
                                                         MessagePortCallback, NULL, NULL);
    CFRelease(portName);

    if (!localPort) {
        return errAENetworkErr;
    }

    /* Add to run loop */
    CFRunLoopSourceRef source = CFMessagePortCreateRunLoopSource(NULL, localPort, 0);
    CFRunLoopAddSource(CFRunLoopGetCurrent(), source, kCFRunLoopCommonModes);
    CFRelease(source);

    /* Store in first connection slot */
    g_connections[0].messagePort = localPort;
    g_connections[0].isConnected = true;

    g_halInitialized = true;
    return noErr;
}

OSErr HAL_SendAppleEvent(const AppleEvent* event, AppleEvent* reply, const ProcessSerialNumber* targetPSN,
                        AESendMode sendMode, AESendPriority sendPriority, long timeoutInTicks) {
    (void)sendPriority;

    if (!event || !targetPSN) return errAENotAEDesc;

    /* Find or create connection to target */
    IPCConnection* conn = NULL;
    for (int i = 1; i < MAX_IPC_CONNECTIONS; i++) {
        if (g_connections[i].targetPSN.highLongOfPSN == targetPSN->highLongOfPSN &&
            g_connections[i].targetPSN.lowLongOfPSN == targetPSN->lowLongOfPSN) {
            conn = &g_connections[i];
            break;
        }
    }

    if (!conn) {
        /* Create new connection */
        for (int i = 1; i < MAX_IPC_CONNECTIONS; i++) {
            if (!g_connections[i].isConnected) {
                conn = &g_connections[i];
                conn->targetPSN = *targetPSN;

                /* Connect to target's message port */
                CFStringRef targetPortName = CFStringCreateWithFormat(NULL, NULL,
                                                                     CFSTR("com.system7.ae.%d"),
                                                                     targetPSN->lowLongOfPSN);

                conn->messagePort = CFMessagePortCreateRemote(NULL, targetPortName);
                CFRelease(targetPortName);

                if (!conn->messagePort) {
                    return connectionInvalid;
                }

                conn->isConnected = true;
                break;
            }
        }
    }

    if (!conn) return errAENetworkErr;

    /* Serialize event */
    Size eventSize = AEGetHandleSize(event->dataHandle);
    void* eventData = AEGetHandleData(event->dataHandle);
    CFDataRef eventCFData = CFDataCreate(NULL, eventData, eventSize);

    /* Send event */
    CFDataRef replyCFData = NULL;
    SInt32 result;

    if (sendMode & kAEWaitReply) {
        CFTimeInterval timeout = timeoutInTicks / 60.0;  /* Convert ticks to seconds */
        result = CFMessagePortSendRequest(conn->messagePort, 0, eventCFData, timeout, timeout,
                                         kCFRunLoopDefaultMode, &replyCFData);
    } else {
        result = CFMessagePortSendRequest(conn->messagePort, 0, eventCFData, 0, 0, NULL, NULL);
    }

    CFRelease(eventCFData);

    if (result != kCFMessagePortSuccess) {
        return errAETimeout;
    }

    /* Handle reply */
    if (replyCFData && reply) {
        Size replySize = CFDataGetLength(replyCFData);
        const void* replyPtr = CFDataGetBytePtr(replyCFData);
        OSErr err = AECreateDesc(typeAppleEvent, replyPtr, replySize, reply);
        CFRelease(replyCFData);
        return err;
    }

    return noErr;
}

#elif defined(__linux__)

/* ========================================================================
 * Linux Implementation (Unix Domain Sockets)
 * ======================================================================== */

OSErr HAL_InitializeIPC(void) {
    if (g_halInitialized) return noErr;

    /* Create a Unix domain socket for receiving events */
    int sockFd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sockFd < 0) {
        return errAENetworkErr;
    }

    /* Set non-blocking */
    int flags = fcntl(sockFd, F_GETFL, 0);
    fcntl(sockFd, F_SETFL, flags | O_NONBLOCK);

    /* Bind to address */
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    snprintf(addr.sun_path, sizeof(addr.sun_path), "/tmp/ae_%d.sock", getpid());

    /* Remove existing socket file */
    unlink(addr.sun_path);

    if (bind(sockFd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(sockFd);
        return errAENetworkErr;
    }

    if (listen(sockFd, 5) < 0) {
        close(sockFd);
        unlink(addr.sun_path);
        return errAENetworkErr;
    }

    /* Store in first connection slot */
    g_connections[0].socketFd = sockFd;
    g_connections[0].address = addr;
    g_connections[0].isConnected = true;

    g_halInitialized = true;
    return noErr;
}

OSErr HAL_SendAppleEvent(const AppleEvent* event, AppleEvent* reply, const ProcessSerialNumber* targetPSN,
                        AESendMode sendMode, AESendPriority sendPriority, long timeoutInTicks) {
    (void)sendPriority;

    if (!event || !targetPSN) return errAENotAEDesc;

    /* Find or create connection to target */
    IPCConnection* conn = NULL;
    for (int i = 1; i < MAX_IPC_CONNECTIONS; i++) {
        if (g_connections[i].targetPSN.highLongOfPSN == targetPSN->highLongOfPSN &&
            g_connections[i].targetPSN.lowLongOfPSN == targetPSN->lowLongOfPSN) {
            conn = &g_connections[i];
            break;
        }
    }

    if (!conn) {
        /* Create new connection */
        for (int i = 1; i < MAX_IPC_CONNECTIONS; i++) {
            if (!g_connections[i].isConnected) {
                conn = &g_connections[i];
                conn->targetPSN = *targetPSN;

                /* Create socket and connect to target */
                conn->socketFd = socket(AF_UNIX, SOCK_STREAM, 0);
                if (conn->socketFd < 0) {
                    return errAENetworkErr;
                }

                struct sockaddr_un targetAddr;
                memset(&targetAddr, 0, sizeof(targetAddr));
                targetAddr.sun_family = AF_UNIX;
                snprintf(targetAddr.sun_path, sizeof(targetAddr.sun_path),
                        "/tmp/ae_%d.sock", targetPSN->lowLongOfPSN);

                if (connect(conn->socketFd, (struct sockaddr*)&targetAddr, sizeof(targetAddr)) < 0) {
                    close(conn->socketFd);
                    return connectionInvalid;
                }

                conn->isConnected = true;
                break;
            }
        }
    }

    if (!conn) return errAENetworkErr;

    /* Send event */
    Size eventSize = AEGetHandleSize(event->dataHandle);
    void* eventData = AEGetHandleData(event->dataHandle);

    /* Send size first */
    if (send(conn->socketFd, &eventSize, sizeof(Size), 0) != sizeof(Size)) {
        return errAENetworkErr;
    }

    /* Send data */
    if (send(conn->socketFd, eventData, eventSize, 0) != (ssize_t)eventSize) {
        return errAENetworkErr;
    }

    /* Wait for reply if requested */
    if ((sendMode & kAEWaitReply) && reply) {
        /* Set timeout */
        struct timeval tv;
        tv.tv_sec = timeoutInTicks / 60;
        tv.tv_usec = (timeoutInTicks % 60) * 16667;  /* Convert ticks to microseconds */
        setsockopt(conn->socketFd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

        /* Receive reply size */
        Size replySize;
        if (recv(conn->socketFd, &replySize, sizeof(Size), 0) != sizeof(Size)) {
            return errAETimeout;
        }

        /* Receive reply data */
        void* replyData = malloc(replySize);
        if (!replyData) return memFullErr;

        if (recv(conn->socketFd, replyData, replySize, 0) != (ssize_t)replySize) {
            free(replyData);
            return errAETimeout;
        }

        OSErr err = AECreateDesc(typeAppleEvent, replyData, replySize, reply);
        free(replyData);
        return err;
    }

    return noErr;
}

#elif defined(_WIN32)

/* ========================================================================
 * Windows Implementation (Named Pipes)
 * ======================================================================== */

OSErr HAL_InitializeIPC(void) {
    if (g_halInitialized) return noErr;

    /* Create a named pipe for receiving events */
    char pipeName[256];
    snprintf(pipeName, sizeof(pipeName), "\\\\.\\pipe\\ae_%d", GetCurrentProcessId());

    HANDLE pipeHandle = CreateNamedPipe(
        pipeName,
        PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
        PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
        PIPE_UNLIMITED_INSTANCES,
        4096, 4096, 0, NULL
    );

    if (pipeHandle == INVALID_HANDLE_VALUE) {
        return errAENetworkErr;
    }

    /* Store in first connection slot */
    g_connections[0].pipeHandle = pipeHandle;
    strcpy(g_connections[0].pipeName, pipeName);
    g_connections[0].isConnected = true;

    g_halInitialized = true;
    return noErr;
}

OSErr HAL_SendAppleEvent(const AppleEvent* event, AppleEvent* reply, const ProcessSerialNumber* targetPSN,
                        AESendMode sendMode, AESendPriority sendPriority, long timeoutInTicks) {
    (void)sendPriority;

    if (!event || !targetPSN) return errAENotAEDesc;

    /* Find or create connection to target */
    IPCConnection* conn = NULL;
    for (int i = 1; i < MAX_IPC_CONNECTIONS; i++) {
        if (g_connections[i].targetPSN.highLongOfPSN == targetPSN->highLongOfPSN &&
            g_connections[i].targetPSN.lowLongOfPSN == targetPSN->lowLongOfPSN) {
            conn = &g_connections[i];
            break;
        }
    }

    if (!conn) {
        /* Create new connection */
        for (int i = 1; i < MAX_IPC_CONNECTIONS; i++) {
            if (!g_connections[i].isConnected) {
                conn = &g_connections[i];
                conn->targetPSN = *targetPSN;

                /* Connect to target's named pipe */
                snprintf(conn->pipeName, sizeof(conn->pipeName),
                        "\\\\.\\pipe\\ae_%d", targetPSN->lowLongOfPSN);

                conn->pipeHandle = CreateFile(
                    conn->pipeName,
                    GENERIC_READ | GENERIC_WRITE,
                    0, NULL, OPEN_EXISTING, 0, NULL
                );

                if (conn->pipeHandle == INVALID_HANDLE_VALUE) {
                    return connectionInvalid;
                }

                conn->isConnected = true;
                break;
            }
        }
    }

    if (!conn) return errAENetworkErr;

    /* Send event */
    Size eventSize = AEGetHandleSize(event->dataHandle);
    void* eventData = AEGetHandleData(event->dataHandle);
    DWORD bytesWritten;

    if (!WriteFile(conn->pipeHandle, eventData, eventSize, &bytesWritten, NULL) ||
        bytesWritten != eventSize) {
        return errAENetworkErr;
    }

    /* Wait for reply if requested */
    if ((sendMode & kAEWaitReply) && reply) {
        DWORD timeout = (timeoutInTicks * 1000) / 60;  /* Convert ticks to milliseconds */
        DWORD bytesRead;
        void* replyData = malloc(4096);
        if (!replyData) return memFullErr;

        if (!ReadFile(conn->pipeHandle, replyData, 4096, &bytesRead, NULL)) {
            free(replyData);
            return errAETimeout;
        }

        OSErr err = AECreateDesc(typeAppleEvent, replyData, bytesRead, reply);
        free(replyData);
        return err;
    }

    return noErr;
}

#endif

/* ========================================================================
 * Common HAL Functions
 * ======================================================================== */

OSErr HAL_CleanupIPC(void) {
    if (!g_halInitialized) return noErr;

    for (int i = 0; i < MAX_IPC_CONNECTIONS; i++) {
        if (g_connections[i].isConnected) {
#ifdef __APPLE__
            if (g_connections[i].messagePort) {
                CFMessagePortInvalidate(g_connections[i].messagePort);
                CFRelease(g_connections[i].messagePort);
            }
#elif defined(__linux__)
            if (g_connections[i].socketFd > 0) {
                close(g_connections[i].socketFd);
            }
            if (i == 0) {
                unlink(g_connections[i].address.sun_path);
            }
#elif defined(_WIN32)
            if (g_connections[i].pipeHandle != INVALID_HANDLE_VALUE) {
                CloseHandle(g_connections[i].pipeHandle);
            }
#endif
            g_connections[i].isConnected = false;
        }
    }

    g_halInitialized = false;
    return noErr;
}

OSErr HAL_ReceiveAppleEvent(AppleEvent* event, long timeoutInTicks) {
    if (!event || !g_halInitialized) return errAENotAEDesc;

#ifdef __APPLE__
    /* Events are received via message port callback */
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, timeoutInTicks / 60.0, true);
#elif defined(__linux__)
    /* Check for incoming connections */
    struct timeval tv;
    tv.tv_sec = timeoutInTicks / 60;
    tv.tv_usec = (timeoutInTicks % 60) * 16667;

    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(g_connections[0].socketFd, &readfds);

    if (select(g_connections[0].socketFd + 1, &readfds, NULL, NULL, &tv) > 0) {
        /* Accept connection */
        int clientFd = accept(g_connections[0].socketFd, NULL, NULL);
        if (clientFd >= 0) {
            /* Read event */
            Size eventSize;
            if (recv(clientFd, &eventSize, sizeof(Size), 0) == sizeof(Size)) {
                void* eventData = malloc(eventSize);
                if (eventData && recv(clientFd, eventData, eventSize, 0) == (ssize_t)eventSize) {
                    OSErr err = AECreateDesc(typeAppleEvent, eventData, eventSize, event);
                    free(eventData);
                    close(clientFd);
                    return err;
                }
                if (eventData) free(eventData);
            }
            close(clientFd);
        }
    }
#elif defined(_WIN32)
    /* Wait for connection */
    if (ConnectNamedPipe(g_connections[0].pipeHandle, NULL) ||
        GetLastError() == ERROR_PIPE_CONNECTED) {
        /* Read event */
        DWORD bytesRead;
        void* eventData = malloc(4096);
        if (eventData && ReadFile(g_connections[0].pipeHandle, eventData, 4096, &bytesRead, NULL)) {
            OSErr err = AECreateDesc(typeAppleEvent, eventData, bytesRead, event);
            free(eventData);
            DisconnectNamedPipe(g_connections[0].pipeHandle);
            return err;
        }
        if (eventData) free(eventData);
    }
#endif

    return errAETimeout;
}