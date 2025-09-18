/*
 * DriverInterface.h
 * System 7.1 Device Manager - Driver Interface Definitions
 *
 * Defines the interface between device drivers and the Device Manager,
 * including entry point conventions and driver communication.
 */

#ifndef DRIVER_INTERFACE_H
#define DRIVER_INTERFACE_H

#include "DeviceTypes.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Driver Entry Point Selectors
 * ============================================================================= */

typedef enum {
    kDriverOpen     = 0,    /* Open the driver */
    kDriverPrime    = 1,    /* Read or Write operation */
    kDriverControl  = 2,    /* Control operation */
    kDriverStatus   = 3,    /* Status operation */
    kDriverClose    = 4,    /* Close the driver */
    kDriverKill     = 5     /* Kill pending operations */
} DriverSelector;

/* =============================================================================
 * Driver Entry Point Function Types
 * ============================================================================= */

/**
 * Driver Open routine
 * Called when the driver is first opened
 *
 * @param pb Parameter block
 * @param dce Device Control Entry
 * @return Error code
 */
typedef int16_t (*DriverOpenProc)(IOParamPtr pb, DCEPtr dce);

/**
 * Driver Prime routine (Read/Write)
 * Called for read and write operations
 *
 * @param pb Parameter block
 * @param dce Device Control Entry
 * @return Error code
 */
typedef int16_t (*DriverPrimeProc)(IOParamPtr pb, DCEPtr dce);

/**
 * Driver Control routine
 * Called for control operations
 *
 * @param pb Parameter block (cast to CntrlParamPtr)
 * @param dce Device Control Entry
 * @return Error code
 */
typedef int16_t (*DriverControlProc)(CntrlParamPtr pb, DCEPtr dce);

/**
 * Driver Status routine
 * Called for status operations
 *
 * @param pb Parameter block (cast to CntrlParamPtr)
 * @param dce Device Control Entry
 * @return Error code
 */
typedef int16_t (*DriverStatusProc)(CntrlParamPtr pb, DCEPtr dce);

/**
 * Driver Close routine
 * Called when the driver is closed
 *
 * @param pb Parameter block
 * @param dce Device Control Entry
 * @return Error code
 */
typedef int16_t (*DriverCloseProc)(IOParamPtr pb, DCEPtr dce);

/**
 * Driver Kill routine
 * Called to kill pending I/O operations
 *
 * @param pb Parameter block
 * @param dce Device Control Entry
 * @return Error code
 */
typedef int16_t (*DriverKillProc)(IOParamPtr pb, DCEPtr dce);

/* =============================================================================
 * Driver Dispatch Table
 * ============================================================================= */

typedef struct DriverDispatchTable {
    DriverOpenProc      drvOpen;      /* Open routine */
    DriverPrimeProc     drvPrime;     /* Prime (Read/Write) routine */
    DriverControlProc   drvControl;   /* Control routine */
    DriverStatusProc    drvStatus;    /* Status routine */
    DriverCloseProc     drvClose;     /* Close routine */
    DriverKillProc      drvKill;      /* Kill routine */
} DriverDispatchTable, *DriverDispatchTablePtr;

/* =============================================================================
 * Modern Driver Interface
 * ============================================================================= */

/**
 * Modern driver registration structure
 */
typedef struct ModernDriverInterface {
    const char         *driverName;      /* Driver name */
    const char         *devicePath;      /* Device path (e.g., "/dev/sda1") */
    uint32_t            driverVersion;   /* Driver version */
    uint32_t            driverType;      /* Driver type flags */

    /* Entry points */
    DriverDispatchTable dispatch;       /* Driver dispatch table */

    /* Modern callbacks */
    int (*init)(void *context);         /* Initialize driver */
    void (*cleanup)(void *context);     /* Cleanup driver */
    int (*suspend)(void *context);      /* Suspend for power management */
    int (*resume)(void *context);       /* Resume from suspend */

    void               *driverContext;   /* Driver-specific context */
} ModernDriverInterface, *ModernDriverInterfacePtr;

/* =============================================================================
 * Driver Context Structure
 * ============================================================================= */

typedef struct DriverContext {
    DCEPtr              dce;             /* Associated DCE */
    void               *driverData;      /* Driver-specific data */
    void               *deviceHandle;    /* Platform device handle */
    uint32_t            flags;           /* Driver flags */
    bool                isModern;        /* True if modern driver */
    ModernDriverInterfacePtr modernIf;   /* Modern interface (if applicable) */
} DriverContext, *DriverContextPtr;

/* =============================================================================
 * Driver Communication Constants
 * ============================================================================= */

/* Standard control codes */
#define kControlKillIO          1       /* Kill pending I/O */
#define kControlGoodbye         -1      /* Driver goodbye */
#define kControlAccRun          65      /* Accessory run */
#define kControlDriverGestalt   43      /* Driver gestalt */

/* Standard status codes */
#define kStatusGetDCE           1       /* Get DCE handle */
#define kStatusDriverGestalt    43      /* Driver gestalt */

/* Device types */
#define kDeviceTypeUnknown      0       /* Unknown device */
#define kDeviceTypeStorage      1       /* Storage device */
#define kDeviceTypeNetwork      2       /* Network device */
#define kDeviceTypeSerial       3       /* Serial device */
#define kDeviceTypePrinter      4       /* Printer device */
#define kDeviceTypeVideo        5       /* Video device */
#define kDeviceTypeAudio        6       /* Audio device */
#define kDeviceTypeInput        7       /* Input device */

/* =============================================================================
 * Driver Registration Functions
 * ============================================================================= */

/**
 * Register a classic Mac OS driver
 *
 * @param driverResource Handle to driver resource
 * @param refNum Desired reference number
 * @return Error code
 */
int16_t RegisterClassicDriver(Handle driverResource, int16_t refNum);

/**
 * Register a modern native driver
 *
 * @param driverInterface Modern driver interface
 * @param refNum Desired reference number
 * @return Error code
 */
int16_t RegisterModernDriver(ModernDriverInterfacePtr driverInterface, int16_t refNum);

/**
 * Unregister a driver
 *
 * @param refNum Reference number of driver to unregister
 * @return Error code
 */
int16_t UnregisterDriver(int16_t refNum);

/* =============================================================================
 * Driver Dispatch Functions
 * ============================================================================= */

/**
 * Dispatch a call to a driver
 *
 * @param selector Driver entry point selector
 * @param pb Parameter block
 * @param dce Device Control Entry
 * @return Error code
 */
int16_t DispatchDriverCall(DriverSelector selector, void *pb, DCEPtr dce);

/**
 * Call driver Open routine
 *
 * @param pb Parameter block
 * @param dce Device Control Entry
 * @return Error code
 */
int16_t CallDriverOpen(IOParamPtr pb, DCEPtr dce);

/**
 * Call driver Prime routine (Read/Write)
 *
 * @param pb Parameter block
 * @param dce Device Control Entry
 * @return Error code
 */
int16_t CallDriverPrime(IOParamPtr pb, DCEPtr dce);

/**
 * Call driver Control routine
 *
 * @param pb Parameter block
 * @param dce Device Control Entry
 * @return Error code
 */
int16_t CallDriverControl(CntrlParamPtr pb, DCEPtr dce);

/**
 * Call driver Status routine
 *
 * @param pb Parameter block
 * @param dce Device Control Entry
 * @return Error code
 */
int16_t CallDriverStatus(CntrlParamPtr pb, DCEPtr dce);

/**
 * Call driver Close routine
 *
 * @param pb Parameter block
 * @param dce Device Control Entry
 * @return Error code
 */
int16_t CallDriverClose(IOParamPtr pb, DCEPtr dce);

/**
 * Call driver Kill routine
 *
 * @param pb Parameter block
 * @param dce Device Control Entry
 * @return Error code
 */
int16_t CallDriverKill(IOParamPtr pb, DCEPtr dce);

/* =============================================================================
 * Driver Validation Functions
 * ============================================================================= */

/**
 * Validate a classic driver resource
 *
 * @param driverResource Handle to driver resource
 * @return True if valid
 */
bool ValidateClassicDriver(Handle driverResource);

/**
 * Validate a modern driver interface
 *
 * @param driverInterface Modern driver interface
 * @return True if valid
 */
bool ValidateModernDriver(ModernDriverInterfacePtr driverInterface);

/**
 * Get driver capabilities flags
 *
 * @param dce Device Control Entry
 * @return Capability flags
 */
uint16_t GetDriverCapabilities(DCEPtr dce);

/**
 * Check if driver supports operation
 *
 * @param dce Device Control Entry
 * @param operation Operation to check (read, write, control, status)
 * @return True if supported
 */
bool DriverSupportsOperation(DCEPtr dce, DriverSelector operation);

/* =============================================================================
 * Driver Helper Functions
 * ============================================================================= */

/**
 * Get driver name from DCE
 *
 * @param dce Device Control Entry
 * @param name Buffer to store name
 * @param maxLen Maximum length of name buffer
 * @return Length of name or error code
 */
int16_t GetDriverName(DCEPtr dce, char *name, int16_t maxLen);

/**
 * Create a driver context
 *
 * @param dce Device Control Entry
 * @param isModern True if modern driver
 * @return Driver context or NULL on error
 */
DriverContextPtr CreateDriverContext(DCEPtr dce, bool isModern);

/**
 * Destroy a driver context
 *
 * @param context Driver context to destroy
 */
void DestroyDriverContext(DriverContextPtr context);

/**
 * Get driver context from DCE
 *
 * @param dce Device Control Entry
 * @return Driver context or NULL
 */
DriverContextPtr GetDriverContext(DCEPtr dce);

#ifdef __cplusplus
}
#endif

#endif /* DRIVER_INTERFACE_H */