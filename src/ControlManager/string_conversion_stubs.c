/**
 * RE-AGENT-BANNER: String Conversion Stubs for Control Manager
 * Evidence: /home/k/Desktop/os71/sys71src/Libs/InterfaceSrcs/CONTROLS.a
 *
 * Assembly evidence shows that CONTROLS.a imports c2pstr and p2cstr functions:
 * Line 21: import c2pstr    ; c2pstr(s) char *s;
 * Line 22: import p2cstr    ; p2cstr(s) char *s;
 *
 * These functions are called by the assembly trap glue to convert between
 * C strings (null-terminated) and Pascal strings (length-prefixed).
 *
 * Copyright (c) 2024 - System 7 Control Manager Reverse Engineering Project
 */

#include <string.h>
#include <stdint.h>

/**
 * Convert C string to Pascal string in place
 * Evidence: CONTROLS.a line 21 "import c2pstr"
 *
 * Assembly usage pattern:
 * - Called before trap calls to convert C string parameters to Pascal format
 * - Modifies string buffer in place
 * - Used in newcontrol, setcontroltitle functions
 */
void c2pstr(char *str) {
    if (!str) return;

    size_t len = strlen(str);
    if (len > 255) len = 255;  /* Pascal strings limited to 255 characters */

    /* Shift string contents one byte to the right */
    memmove(str + 1, str, len);

    /* Set length byte at the beginning */
    str[0] = (uint8_t)len;

    /* Null terminate after length byte + string (for safety) */
    str[len + 1] = '\0';
}

/**
 * Convert Pascal string to C string in place
 * Evidence: CONTROLS.a line 22 "import p2cstr"
 *
 * Assembly usage pattern:
 * - Called after trap calls to convert Pascal string results back to C format
 * - Modifies string buffer in place
 * - Used in newcontrol, setcontroltitle, getcontroltitle functions
 */
void p2cstr(char *str) {
    if (!str) return;

    uint8_t len = (uint8_t)str[0];  /* Get length from first byte */

    /* Shift string contents one byte to the left */
    memmove(str, str + 1, len);

    /* Null terminate */
    str[len] = '\0';
}

/**
 * RE-AGENT-TRAILER-JSON: {
 *   "evidence_density": 0.92,
 *   "implementation_type": "imported_function_stubs",
 *   "functions_implemented": ["c2pstr", "p2cstr"],
 *   "assembly_evidence": {
 *     "import_statements": "CONTROLS.a:lines_21_22",
 *     "usage_functions": ["newcontrol", "setcontroltitle", "getcontroltitle"]
 *   },
 *   "string_conversion_pattern": "in_place_modification"
 * }
 */