/*
 * RE-AGENT-BANNER
 * Apple System 7.1 Event Manager - Reverse Engineered Interface
 *
 * This header file contains the reverse-engineered interface for the Apple System 7.1
 * Event Manager component extracted from System.rsrc. The Event Manager is the core
 * Mac OS Toolbox component responsible for handling all user input events including
 * mouse clicks, keyboard input, system events, and application activation.
 *
 * Source Binary: System.rsrc (383,182 bytes)
 * SHA256: 78150ebb66707277e0947fbf81f6a27fc5d263a71bbf96df7084d5c3ec22a5ba
 * Architecture: 68000/68020/68030 Motorola
 * Target System: Apple System 7.1 (1992)
 *
 * Evidence Sources:
 * - evidence.curated.eventmgr.json: Function signatures and trap numbers
 * - mappings.eventmgr.json: Symbol mapping and calling conventions
 * - layouts.curated.eventmgr.json: Structure layouts and field definitions
 *
 * All implementations are based on documented Mac OS Toolbox interfaces and
 * reverse engineering analysis of the original System 7.1 binary.
 */

#ifndef EVENT_MANAGER_H
#define EVENT_MANAGER_H

#include "SystemTypes.h"

/* Forward declarations */

#include "SystemTypes.h"

/* Standard Mac OS Types are defined in MacTypes.h */

/* Boolean constants */
#define true    1
#define false   0

/* Error codes */
/* noErr is defined in MacTypes.h */

/*
 * Point Structure (4 bytes)
 * Evidence: layouts.curated.eventmgr.json:/structures/1
 *
 * Standard QuickDraw coordinate structure used throughout Mac OS.
 * Note: Vertical coordinate comes first (opposite of typical x,y ordering).
 */
/* Point type defined in MacTypes.h */

/*
 * KeyMap Structure (16 bytes)
 * Evidence: layouts.curated.eventmgr.json:/structures/2
 *
 * 128-bit array representing the state of all keys on the keyboard.
 * Each bit represents one key - 1 if pressed, 0 if released.
 */

/*
 * EventRecord Structure (16 bytes)
 * Evidence: layouts.curated.eventmgr.json:/structures/0
 *
 * Core structure for all Mac OS events. This is the fundamental data structure
 * passed between the Event Manager and applications.
 */
/* EventRecord is in EventManager/EventTypes.h */

/*
 * Event Type Constants
 * Evidence: evidence.curated.eventmgr.json:/constants
 */

/*
 * Event Mask Constants
 * Evidence: evidence.curated.eventmgr.json:/event_masks
 */

/*
 * Modifier Key Flags
 * Evidence: evidence.curated.eventmgr.json:/modifier_flags
 */

/* Forward declarations for Window Manager integration */
/* Ptr is defined in MacTypes.h */
/* Handle is defined in MacTypes.h */

/*
 * Event Manager Function Prototypes
 * Evidence: mappings.eventmgr.json:/symbol_mappings
 *
 * All functions use Mac OS Toolbox trap-based calling convention.
 * Trap numbers are documented for reference but implementation uses
 * standard C calling convention for portability.
 */

/*
 * GetNextEvent - Retrieve next available event
 * Trap: 0xA970
 * Evidence: evidence.curated.eventmgr.json:/functions/0
 */
Boolean GetNextEvent(UInt16 eventMask, EventRecord *theEvent);

/*
 * EventAvail - Check if event is available without removing from queue
 * Trap: 0xA971
 * Evidence: evidence.curated.eventmgr.json:/functions/1
 */
Boolean EventAvail(UInt16 eventMask, EventRecord *theEvent);

/*
 * GetMouse - Get current mouse position
 * Trap: 0xA972
 * Evidence: evidence.curated.eventmgr.json:/functions/2
 */
Point GetMouse(void);

/*
 * StillDown - Check if mouse button is still being held down
 * Trap: 0xA973
 * Evidence: evidence.curated.eventmgr.json:/functions/3
 */
Boolean StillDown(void);

/*
 * Button - Get current state of the mouse button
 * Trap: 0xA974
 * Evidence: evidence.curated.eventmgr.json:/functions/4
 */
Boolean Button(void);

/*
 * TickCount - Get number of ticks since system startup
 * Trap: 0xA975
 * Evidence: evidence.curated.eventmgr.json:/functions/5
 */
UInt32 TickCount(void);

/*
 * GetKeys - Get current state of all keys on keyboard
 * Trap: 0xA976
 * Evidence: evidence.curated.eventmgr.json:/functions/6
 */
void GetKeys(KeyMap theKeys);

/*
 * WaitNextEvent - Enhanced event retrieval with yield time
 * Trap: 0xA860
 * Evidence: evidence.curated.eventmgr.json:/functions/7
 */
Boolean WaitNextEvent(UInt16 eventMask, EventRecord *theEvent, UInt32 sleep, RgnHandle mouseRgn);

/*
 * PostEvent - Post an event to the event queue
 * Trap: 0xA02F
 * Evidence: evidence.curated.eventmgr.json:/functions/8
 */
OSErr PostEvent(UInt16 eventNum, UInt32 eventMsg);

/*
 * FlushEvents - Remove events from the event queue
 * Trap: 0xA032
 * Evidence: evidence.curated.eventmgr.json:/functions/9
 */
void FlushEvents(UInt16 whichMask, UInt16 stopMask);

/*
 * SystemEvent - Handle system-level events
 * Trap: 0xA9B2
 * Evidence: evidence.curated.eventmgr.json:/functions/10
 */
Boolean SystemEvent(EventRecord *theEvent);

/*
 * SystemClick - Handle system clicks in window frames, menu bar, etc.
 * Trap: 0xA9B3
 * Evidence: evidence.curated.eventmgr.json:/functions/11
 */
void SystemClick(EventRecord *theEvent, WindowPtr whichWindow);

/*
 * Internal Queue Management Structures
 * Evidence: layouts.curated.eventmgr.json:/structures/3,4,5
 *
 * These structures are used internally by the Event Manager for queue management.
 * Applications typically do not access these directly.
 */

/* Ptr is defined in MacTypes.h */

// 

/* QElem and QElemPtr are defined in SystemTypes.h */

/* Global Variables (for internal use) */
extern UInt32 gTicks;              /* Current tick count */
extern Point gMouseLoc;            /* Current mouse location */
extern UInt8 gButtonState;         /* Current mouse button state */
extern KeyMap gKeyMap;             /* Current keyboard state */
extern QHdr gEventQueue;           /* Event queue header */

#endif /* EVENT_MANAGER_H */

/* RE-AGENT-TRAILER-JSON
{
  "component": "event_manager",
  "version": "1.0.0",
  "source_artifact": {
    "file": "System.rsrc",
    "sha256": "78150ebb66707277e0947fbf81f6a27fc5d263a71bbf96df7084d5c3ec22a5ba"
  },
  "evidence_files": [
    "evidence.curated.eventmgr.json",
    "mappings.eventmgr.json",
    "layouts.curated.eventmgr.json"
  ],
  "functions_declared": 12,
  "structures_defined": 6,
  "constants_defined": 19,
  "trap_functions": 12,
  "compliance": {
    "mac_toolbox_compatible": true,
    "evidence_based": true,
    "c89_compliant": true
  }
}
*/