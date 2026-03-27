/* Icon Resource Loading
 * Handles ICN#, cicn, BNDL/FREF lookups
 */

#include "Finder/Icon/icon_resources.h"
#include <string.h>
#include <stddef.h>
#include <stdlib.h>
#include "Resources/icons_generated.h"
#include "ResourceManager.h"
#include "SystemTypes.h"
#include "MemoryMgr/MemoryManager.h"

/* Resource types */
#define FOUR_CHAR_CODE(x) ((UInt32)(x))
#define kICN_Type  FOUR_CHAR_CODE('ICN#')
#define kCicn_Type FOUR_CHAR_CODE('cicn')

/* Static buffers for icon data (persistent across calls) */
static uint8_t gIconBitmap[128];
static uint8_t gIconMask[128];

/* Load icon family by resource ID */
bool IconRes_LoadFamilyByID(int16_t rsrcID, IconFamily* out) {
    /* First check generated icon table (imported resources) */
    if (IconGen_FindByID(rsrcID, out)) {
        return true;
    }

    /* Try to load from resource fork files */
    /* First try cicn (color icon) - preferred */
    Handle cicnHandle = GetResource(kCicn_Type, rsrcID);
    if (cicnHandle != NULL) {
        /* cicn format is complex (PixMap + ColorTable + pixel data)
         * For now, fall through to ICN# until cicn parser is implemented */
        ReleaseResource(cicnHandle);
    }

    /* Try ICN# (1-bit icon with mask) */
    Handle icnHandle = GetResource(kICN_Type, rsrcID);
    if (icnHandle == NULL) {
        return false;
    }

    /* Load the resource data into memory */
    LoadResource(icnHandle);

    /* Get resource size - should be 256 bytes (128 image + 128 mask) */
    SInt32 size = GetHandleSize(icnHandle);
    if (size != 256) {
        ReleaseResource(icnHandle);
        return false;
    }

    /* Lock the handle and get pointer to data */
    UInt8 state = HGetState(icnHandle);
    HLock(icnHandle);
    uint8_t* data = (uint8_t*)*icnHandle;

    /* Copy icon bitmap (first 128 bytes) */
    memcpy(gIconBitmap, data, 128);

    /* Copy icon mask (next 128 bytes) */
    memcpy(gIconMask, data + 128, 128);

    /* Restore handle state and release */
    HSetState(icnHandle, state);
    ReleaseResource(icnHandle);

    /* Fill IconFamily structure */
    memset(out, 0, sizeof(IconFamily));
    out->large.w = 32;
    out->large.h = 32;
    out->large.depth = kIconDepth1;
    out->large.img1b = gIconBitmap;
    out->large.mask1b = gIconMask;
    out->large.argb32 = NULL;
    out->hasSmall = false;

    return true;
}

/* Map type/creator to icon resource ID */
bool IconRes_MapTypeCreatorToIcon(uint32_t type, uint32_t creator, int16_t* outRsrcID) {
    /* Simple hardcoded mappings for now */

    /* Finder application */
    if (creator == 'MACS' || type == 'FNDR') {
        *outRsrcID = 999; /* Finder application icon (custom icon from finder.png) */
        return true;
    }

    /* Application */
    if (type == 'APPL') {
        *outRsrcID = 128;  /* Generic app icon */
        return true;
    }

    /* TeachText document */
    if (type == 'TEXT' && creator == 'ttxt') {
        *outRsrcID = 129;  /* TeachText document */
        return true;
    }

    /* SimpleText document */
    if (type == 'TEXT' && creator == 'stxt') {
        *outRsrcID = 130;
        return true;
    }

    /* Generic text file */
    if (type == 'TEXT') {
        *outRsrcID = 131;
        return true;
    }

    return false;
}

/* Load custom icon from path */
bool IconRes_LoadCustomIconForPath(const char* path, IconFamily* out) {
    if (!path || !out) {
        return false;
    }

    /* Check for Icon\r file in folder */
    /* Icon\r is a special file with 0x0D (carriage return) as last char of name */
    char iconPath[1024];
    size_t pathLen = strlen(path);

    if (pathLen + 7 > sizeof(iconPath)) {  /* 7 = "/Icon" + '\r' + '\0' */
        return false;
    }

    /* Build path to Icon\r file */
    strncpy(iconPath, path, sizeof(iconPath) - 1);
    iconPath[sizeof(iconPath) - 1] = '\0';
    {
        size_t len = strlen(iconPath);
        if (len > 0 && iconPath[len - 1] != '/') {
            iconPath[len++] = '/';
            iconPath[len] = '\0';
        }
        /* Append "Icon\r" */
        if (len + 5 < sizeof(iconPath)) {
            memcpy(&iconPath[len], "Icon\r", 5);
            iconPath[len + 5] = '\0';
        }
    }

    /* Try to open the Icon\r file's resource fork */
    /* Convert to unsigned char* for OpenResFile */
    unsigned char pIconPath[256];
    size_t iconPathLen = strlen(iconPath);
    if (iconPathLen > 255) {
        return false;
    }
    pIconPath[0] = (unsigned char)iconPathLen;
    memcpy(&pIconPath[1], iconPath, iconPathLen);

    /* Open resource fork of Icon\r file */
    SInt16 refNum = OpenResFile(pIconPath);
    if (refNum == -1) {
        /* No Icon\r file found */
        return false;
    }

    /* Try to load ICN# resource ID 128 (standard custom icon ID) */
    UseResFile(refNum);
    Handle icnHandle = Get1Resource(kICN_Type, 128);

    if (icnHandle != NULL) {
        /* Load and parse ICN# resource */
        LoadResource(icnHandle);
        SInt32 size = GetHandleSize(icnHandle);

        if (size == 256) {
            UInt8 state = HGetState(icnHandle);
            HLock(icnHandle);
            uint8_t* data = (uint8_t*)*icnHandle;

            /* Copy icon bitmap (first 128 bytes) */
            memcpy(gIconBitmap, data, 128);

            /* Copy icon mask (next 128 bytes) */
            memcpy(gIconMask, data + 128, 128);

            HSetState(icnHandle, state);
            ReleaseResource(icnHandle);

            /* Fill IconFamily structure */
            memset(out, 0, sizeof(IconFamily));
            out->large.w = 32;
            out->large.h = 32;
            out->large.depth = kIconDepth1;
            out->large.img1b = gIconBitmap;
            out->large.mask1b = gIconMask;
            out->large.argb32 = NULL;
            out->hasSmall = false;

            CloseResFile(refNum);
            return true;
        }

        ReleaseResource(icnHandle);
    }

    CloseResFile(refNum);

    /* HFS+ extended attributes not supported yet */
    return false;
}
