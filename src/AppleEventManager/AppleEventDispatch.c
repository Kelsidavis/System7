/*
 * Copyright (c) 2024 System7 Project
 * MIT License - See LICENSE file
 */
/*
 * AppleEventDispatch.c
 *
 * Apple Event dispatch and processing implementation
 * Handles event routing, processing, and responses
 *
 * Based on Mac OS 7.1 Apple Event Manager
 */

#include "AppleEventManager/AppleEventManager.h"
#include "AppleEventManager/EventHandlers.h"
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

/* External access to AEMgrState */
extern pthread_mutex_t g_aeMgrMutex;
extern AEHandlerTableEntry* g_eventHandlers;

/* ========================================================================
 * Event Processing Functions
 * ======================================================================== */

OSErr AEProcessAppleEvent(const AppleEvent* event, AppleEvent* reply, AESendMode sendMode, long timeoutInTicks) {
    if (!event) return errAENotAppleEvent;

    /* Look for a handler */
    AEEventClass eventClass;
    AEEventID eventID;
    Size actualSize;

    OSErr err = AEGetKeyPtr(event, keyEventClassAttr, typeType, NULL,
                           &eventClass, sizeof(eventClass), &actualSize);
    if (err != noErr) return err;

    err = AEGetKeyPtr(event, keyEventIDAttr, typeType, NULL,
                     &eventID, sizeof(eventID), &actualSize);
    if (err != noErr) return err;

    /* Find and execute handler */
    pthread_mutex_lock(&g_aeMgrMutex);

    AEHandlerTableEntry* entry = g_eventHandlers;
    while (entry) {
        if (entry->eventClass == eventClass && entry->eventID == eventID) {
            EventHandlerProcPtr handler = entry->handler;
            int32_t refcon = entry->handlerRefcon;
            pthread_mutex_unlock(&g_aeMgrMutex);

            /* Execute handler */
            return handler(event, reply, refcon);
        }
        entry = entry->next;
    }

    /* Check for wildcard handlers */
    entry = g_eventHandlers;
    while (entry) {
        if ((entry->eventClass == typeWildCard || entry->eventClass == eventClass) &&
            entry->eventID == typeWildCard) {
            EventHandlerProcPtr handler = entry->handler;
            int32_t refcon = entry->handlerRefcon;
            pthread_mutex_unlock(&g_aeMgrMutex);

            /* Execute wildcard handler */
            return handler(event, reply, refcon);
        }
        entry = entry->next;
    }

    pthread_mutex_unlock(&g_aeMgrMutex);
    return errAEEventNotHandled;
}

OSErr AESendMessage(const AppleEvent* event, AppleEvent* reply, AESendMode sendMode, long timeoutInTicks) {
    if (!event) return errAENotAppleEvent;

    /* Get target address */
    AEAddressDesc target;
    OSErr err = AEGetKeyDesc(event, keyAddressAttr, typeWildCard, &target);

    if (err != noErr) {
        /* No address means same process */
        return AEProcessAppleEvent(event, reply, sendMode, timeoutInTicks);
    }

    /* Extract target process */
    ProcessSerialNumber psn = {0, kCurrentProcess};
    if (target.descriptorType == typeProcessSerialNumber && target.dataHandle) {
        Size psnSize = AEGetHandleSize(target.dataHandle);
        if (psnSize == sizeof(ProcessSerialNumber)) {
            memcpy(&psn, AEGetHandleData(target.dataHandle), sizeof(ProcessSerialNumber));
        }
    }

    /* Send via HAL */
    err = HAL_SendAppleEvent(event, reply, &psn, sendMode, kAENormalPriority, timeoutInTicks);

    AEDisposeDesc(&target);
    return err;
}

/* ========================================================================
 * List Item Access Functions
 * ======================================================================== */

OSErr AEGetNthPtr(const AEDescList* list, int32_t index, DescType desiredType,
                  DescType* typeCode, void* dataPtr, Size maximumSize, Size* actualSize) {
    if (!list || !actualSize) return errAENotAEDesc;
    if (!list->dataHandle) return errAECorruptData;

    /* AEListHeader structure from core */
    typedef struct {
        int32_t count;
        bool isRecord;
        Size dataSize;
    } AEListHeader;

    typedef struct {
        AEKeyword keyword;
        DescType descriptorType;
        Size dataSize;
    } AEListItem;

    AEListHeader* header = (AEListHeader*)AEGetHandleData(list->dataHandle);

    if (index < 1 || index > header->count) {
        return errAEIllegalIndex;
    }

    /* Find the nth item */
    char* dataPtr_base = (char*)header + sizeof(AEListHeader);
    char* currentPos = dataPtr_base;

    for (int32_t i = 1; i < index; i++) {
        AEListItem* item = (AEListItem*)currentPos;
        currentPos += sizeof(AEListItem) + item->dataSize;
    }

    AEListItem* targetItem = (AEListItem*)currentPos;

    if (typeCode) *typeCode = targetItem->descriptorType;
    *actualSize = targetItem->dataSize;

    if (dataPtr && maximumSize > 0) {
        Size copySize = (targetItem->dataSize < maximumSize) ? targetItem->dataSize : maximumSize;
        memcpy(dataPtr, (char*)targetItem + sizeof(AEListItem), copySize);
    }

    /* Handle type coercion if needed */
    if (desiredType != typeWildCard && desiredType != targetItem->descriptorType) {
        /* Create temporary descriptor and coerce */
        AEDesc tempDesc, coercedDesc;
        OSErr err = AECreateDesc(targetItem->descriptorType,
                                (char*)targetItem + sizeof(AEListItem),
                                targetItem->dataSize, &tempDesc);
        if (err != noErr) return err;

        err = AECoerceDesc(&tempDesc, desiredType, &coercedDesc);
        AEDisposeDesc(&tempDesc);

        if (err != noErr) return errAECoercionFail;

        Size coercedSize = AEGetHandleSize(coercedDesc.dataHandle);
        *actualSize = coercedSize;

        if (dataPtr && maximumSize > 0) {
            Size copySize = (coercedSize < maximumSize) ? coercedSize : maximumSize;
            memcpy(dataPtr, AEGetHandleData(coercedDesc.dataHandle), copySize);
        }

        if (typeCode) *typeCode = coercedDesc.descriptorType;
        AEDisposeDesc(&coercedDesc);
    }

    return noErr;
}

OSErr AEGetNthDesc(const AEDescList* list, int32_t index, DescType desiredType,
                   DescType* typeCode, AEDesc* result) {
    if (!result) return errAENotAEDesc;

    /* First get the size */
    DescType actualType;
    Size actualSize;
    OSErr err = AEGetNthPtr(list, index, desiredType, &actualType, NULL, 0, &actualSize);
    if (err != noErr) return err;

    /* Create a temporary buffer */
    void* tempData = malloc(actualSize);
    if (!tempData) return memFullErr;

    /* Get the actual data */
    err = AEGetNthPtr(list, index, desiredType, &actualType, tempData, actualSize, &actualSize);
    if (err != noErr) {
        free(tempData);
        return err;
    }

    /* Create the result descriptor */
    err = AECreateDesc(actualType, tempData, actualSize, result);
    free(tempData);

    if (typeCode) *typeCode = actualType;
    return err;
}