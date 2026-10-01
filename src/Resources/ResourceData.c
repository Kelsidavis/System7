/*
 * ResourceData.c - System 7 Resource Data Management
 *
 * Provides access to embedded System 7 resources including icons, cursors,
 * patterns, and sounds extracted from original System 7 resource files.
 *
 * Based on Inside Macintosh: More Macintosh Toolbox
 */

#include "Resources/ResourceData.h"
#include "Resources/system7_resources.h"
#include "SystemTypes.h"
#include "System71StdLib.h"

/* Debug logging */
#define RESDATA_DEBUG 0

#if RESDATA_DEBUG
extern void serial_puts(const char* str);
#define RESDATA_LOG(...) do { \
    char buf[256]; \
    snprintf(buf, sizeof(buf), "[ResData] " __VA_ARGS__); \
    serial_puts(buf); \
} while(0)
#else
#define RESDATA_LOG(...)
#endif

/* Global state flag */
static Boolean gResourceDataInitialized = false;

/*
 * InitResourceData - Initialize the resource data system
 *
 * Initializes the embedded System 7 resource data system. This function
 * prepares access to built-in resources such as icons, cursors, patterns,
 * and sounds.
 *
 * Since all resources are statically compiled into the binary as const
 * arrays defined in system7_resources.h, this function primarily serves
 * as an initialization marker and can perform any needed one-time setup.
 *
 * Returns:
 *   noErr (0) on success
 *
 * Based on Inside Macintosh: More Macintosh Toolbox, Chapter 7
 */
OSErr InitResourceData(void) {
    if (gResourceDataInitialized) {
        /* Already initialized */
        RESDATA_LOG("InitResourceData: Already initialized\\n");
        return noErr;
    }

    /* Mark as initialized */
    gResourceDataInitialized = true;

    RESDATA_LOG("InitResourceData: Resource data system initialized\\n");

    /* All resources are statically defined in system7_resources.h,
     * so no dynamic initialization is needed. Future implementations
     * could add resource table setup, caching, or validation here. */

    return noErr;
}

/*
 * GetResourceDataInitialized - Check if resource data is initialized
 *
 * Returns:
 *   true if InitResourceData has been called, false otherwise
 */
Boolean GetResourceDataInitialized(void) {
    return gResourceDataInitialized;
}

/* The 32x32 alert icons, by kind: 1 stop, 2 note, 3 caution. */
const unsigned char* Alert_IconBitmap(SInt16 kind);
const unsigned char* Alert_IconBitmap(SInt16 kind) {
    switch (kind) {
        case 1:  return stop_icon;
        case 2:  return note_icon;
        case 3:  return caution_icon;
        default: return NULL;
    }
}
