/*
 * DITLBuilder.h - Build a dialog item list at runtime, and run a modal box
 *
 * System 7 dialogs normally come from a 'DITL' resource, but the Finder has a
 * few alerts whose text depends on runtime data (a file name, an item count),
 * so their lists have to be assembled in memory. Three places were doing that
 * by hand, each emitting the same header bytes literally, and each free to get
 * the format subtly wrong - one of them did, silently losing both its buttons
 * for any message of odd length.
 *
 * Copyright (c) 2025 - System 7.1 Portable Project
 */

#ifndef DITL_BUILDER_H
#define DITL_BUILDER_H

#include "SystemTypes.h"

typedef struct DITLBuilder {
    Handle   list;      /* the item list under construction, NULL on failure */
    UInt8*   next;      /* write cursor */
    UInt8*   limit;     /* one past the last writable byte */
    SInt16   count;     /* items added so far */
    Boolean  overflow;  /* an item did not fit; the list is unusable */
} DITLBuilder;

/*
 * Start a list with room for `capacity` bytes of item data. Returns false if
 * the handle could not be allocated, in which case the builder must not be
 * used. DITL_Finish always leaves the builder empty, so failure paths only
 * ever need to skip the dialog.
 */
Boolean DITL_Begin(DITLBuilder* b, Size capacity);

/*
 * Append one item. `type` is a dialog item type (statText, editText, or
 * ctrlItem + btnCtrl and friends); `box` is in dialog-local coordinates;
 * `text` is a C string, and may be NULL for items that carry none. Text longer
 * than 255 bytes is truncated, since the on-disk format stores the length in
 * one byte.
 *
 * Item numbers are 1-based in the order added, matching what GetDialogItem and
 * DialogSelect report.
 */
void DITL_AddItem(DITLBuilder* b, SInt16 type, const Rect* box, const char* text);

/* Convenience wrappers for the three kinds the Finder actually builds. */
void DITL_AddText(DITLBuilder* b, SInt16 top, SInt16 left, SInt16 bottom, SInt16 right,
                  const char* text);
void DITL_AddButton(DITLBuilder* b, SInt16 top, SInt16 left, SInt16 bottom, SInt16 right,
                    const char* title);
void DITL_AddEditText(DITLBuilder* b, SInt16 top, SInt16 left, SInt16 bottom, SInt16 right,
                      const char* initialText);

/*
 * Seal the list and hand over the handle, which the caller passes to NewDialog
 * and no longer owns. Returns NULL if anything went wrong along the way, after
 * releasing the partial list.
 */
Handle DITL_Finish(DITLBuilder* b);

/*
 * Run a modal dialog to completion and return the item that dismissed it.
 *
 * Draws the dialog first: these boxes are created and shown inside a menu
 * command, and nothing guarantees an update event arrives for a window that
 * was just created, so a hand-rolled loop that waits for one shows an empty
 * frame. Pumps the input devices itself for the same reason - the main event
 * loop, the only other caller of ProcessModernInput, is blocked behind us.
 *
 * Return and Enter select `defaultItem`; Escape and Command-. select
 * `cancelItem`. Pass 0 for either to disable that key. The dialog is not
 * disposed; the caller still owns it.
 */
SInt16 RunModalDialogBox(DialogPtr dlg, SInt16 defaultItem, SInt16 cancelItem);

#endif /* DITL_BUILDER_H */
