/*
 * ADBManager.h - Apple Desktop Bus Manager API definitions
 *
 * Portable System 7.1 Implementation
 * Based on Apple's original ADB Manager from Mac OS 7.1 source code
 *
 * This header provides complete ADB Manager functionality including:
 * - ADB protocol implementation with collision detection
 * - Device registration and address assignment
 * - Keyboard key code translation and repeat handling
 * - Mouse movement delta calculation and button state
 * - Service request processing and collision resolution
 * - Device handler installation and management
 * - Hardware abstraction for modern input devices
 */

#ifndef __ADBMANAGER_H__
#define __ADBMANAGER_H__

#include <stdint.h>
#include <stdbool.h>

/* ADB Protocol Constants */
#define ADB_MAX_DEVICES         16
#define ADB_MAX_DATA_SIZE       8
#define ADB_COMMAND_QUEUE_SIZE  8
#define ADB_DEVICE_TABLE_SIZE   (ADB_MAX_DEVICES * sizeof(ADBDeviceEntry))

/* ADB Command Types */
#define ADB_RESET_CMD           0x00
#define ADB_FLUSH_CMD           0x01
#define ADB_LISTEN_CMD          0x08
#define ADB_TALK_CMD            0x0C
#define ADB_MASK_CMD            0x0C

/* ADB Device Addresses */
#define ADB_KEYBOARD_ADDR       0x02
#define ADB_MOUSE_ADDR          0x03

/* ADB Register Numbers */
#define ADB_REG0                0
#define ADB_REG1                1
#define ADB_REG2                2
#define ADB_REG3                3

/* ADB Status Flags */
#define ADB_FLAG_QUEUE_EMPTY    0x04
#define ADB_FLAG_INIT           0x20
#define ADB_FLAG_BUSY           0x10
#define ADB_FLAG_NO_REPLY       0x20
#define ADB_FLAG_SRQ            0x40
#define ADB_FLAG_AUTO_POLL      0x80

/* Keyboard Constants */
#define KBD_BUFFER_COUNT        2
#define KBD_BUFFER_SIZE         10
#define KBD_DATA_SIZE           (4 + 16 + 4 + 4 + 1 + 1 + (KBD_BUFFER_COUNT * KBD_BUFFER_SIZE))

/* Key Modifier Flags (matches Mac OS event manager) */
#define KBD_MOD_COMMAND         0x01
#define KBD_MOD_SHIFT           0x02
#define KBD_MOD_CAPS_LOCK       0x04
#define KBD_MOD_OPTION          0x08
#define KBD_MOD_CONTROL         0x10
#define KBD_MOD_RIGHT_SHIFT     0x20
#define KBD_MOD_RIGHT_OPTION    0x40
#define KBD_MOD_RIGHT_CONTROL   0x80

/* Key Event Types */
#define KEY_DOWN_EVENT          3
#define KEY_UP_EVENT            4

/* Forward Declarations */
typedef struct ADBManager ADBManager;
typedef struct ADBDeviceEntry ADBDeviceEntry;
typedef struct ADBCmdQEntry ADBCmdQEntry;
typedef struct ADBOpBlock ADBOpBlock;

/* Completion Routine Function Pointer */
typedef void (*ADBCompletionProc)(uint8_t command, uint32_t deviceInfo,
                                   uint8_t* buffer, void* userData);

/* Hardware Abstraction Layer Function Pointers */
typedef int (*ADBStartRequestProc)(ADBManager* adbMgr, uint8_t command,
                                   uint8_t* data, int dataLen, bool isImplicit);
typedef void (*ADBPollDeviceProc)(ADBManager* adbMgr);
typedef void (*ADBResetBusProc)(ADBManager* adbMgr);

/* ADB Device Entry - represents one device in the device table */
struct ADBDeviceEntry {
    uint8_t deviceType;        /* Device handler ID */
    uint8_t originalAddr;      /* Original ADB address */
    uint8_t currentAddr;       /* Current ADB address */
    uint8_t unused;            /* Alignment padding */
    ADBCompletionProc handler; /* Device completion routine */
    void* userData;            /* Optional user data pointer */
};

/* ADB Command Queue Entry - represents queued ADB operations */
struct ADBCmdQEntry {
    uint8_t command;           /* ADB command byte */
    uint8_t unused;            /* Alignment padding */
    uint8_t* buffer;           /* Data buffer pointer */
    ADBCompletionProc completion; /* Completion routine */
    void* userData;            /* Optional user data */
};

/* ADB Operation Block - parameter block for ADBOp calls */
struct ADBOpBlock {
    uint8_t* dataBuffer;       /* Pointer to data buffer */
    ADBCompletionProc serviceRoutine; /* Completion routine */
    void* dataArea;            /* Optional data area pointer */
};

/* Keyboard Driver Data Structure */
typedef struct {
    void* kmapPtr;             /* KMAP resource pointer */
    uint8_t keyBits[16];       /* Key state bitmap (128 keys / 8 bits) */
    void* kchrPtr;             /* KCHR resource pointer */
    uint32_t deadKeyState;     /* Dead key processing state */
    uint8_t noADBOp;           /* Flag to disable ADB operations */
    uint8_t numBuffers;        /* Number of available buffers */
    uint8_t buffers[KBD_BUFFER_COUNT * KBD_BUFFER_SIZE]; /* Data buffers */
} KeyboardDriverData;

/* Hardware Abstraction Layer Interface */
typedef struct {
    ADBStartRequestProc startRequest;
    ADBPollDeviceProc pollDevice;
    ADBResetBusProc resetBus;
    void* platformData;        /* Platform-specific data */
} ADBHardwareInterface;

/* Main ADB Manager State Structure */
struct ADBManager {
    /* Device Management */
    ADBDeviceEntry deviceTable[ADB_MAX_DEVICES];
    uint16_t deviceMap;        /* Bitmap of active device addresses */
    uint16_t hasDevice;        /* Bitmap of detected devices */
    uint8_t deviceTableOffset; /* Current device table offset */
    uint8_t moveCount;         /* Address collision resolution counter */

    /* Command Queue Management */
    ADBCmdQEntry commandQueue[ADB_COMMAND_QUEUE_SIZE];
    ADBCmdQEntry* queueBegin;  /* Queue start pointer */
    ADBCmdQEntry* queueEnd;    /* Queue end pointer */
    ADBCmdQEntry* queueHead;   /* Queue head pointer */
    ADBCmdQEntry* queueTail;   /* Queue tail pointer */

    /* Current Operation State */
    uint8_t lastCommand;       /* Last ADB command issued */
    uint8_t flags;             /* Status flags */
    uint8_t auxFlags;          /* Auxiliary hardware-specific flags */
    uint8_t pollAddress;       /* Current polling address */
    uint8_t newAddress;        /* New address for collision resolution */
    uint8_t pollCommand;       /* Current poll command */
    uint8_t initAddress;       /* Address being initialized */

    /* Data Buffer */
    uint8_t dataCount;         /* Number of bytes in poll buffer */
    uint8_t pollBuffer[ADB_MAX_DATA_SIZE]; /* Internal data buffer */

    /* Hardware Interface */
    ADBHardwareInterface hardware;

    /* Platform Integration */
    bool interruptsEnabled;    /* Interrupt state flag */
    uint32_t tickCount;        /* System tick counter */

    /* Keyboard State */
    uint8_t keyboardType;      /* Type of last keyboard used */
    uint8_t keyboardLast;      /* Last keyboard address */
    uint16_t keyLast;          /* Last key event (for auto-repeat) */
    uint16_t hiKeyLast;        /* High word of last key event */
    uint32_t keyTime;          /* Time of last key event */
    uint32_t keyRepeatTime;    /* Time for next auto-repeat */
    uint8_t keyMap[16];        /* Current key state bitmap */
};

/* ADB Manager API Functions */

/* Initialization */
int ADBManager_Initialize(ADBManager* mgr, ADBHardwareInterface* hardware);
void ADBManager_Shutdown(ADBManager* mgr);
int ADBManager_Reinitialize(ADBManager* mgr);

/* Device Management */
int ADBManager_CountDevices(ADBManager* mgr);
int ADBManager_GetDeviceInfo(ADBManager* mgr, uint8_t address, ADBDeviceEntry* info);
int ADBManager_SetDeviceInfo(ADBManager* mgr, uint8_t address, ADBCompletionProc handler, void* userData);
int ADBManager_GetIndexedDevice(ADBManager* mgr, int index, ADBDeviceEntry* info);

/* ADB Operations */
int ADBManager_SendCommand(ADBManager* mgr, uint8_t command, ADBOpBlock* opBlock);
int ADBManager_Talk(ADBManager* mgr, uint8_t address, uint8_t reg, uint8_t* buffer, int* bufferLen);
int ADBManager_Listen(ADBManager* mgr, uint8_t address, uint8_t reg, uint8_t* data, int dataLen);
int ADBManager_Flush(ADBManager* mgr, uint8_t address);

/* Internal Processing */
void ADBManager_ProcessInterrupt(ADBManager* mgr);
void ADBManager_ProcessAutoPolling(ADBManager* mgr);
void ADBManager_HandleServiceRequest(ADBManager* mgr);
void ADBManager_RunNextRequest(ADBManager* mgr);

/* Request Completion */
void ADBManager_ExplicitRequestDone(ADBManager* mgr, int dataLen, uint8_t command, uint8_t* data);
void ADBManager_ImplicitRequestDone(ADBManager* mgr, int dataLen, uint8_t command, uint8_t* data);

/* Keyboard Support */
void ADBManager_InstallKeyboardDrivers(ADBManager* mgr);
void ADBManager_ProcessKeyboardData(ADBManager* mgr, uint8_t* data, int dataLen, uint8_t address);
uint32_t ADBManager_TranslateKey(void* kchrTable, uint16_t keyCode, uint32_t* deadKeyState);

/* Mouse/Pointing Device Support */
void ADBManager_ProcessMouseData(ADBManager* mgr, uint8_t* data, int dataLen, uint8_t address);

/* Hardware Abstraction Helpers */
void ADBManager_SetHardwareInterface(ADBManager* mgr, ADBHardwareInterface* hardware);
bool ADBManager_IsBusy(ADBManager* mgr);
void ADBManager_SetInterruptState(ADBManager* mgr, bool enabled);

/* Utility Functions */
const char* ADBManager_GetErrorString(int errorCode);
void ADBManager_DumpState(ADBManager* mgr); /* Debug function */

/* Error Codes */
#define ADB_NO_ERROR            0
#define ADB_ERROR_QUEUE_FULL    -1
#define ADB_ERROR_DEVICE_NOT_FOUND -2
#define ADB_ERROR_TIMEOUT       -3
#define ADB_ERROR_INVALID_PARAM -4
#define ADB_ERROR_NOT_INITIALIZED -5
#define ADB_ERROR_HARDWARE      -6

/* Callback Types for Platform Integration */
typedef void (*ADBEventCallback)(int eventType, uint32_t eventData, void* userData);
typedef void (*ADBTimerCallback)(ADBManager* mgr, void* userData);

/* Platform Integration */
void ADBManager_SetEventCallback(ADBManager* mgr, ADBEventCallback callback, void* userData);
void ADBManager_SetTimerCallback(ADBManager* mgr, ADBTimerCallback callback, void* userData);
void ADBManager_PostKeyEvent(int eventType, uint16_t keyCode, uint8_t modifiers, uint16_t asciiCode);
void ADBManager_PostMouseEvent(int eventType, int16_t deltaX, int16_t deltaY, uint8_t buttonState);

#endif /* __ADBMANAGER_H__ */