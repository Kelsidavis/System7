/************************************************************
 *
 * TextUndo.c
 * System 7.1 Portable - TextEdit Undo/Redo Implementation
 *
 * Undo and redo functionality for TextEdit operations.
 * Provides comprehensive edit history management with
 * support for text insertion, deletion, and style changes.
 *
 * Copyright (c) 2024 System7.1-Portable Project
 * Based on Apple Computer, Inc. TextEdit Manager 1985-1992
 *
 ************************************************************/

#include "TextEdit/TextEdit.h"
#include "TextEdit/TextTypes.h"
#include "TextEdit/TextSelection.h"
#include "Memory.h"
#include <string.h>
#include <stdlib.h>

/************************************************************
 * Undo Constants and Types
 ************************************************************/

#define kTEMaxUndoLevels    10
#define kTEMaxUndoSize      32768

/* Undo operation types */
enum {
    kTEUndoNone = 0,
    kTEUndoTyping = 1,
    kTEUndoDelete = 2,
    kTEUndoCut = 3,
    kTEUndoPaste = 4,
    kTEUndoClear = 5,
    kTEUndoStyle = 6,
    kTEUndoDrop = 7
};

/* Undo record structure */
typedef struct TEUndoRecord {
    short undoType;             /* Type of operation */
    long undoStart;             /* Start position */
    long undoEnd;               /* End position */
    long undoLength;            /* Length of undo data */
    Handle undoText;            /* Undo text data */
    StScrpHandle undoStyles;    /* Undo style data */
    long selStart;              /* Selection start before operation */
    long selEnd;                /* Selection end before operation */
    long timestamp;             /* Time of operation */
    Boolean canMerge;           /* Can merge with next operation */
    struct TEUndoRecord *next;  /* Next undo record */
} TEUndoRecord;

/* Undo manager structure */
typedef struct TEUndoManager {
    TEUndoRecord *undoList;     /* List of undo records */
    TEUndoRecord *redoList;     /* List of redo records */
    TEUndoRecord *currentRecord; /* Current undo record being built */
    short undoCount;            /* Number of undo records */
    short redoCount;            /* Number of redo records */
    long totalUndoSize;         /* Total size of undo data */
    Boolean undoEnabled;        /* Undo system enabled */
    Boolean grouping;           /* Currently grouping operations */
} TEUndoManager;

/************************************************************
 * Global Undo State
 ************************************************************/

static TEUndoManager gUndoManager = {0};
static Boolean gUndoInitialized = false;

/************************************************************
 * Internal Undo Utilities
 ************************************************************/

static OSErr TEInitUndoSystem(void)
{
    if (gUndoInitialized) return noErr;

    memset(&gUndoManager, 0, sizeof(TEUndoManager));
    gUndoManager.undoEnabled = true;

    gUndoInitialized = true;
    return noErr;
}

static void TEDisposeUndoRecord(TEUndoRecord *record)
{
    if (!record) return;

    if (record->undoText) {
        DisposeHandle(record->undoText);
    }
    if (record->undoStyles) {
        DisposeHandle((Handle)record->undoStyles);
    }

    free(record);
}

static void TEDisposeUndoList(TEUndoRecord *list)
{
    TEUndoRecord *current = list;
    TEUndoRecord *next;

    while (current) {
        next = current->next;
        TEDisposeUndoRecord(current);
        current = next;
    }
}

static TEUndoRecord* TECreateUndoRecord(short undoType, long start, long end,
                                       Handle textData, StScrpHandle styleData)
{
    TEUndoRecord *record;
    OSErr err;

    record = (TEUndoRecord*)malloc(sizeof(TEUndoRecord));
    if (!record) return NULL;

    memset(record, 0, sizeof(TEUndoRecord));

    record->undoType = undoType;
    record->undoStart = start;
    record->undoEnd = end;
    record->timestamp = TickCount();
    record->canMerge = false;
    record->next = NULL;

    /* Copy text data if provided */
    if (textData && GetHandleSize(textData) > 0) {
        record->undoLength = GetHandleSize(textData);
        record->undoText = NewHandle(record->undoLength);
        if (record->undoText) {
            HLock(textData);
            HLock(record->undoText);
            memcpy(*record->undoText, *textData, record->undoLength);
            HUnlock(record->undoText);
            HUnlock(textData);
        }
    }

    /* Copy style data if provided */
    if (styleData) {
        long styleSize = GetHandleSize((Handle)styleData);
        if (styleSize > 0) {
            record->undoStyles = (StScrpHandle)NewHandle(styleSize);
            if (record->undoStyles) {
                HLock((Handle)styleData);
                HLock((Handle)record->undoStyles);
                memcpy(*record->undoStyles, *styleData, styleSize);
                HUnlock((Handle)record->undoStyles);
                HUnlock((Handle)styleData);
            }
        }
    }

    return record;
}

static void TEAddUndoRecord(TEUndoRecord *record)
{
    if (!record || !gUndoManager.undoEnabled) {
        if (record) TEDisposeUndoRecord(record);
        return;
    }

    /* Clear redo list when adding new undo */
    if (gUndoManager.redoList) {
        TEDisposeUndoList(gUndoManager.redoList);
        gUndoManager.redoList = NULL;
        gUndoManager.redoCount = 0;
    }

    /* Add to front of undo list */
    record->next = gUndoManager.undoList;
    gUndoManager.undoList = record;
    gUndoManager.undoCount++;

    /* Update total size */
    gUndoManager.totalUndoSize += record->undoLength;

    /* Trim list if too many records or too much data */
    while (gUndoManager.undoCount > kTEMaxUndoLevels ||
           gUndoManager.totalUndoSize > kTEMaxUndoSize) {
        TEUndoRecord *last = gUndoManager.undoList;
        TEUndoRecord *prev = NULL;

        /* Find last record */
        while (last && last->next) {
            prev = last;
            last = last->next;
        }

        if (last) {
            if (prev) {
                prev->next = NULL;
            } else {
                gUndoManager.undoList = NULL;
            }

            gUndoManager.totalUndoSize -= last->undoLength;
            gUndoManager.undoCount--;
            TEDisposeUndoRecord(last);
        }
    }
}

static Boolean TECanMergeOperations(const TEUndoRecord *prev, short newType,
                                   long newStart, long newEnd, long timestamp)
{
    if (!prev || !prev->canMerge) return false;
    if (prev->undoType != newType) return false;
    if (timestamp - prev->timestamp > 60) return false; /* 1 second */

    switch (newType) {
        case kTEUndoTyping:
            /* Can merge consecutive typing at adjacent positions */
            return (newStart == prev->undoEnd);

        case kTEUndoDelete:
            /* Can merge consecutive deletions at same position */
            return (newStart == prev->undoStart || newEnd == prev->undoStart);

        default:
            return false;
    }
}

/************************************************************
 * Core Undo Functions
 ************************************************************/

OSErr TEEnableUndo(TEHandle hTE, Boolean enable)
{
    TEInitUndoSystem();
    gUndoManager.undoEnabled = enable;

    if (!enable) {
        /* Clear all undo/redo data */
        TEClearUndoHistory(hTE);
    }

    return noErr;
}

Boolean TEIsUndoEnabled(TEHandle hTE)
{
    if (!gUndoInitialized) TEInitUndoSystem();
    return gUndoManager.undoEnabled;
}

OSErr TEClearUndoHistory(TEHandle hTE)
{
    if (!gUndoInitialized) return noErr;

    if (gUndoManager.undoList) {
        TEDisposeUndoList(gUndoManager.undoList);
        gUndoManager.undoList = NULL;
    }

    if (gUndoManager.redoList) {
        TEDisposeUndoList(gUndoManager.redoList);
        gUndoManager.redoList = NULL;
    }

    if (gUndoManager.currentRecord) {
        TEDisposeUndoRecord(gUndoManager.currentRecord);
        gUndoManager.currentRecord = NULL;
    }

    gUndoManager.undoCount = 0;
    gUndoManager.redoCount = 0;
    gUndoManager.totalUndoSize = 0;

    return noErr;
}

Boolean TECanUndo(TEHandle hTE)
{
    if (!gUndoInitialized) TEInitUndoSystem();
    return (gUndoManager.undoEnabled && gUndoManager.undoList != NULL);
}

Boolean TECanRedo(TEHandle hTE)
{
    if (!gUndoInitialized) TEInitUndoSystem();
    return (gUndoManager.undoEnabled && gUndoManager.redoList != NULL);
}

OSErr TEUndo(TEHandle hTE)
{
    TEUndoRecord *undoRecord;
    TERec **teRec;
    Handle currentText = NULL;
    long currentLength;

    if (!hTE || !*hTE || !TECanUndo(hTE)) return paramErr;

    teRec = (TERec **)hTE;
    undoRecord = gUndoManager.undoList;

    /* Remove from undo list */
    gUndoManager.undoList = undoRecord->next;
    gUndoManager.undoCount--;
    gUndoManager.totalUndoSize -= undoRecord->undoLength;

    /* Save current state for redo */
    if (undoRecord->undoType != kTEUndoNone) {
        Handle redoText = NULL;
        long redoStart = undoRecord->undoStart;
        long redoEnd = undoRecord->undoEnd;

        /* Get current text in the affected range */
        if (undoRecord->undoStart < (**teRec).teLength) {
            long extractStart = undoRecord->undoStart;
            long extractEnd = (undoRecord->undoEnd > (**teRec).teLength) ?
                             (**teRec).teLength : undoRecord->undoEnd;

            if (extractEnd > extractStart) {
                long extractLength = extractEnd - extractStart;
                redoText = NewHandle(extractLength);
                if (redoText) {
                    HLock((**teRec).hText);
                    HLock(redoText);
                    memcpy(*redoText, *(**teRec).hText + extractStart, extractLength);
                    HUnlock(redoText);
                    HUnlock((**teRec).hText);
                }
            }
        }

        /* Create redo record */
        TEUndoRecord *redoRecord = TECreateUndoRecord(undoRecord->undoType,
                                                     redoStart, redoEnd, redoText, NULL);
        if (redoRecord) {
            redoRecord->selStart = (**teRec).selStart;
            redoRecord->selEnd = (**teRec).selEnd;

            /* Add to redo list */
            redoRecord->next = gUndoManager.redoList;
            gUndoManager.redoList = redoRecord;
            gUndoManager.redoCount++;
        }

        if (redoText) DisposeHandle(redoText);
    }

    /* Apply the undo operation */
    switch (undoRecord->undoType) {
        case kTEUndoTyping:
        case kTEUndoPaste:
            /* Remove inserted text */
            TESetSelection(hTE, undoRecord->undoStart, undoRecord->undoEnd);
            TEDelete(hTE);
            break;

        case kTEUndoDelete:
        case kTEUndoCut:
        case kTEUndoClear:
            /* Restore deleted text */
            TESetSelection(hTE, undoRecord->undoStart, undoRecord->undoStart);
            if (undoRecord->undoText && undoRecord->undoLength > 0) {
                HLock(undoRecord->undoText);
                TEInsert(*undoRecord->undoText, undoRecord->undoLength, hTE);
                HUnlock(undoRecord->undoText);
            }
            break;

        case kTEUndoStyle:
            /* Restore style changes - would be implemented in full version */
            break;
    }

    /* Restore selection */
    TESetSelection(hTE, undoRecord->selStart, undoRecord->selEnd);

    /* Dispose the undo record */
    TEDisposeUndoRecord(undoRecord);

    /* Update display */
    TECalText(hTE);
    TEUpdate(NULL, hTE);

    return noErr;
}

OSErr TERedo(TEHandle hTE)
{
    TEUndoRecord *redoRecord;
    TERec **teRec;

    if (!hTE || !*hTE || !TECanRedo(hTE)) return paramErr;

    teRec = (TERec **)hTE;
    redoRecord = gUndoManager.redoList;

    /* Remove from redo list */
    gUndoManager.redoList = redoRecord->next;
    gUndoManager.redoCount--;

    /* Create new undo record for this operation */
    Handle undoText = NULL;
    if (redoRecord->undoStart < (**teRec).teLength) {
        long extractStart = redoRecord->undoStart;
        long extractEnd = (redoRecord->undoEnd > (**teRec).teLength) ?
                         (**teRec).teLength : redoRecord->undoEnd;

        if (extractEnd > extractStart) {
            long extractLength = extractEnd - extractStart;
            undoText = NewHandle(extractLength);
            if (undoText) {
                HLock((**teRec).hText);
                HLock(undoText);
                memcpy(*undoText, *(**teRec).hText + extractStart, extractLength);
                HUnlock(undoText);
                HUnlock((**teRec).hText);
            }
        }
    }

    TEUndoRecord *undoRecord = TECreateUndoRecord(redoRecord->undoType,
                                                 redoRecord->undoStart, redoRecord->undoEnd,
                                                 undoText, NULL);
    if (undoRecord) {
        undoRecord->selStart = (**teRec).selStart;
        undoRecord->selEnd = (**teRec).selEnd;
        TEAddUndoRecord(undoRecord);
    }

    if (undoText) DisposeHandle(undoText);

    /* Apply the redo operation */
    switch (redoRecord->undoType) {
        case kTEUndoTyping:
        case kTEUndoPaste:
            /* Restore inserted text */
            TESetSelection(hTE, redoRecord->undoStart, redoRecord->undoStart);
            if (redoRecord->undoText && redoRecord->undoLength > 0) {
                HLock(redoRecord->undoText);
                TEInsert(*redoRecord->undoText, redoRecord->undoLength, hTE);
                HUnlock(redoRecord->undoText);
            }
            break;

        case kTEUndoDelete:
        case kTEUndoCut:
        case kTEUndoClear:
            /* Remove text */
            TESetSelection(hTE, redoRecord->undoStart, redoRecord->undoEnd);
            TEDelete(hTE);
            break;
    }

    /* Restore selection */
    TESetSelection(hTE, redoRecord->selStart, redoRecord->selEnd);

    /* Dispose the redo record */
    TEDisposeUndoRecord(redoRecord);

    /* Update display */
    TECalText(hTE);
    TEUpdate(NULL, hTE);

    return noErr;
}

/************************************************************
 * Undo Recording Functions
 ************************************************************/

OSErr TEBeginUndoGroup(TEHandle hTE)
{
    if (!gUndoInitialized) TEInitUndoSystem();
    if (!gUndoManager.undoEnabled) return noErr;

    gUndoManager.grouping = true;
    return noErr;
}

OSErr TEEndUndoGroup(TEHandle hTE)
{
    if (!gUndoInitialized) return noErr;

    gUndoManager.grouping = false;

    /* Finish current record if any */
    if (gUndoManager.currentRecord) {
        TEAddUndoRecord(gUndoManager.currentRecord);
        gUndoManager.currentRecord = NULL;
    }

    return noErr;
}

OSErr TERecordUndoTyping(TEHandle hTE, long position, const void* text, long length)
{
    TERec **teRec;
    Handle textHandle;
    TEUndoRecord *record;

    if (!hTE || !*hTE || !gUndoManager.undoEnabled) return noErr;
    if (!text || length <= 0) return noErr;

    teRec = (TERec **)hTE;

    /* Check if we can merge with previous typing operation */
    if (gUndoManager.undoList &&
        TECanMergeOperations(gUndoManager.undoList, kTEUndoTyping,
                           position, position + length, TickCount())) {
        /* Extend the existing record */
        record = gUndoManager.undoList;
        record->undoEnd = position + length;
        record->timestamp = TickCount();
        return noErr;
    }

    /* Create text handle for undo data */
    textHandle = NewHandle(length);
    if (!textHandle) return MemError();

    HLock(textHandle);
    memcpy(*textHandle, text, length);
    HUnlock(textHandle);

    /* Create undo record */
    record = TECreateUndoRecord(kTEUndoTyping, position, position + length,
                               textHandle, NULL);

    DisposeHandle(textHandle);

    if (record) {
        record->selStart = (**teRec).selStart;
        record->selEnd = (**teRec).selEnd;
        record->canMerge = true;
        TEAddUndoRecord(record);
    }

    return noErr;
}

OSErr TERecordUndoDelete(TEHandle hTE, long start, long end, const void* deletedText, long deletedLength)
{
    TERec **teRec;
    Handle textHandle = NULL;
    TEUndoRecord *record;

    if (!hTE || !*hTE || !gUndoManager.undoEnabled) return noErr;

    teRec = (TERec **)hTE;

    /* Create text handle for deleted text */
    if (deletedText && deletedLength > 0) {
        textHandle = NewHandle(deletedLength);
        if (textHandle) {
            HLock(textHandle);
            memcpy(*textHandle, deletedText, deletedLength);
            HUnlock(textHandle);
        }
    }

    /* Create undo record */
    record = TECreateUndoRecord(kTEUndoDelete, start, end, textHandle, NULL);

    if (textHandle) DisposeHandle(textHandle);

    if (record) {
        record->selStart = (**teRec).selStart;
        record->selEnd = (**teRec).selEnd;
        TEAddUndoRecord(record);
    }

    return noErr;
}

OSErr TERecordUndoCut(TEHandle hTE)
{
    TERec **teRec;
    Handle selectedText;
    OSErr err;

    if (!hTE || !*hTE || !gUndoManager.undoEnabled) return noErr;

    teRec = (TERec **)hTE;

    if ((**teRec).selStart == (**teRec).selEnd) return noErr; /* Nothing selected */

    /* Get selected text */
    err = TEGetSelectedText(hTE, &selectedText);
    if (err != noErr || !selectedText) return err;

    /* Record the undo */
    err = TERecordUndoDelete(hTE, (**teRec).selStart, (**teRec).selEnd,
                           *selectedText, GetHandleSize(selectedText));

    DisposeHandle(selectedText);
    return err;
}

OSErr TERecordUndoPaste(TEHandle hTE, long position, long length)
{
    TERec **teRec;
    TEUndoRecord *record;

    if (!hTE || !*hTE || !gUndoManager.undoEnabled) return noErr;

    teRec = (TERec **)hTE;

    /* Create undo record for paste operation */
    record = TECreateUndoRecord(kTEUndoPaste, position, position + length, NULL, NULL);

    if (record) {
        record->selStart = (**teRec).selStart;
        record->selEnd = (**teRec).selEnd;
        TEAddUndoRecord(record);
    }

    return noErr;
}

/************************************************************
 * Undo Information Functions
 ************************************************************/

OSErr TEGetUndoInfo(TEHandle hTE, TEUndoInfo* info)
{
    if (!info || !gUndoInitialized) return paramErr;

    memset(info, 0, sizeof(TEUndoInfo));

    if (gUndoManager.undoList) {
        TEUndoRecord *record = gUndoManager.undoList;
        info->undoType = record->undoType;
        info->undoStart = record->undoStart;
        info->undoEnd = record->undoEnd;
        info->undoText = record->undoText;
        info->undoStyles = record->undoStyles;
    }

    return noErr;
}

short TEGetUndoCount(TEHandle hTE)
{
    if (!gUndoInitialized) TEInitUndoSystem();
    return gUndoManager.undoCount;
}

short TEGetRedoCount(TEHandle hTE)
{
    if (!gUndoInitialized) TEInitUndoSystem();
    return gUndoManager.redoCount;
}

OSErr TEGetUndoDescription(TEHandle hTE, StringPtr description, short maxLength)
{
    if (!description || !gUndoInitialized) return paramErr;

    description[0] = 0; /* Empty string */

    if (gUndoManager.undoList) {
        TEUndoRecord *record = gUndoManager.undoList;
        const char *actionName = "Undo";

        switch (record->undoType) {
            case kTEUndoTyping:
                actionName = "Undo Typing";
                break;
            case kTEUndoDelete:
                actionName = "Undo Delete";
                break;
            case kTEUndoCut:
                actionName = "Undo Cut";
                break;
            case kTEUndoPaste:
                actionName = "Undo Paste";
                break;
            case kTEUndoClear:
                actionName = "Undo Clear";
                break;
            case kTEUndoStyle:
                actionName = "Undo Style Change";
                break;
            default:
                actionName = "Undo";
                break;
        }

        short len = strlen(actionName);
        if (len >= maxLength) len = maxLength - 1;

        description[0] = len;
        memcpy(description + 1, actionName, len);
    }

    return noErr;
}

long TEGetUndoMemoryUsage(TEHandle hTE)
{
    if (!gUndoInitialized) TEInitUndoSystem();
    return gUndoManager.totalUndoSize;
}

/************************************************************
 * Cleanup Functions
 ************************************************************/

void TECleanupUndoSystem(void)
{
    if (!gUndoInitialized) return;

    if (gUndoManager.undoList) {
        TEDisposeUndoList(gUndoManager.undoList);
        gUndoManager.undoList = NULL;
    }

    if (gUndoManager.redoList) {
        TEDisposeUndoList(gUndoManager.redoList);
        gUndoManager.redoList = NULL;
    }

    if (gUndoManager.currentRecord) {
        TEDisposeUndoRecord(gUndoManager.currentRecord);
        gUndoManager.currentRecord = NULL;
    }

    memset(&gUndoManager, 0, sizeof(TEUndoManager));
    gUndoInitialized = false;
}