/*
 * DialogResources.c - loading dialog and alert templates from resources
 *
 * The Dialog Manager's GetNewDialog and the Alert family call these. They
 * were stubs in sys71_stubs.c with the wrong signatures - one argument where
 * the callers pass two, a pointer returned where they read an OSErr - so the
 * callers' templates were never filled in and every dialog or alert built
 * from a resource failed.
 *
 * Layouts from Inside Macintosh: Macintosh Toolbox Essentials:
 *
 *   'DLOG' (6-151)  Rect boundsRect; SInt16 procID; Boolean visible, filler;
 *                   Boolean goAwayFlag, filler; SInt32 refCon; SInt16 itemsID;
 *                   Str255 title (only as long as it is); then, from System 7,
 *                   an optional word-aligned positioning SInt16.
 *   'ALRT' (6-156)  Rect boundsRect; SInt16 itemsID; SInt16 stages; then the
 *                   same optional positioning word.
 *
 * All of it big-endian, as the resource file stores it.
 */

#include "SystemTypes.h"
#include "DialogManager/DialogResources.h"
#include "ResourceManager.h"
#include "MemoryMgr/MemoryManager.h"
#include <string.h>

enum {
    kDLOGFixedBytes = 20,   /* everything before the title's length byte */
    kALRTFixedBytes = 12
};

static SInt16 ReadBE16(const UInt8* p) {
    return (SInt16)(((UInt16)p[0] << 8) | p[1]);
}

static SInt32 ReadBE32(const UInt8* p) {
    return (SInt32)(((UInt32)p[0] << 24) | ((UInt32)p[1] << 16) |
                    ((UInt32)p[2] << 8) | p[3]);
}

static void ReadRect(const UInt8* p, Rect* r) {
    r->top    = ReadBE16(p);
    r->left   = ReadBE16(p + 2);
    r->bottom = ReadBE16(p + 4);
    r->right  = ReadBE16(p + 6);
}

OSErr ParseDLOGResource(Handle resourceData, DialogTemplate** template) {
    if (!template) return paramErr;
    *template = NULL;
    if (!resourceData || !*resourceData) return paramErr;

    const u32 size = GetHandleSize(resourceData);
    const UInt8* p = (const UInt8*)*resourceData;
    if (size < kDLOGFixedBytes + 1) return resNotFound;

    DialogTemplate* t = (DialogTemplate*)NewPtrClear(sizeof(DialogTemplate));
    if (!t) return memFullErr;

    ReadRect(p, &t->boundsRect);
    t->procID     = ReadBE16(p + 8);
    t->visible    = p[10] != 0;
    t->goAwayFlag = p[12] != 0;
    t->refCon     = ReadBE32(p + 14);
    t->itemsID    = ReadBE16(p + 18);

    /* A title longer than the data holding it is cut at the end of the data
     * rather than read past it. */
    UInt8 len = p[kDLOGFixedBytes];
    if ((u32)kDLOGFixedBytes + 1 + len > size) {
        len = (UInt8)(size - kDLOGFixedBytes - 1);
    }
    t->title[0] = len;
    memcpy(&t->title[1], p + kDLOGFixedBytes + 1, len);

    *template = t;
    return noErr;
}

OSErr ParseALRTResource(Handle resourceData, AlertTemplate** template) {
    if (!template) return paramErr;
    *template = NULL;
    if (!resourceData || !*resourceData) return paramErr;

    const u32 size = GetHandleSize(resourceData);
    const UInt8* p = (const UInt8*)*resourceData;
    if (size < kALRTFixedBytes) return resNotFound;

    AlertTemplate* t = (AlertTemplate*)NewPtrClear(sizeof(AlertTemplate));
    if (!t) return memFullErr;

    ReadRect(p, &t->boundsRect);
    t->itemsID = ReadBE16(p + 8);
    t->stages  = ReadBE16(p + 10);

    *template = t;
    return noErr;
}

OSErr LoadDialogTemplate(SInt16 dialogID, DialogTemplate** template) {
    if (!template) return paramErr;
    *template = NULL;

    Handle res = GetResource(kDialogResourceType, dialogID);
    if (!res) return resNotFound;
    OSErr err = ParseDLOGResource(res, template);
    ReleaseResource(res);
    return err;
}

OSErr LoadAlertTemplate(SInt16 alertID, AlertTemplate** template) {
    if (!template) return paramErr;
    *template = NULL;

    Handle res = GetResource(kAlertResourceType, alertID);
    if (!res) return resNotFound;
    OSErr err = ParseALRTResource(res, template);
    ReleaseResource(res);
    return err;
}

OSErr LoadDialogItemList(SInt16 itemListID, Handle* itemList) {
    if (!itemList) return paramErr;
    *itemList = NULL;

    Handle res = GetResource(kDialogItemResourceType, itemListID);
    if (!res || !*res) return resNotFound;

    const u32 size = GetHandleSize(res);
    Handle copy = NewHandle(size);
    if (!copy) {
        ReleaseResource(res);
        return memFullErr;
    }
    memcpy(*copy, *res, size);
    ReleaseResource(res);

    *itemList = copy;
    return noErr;
}

void DisposeDialogTemplate(DialogTemplate* template) {
    if (template) DisposePtr(template);
}

void DisposeAlertTemplate(AlertTemplate* template) {
    if (template) DisposePtr(template);
}

void DisposeDialogItemList(Handle itemList) {
    if (itemList) DisposeHandle(itemList);
}
