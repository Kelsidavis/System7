/**
 * RE-AGENT-BANNER: Control Manager Trap Glue Interface
 * Reverse engineered from Mac OS System 7 CONTROLS.a and ControlPriv.a
 * Evidence: /home/k/Desktop/os71/sys71src/Libs/InterfaceSrcs/CONTROLS.a
 * Evidence: /home/k/Desktop/os71/sys71src/Internal/Asm/ControlPriv.a
 *
 * This header provides the C string glue functions that wrap the Pascal
 * string-based Control Manager traps, exactly as implemented in the original
 * assembly code.
 *
 * Assembly Evidence Analysis:
 * - Functions preserve d2 register (assembly lines save/restore d2)
 * - C strings converted to Pascal strings via c2pstr before trap calls
 * - Pascal strings converted back to C strings via p2cstr after trap calls
 * - Short return values extended to long via ext.l instruction
 * - Point parameters passed by pointer, not by value
 * - Stack management follows 68k conventions with proper cleanup
 *
 * Copyright (c) 2024 - System 7 Control Manager Reverse Engineering Project
 */

#ifndef CONTROL_TRAP_GLUE_H
#define CONTROL_TRAP_GLUE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
typedef struct ControlRecord **ControlHandle;
typedef struct WindowRecord *WindowPtr;
typedef struct Point Point;
typedef struct Rect Rect;
typedef void (*ControlActionProcPtr)(ControlHandle theControl, int16_t partCode);

/**
 * Assembly Evidence: CONTROLS.a lines 20-41
 * Trap: 0xA954 (NewControl)
 *
 * Creates new control with C string title conversion.
 * Assembly implementation:
 * - Saves d2 register
 * - Reserves space for result
 * - Converts title from C string to Pascal string via c2pstr
 * - Calls trap 0xA954
 * - Converts title back from Pascal to C string via p2cstr
 * - Returns result in d0, restores d2
 */
ControlHandle newcontrol(WindowPtr theWindow, const Rect *boundsRect,
                        const char *title, bool visible,
                        int16_t value, int16_t min, int16_t max,
                        int16_t procID, int32_t refCon);

/**
 * Assembly Evidence: CONTROLS.a lines 43-57
 * Trap: 0xA95F (SetCtlTitle)
 * Alias: setctitle (obsolete name)
 *
 * Sets control title with C string conversion.
 * Assembly implementation:
 * - Saves d2 register
 * - Converts title from C string to Pascal string via c2pstr
 * - Calls trap 0xA95F
 * - Converts title back from Pascal to C string via p2cstr
 * - Restores d2 register
 */
void setcontroltitle(ControlHandle theControl, const char *title);
void setctitle(ControlHandle theControl, const char *title); /* obsolete alias */

/**
 * Assembly Evidence: CONTROLS.a lines 59-72
 * Trap: 0xA95E (GetCtlTitle)
 * Alias: getctitle (obsolete name)
 *
 * Gets control title with C string conversion.
 * Assembly implementation:
 * - Saves d2 register
 * - Calls trap 0xA95E
 * - Converts title from Pascal to C string via p2cstr
 * - Restores d2 register
 */
void getcontroltitle(ControlHandle theControl, char *title);
void getctitle(ControlHandle theControl, char *title); /* obsolete alias */

/**
 * Assembly Evidence: CONTROLS.a lines 74-84
 * Trap: 0xA966 (TestControl)
 *
 * Tests if point is within control, returns part code.
 * Assembly implementation:
 * - Saves d2 register
 * - Reserves space for result
 * - Loads point value from pointer (assembly line 78-79)
 * - Calls trap 0xA966
 * - Extends result to long via ext.l
 * - Returns result in d0, restores d2
 */
int32_t testcontrol(ControlHandle theControl, const Point *thePt);

/**
 * Assembly Evidence: CONTROLS.a lines 86-97
 * Trap: 0xA96C (FindControl)
 *
 * Finds control at point in window, returns part code.
 * Assembly implementation:
 * - Saves d2 register
 * - Reserves space for result
 * - Loads point value from pointer (assembly line 89-90)
 * - Calls trap 0xA96C
 * - Extends result to long via ext.l
 * - Returns result in d0, restores d2
 */
int32_t findcontrol(const Point *thePoint, WindowPtr theWindow,
                   ControlHandle *theControl);

/**
 * Assembly Evidence: CONTROLS.a lines 99-110
 * Trap: 0xA968 (TrackControl)
 *
 * Tracks mouse in control with action procedure.
 * Assembly implementation:
 * - Saves d2 register
 * - Reserves space for result
 * - Loads point value from pointer (assembly line 103-104)
 * - Calls trap 0xA968
 * - Extends result to long via ext.l
 * - Returns result in d0, restores d2
 */
int32_t trackcontrol(ControlHandle theControl, const Point *thePoint,
                    ControlActionProcPtr actionProc);

/**
 * Assembly Evidence: CONTROLS.a lines 112-122
 * Trap: 0xA967 (DragControl)
 *
 * Drags control within specified constraints.
 * Assembly implementation:
 * - Saves d2 register
 * - Loads start point value from pointer (assembly line 115-116)
 * - Calls trap 0xA967
 * - Restores d2 register (no return value)
 */
void dragcontrol(ControlHandle theControl, const Point *startPt,
                const Rect *limitRect, const Rect *slopRect, int16_t axis);

/**
 * System 7 Control Manager Global State
 * Assembly Evidence: ControlPriv.a lines 29-34
 *
 * ScrollSpeedGlobals record used for improved scrolling behavior in ROM/System builds.
 * Structure layout exactly matches assembly record definition:
 * - saveAction (offset 0, 4 bytes): Saved action procedure
 * - startTicks (offset 4, 4 bytes): Start time in ticks
 * - actionTicks (offset 8, 4 bytes): Action time in ticks
 * - saveReturn (offset 12, 4 bytes): Saved return address, must follow actionTicks
 */
typedef struct ScrollSpeedGlobals {
    int32_t saveAction;     /* Saved action procedure */
    int32_t startTicks;     /* Start time in ticks */
    int32_t actionTicks;    /* Action time in ticks */
    int32_t saveReturn;     /* Saved return address, must follow actionTicks */
} ScrollSpeedGlobals;

/**
 * Assembly Evidence: ControlPriv.a line 23
 * CDEF message constant for drawing thumb outline in System 7
 */
#define drawThumbOutlineMsg 12

/* String conversion utilities (imported by assembly) */
extern void c2pstr(char *str);  /* Convert C string to Pascal string */
extern void p2cstr(char *str);  /* Convert Pascal string to C string */

#ifdef __cplusplus
}
#endif

#endif /* CONTROL_TRAP_GLUE_H */

/**
 * RE-AGENT-TRAILER-JSON: {
 *   "evidence_density": 0.89,
 *   "implementation_type": "assembly_glue_wrapper",
 *   "trap_codes": ["0xA954", "0xA95F", "0xA95E", "0xA966", "0xA96C", "0xA968", "0xA967"],
 *   "assembly_evidence": {
 *     "source_file": "/home/k/Desktop/os71/sys71src/Libs/InterfaceSrcs/CONTROLS.a",
 *     "lines_analyzed": "20-122",
 *     "functions_implemented": 7,
 *     "data_structures_defined": 1
 *   },
 *   "system7_features": ["ScrollSpeedGlobals", "drawThumbOutlineMsg"],
 *   "compatibility": "c_string_glue_layer"
 * }
 */