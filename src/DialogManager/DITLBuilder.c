/*
 * DITLBuilder.c - Build a dialog item list at runtime, and run a modal box
 *
 * The 'DITL' format, per Inside Macintosh: Macintosh Toolbox Essentials 6-152:
 *
 *   +0   int16   number of items minus one
 *   then, for each item:
 *   +0   int32   placeholder for the item's handle (zero on disk)
 *   +4   Rect    display rectangle, in dialog-local coordinates
 *   +12  int8    item type
 *   +13  int8    length of the item's data
 *   +14  ...     the data itself
 *
 * Each item begins on an even byte boundary, so an item whose data length is
 * odd is followed by one pad byte. That rule is the whole reason this file
 * exists: it is invisible until some runtime string happens to come out odd,
 * and then the parser reads the following item's header one byte off and the
 * rest of the list is garbage.
 *
 * Copyright (c) 2025 - System 7.1 Portable Project
 */

#include "SystemTypes.h"
#include "System71StdLib.h"
#include "MemoryMgr/MemoryManager.h"
#include "QuickDraw/QuickDraw.h"   /* SetRect - was being called undeclared */
#include "EventManager/EventTypes.h"
#include "DialogManager/DITLBuilder.h"

/* Header bytes before an item's data: 4 handle + 8 rect + 1 type + 1 length. */
#define kDITLItemHeaderSize 14

extern void DrawDialog(DialogPtr theDialog);
extern Boolean IsDialogEvent(const EventRecord* theEvent);
extern Boolean DialogSelect(const EventRecord* theEvent, DialogPtr* theDialog, SInt16* itemHit);
extern Boolean GetNextEvent(short eventMask, EventRecord* theEvent);
extern void SystemTask(void);
extern void EventPumpYield(void);

Boolean DITL_Begin(DITLBuilder* b, Size capacity)
{
    if (!b) return false;

    b->list = NULL;
    b->next = NULL;
    b->limit = NULL;
    b->count = 0;
    b->overflow = false;

    /* Room for the count word as well as the items. */
    Handle h = NewHandleClear(capacity + 2);
    if (!h) {
        b->overflow = true;
        return false;
    }

    HLock(h);
    b->list = h;
    b->next = (UInt8*)*h + 2;   /* count is filled in by DITL_Finish */
    b->limit = (UInt8*)*h + capacity + 2;
    return true;
}

/*
 * The one place that lays an item down. Both the C-string and Pascal-string
 * entry points come through here, so the alignment rule is applied once
 * however the caller happens to hold its text.
 */
static void DITL_AddRaw(DITLBuilder* b, SInt16 type, const Rect* box,
                        const void* bytes, Size len)
{
    if (!b || !b->list || b->overflow || !box) return;
    if (len > 255) len = 255;

    /* An odd-length item is followed by a pad byte so the next one starts even. */
    Size need = kDITLItemHeaderSize + len + ((len & 1) ? 1 : 0);
    if (b->next + need > b->limit) {
        b->overflow = true;
        return;
    }

    UInt8* p = b->next;

    /* Handle placeholder - the Dialog Manager fills this in when the list is
     * parsed, so it is zero here. */
    *p++ = 0; *p++ = 0; *p++ = 0; *p++ = 0;

    /* Rect, big-endian int16s in top/left/bottom/right order. */
    *p++ = (UInt8)((box->top >> 8) & 0xFF);    *p++ = (UInt8)(box->top & 0xFF);
    *p++ = (UInt8)((box->left >> 8) & 0xFF);   *p++ = (UInt8)(box->left & 0xFF);
    *p++ = (UInt8)((box->bottom >> 8) & 0xFF); *p++ = (UInt8)(box->bottom & 0xFF);
    *p++ = (UInt8)((box->right >> 8) & 0xFF);  *p++ = (UInt8)(box->right & 0xFF);

    *p++ = (UInt8)(type & 0xFF);
    *p++ = (UInt8)len;
    if (len) {
        memcpy(p, bytes, len);
        p += len;
    }
    if (len & 1) {
        *p++ = 0;
    }

    b->next = p;
    b->count++;
}

void DITL_AddItem(DITLBuilder* b, SInt16 type, const Rect* box, const char* text)
{
    DITL_AddRaw(b, type, box, text, text ? (Size)strlen(text) : 0);
}

/*
 * Append an item whose text is a Pascal string.
 *
 * Callers that hold a Str255 used to convert it themselves, and getting that
 * wrong is its own recurring bug - passing one to %s prints the length byte as
 * a character. Taking the string as what it is removes the conversion.
 */
void DITL_AddItemPascal(DITLBuilder* b, SInt16 type, const Rect* box,
                        const unsigned char* pstr)
{
    DITL_AddRaw(b, type, box, pstr ? pstr + 1 : NULL, pstr ? (Size)pstr[0] : 0);
}

void DITL_AddButtonPascal(DITLBuilder* b, SInt16 top, SInt16 left, SInt16 bottom,
                          SInt16 right, const unsigned char* title)
{
    Rect r;
    SetRect(&r, left, top, right, bottom);
    DITL_AddItemPascal(b, ctrlItem + btnCtrl, &r, title);
}

void DITL_AddTextPascal(DITLBuilder* b, SInt16 top, SInt16 left, SInt16 bottom,
                        SInt16 right, const unsigned char* text)
{
    Rect r;
    SetRect(&r, left, top, right, bottom);
    DITL_AddItemPascal(b, statText, &r, text);
}

void DITL_AddText(DITLBuilder* b, SInt16 top, SInt16 left, SInt16 bottom, SInt16 right,
                  const char* text)
{
    Rect r;
    SetRect(&r, left, top, right, bottom);
    DITL_AddItem(b, statText, &r, text);
}

void DITL_AddButton(DITLBuilder* b, SInt16 top, SInt16 left, SInt16 bottom, SInt16 right,
                    const char* title)
{
    Rect r;
    SetRect(&r, left, top, right, bottom);
    DITL_AddItem(b, ctrlItem + btnCtrl, &r, title);
}

void DITL_AddEditText(DITLBuilder* b, SInt16 top, SInt16 left, SInt16 bottom, SInt16 right,
                      const char* initialText)
{
    Rect r;
    SetRect(&r, left, top, right, bottom);
    DITL_AddItem(b, editText, &r, initialText);
}

Handle DITL_Finish(DITLBuilder* b)
{
    if (!b) return NULL;

    Handle h = b->list;
    SInt16 count = b->count;
    Boolean bad = b->overflow || !h || count <= 0;

    b->list = NULL;
    b->next = NULL;
    b->limit = NULL;
    b->count = 0;

    if (!h) return NULL;

    if (bad) {
        HUnlock(h);
        DisposeHandle(h);
        return NULL;
    }

    /* The count word stores one less than the number of items. */
    UInt8* base = (UInt8*)*h;
    base[0] = (UInt8)(((count - 1) >> 8) & 0xFF);
    base[1] = (UInt8)((count - 1) & 0xFF);

    HUnlock(h);
    return h;
}

SInt16 RunModalDialogBox(DialogPtr dlg, SInt16 defaultItem, SInt16 cancelItem)
{
    if (!dlg) return 0;

    /* Tell the dialog which item is default before the first draw, so
     * DrawDialogItemByType rings it. NewDialog leaves aDefItem at 1, which for
     * these runtime alerts is the message text - a static text item never draws
     * a ring, so no button ever got one. */
    if (defaultItem > 0) {
        extern OSErr SetDialogDefaultItem(DialogPtr theDialog, SInt16 newItem);
        SetDialogDefaultItem(dlg, defaultItem);
    }
    if (cancelItem > 0) {
        extern OSErr SetDialogCancelItem(DialogPtr theDialog, SInt16 newItem);
        SetDialogCancelItem(dlg, cancelItem);
    }

    DrawDialog(dlg);

    SInt16 itemHit = 0;
    while (itemHit == 0) {
        EventRecord event;
        if (GetNextEvent(everyEvent, &event)) {
            if (IsDialogEvent(&event)) {
                DialogPtr which;
                SInt16 item;
                if (DialogSelect(&event, &which, &item) && which == dlg) {
                    /* Only a button dismisses; clicks in static or edit text
                     * are reported too and must not end the loop. */
                    if (item == defaultItem || item == cancelItem) {
                        itemHit = item;
                    }
                }
            }
            if (event.what == keyDown || event.what == autoKey) {
                char ch = (char)(event.message & 0xFF);
                if ((ch == '\r' || ch == 0x03) && defaultItem) {
                    itemHit = defaultItem;
                } else if (ch == 0x1B && cancelItem) {
                    itemHit = cancelItem;
                } else if (ch == '.' && (event.modifiers & cmdKey) && cancelItem) {
                    itemHit = cancelItem;
                }
            }
        }

        EventPumpYield();
        SystemTask();
    }

    return itemHit;
}
