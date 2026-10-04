/*
 * DeskManager.h - Main Desk Manager API
 *
 * Provides the core API for managing desk accessories (DAs) in System 7.1.
 * Desk accessories are small utility programs that provide essential daily
 * computing functions like Calculator, Key Caps, Alarm Clock, and Chooser.
 *
 * Derived from ROM analysis (System 7)
 */

#ifndef DESKMANAGER_H
#define DESKMANAGER_H

#include "SystemTypes.h"
#include "DeskManagerTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Desk Manager Constants */
#define DESK_MGR_VERSION        0x0701      /* System 7.1 */
#define MAX_DESK_ACCESSORIES    64          /* Maximum concurrent DAs */
#define DA_NAME_LENGTH          31          /* Maximum native DA name length */

/* Desk Accessory Types */
#define DA_TYPE_CALCULATOR      1
#define DA_TYPE_KEYCAPS         2
#define DA_TYPE_ALARM           3
#define DA_TYPE_CHOOSER         4
#define DA_TYPE_NOTEPAD         5

/**
 * Initialize the Desk Manager
 * @return 0 on success, negative on error
 */
int DeskManager_Initialize(void);

/**
 * Shutdown the Desk Manager
 */
void DeskManager_Shutdown(void);

/**
 * Open a desk accessory by name
 * @param name Name of the desk accessory
 * @return Reference number on success, negative on error
 */
SInt16 OpenDeskAcc(const char *name);

/**
 * Close a desk accessory
 * @param refNum Reference number of the DA to close
 */
void CloseDeskAcc(SInt16 refNum);

/**
 * Suspend a desk accessory (pause its execution)
 * @param refNum Reference number of the DA to suspend
 * @return 0 on success, negative on error
 */
int DeskManager_SuspendDA(SInt16 refNum);

/**
 * Resume a desk accessory (resume its execution)
 * @param refNum Reference number of the DA to resume
 * @return 0 on success, negative on error
 */
int DeskManager_ResumeDA(SInt16 refNum);

/**
 * Suspend all desk accessories
 * @return Number of DAs suspended
 */
int DeskManager_SuspendAll(void);

/**
 * Resume all desk accessories
 * @return Number of DAs resumed
 */
int DeskManager_ResumeAll(void);

/**
 * Handle system-level events for desk accessories
 * @param event Pointer to event record
 * @return true if event was handled by a DA
 */
Boolean SystemEvent(const EventRecord *event);

/**
 * Handle mouse clicks in system areas
 * @param event Pointer to event record
 * @param window Window where click occurred
 */
void SystemClick(const EventRecord *event, WindowRecord *window);

/**
 * Redraw the desk accessory that owns a window
 * @param window Window being updated
 * @param event The update event, delivered to the DA if it has no update proc
 * @return true if an accessory owned the window and redrew it
 */
Boolean SystemUpdate(WindowRecord *window, const EventRecord *event);

/**
 * Perform periodic processing for all DAs
 */
void SystemTask(void);

/**
 * Route a selection from the active accessory's registered menu
 * @param menuResult Menu selection result
 */
void SystemMenu(SInt32 menuResult);

/**
 * Handle system edit operations
 * @param editCmd Edit command (cut, copy, paste, etc.)
 * @return true if command was handled
 */
Boolean SystemEdit(SInt16 editCmd);

/* Desk Accessory Management Functions */

/**
 * Get desk accessory by reference number
 * @param refNum Reference number
 * @return Pointer to DA or NULL if not found
 */
DeskAccessory *DA_GetByRefNum(SInt16 refNum);

/**
 * Get desk accessory by name
 * @param name DA name
 * @return Pointer to DA or NULL if not found
 */
DeskAccessory *DA_GetByName(const char *name);

/**
 * Get currently active desk accessory
 * @return Pointer to active DA or NULL if none
 */
DeskAccessory *DA_GetActive(void);

/**
 * Set active desk accessory
 * @param da Pointer to DA to activate
 * @return 0 on success, negative on error
 */
int DA_SetActive(DeskAccessory *da);

/**
 * Send message to desk accessory
 * @param da Pointer to DA
 * @param message Message type
 * @param param1 First parameter
 * @param param2 Second parameter
 * @return Result from DA
 */
int DA_SendMessage(DeskAccessory *da, DAMessage message,
                   void *param1, void *param2);

/* DA Preferences and Persistence */

/**
 * Set DA preference (key-value pair)
 * @param daName DA name
 * @param key Preference key
 * @param value Preference value
 * @return 0 on success, negative on error
 */
int DA_SetPreference(const char *daName, const char *key, const char *value);

/**
 * Get DA preference
 * @param daName DA name
 * @param key Preference key
 * @param buffer Buffer to store value
 * @param bufferSize Size of buffer
 * @return 0 on success, negative on error
 */
int DA_GetPreference(const char *daName, const char *key, char *buffer, int bufferSize);

/**
 * Save DA preferences to storage
 * @param daName DA name (NULL for all)
 * @return 0 on success, negative on error
 */
int DA_SavePreferences(const char *daName);

/**
 * Load DA preferences from storage
 * @param daName DA name (NULL for all)
 * @return 0 on success, negative on error
 */
int DA_LoadPreferences(const char *daName);



/* Utility Functions */

/**
 * Get Desk Manager version
 * @return Version number (0x0701 for System 7.1)
 */
UInt16 DeskManager_GetVersion(void);

/**
 * Get number of open desk accessories
 * @return Number of open DAs
 */
SInt16 DeskManager_GetDACount(void);

/**
 * Check if a DA is installed
 * @param name DA name
 * @return true if DA is available
 */
Boolean DeskManager_IsDAAvailable(const char *name);

/* Built-in Desk Accessories */

/**
 * Register built-in desk accessories
 * @return 0 on success, negative on error
 */
int DeskManager_RegisterBuiltinDAs(void);

/* Additional error-code names and legacy spelling. */
#define DESK_ERR_INVALID_PARAM  DESK_ERR_PARAM_ERROR
#define DESK_ERR_DA_ERROR       -6      /* DA-specific error */

#ifdef __cplusplus
}
#endif

#endif /* DESKMANAGER_H */
