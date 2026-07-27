#include "MemoryMgr/MemoryManager.h"
/*
#include "DialogManager/DialogInternal.h"
 * DialogResources.c - Dialog Resource Management Implementation
 *
 * This module provides resource loading and management for dialog templates
 * (DLOG), dialog item lists (DITL), and alert templates (ALRT), maintaining
 * exact Mac System 7.1 compatibility.
 */

#include <stdlib.h>
#include <string.h>

#include "SystemTypes.h"
#include "System71StdLib.h"
#include "DialogManager/DialogResources.h"
#include "DialogManager/DialogTypes.h"
#include "DialogManager/DialogItems.h"
#include "DialogManager/DialogLogging.h"

/* External Resource Manager functions */
extern Handle GetResource(ResType theType, SInt16 theID);
extern void ReleaseResource(Handle theResource);
extern SInt32 GetHandleSize(Handle h);
extern void HLock(Handle h);
extern void HUnlock(Handle h);

/* Dialog item type constants from Inside Macintosh */
#define kDITLItemType_Button      4
#define kDITLItemType_CheckBox    5
#define kDITLItemType_RadioButton 6
#define kDITLItemType_Control     7
#define kDITLItemType_StaticText  8
#define kDITLItemType_EditText    16
#define kDITLItemType_Icon        32
#define kDITLItemType_Picture     64
#define kDITLItemType_UserItem    0

/* Global resource state */
static struct {
    Boolean initialized;
    Boolean cachingEnabled;
    SInt16 resourceFile;
    char resourcePath[256];
    char languageCode[8];
} gResourceState = {0};

/* Private function prototypes */
static OSErr ParseDLOGResourceData(const UInt8* data, SInt32 dataSize, DialogTemplate** template);
static OSErr ParseDITLResourceData(const UInt8* data, SInt32 dataSize, Handle* itemList);
static OSErr ParseALRTResourceData(const UInt8* data, SInt32 dataSize, AlertTemplate** template);
static SInt16 ReadSInt16BE(const UInt8** dataPtr);
static SInt32 ReadSInt32BE(const UInt8** dataPtr);
static void ReadRect(const UInt8** dataPtr, Rect* rect);
static void ReadPString(const UInt8** dataPtr, unsigned char* str);

/*
 * InitDialogResources - Initialize dialog resource subsystem
 */
void InitDialogResources(void)
{
    if (gResourceState.initialized) {
        return;
    }

    memset(&gResourceState, 0, sizeof(gResourceState));
    gResourceState.initialized = true;
    gResourceState.cachingEnabled = false;
    gResourceState.resourceFile = 0; /* Use current resource file */
    gResourceState.resourcePath[0] = '\0';
    strncpy(gResourceState.languageCode, "en", sizeof(gResourceState.languageCode) - 1);
    gResourceState.languageCode[sizeof(gResourceState.languageCode) - 1] = '\0';

    // printf("Dialog resource subsystem initialized\n");
}

/*
 * LoadDialogTemplate - Load DLOG resource
 */
OSErr LoadDialogTemplate(SInt16 dialogID, DialogTemplate** template)
{
    Handle resourceHandle = NULL;
    OSErr err = noErr;

    if (!template) {
        return -50; /* paramErr */
    }

    *template = NULL;

    if (!gResourceState.initialized) {
        // printf("Error: Dialog resource subsystem not initialized\n");
        return -1; /* resNotFound */
    }

    /* Load DLOG resource */
    resourceHandle = GetResource(kDialogResourceType, dialogID);
    if (!resourceHandle) {
        // printf("Error: DLOG resource %d not found\n", dialogID);
        return -192; /* resNotFound */
    }

    /* Parse the resource data */
    HLock(resourceHandle);
    SInt32 dataSize = GetHandleSize(resourceHandle);
    err = ParseDLOGResourceData((const UInt8*)*resourceHandle, dataSize, template);
    HUnlock(resourceHandle);

    ReleaseResource(resourceHandle);

    if (err != noErr) {
        // printf("Error: Failed to parse DLOG resource %d (error %d)\n", dialogID, err);
        return err;
    }

    // printf("Loaded DLOG resource %d successfully\n", dialogID);
    return noErr;
}

/*
 * LoadDialogItemList - Load DITL resource
 */
OSErr LoadDialogItemList(SInt16 itemListID, Handle* itemList)
{
    Handle resourceHandle = NULL;
    OSErr err = noErr;

    if (!itemList) {
        return -50; /* paramErr */
    }

    *itemList = NULL;

    if (!gResourceState.initialized) {
        // printf("Error: Dialog resource subsystem not initialized\n");
        return -1; /* resNotFound */
    }

    /* Load DITL resource */
    resourceHandle = GetResource(kDialogItemResourceType, itemListID);
    if (!resourceHandle) {
        // printf("Error: DITL resource %d not found\n", itemListID);
        return -192; /* resNotFound */
    }

    /* Parse the resource data */
    HLock(resourceHandle);
    SInt32 dataSize = GetHandleSize(resourceHandle);
    err = ParseDITLResourceData((const UInt8*)*resourceHandle, dataSize, itemList);
    HUnlock(resourceHandle);

    ReleaseResource(resourceHandle);

    if (err != noErr) {
        // printf("Error: Failed to parse DITL resource %d (error %d)\n", itemListID, err);
        return err;
    }

    // printf("Loaded DITL resource %d successfully\n", itemListID);
    return noErr;
}

/*
 * LoadAlertTemplate - Load ALRT resource
 */
OSErr LoadAlertTemplate(SInt16 alertID, AlertTemplate** template)
{
    Handle resourceHandle = NULL;
    OSErr err = noErr;

    if (!template) {
        return -50; /* paramErr */
    }

    *template = NULL;

    if (!gResourceState.initialized) {
        // printf("Error: Dialog resource subsystem not initialized\n");
        return -1; /* resNotFound */
    }

    /* Load ALRT resource */
    resourceHandle = GetResource(kAlertResourceType, alertID);
    if (!resourceHandle) {
        // printf("Error: ALRT resource %d not found\n", alertID);
        return -192; /* resNotFound */
    }

    /* Parse the resource data */
    HLock(resourceHandle);
    SInt32 dataSize = GetHandleSize(resourceHandle);
    err = ParseALRTResourceData((const UInt8*)*resourceHandle, dataSize, template);
    HUnlock(resourceHandle);

    ReleaseResource(resourceHandle);

    if (err != noErr) {
        // printf("Error: Failed to parse ALRT resource %d (error %d)\n", alertID, err);
        return err;
    }

    // printf("Loaded ALRT resource %d successfully\n", alertID);
    return noErr;
}

/*
 * DisposeDialogTemplate - Dispose dialog template
 */
void DisposeDialogTemplate(DialogTemplate* template)
{
    if (template) {
        DisposePtr((Ptr)template);
    }
}

/*
 * DisposeAlertTemplate - Dispose alert template
 */
void DisposeAlertTemplate(AlertTemplate* template)
{
    if (template) {
        DisposePtr((Ptr)template);
    }
}

/*
 * DisposeDialogItemList - Dispose dialog item list
 */
void DisposeDialogItemList(Handle itemList)
{
    if (itemList) {
        if (*itemList) {
            DisposePtr((Ptr)*itemList);
        }
        DisposePtr((Ptr)itemList);
    }
}

/*
 * CleanupDialogResources - Cleanup dialog resource subsystem
 */
void CleanupDialogResources(void)
{
    if (!gResourceState.initialized) {
        return;
    }

    gResourceState.initialized = false;
    // printf("Dialog resource subsystem cleaned up\n");
}

/*
 * Private implementation functions
 */

static OSErr ParseDLOGResourceData(const UInt8* data, SInt32 dataSize, DialogTemplate** template)
{
    const UInt8* dataPtr = data;
    DialogTemplate* tpl = NULL;

    if (!data || dataSize < 18 || !template) {
        return -50; /* paramErr */
    }

    /* Allocate template structure */
    tpl = (DialogTemplate*)NewPtr(sizeof(DialogTemplate));
    if (!tpl) {
        return -108; /* memFullErr */
    }

    memset(tpl, 0, sizeof(DialogTemplate));

    /* Parse DLOG resource format (Inside Macintosh):
     * - boundsRect (Rect, 8 bytes)
     * - procID (SInt16, 2 bytes)
     * - visible (Boolean, 1 byte)
     * - filler1 (Boolean, 1 byte)
     * - goAwayFlag (Boolean, 1 byte)
     * - filler2 (Boolean, 1 byte)
     * - refCon (SInt32, 4 bytes)
     * - itemsID (SInt16, 2 bytes)
     * - title (Pascal string)
     */

    ReadRect(&dataPtr, &tpl->boundsRect);
    tpl->procID = ReadSInt16BE(&dataPtr);
    tpl->visible = *dataPtr++;
    tpl->filler1 = *dataPtr++;
    tpl->goAwayFlag = *dataPtr++;
    tpl->filler2 = *dataPtr++;
    tpl->refCon = ReadSInt32BE(&dataPtr);
    tpl->itemsID = ReadSInt16BE(&dataPtr);
    ReadPString(&dataPtr, tpl->title);

    *template = tpl;

    // printf("Parsed DLOG: bounds=(%d,%d,%d,%d) procID=%d itemsID=%d title='%.*s'\n",
           tpl->boundsRect.top, tpl->boundsRect.left,
           tpl->boundsRect.bottom, tpl->boundsRect.right,
           tpl->procID, tpl->itemsID,
           tpl->title[0], &tpl->title[1]);

    return noErr;
}

static OSErr ParseDITLResourceData(const UInt8* data, SInt32 dataSize, Handle* itemList)
{
    const UInt8* dataPtr = data;
    SInt16 itemCount;
    Handle list = NULL;

    if (!data || dataSize < 2 || !itemList) {
        return -50; /* paramErr */
    }

    /* DITL format (Inside Macintosh):
     * - itemCount-1 (SInt16, 2 bytes) - stored as count-1!
     * - items (variable length)
     *
     * Each item:
     * - placeholder (Handle, 4 bytes) - unused
     * - bounds (Rect, 8 bytes)
     * - type (UInt8, 1 byte)
     * - data (variable, depends on type)
     */

    /* Read item count (stored as count-1 in resource) */
    itemCount = ReadSInt16BE(&dataPtr) + 1;

    // printf("DITL has %d items\n", itemCount);

    /* Allocate handle for item list */
    /* We'll store the raw DITL data for now */
    SInt32 listSize = dataSize;
    list = (Handle)NewPtr(sizeof(Ptr));
    if (!list) {
        return -108; /* memFullErr */
    }

    *list = (Ptr)NewPtr(listSize);
    if (!*list) {
        DisposePtr((Ptr)list);
        return -108; /* memFullErr */
    }

    /* Copy the entire DITL data */
    memcpy(*list, data, dataSize);

    *itemList = list;

    // printf("Parsed DITL: %d items, %ld bytes\n", itemCount, (long)dataSize);

    return noErr;
}

static OSErr ParseALRTResourceData(const UInt8* data, SInt32 dataSize, AlertTemplate** template)
{
    const UInt8* dataPtr = data;
    AlertTemplate* tpl = NULL;

    if (!data || dataSize < 16 || !template) {
        return -50; /* paramErr */
    }

    /* Allocate template structure */
    tpl = (AlertTemplate*)NewPtr(sizeof(AlertTemplate));
    if (!tpl) {
        return -108; /* memFullErr */
    }

    memset(tpl, 0, sizeof(AlertTemplate));

    /* Parse ALRT resource format (Inside Macintosh):
     * - boundsRect (Rect, 8 bytes)
     * - itemsID (SInt16, 2 bytes)
     * - stages (StageList, 2 bytes - 4 stages, 4 bits each)
     */

    ReadRect(&dataPtr, &tpl->boundsRect);
    tpl->itemsID = ReadSInt16BE(&dataPtr);

    /* Read stage list - stored as a 16-bit value */
    tpl->stages = ReadSInt16BE(&dataPtr);

    *template = tpl;

    // printf("Parsed ALRT: bounds=(%d,%d,%d,%d) itemsID=%d\n",
           tpl->boundsRect.top, tpl->boundsRect.left,
           tpl->boundsRect.bottom, tpl->boundsRect.right,
           tpl->itemsID);

    return noErr;
}

/* Helper functions for reading big-endian data */

static SInt16 ReadSInt16BE(const UInt8** dataPtr)
{
    SInt16 value = ((*dataPtr)[0] << 8) | (*dataPtr)[1];
    *dataPtr += 2;
    return value;
}

static SInt32 ReadSInt32BE(const UInt8** dataPtr)
{
    SInt32 value = ((*dataPtr)[0] << 24) | ((*dataPtr)[1] << 16) |
                   ((*dataPtr)[2] << 8) | (*dataPtr)[3];
    *dataPtr += 4;
    return value;
}

static void ReadRect(const UInt8** dataPtr, Rect* rect)
{
    rect->top = ReadSInt16BE(dataPtr);
    rect->left = ReadSInt16BE(dataPtr);
    rect->bottom = ReadSInt16BE(dataPtr);
    rect->right = ReadSInt16BE(dataPtr);
}

static void ReadPString(const UInt8** dataPtr, unsigned char* str)
{
    UInt8 length = **dataPtr;
    (*dataPtr)++;

    str[0] = length;
    memcpy(&str[1], *dataPtr, length);

    *dataPtr += length;

    /* Align to even byte boundary */
    if (length & 1) {
        (*dataPtr)++;
    }
}

/* Stub implementations for additional resource functions */

DialogTemplate* CreateDialogTemplate(const Rect* bounds, SInt16 procID,
                                     Boolean visible, Boolean goAway, SInt32 refCon,
                                     const unsigned char* title, SInt16 itemsID)
{
    DialogTemplate* tpl = (DialogTemplate*)NewPtr(sizeof(DialogTemplate));
    if (!tpl) {
        return NULL;
    }

    tpl->boundsRect = *bounds;
    tpl->procID = procID;
    tpl->visible = visible;
    tpl->goAwayFlag = goAway;
    tpl->refCon = refCon;
    tpl->itemsID = itemsID;

    if (title) {
        memcpy(tpl->title, title, title[0] + 1);
    } else {
        tpl->title[0] = 0;
    }

    return tpl;
}

AlertTemplate* CreateAlertTemplate(const Rect* bounds, SInt16 itemsID,
                                   StageList stages)
{
    AlertTemplate* tpl = (AlertTemplate*)NewPtr(sizeof(AlertTemplate));
    if (!tpl) {
        return NULL;
    }

    tpl->boundsRect = *bounds;
    tpl->itemsID = itemsID;
    tpl->stages = stages;

    return tpl;
}

Handle CreateDialogItemList(SInt16 itemCount)
{
    /* Create a minimal DITL structure */
    SInt32 listSize = 2; /* Start with item count */
    Handle list = (Handle)NewPtr(sizeof(Ptr));
    if (!list) {
        return NULL;
    }

    *list = (Ptr)NewPtr(listSize);
    if (!*list) {
        DisposePtr((Ptr)list);
        return NULL;
    }

    /* Write item count-1 (as per DITL format) */
    UInt8* data = (UInt8*)*list;
    data[0] = ((itemCount - 1) >> 8) & 0xFF;
    data[1] = (itemCount - 1) & 0xFF;

    return list;
}

Boolean ValidateDialogTemplate(const DialogTemplate* template)
{
    return (template != NULL);
}

Boolean ValidateDialogItemList(Handle itemList)
{
    return (itemList != NULL && *itemList != NULL);
}

Boolean ValidateAlertTemplate(const AlertTemplate* template)
{
    return (template != NULL);
}

void SetDialogResourceCaching(Boolean enabled)
{
    if (gResourceState.initialized) {
        gResourceState.cachingEnabled = enabled;
    }
}

void FlushDialogResourceCache(void)
{
    // printf("FlushDialogResourceCache\n");
}

void PreloadDialogResources(const SInt16* resourceIDs, SInt16 count)
{
    // printf("PreloadDialogResources: %d resources\n", count);
}

void SetDialogResourceLanguage(const char* languageCode)
{
    if (gResourceState.initialized && languageCode) {
        strncpy(gResourceState.languageCode, languageCode, 7);
        gResourceState.languageCode[7] = '\0';
    }
}

void GetDialogResourceLanguage(char* languageCode, size_t codeSize)
{
    if (gResourceState.initialized && languageCode && codeSize > 0) {
        strncpy(languageCode, gResourceState.languageCode, codeSize - 1);
        languageCode[codeSize - 1] = '\0';
    }
}

void DumpDialogTemplate(const DialogTemplate* template)
{
    if (!template) {
        // printf("DumpDialogTemplate: NULL template\n");
        return;
    }

    // printf("Dialog Template:\n");
    // printf("  Bounds: (%d, %d, %d, %d)\n",
           template->boundsRect.top, template->boundsRect.left,
           template->boundsRect.bottom, template->boundsRect.right);
    // printf("  ProcID: %d\n", template->procID);
    // printf("  Visible: %d\n", template->visible);
    // printf("  GoAway: %d\n", template->goAwayFlag);
    // printf("  RefCon: %ld\n", (long)template->refCon);
    // printf("  ItemsID: %d\n", template->itemsID);
    // printf("  Title: '%.*s'\n", template->title[0], &template->title[1]);
}

void DumpDialogItemList(Handle itemList)
{
    // printf("DumpDialogItemList\n");
}

OSErr ParseDLOGResource(Handle resourceData, DialogTemplate** template)
{
    if (!resourceData || !*resourceData) {
        return -50; /* paramErr */
    }

    SInt32 dataSize = GetHandleSize(resourceData);
    return ParseDLOGResourceData((const UInt8*)*resourceData, dataSize, template);
}

OSErr ParseDITLResource(Handle resourceData, Handle* itemList)
{
    if (!resourceData || !*resourceData) {
        return -50; /* paramErr */
    }

    SInt32 dataSize = GetHandleSize(resourceData);
    return ParseDITLResourceData((const UInt8*)*resourceData, dataSize, itemList);
}

OSErr ParseALRTResource(Handle resourceData, AlertTemplate** template)
{
    if (!resourceData || !*resourceData) {
        return -50; /* paramErr */
    }

    SInt32 dataSize = GetHandleSize(resourceData);
    return ParseALRTResourceData((const UInt8*)*resourceData, dataSize, template);
}

void SetDialogResourceFile(SInt16 refNum)
{
    if (gResourceState.initialized) {
        gResourceState.resourceFile = refNum;
    }
}

SInt16 GetDialogResourceFile(void)
{
    return gResourceState.initialized ? gResourceState.resourceFile : 0;
}

void SetDialogResourcePath(const char* path)
{
    if (gResourceState.initialized && path) {
        strncpy(gResourceState.resourcePath, path, 255);
        gResourceState.resourcePath[255] = '\0';
    }
}

/* Stub implementations for advanced features */

OSErr AddDialogItem(Handle itemList, SInt16 itemNo, SInt16 itemType,
                   const Rect* bounds, const unsigned char* title)
{
    return noErr;
}

OSErr AddButtonItem(Handle itemList, SInt16 itemNo, const Rect* bounds,
                   const unsigned char* title, Boolean isDefault)
{
    return noErr;
}

OSErr AddTextItem(Handle itemList, SInt16 itemNo, const Rect* bounds,
                 const unsigned char* text, Boolean isEditable)
{
    return noErr;
}

OSErr AddIconItem(Handle itemList, SInt16 itemNo, const Rect* bounds,
                 SInt16 iconID)
{
    return noErr;
}

OSErr AddUserItem(Handle itemList, SInt16 itemNo, const Rect* bounds,
                 UserItemProcPtr userProc)
{
    return noErr;
}

Boolean ConvertDialogTemplateToNative(const DialogTemplate* template,
                                     void** nativeTemplate)
{
    return false;
}

Boolean ConvertNativeToDialogTemplate(const void* nativeTemplate,
                                     DialogTemplate** template)
{
    return false;
}

OSErr LoadLocalizedDialogTemplate(SInt16 dialogID, DialogTemplate** template)
{
    return LoadDialogTemplate(dialogID, template);
}

OSErr LoadDialogFromJSON(const char* jsonData, DialogTemplate** template)
{
    return -1; /* Not implemented */
}

OSErr SaveDialogToJSON(const DialogTemplate* template, char* jsonData,
                      size_t dataSize)
{
    return -1; /* Not implemented */
}

OSErr LoadDialogFromXML(const char* xmlData, DialogTemplate** template)
{
    return -1; /* Not implemented */
}

void* GetDialogResourceData(SInt16 resourceID, OSType resourceType, size_t* dataSize)
{
    return NULL;
}
