/*
 * DeviceManager.h
 * System 7.1 Device Manager - Main API Interface
 *
 * Portable C implementation of the Mac OS System 7.1 Device Manager.
 * This header provides the complete Device Manager API for device driver
 * management and I/O operations.
 */

#ifndef DEVICE_MANAGER_H
#define DEVICE_MANAGER_H

#include "DeviceTypes.h"
#include "ResourceManager.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Device Manager Core Functions
 * ============================================================================= */

/**
 * Initialize the Device Manager subsystem
 *
 * @return Error code (noErr on success)
 */
int16_t DeviceManager_Initialize(void);

/**
 * Shutdown the Device Manager subsystem
 */
void DeviceManager_Shutdown(void);

/**
 * Get the DCE handle for a given reference number
 *
 * @param refNum Driver reference number
 * @return DCE handle or NULL if not found
 */
DCEHandle GetDCtlEntry(int16_t refNum);

/**
 * Install a driver from a driver resource
 *
 * @param drvrPtr Pointer to driver header
 * @param refNum Desired reference number
 * @return Error code
 */
int16_t DrvrInstall(DriverHeaderPtr drvrPtr, int16_t refNum);

/**
 * Install a driver with reserved memory
 *
 * @param drvrPtr Pointer to driver header
 * @param refNum Desired reference number
 * @return Error code
 */
int16_t DrvrInstallResrvMem(DriverHeaderPtr drvrPtr, int16_t refNum);

/**
 * Remove a driver from the system
 *
 * @param refNum Driver reference number
 * @return Error code
 */
int16_t DrvrRemove(int16_t refNum);

/* =============================================================================
 * Device I/O Operations
 * ============================================================================= */

/**
 * Open a device driver
 *
 * @param name Driver name (Pascal string)
 * @param drvrRefNum Pointer to store reference number
 * @return Error code
 */
int16_t OpenDriver(const uint8_t *name, int16_t *drvrRefNum);

/**
 * Close a device driver
 *
 * @param refNum Driver reference number
 * @return Error code
 */
int16_t CloseDriver(int16_t refNum);

/**
 * Perform a Control operation on a device
 *
 * @param refNum Driver reference number
 * @param csCode Control code
 * @param csParamPtr Pointer to control parameters
 * @return Error code
 */
int16_t Control(int16_t refNum, int16_t csCode, const void *csParamPtr);

/**
 * Perform a Status operation on a device
 *
 * @param refNum Driver reference number
 * @param csCode Status code
 * @param csParamPtr Pointer to status parameters
 * @return Error code
 */
int16_t Status(int16_t refNum, int16_t csCode, void *csParamPtr);

/**
 * Kill pending I/O operations on a device
 *
 * @param refNum Driver reference number
 * @return Error code
 */
int16_t KillIO(int16_t refNum);

/* =============================================================================
 * Parameter Block I/O Operations
 * ============================================================================= */

/**
 * Open operation using parameter block
 *
 * @param paramBlock Pointer to I/O parameter block
 * @param async True for asynchronous operation
 * @return Error code
 */
int16_t PBOpen(IOParamPtr paramBlock, bool async);

/**
 * Close operation using parameter block
 *
 * @param paramBlock Pointer to I/O parameter block
 * @param async True for asynchronous operation
 * @return Error code
 */
int16_t PBClose(IOParamPtr paramBlock, bool async);

/**
 * Read operation using parameter block
 *
 * @param paramBlock Pointer to I/O parameter block
 * @param async True for asynchronous operation
 * @return Error code
 */
int16_t PBRead(IOParamPtr paramBlock, bool async);

/**
 * Write operation using parameter block
 *
 * @param paramBlock Pointer to I/O parameter block
 * @param async True for asynchronous operation
 * @return Error code
 */
int16_t PBWrite(IOParamPtr paramBlock, bool async);

/**
 * Control operation using parameter block
 *
 * @param paramBlock Pointer to control parameter block
 * @param async True for asynchronous operation
 * @return Error code
 */
int16_t PBControl(CntrlParamPtr paramBlock, bool async);

/**
 * Status operation using parameter block
 *
 * @param paramBlock Pointer to control parameter block
 * @param async True for asynchronous operation
 * @return Error code
 */
int16_t PBStatus(CntrlParamPtr paramBlock, bool async);

/**
 * Kill I/O operation using parameter block
 *
 * @param paramBlock Pointer to I/O parameter block
 * @param async True for asynchronous operation
 * @return Error code
 */
int16_t PBKillIO(IOParamPtr paramBlock, bool async);

/* =============================================================================
 * Synchronous/Asynchronous Variants
 * ============================================================================= */

/* Synchronous variants */
#define PBOpenSync(pb)      PBOpen((pb), false)
#define PBCloseSync(pb)     PBClose((pb), false)
#define PBReadSync(pb)      PBRead((pb), false)
#define PBWriteSync(pb)     PBWrite((pb), false)
#define PBControlSync(pb)   PBControl((pb), false)
#define PBStatusSync(pb)    PBStatus((pb), false)
#define PBKillIOSync(pb)    PBKillIO((pb), false)

/* Asynchronous variants */
#define PBOpenAsync(pb)     PBOpen((pb), true)
#define PBCloseAsync(pb)    PBClose((pb), true)
#define PBReadAsync(pb)     PBRead((pb), true)
#define PBWriteAsync(pb)    PBWrite((pb), true)
#define PBControlAsync(pb)  PBControl((pb), true)
#define PBStatusAsync(pb)   PBStatus((pb), true)
#define PBKillIOAsync(pb)   PBKillIO((pb), true)

/* =============================================================================
 * Driver Management
 * ============================================================================= */

/**
 * Load a driver from resources
 *
 * @param driverName Name of driver resource
 * @param resID Resource ID (0 to search by name)
 * @return Handle to driver resource
 */
Handle LoadDriverResource(const uint8_t *driverName, int16_t resID);

/**
 * Find a driver by name in the unit table
 *
 * @param driverName Name to search for
 * @return Reference number or error code
 */
int16_t FindDriverByName(const uint8_t *driverName);

/**
 * Allocate a new unit table entry
 *
 * @return Reference number or error code
 */
int16_t AllocateUnitTableEntry(void);

/**
 * Deallocate a unit table entry
 *
 * @param refNum Reference number to deallocate
 * @return Error code
 */
int16_t DeallocateUnitTableEntry(int16_t refNum);

/* =============================================================================
 * I/O Completion and Async Support
 * ============================================================================= */

/**
 * Mark an I/O operation as complete
 *
 * @param paramBlock Parameter block to complete
 */
void IODone(IOParamPtr paramBlock);

/**
 * Add a parameter block to a driver's I/O queue
 *
 * @param dce Device Control Entry
 * @param paramBlock Parameter block to queue
 */
void EnqueueIORequest(DCEPtr dce, IOParamPtr paramBlock);

/**
 * Remove the next parameter block from a driver's I/O queue
 *
 * @param dce Device Control Entry
 * @return Next parameter block or NULL
 */
IOParamPtr DequeueIORequest(DCEPtr dce);

/**
 * Check if a driver has pending I/O operations
 *
 * @param dce Device Control Entry
 * @return True if driver has pending operations
 */
bool DriverHasPendingIO(DCEPtr dce);

/* =============================================================================
 * Driver Validation and Security
 * ============================================================================= */

/**
 * Validate a driver before installation
 *
 * @param drvrPtr Pointer to driver header
 * @param size Size of driver in bytes
 * @return True if driver is valid
 */
bool ValidateDriver(DriverHeaderPtr drvrPtr, uint32_t size);

/**
 * Check if a reference number is valid
 *
 * @param refNum Reference number to check
 * @return True if valid
 */
bool IsValidRefNum(int16_t refNum);

/**
 * Check if a DCE is valid and open
 *
 * @param dce Device Control Entry to check
 * @return True if valid and open
 */
bool IsDCEValid(DCEPtr dce);

/* =============================================================================
 * Unit Table Management
 * ============================================================================= */

/**
 * Get the size of the unit table
 *
 * @return Number of entries in unit table
 */
int16_t GetUnitTableSize(void);

/**
 * Expand the unit table
 *
 * @param newSize New size for unit table
 * @return Error code
 */
int16_t ExpandUnitTable(int16_t newSize);

/**
 * Compact the unit table (remove unused entries)
 *
 * @return Number of entries removed
 */
int16_t CompactUnitTable(void);

/* =============================================================================
 * Device Manager Configuration
 * ============================================================================= */

/**
 * Set the chooser alert state
 *
 * @param alertState True to enable chooser alerts
 * @return Previous alert state
 */
bool SetChooserAlert(bool alertState);

/**
 * Get Device Manager statistics
 *
 * @param stats Pointer to statistics structure
 */
void GetDeviceManagerStats(void *stats);

/* =============================================================================
 * Modern Platform Abstraction
 * ============================================================================= */

/**
 * Register a modern device driver
 *
 * @param devicePath Path to device (e.g., "/dev/sda1")
 * @param driverType Type of driver
 * @param refNum Reference number to assign
 * @return Error code
 */
int16_t RegisterModernDevice(const char *devicePath, uint32_t driverType, int16_t refNum);

/**
 * Unregister a modern device driver
 *
 * @param refNum Reference number of device to unregister
 * @return Error code
 */
int16_t UnregisterModernDevice(int16_t refNum);

/**
 * Simulate hardware interrupt for device
 *
 * @param refNum Reference number of device
 * @param interruptType Type of interrupt
 * @return Error code
 */
int16_t SimulateDeviceInterrupt(int16_t refNum, uint32_t interruptType);

#ifdef __cplusplus
}
#endif

#endif /* DEVICE_MANAGER_H */