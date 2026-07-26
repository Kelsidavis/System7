#include "MemoryMgr/MemoryManager.h"
/*
#include "DialogManager/DialogInternal.h"
 * DialogResourceParser.c - DLOG and DITL Resource Parsing
 *
 * Implements faithful System 7.1-compatible resource parsing for dialog templates
 * and dialog item lists. This module parses the binary resource format and creates
 * internal structures used by the Dialog Manager.
 */

#include <stdlib.h>
#include <string.h>
#include "SystemTypes.h"
#include "System71StdLib.h"
#include "DialogManager/DialogManager.h"
#include "DialogManager/DialogTypes.h"
#include "DialogManager/DialogManagerInternal.h"
#include "DialogManager/DialogResourceParser.h"
#include "DialogManager/DialogLogging.h"

/* DITL resource format (System 7.1):
 *
 * Word: item count - 1  (so count = value + 1)
 *
 * For each item:
 *   Long: placeholder (always 0)
 *   Rect: display rectangle (top, left, bottom, right as 4 words)
 *   Byte: item type (with flags in high bit)
 *   Byte or Word: item data length
 *   Data: item-specific data (varies by type)
 */

/* Maximum data length for dialog item text - prevents excessive allocations */
#define MAX_DIALOG_ITEM_DATA 4096

/* Parse DITL resource into DialogItemEx array */
OSErr ParseDITL(Handle ditlHandle, DialogItemEx** items, SInt16* itemCount) {
    unsigned char* p;
    SInt16 count;
    SInt16 i;
    DialogItemEx* itemArray;

    if (!ditlHandle || !items || !itemCount) {
        // DIALOG_LOG_DEBUG("Dialog: ParseDITL - invalid parameters\n");
        return -1;
    }

    if (!*ditlHandle) {
        // DIALOG_LOG_DEBUG("Dialog: ParseDITL - null handle data\n");
        return -1;
    }

    /* CRITICAL: Lock handle before dereferencing to prevent heap compaction issues */
    HLock(ditlHandle);
    p = (unsigned char*)*ditlHandle;

    /* Validate handle data after lock */
    if (!p) {
        HUnlock(ditlHandle);
        return -1;
    }

    /* Get handle size to validate bounds during parsing */
    Size handleSize = GetHandleSize(ditlHandle);
    if (handleSize < 2) {
        HUnlock(ditlHandle);
        return -1;
    }
    unsigned char* pEnd = p + handleSize;

    /* Read item count (stored as count-1 in resource) */
    count = ((SInt16)p[0] << 8) | p[1];
    p += 2;

    /* Validate raw count value before adding 1 to prevent integer overflow */
    if (count < 0 || count >= 256) {
        // DIALOG_LOG_DEBUG("Dialog: ParseDITL - invalid raw item count %d\n", count);
        HUnlock(ditlHandle);
        return -1;
    }

    count += 1;  /* Actual count is stored value + 1 */

    *itemCount = count;
    // DIALOG_LOG_DEBUG("Dialog: Parsing DITL with %d items\n", count);

    /* Check for integer overflow in allocation size */
    if (count > SIZE_MAX / sizeof(DialogItemEx)) {
        HUnlock(ditlHandle);
        return -108;  /* memFullErr */
    }

    /* Allocate item array */
    itemArray = (DialogItemEx*)NewPtr(count * sizeof(DialogItemEx));
    if (!itemArray) {
        // DIALOG_LOG_DEBUG("Dialog: ParseDITL - malloc failed\n");
        HUnlock(ditlHandle);
        return -108;  /* memFullErr */
    }

    memset(itemArray, 0, count * sizeof(DialogItemEx));

    /* Parse each item */
    for (i = 0; i < count; i++) {
        SInt16 itemType;
        SInt16 dataLen;
        Rect bounds;

        /* Validate we have enough bytes for item header (4 + 8 + 1 + 1 = 14 bytes minimum) */
        if (p + 14 > pEnd) {
            // DIALOG_LOG_DEBUG("Dialog: ParseDITL - truncated item data at item %d\n", i);
            DisposePtr((Ptr)itemArray);
            HUnlock(ditlHandle);
            return -1;
        }

        /* Skip placeholder (4 bytes) */
        p += 4;

        /* Read bounds (8 bytes: top, left, bottom, right) */
        bounds.top = ((SInt16)p[0] << 8) | p[1];
        bounds.left = ((SInt16)p[2] << 8) | p[3];
        bounds.bottom = ((SInt16)p[4] << 8) | p[5];
        bounds.right = ((SInt16)p[6] << 8) | p[7];
        p += 8;

        /* Read item type */
        itemType = *p++;

        /* Read data length (byte or word depending on odd length) */
        dataLen = *p++;
        if (dataLen == 0xFF) {
            /* Validate we have 2 more bytes for long length */
            if (p + 2 > pEnd) {
                DisposePtr((Ptr)itemArray);
                HUnlock(ditlHandle);
                return -1;
            }
            /* Long data length (next word) */
            dataLen = ((SInt16)p[0] << 8) | p[1];
            p += 2;
        }

        /* Cap dataLen to prevent excessive allocations */
        if (dataLen > MAX_DIALOG_ITEM_DATA) {
            dataLen = MAX_DIALOG_ITEM_DATA;
        }

        /* Initialize item */
        itemArray[i].bounds = bounds;
        itemArray[i].type = itemType;
        itemArray[i].index = i;
        itemArray[i].visible = true;
        itemArray[i].enabled = (itemType & itemDisable) == 0;
        itemArray[i].refCon = 0;
        itemArray[i].userData = NULL;
        itemArray[i].platformData = NULL;

        /* Parse item data based on type */
        {
            SInt16 baseType = itemType & itemTypeMask;

            /*
             * A control's DITL type is ctrlItem (4) plus the control kind, so a
             * push button is 4, a checkbox 5, a radio 6. This tested
             * baseType against btnCtrl/chkCtrl/radCtrl - 0, 1 and 2 - which are
             * the control kinds on their own and never appear as an item type,
             * so no control ever had its title copied and every button reached
             * DrawDialogButton with data == NULL. Static text (8) matched, which
             * is why the Empty Trash prompt drew and its OK and Cancel buttons
             * came out as empty outlines. Testing 0 also collided with userItem,
             * which is 0.
             */
            if ((baseType >= ctrlItem && baseType <= ctrlItem + resCtrl) ||
                baseType == statText || baseType == editText) {
                /* Text-based item - keep the string in Pascal form.
                 *
                 * The length byte has already been consumed into dataLen above,
                 * so it has to be put back: every consumer treats item->data as
                 * a Pascal string. DrawDialogButton tests title[0] > 0 and calls
                 * StringWidth(title); DrawDialogStaticText does the same. Storing
                 * the bare text meant the first character was read as the length
                 * and the text drawn from the second, so "Are you sure you want
                 * to permanently remove the items in the Trash?" - 66 characters
                 * - rendered as "re you sure..." with a length of 'A', 65, and
                 * "OK" became a 79-character string starting at 'K'. */
                if (dataLen > 0) {
                    /* Validate we have enough bytes available */
                    if (p + dataLen > pEnd) {
                        DisposePtr((Ptr)itemArray);
                        HUnlock(ditlHandle);
                        return -1;
                    }
                    SInt16 pascalLen = (dataLen > 255) ? 255 : dataLen;
                    /*
                     * The text lives in a Handle, not a Ptr, and `data` is just
                     * a cached pointer into it.
                     *
                     * GetDialogItem is documented to hand back a Handle, and
                     * that is what GetDialogItemText, SetDialogItemText and the
                     * TextEdit glue all write through - but the parser only
                     * ever filled in `data`, leaving `handle` NULL, while
                     * DrawDialogItemByType drew from `data`. So the read side
                     * and the write side were looking at different storage:
                     * typing into a dialog updated a handle nobody had, and the
                     * field redrew from text nobody had changed.
                     *
                     * The block is locked for the life of the dialog, so the
                     * cached pointer stays valid; DialogItem_SyncText refreshes
                     * it after any write that may have resized the handle.
                     */
                    Handle textHandle = NewHandle(pascalLen + 2);
                    if (textHandle) {
                        HLock(textHandle);
                        unsigned char* textData = (unsigned char*)*textHandle;
                        textData[0] = (unsigned char)pascalLen;
                        memcpy(textData + 1, p, pascalLen);
                        textData[pascalLen + 1] = 0;   /* also usable as a C string */
                        itemArray[i].handle = textHandle;
                        itemArray[i].data = textData;
                    }
                }
            } else if (baseType == iconItem || baseType == picItem) {
                /* Resource ID stored as 2-byte integer */
                if (dataLen >= 2) {
                    if (p + 2 > pEnd) {
                        DisposePtr((Ptr)itemArray);
                        HUnlock(ditlHandle);
                        return -1;
                    }
                    SInt16 resID = ((SInt16)p[0] << 8) | p[1];
                    itemArray[i].refCon = resID;
                }
            } else if (baseType == userItem) {
                /* User item has no data, just note it */
                itemArray[i].handle = NULL;
            }
        }

        /* Validate pointer advance won't exceed bounds */
        if (p + dataLen > pEnd) {
            DisposePtr((Ptr)itemArray);
            HUnlock(ditlHandle);
            return -1;
        }
        p += dataLen;

        /* Align to word boundary */
        if ((unsigned long)p & 1) {
            p++;
        }

        // DIALOG_LOG_DEBUG("Dialog: Item %d: type=%d bounds=(%d,%d,%d,%d)\n", i+1, itemType, bounds.top, bounds.left, bounds.bottom, bounds.right);
    }

    *items = itemArray;
    HUnlock(ditlHandle);
    return 0;  /* noErr */
}

/* Free parsed DITL items */
void FreeParsedDITL(DialogItemEx* items, SInt16 itemCount) {
    SInt16 i;

    if (!items) return;

    for (i = 0; i < itemCount; i++) {
        /* data points into handle for text items; free the handle, not the
         * pointer. Items whose handle is something else (a user item's proc)
         * carry no data pointer, so this stays in step. */
        if (items[i].handle && items[i].data) {
            HUnlock(items[i].handle);
            DisposeHandle(items[i].handle);
            items[i].handle = NULL;
            items[i].data = NULL;
        } else if (items[i].data) {
            DisposePtr((Ptr)items[i].data);
            items[i].data = NULL;
        }
    }

    DisposePtr((Ptr)items);
}

/* Create a simple default DITL for testing (OK and Cancel buttons) */
Handle CreateDefaultDITL(void) {
    /* Simplified DITL with just OK button for now */
    static unsigned char defaultDITL[] = {
        0x00, 0x00,  /* 1 item (stored as 0) */

        /* Item 1: OK Button */
        0x00, 0x00, 0x00, 0x00,  /* Placeholder */
        0x00, 0x78,  /* top = 120 */
        0x00, 0xFA,  /* left = 250 */
        0x00, 0x8C,  /* bottom = 140 */
        0x01, 0x3E,  /* right = 318 */
        0x04,        /* type = btnCtrl (4) */
        0x02,        /* data length = 2 */
        0x4F, 0x4B   /* "OK" in Pascal string format (length byte omitted for simplicity) */
    };

    Handle h = (Handle)NewPtr(sizeof(Ptr));
    if (!h) return NULL;

    *h = (Ptr)NewPtr(sizeof(defaultDITL));
    if (!*h) {
        DisposePtr((Ptr)h);
        return NULL;
    }

    memcpy(*h, defaultDITL, sizeof(defaultDITL));

    return h;
}
