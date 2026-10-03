/*
 * DebugUtils.c - Debugging and Error Reporting Utilities
 *
 * Implements System 7 debugging support functions:
 * - Debugger: Drop into low-level debugger
 * - DebugStr: Display debug string and optionally drop into debugger
 *
 * These functions are used by applications and system software for
 * debugging, error reporting, and assertions.
 *
 * Based on Inside Macintosh: Operating System Utilities, Chapter 1
 */

#include "SystemTypes.h"
#include "System71StdLib.h"

/* Forward declarations */
void Debugger(void);
void DebugStr(ConstStr255Param debuggerMsg);

/* External serial output function */

/* Debug logging */
#define DEBUG_UTILS_DEBUG 1

#if DEBUG_UTILS_DEBUG
#define DBG_LOG(...) do { \
    char buf[256]; \
    snprintf(buf, sizeof(buf), "[DebugUtils] " __VA_ARGS__); \
    serial_puts(buf); \
} while(0)
#else
#define DBG_LOG(...)
#endif

/*
 * Debugger - Drop into low-level debugger
 *
 * Invokes the low-level debugger (MacsBug or similar). In classic Mac OS,
 * this would display the debugger screen and allow inspection of memory,
 * registers, and stack. Applications use this as a debugging breakpoint.
 *
 * In our implementation, this logs the event and halts execution in a
 * controlled manner that can be inspected via serial output.
 *
 * Parameters:
 *   None
 *
 * Side effects:
 *   - Logs debug entry to serial port
 *   - In debug builds, may halt or enter interactive debugger
 *   - In release builds, may be a no-op or minimal logging
 *
 * Notes:
 * - Applications call this with _Debugger trap ($A9FF)
 * - Often used in Assert() macros
 * - Classic MacsBug would show: "User break at $XXXXXXXX"
 * - Developers can set conditional breakpoints with this
 *
 * Example usage:
 *   if (ptr == NULL) {
 *       DebugStr("\pNULL pointer in MyFunction");
 *       Debugger();  // Drop into debugger
 *   }
 *
 * Example with assertions:
 *   #define Assert(cond) \
 *       if (!(cond)) { \
 *           DebugStr("\pAssertion failed: " #cond); \
 *           Debugger(); \
 *       }
 *
 * Based on Inside Macintosh: Operating System Utilities, Chapter 1-14
 */
void Debugger(void) {
    DBG_LOG("*** DEBUGGER() INVOKED ***\n");
    DBG_LOG("Debugger: Application called Debugger() trap\n");
    DBG_LOG("Debugger: This would normally enter MacsBug or low-level debugger\n");

    /* In classic Mac OS, this would:
     * 1. Save all registers
     * 2. Display debugger screen (MacsBug)
     * 3. Show disassembly at current PC
     * 4. Allow memory inspection, breakpoints, stack trace
     * 5. Resume execution with 'g' command
     *
     * In our implementation, we log the event. A real debugger
     * integration could set a breakpoint here or dump state.
     */

#ifdef DEBUG_BUILD
    /* In debug builds, we could halt execution */
    DBG_LOG("Debugger: Halting execution for inspection\n");
    DBG_LOG("Debugger: (In MacsBug, you would type 'g' to continue)\n");

    /* Infinite loop that can be broken by external debugger */
    volatile int debugger_break = 1;
    while (debugger_break) {
        /* External debugger can set debugger_break = 0 to continue */
        __asm__ volatile("nop");
    }
#else
    /* In release builds, just log and continue */
    DBG_LOG("Debugger: Continuing execution (release build)\n");
#endif

    DBG_LOG("Debugger: Returning from debugger\n");
}

/*
 * DebugStr - Display debug string
 *
 * Displays a Pascal string as a debug message and optionally drops into
 * the low-level debugger. In classic Mac OS with MacsBug installed, this
 * would show the string in the debugger screen.
 *
 * Without MacsBug, the string might be logged to a debug file or ignored.
 * In System 7, this is commonly used for assertions and error reporting
 * during development.
 *
 * Parameters:
 *   debuggerMsg - Pascal string (length byte + text) containing debug message
 *                 Maximum 255 characters
 *
 * Side effects:
 *   - Logs message to serial port
 *   - May invoke Debugger() depending on system configuration
 *   - Does not return if debugger halts
 *
 * Notes:
 * - Applications call this with _DebugStr trap ($ABFF)
 * - String format is Pascal: byte 0 = length, bytes 1-N = text
 * - MacsBug would display: "# <message text>"
 * - Commonly used in assertions and precondition checks
 * - Can be conditionally compiled out in release builds
 *
 * Example usage:
 *   DebugStr("\pInvalid parameter in OpenFile");
 *
 *   // With Str255 variable:
 *   Str255 msg;
 *   sprintf((char*)msg+1, "Value out of range: %d", value);
 *   msg[0] = strlen((char*)msg+1);
 *   DebugStr(msg);
 *
 * Example with Debugger():
 *   if (handle == NULL) {
 *       DebugStr("\pNULL handle in critical section");
 *       Debugger();  // Halt for inspection
 *   }
 *
 * Preprocessor macro for debug-only code:
 *   #ifdef DEBUG
 *   #define DebugMsg(msg) DebugStr("\p" msg)
 *   #else
 *   #define DebugMsg(msg)
 *   #endif
 *
 * Based on Inside Macintosh: Operating System Utilities, Chapter 1-15
 */
void DebugStr(ConstStr255Param debuggerMsg) {
    if (debuggerMsg == NULL) {
        DBG_LOG("DebugStr: NULL message pointer\n");
        return;
    }

    /* Pascal string: byte 0 is length, bytes 1-length are the text */
    unsigned char length = debuggerMsg[0];

    if (length == 0) {
        DBG_LOG("DebugStr: Empty debug string\n");
        return;
    }

    /* Convert Pascal string to C string for logging */
    char c_str[256];

    /* Copy Pascal string to C string */
    memcpy(c_str, debuggerMsg + 1, length);
    c_str[length] = '\0';

    /* Log the debug message */
    DBG_LOG("*** DEBUG STRING ***\n");
    DBG_LOG("DebugStr: %s\n", c_str);

    /* In classic Mac OS with MacsBug:
     * - Would display "# <message>" in debugger screen
     * - Would show disassembly and register state
     * - User could type 'g' to continue or set breakpoints
     *
     * Without MacsBug:
     * - Message might be written to debug log file
     * - Or displayed in a modal alert dialog
     * - Or ignored completely
     */

#ifdef DEBUG_BUILD
    /* In debug builds, optionally invoke the debugger */
    DBG_LOG("DebugStr: Invoking Debugger()\n");
    Debugger();
#else
    /* In release builds, just log the message */
    DBG_LOG("DebugStr: Message logged (release build - no debugger)\n");
#endif
}
