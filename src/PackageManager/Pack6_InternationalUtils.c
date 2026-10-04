/*
 * Pack6_InternationalUtils.c - International Utilities Package (Pack6)
 *
 * Implements Pack6, the International Utilities Package for Mac OS System 7.
 * This package provides access to international resources and settings for
 * locale-specific formatting, measurement systems, and regional preferences.
 *
 * Based on Inside Macintosh: Operating System Utilities, Chapter 8
 */

#include "SystemTypes.h"
#include "System71StdLib.h"
#include "PackageManager/InternationalUtilities.h"

/* Forward declaration for Pack6 dispatcher */
OSErr Pack6_Dispatch(short selector, void* params);

/* Debug logging */
#define PACK6_DEBUG 0

#if PACK6_DEBUG
#define PACK6_LOG(...) do { \
    char buf[256]; \
    snprintf(buf, sizeof(buf), "[Pack6] " __VA_ARGS__); \
    serial_puts(buf); \
} while(0)
#else
#define PACK6_LOG(...)
#endif

/* Pack6 selectors */
#define kPack6GetIntl       0  /* Get international resource */
#define kPack6SetIntl       1  /* Set international resource */
#define kPack6Metric        2  /* Check if using metric system */
#define kPack6ClearCache    3  /* Clear international resource cache */

/* Parameter block for IUGetIntl */
typedef struct {
    SInt16  theID;      /* Input: International resource ID (0-3) */
    Handle  result;     /* Output: Handle to international resource */
} IUGetIntlParams;

/* Parameter block for IUSetIntl */
typedef struct {
    SInt16       refNum;      /* Input: Reference number (unused, for compatibility) */
    SInt16       theID;       /* Input: International resource ID (0-3) */
    const void*  intlParam;   /* Input: Pointer to international resource data */
} IUSetIntlParams;

/* Parameter block for IUMetric */
typedef struct {
    Boolean result;  /* Output: true if metric, false if imperial */
} IUMetricParams;

/*
 * Pack6_GetIntl - Get international resource handler
 *
 * Handles the Pack6 IUGetIntl selector. Retrieves an international resource
 * handle containing locale-specific settings.
 *
 * Parameters:
 *   params - Pointer to IUGetIntlParams structure
 *
 * Returns:
 *   noErr if successful
 *   paramErr if parameters are invalid
 */
static OSErr Pack6_GetIntl(IUGetIntlParams* params) {
    if (!params) {
        PACK6_LOG("GetIntl: NULL params\n");
        return paramErr;
    }

    PACK6_LOG("GetIntl: Getting international resource %d\n", params->theID);

    /* Call the actual function */
    params->result = IUGetIntl(params->theID);

    /* Note: IUGetIntl returns NULL on error, but we return noErr
     * because the function itself handles error logging */
    return noErr;
}

/*
 * Pack6_SetIntl - Set international resource handler
 *
 * Handles the Pack6 IUSetIntl selector. Sets or updates an international
 * resource with new locale settings.
 *
 * Parameters:
 *   params - Pointer to IUSetIntlParams structure
 *
 * Returns:
 *   noErr if successful
 *   paramErr if parameters are invalid
 */
static OSErr Pack6_SetIntl(IUSetIntlParams* params) {
    if (!params) {
        PACK6_LOG("SetIntl: NULL params\n");
        return paramErr;
    }

    if (!params->intlParam) {
        PACK6_LOG("SetIntl: NULL intlParam\n");
        return paramErr;
    }

    PACK6_LOG("SetIntl: Setting international resource %d\n", params->theID);

    /* Call the actual function */
    IUSetIntl(params->refNum, params->theID, params->intlParam);

    return noErr;
}

/*
 * Pack6_Metric - Check measurement system handler
 *
 * Handles the Pack6 IUMetric selector. Determines whether the system is
 * configured to use metric or imperial measurements.
 *
 * Parameters:
 *   params - Pointer to IUMetricParams structure
 *
 * Returns:
 *   noErr always (function cannot fail)
 */
static OSErr Pack6_Metric(IUMetricParams* params) {
    if (!params) {
        PACK6_LOG("Metric: NULL params\n");
        return paramErr;
    }

    PACK6_LOG("Metric: Checking measurement system\n");

    /* Call the actual function */
    params->result = IUMetric();

    PACK6_LOG("Metric: System is using %s measurements\n",
              params->result ? "metric" : "imperial");

    return noErr;
}

/*
 * Pack6_ClearCache - Clear cache handler
 *
 * Handles the Pack6 IUClearCache selector. Clears the cache of international
 * resources, forcing them to be reloaded on next access.
 *
 * Parameters:
 *   params - Not used (may be NULL)
 *
 * Returns:
 *   noErr always
 */
static OSErr Pack6_ClearCache(void* params) {
    (void)params;  /* Unused */

    PACK6_LOG("ClearCache: Clearing international resource cache\n");

    /* Call the actual function */
    IUClearCache();

    return noErr;
}

/*
 * Pack6_Dispatch - Pack6 package dispatcher
 *
 * Main dispatcher for the International Utilities Package (Pack6).
 * Routes selector calls to the appropriate international utility function.
 *
 * Parameters:
 *   selector - Function selector:
 *              0 = IUGetIntl (get international resource)
 *              1 = IUSetIntl (set international resource)
 *              2 = IUMetric (check if using metric)
 *              3 = IUClearCache (clear resource cache)
 *   params - Pointer to selector-specific parameter block
 *
 * Returns:
 *   noErr if successful
 *   paramErr if selector is invalid or params are NULL (except for ClearCache)
 *
 * Example usage through Package Manager:
 *   IUGetIntlParams params;
 *   params.theID = 0;  // Get Intl0 resource (date/time/number formatting)
 *   CallPackage(6, 0, &params);  // Call Pack6, IUGetIntl selector
 *   if (params.result != NULL) {
 *       // Use the international resource
 *   }
 *
 * Based on Inside Macintosh: Operating System Utilities, Chapter 8
 */
OSErr Pack6_Dispatch(short selector, void* params) {
    PACK6_LOG("Dispatch: selector=%d, params=%p\n", selector, params);

    switch (selector) {
        case kPack6GetIntl:
            PACK6_LOG("Dispatch: IUGetIntl\n");
            if (!params) {
                PACK6_LOG("Dispatch: NULL params for GetIntl\n");
                return paramErr;
            }
            return Pack6_GetIntl((IUGetIntlParams*)params);

        case kPack6SetIntl:
            PACK6_LOG("Dispatch: IUSetIntl\n");
            if (!params) {
                PACK6_LOG("Dispatch: NULL params for SetIntl\n");
                return paramErr;
            }
            return Pack6_SetIntl((IUSetIntlParams*)params);

        case kPack6Metric:
            PACK6_LOG("Dispatch: IUMetric\n");
            if (!params) {
                PACK6_LOG("Dispatch: NULL params for Metric\n");
                return paramErr;
            }
            return Pack6_Metric((IUMetricParams*)params);

        case kPack6ClearCache:
            PACK6_LOG("Dispatch: IUClearCache\n");
            /* ClearCache doesn't require params */
            return Pack6_ClearCache(params);

        default:
            PACK6_LOG("Dispatch: Invalid selector %d\n", selector);
            return paramErr;
    }
}
