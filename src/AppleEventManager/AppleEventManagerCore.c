/*
 * Copyright (c) 2024 System7 Project
 * MIT License - See LICENSE file
 */
/*
 * AppleEventManagerCore.c
 *
 * Core Apple Event Manager implementation
 * Provides the fundamental Apple Event system for inter-application communication
 *
 * Based on Mac OS 7.1 Apple Event Manager
 */

#include "AppleEventManager/AppleEventManager.h"
#include "AppleEventManager/EventDescriptors.h"
#include "AppleEventManager/EventHandlers.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>
#include <pthread.h>

/* ========================================================================
 * Internal Data Structures
 * ======================================================================== */

/* Apple Event Manager state */
typedef struct AEMgrState {
    bool initialized;
    pthread_mutex_t mutex;

    /* Current event context */
    const AppleEvent* currentEvent;
    AppleEvent* currentReply;
    bool eventSuspended;

    /* Interaction settings */
    AEInteractAllowed interactionLevel;
    int32_t defaultTimeout;

    /* Handler tables */
    AEHandlerTableEntry* eventHandlers;
    AECoercionHandlerEntry* coercionHandlers;
    AESpecialHandlerEntry* specialHandlers;

    /* Statistics */
    int32_t eventsProcessed;
    int32_t descriptorsCreated;
    int32_t handlersInstalled;

    /* Memory tracking */
    int32_t totalHandles;
    Size totalMemoryAllocated;
} AEMgrState;

static AEMgrState g_aeMgrState = {0};

/* Handle structure for Apple Event data */
typedef struct AEHandle {
    Size size;
    void* data;
    bool locked;
    int32_t refCount;
} AEHandle;

/* ========================================================================
 * Memory Management Functions
 * ======================================================================== */

Handle AEAllocateHandle(Size size) {
    AEHandle* handle = malloc(sizeof(AEHandle));
    if (!handle) return NULL;

    handle->data = malloc(size);
    if (!handle->data) {
        free(handle);
        return NULL;
    }

    handle->size = size;
    handle->locked = false;
    handle->refCount = 1;

    pthread_mutex_lock(&g_aeMgrState.mutex);
    g_aeMgrState.totalHandles++;
    g_aeMgrState.totalMemoryAllocated += size + sizeof(AEHandle);
    pthread_mutex_unlock(&g_aeMgrState.mutex);

    return (Handle)handle;
}

void AEDisposeHandle(Handle h) {
    if (!h) return;

    AEHandle* handle = (AEHandle*)h;
    handle->refCount--;

    if (handle->refCount <= 0) {
        pthread_mutex_lock(&g_aeMgrState.mutex);
        g_aeMgrState.totalHandles--;
        g_aeMgrState.totalMemoryAllocated -= (handle->size + sizeof(AEHandle));
        pthread_mutex_unlock(&g_aeMgrState.mutex);

        free(handle->data);
        free(handle);
    }
}

OSErr AESetHandleSize(Handle h, Size newSize) {
    if (!h) return errAENotAEDesc;

    AEHandle* handle = (AEHandle*)h;
    void* newData = realloc(handle->data, newSize);
    if (!newData) return memFullErr;

    pthread_mutex_lock(&g_aeMgrState.mutex);
    g_aeMgrState.totalMemoryAllocated += (newSize - handle->size);
    pthread_mutex_unlock(&g_aeMgrState.mutex);

    handle->data = newData;
    handle->size = newSize;
    return noErr;
}

Size AEGetHandleSize(Handle h) {
    if (!h) return 0;
    return ((AEHandle*)h)->size;
}

void AEHLock(Handle h) {
    if (h) ((AEHandle*)h)->locked = true;
}

void AEHUnlock(Handle h) {
    if (h) ((AEHandle*)h)->locked = false;
}

void* AEGetHandleData(Handle h) {
    if (!h) return NULL;
    return ((AEHandle*)h)->data;
}

/* ========================================================================
 * Apple Event Manager Initialization
 * ======================================================================== */

OSErr AEManagerInit(void) {
    if (g_aeMgrState.initialized) {
        return noErr;
    }

    memset(&g_aeMgrState, 0, sizeof(AEMgrState));

    if (pthread_mutex_init(&g_aeMgrState.mutex, NULL) != 0) {
        return errAENotAppleEvent;
    }

    g_aeMgrState.initialized = true;
    g_aeMgrState.interactionLevel = kAEInteractWithLocal;
    g_aeMgrState.defaultTimeout = kAEDefaultTimeout;
    g_aeMgrState.currentEvent = NULL;
    g_aeMgrState.currentReply = NULL;
    g_aeMgrState.eventSuspended = false;

    /* Install built-in coercion handlers */
    // Text coercions will be implemented in EventCoercion.c

    return noErr;
}

void AEManagerCleanup(void) {
    if (!g_aeMgrState.initialized) {
        return;
    }

    pthread_mutex_lock(&g_aeMgrState.mutex);

    /* Clean up handler tables */
    AEHandlerTableEntry* handler = g_aeMgrState.eventHandlers;
    while (handler) {
        AEHandlerTableEntry* next = handler->next;
        free(handler);
        handler = next;
    }

    AECoercionHandlerEntry* coercion = g_aeMgrState.coercionHandlers;
    while (coercion) {
        AECoercionHandlerEntry* next = coercion->next;
        free(coercion);
        coercion = next;
    }

    AESpecialHandlerEntry* special = g_aeMgrState.specialHandlers;
    while (special) {
        AESpecialHandlerEntry* next = special->next;
        free(special);
        special = next;
    }

    g_aeMgrState.initialized = false;
    pthread_mutex_unlock(&g_aeMgrState.mutex);
    pthread_mutex_destroy(&g_aeMgrState.mutex);
}

/* ========================================================================
 * Descriptor Creation and Manipulation
 * ======================================================================== */

OSErr AECreateDesc(DescType typeCode, const void* dataPtr, Size dataSize, AEDesc* result) {
    if (!result) return errAENotAEDesc;
    if (!g_aeMgrState.initialized) return errAENewerVersion;

    /* Initialize the descriptor */
    result->descriptorType = typeCode;
    result->dataHandle = NULL;

    if (dataSize > 0 && dataPtr) {
        result->dataHandle = AEAllocateHandle(dataSize);
        if (!result->dataHandle) {
            return memFullErr;
        }

        memcpy(AEGetHandleData(result->dataHandle), dataPtr, dataSize);
    }

    pthread_mutex_lock(&g_aeMgrState.mutex);
    g_aeMgrState.descriptorsCreated++;
    pthread_mutex_unlock(&g_aeMgrState.mutex);

    return noErr;
}

OSErr AEDisposeDesc(AEDesc* theAEDesc) {
    if (!theAEDesc) return errAENotAEDesc;

    if (theAEDesc->dataHandle) {
        AEDisposeHandle(theAEDesc->dataHandle);
        theAEDesc->dataHandle = NULL;
    }

    theAEDesc->descriptorType = typeNull;
    return noErr;
}

OSErr AEDuplicateDesc(const AEDesc* theAEDesc, AEDesc* result) {
    if (!theAEDesc || !result) return errAENotAEDesc;

    result->descriptorType = theAEDesc->descriptorType;
    result->dataHandle = NULL;

    if (theAEDesc->dataHandle) {
        Size dataSize = AEGetHandleSize(theAEDesc->dataHandle);
        result->dataHandle = AEAllocateHandle(dataSize);
        if (!result->dataHandle) {
            return memFullErr;
        }

        memcpy(AEGetHandleData(result->dataHandle),
               AEGetHandleData(theAEDesc->dataHandle),
               dataSize);
    }

    return noErr;
}

/* ========================================================================
 * List and Record Operations
 * ======================================================================== */

typedef struct AEListHeader {
    int32_t count;
    bool isRecord;
    Size dataSize;
} AEListHeader;

typedef struct AEListItem {
    AEKeyword keyword;  /* Only used for records */
    DescType descriptorType;
    Size dataSize;
    /* Data follows immediately */
} AEListItem;

OSErr AECreateList(const void* factoringPtr, Size factoredSize, bool isRecord, AEDescList* resultList) {
    if (!resultList) return errAENotAEDesc;

    Size initialSize = sizeof(AEListHeader);
    if (factoredSize > 0) {
        initialSize += factoredSize;
    }

    resultList->descriptorType = isRecord ? typeAERecord : typeAEList;
    resultList->dataHandle = AEAllocateHandle(initialSize);
    if (!resultList->dataHandle) {
        return memFullErr;
    }

    AEListHeader* header = (AEListHeader*)AEGetHandleData(resultList->dataHandle);
    header->count = 0;
    header->isRecord = isRecord;
    header->dataSize = sizeof(AEListHeader);

    if (factoredSize > 0 && factoringPtr) {
        memcpy((char*)header + sizeof(AEListHeader), factoringPtr, factoredSize);
        header->dataSize += factoredSize;
    }

    return noErr;
}

OSErr AECountItems(const AEDescList* theAEDescList, int32_t* theCount) {
    if (!theAEDescList || !theCount) return errAENotAEDesc;
    if (!theAEDescList->dataHandle) return errAECorruptData;

    AEListHeader* header = (AEListHeader*)AEGetHandleData(theAEDescList->dataHandle);
    *theCount = header->count;
    return noErr;
}

OSErr AEPutPtr(const AEDescList* theAEDescList, int32_t index, DescType typeCode, const void* dataPtr, Size dataSize) {
    if (!theAEDescList) return errAENotAEDesc;
    if (!theAEDescList->dataHandle) return errAECorruptData;

    AEListHeader* header = (AEListHeader*)AEGetHandleData(theAEDescList->dataHandle);

    /* For records, index should be 0 or negative to append */
    if (header->isRecord) {
        return errAEWrongDataType;
    }

    if (index < 0 || index > header->count + 1) {
        return errAEIllegalIndex;
    }

    /* Calculate new size needed */
    Size itemSize = sizeof(AEListItem) + dataSize;
    Size currentSize = AEGetHandleSize(theAEDescList->dataHandle);
    Size newSize = currentSize + itemSize;

    /* Resize handle */
    OSErr err = AESetHandleSize(theAEDescList->dataHandle, newSize);
    if (err != noErr) return err;

    /* Refresh header pointer after resize */
    header = (AEListHeader*)AEGetHandleData(theAEDescList->dataHandle);

    /* Find insertion point */
    char* dataPtr_base = (char*)header + sizeof(AEListHeader);
    char* insertPoint = dataPtr_base + header->dataSize - sizeof(AEListHeader);

    if (index <= header->count) {
        /* Find the correct position */
        char* currentPos = dataPtr_base;
        for (int32_t i = 0; i < index && i < header->count; i++) {
            AEListItem* item = (AEListItem*)currentPos;
            currentPos += sizeof(AEListItem) + item->dataSize;
        }
        insertPoint = currentPos;

        /* Move existing data if inserting in middle */
        if (index < header->count) {
            Size moveSize = (dataPtr_base + header->dataSize - sizeof(AEListHeader)) - insertPoint;
            memmove(insertPoint + itemSize, insertPoint, moveSize);
        }
    }

    /* Create new item */
    AEListItem* newItem = (AEListItem*)insertPoint;
    newItem->keyword = 0;  /* Not used for lists */
    newItem->descriptorType = typeCode;
    newItem->dataSize = dataSize;

    if (dataSize > 0 && dataPtr) {
        memcpy((char*)newItem + sizeof(AEListItem), dataPtr, dataSize);
    }

    header->count++;
    header->dataSize += itemSize;

    return noErr;
}

OSErr AEPutDesc(const AEDescList* theAEDescList, int32_t index, const AEDesc* theAEDesc) {
    if (!theAEDesc) return errAENotAEDesc;

    Size dataSize = 0;
    void* dataPtr = NULL;

    if (theAEDesc->dataHandle) {
        dataSize = AEGetHandleSize(theAEDesc->dataHandle);
        dataPtr = AEGetHandleData(theAEDesc->dataHandle);
    }

    return AEPutPtr(theAEDescList, index, theAEDesc->descriptorType, dataPtr, dataSize);
}

/* ========================================================================
 * Record Operations
 * ======================================================================== */

OSErr AEPutKeyPtr(const AERecord* theAERecord, AEKeyword theAEKeyword, DescType typeCode, const void* dataPtr, Size dataSize) {
    if (!theAERecord) return errAENotAEDesc;
    if (!theAERecord->dataHandle) return errAECorruptData;

    AEListHeader* header = (AEListHeader*)AEGetHandleData(theAERecord->dataHandle);
    if (!header->isRecord) {
        return errAEWrongDataType;
    }

    /* Check if key already exists and replace if so */
    char* dataPtr_base = (char*)header + sizeof(AEListHeader);
    char* currentPos = dataPtr_base;

    for (int32_t i = 0; i < header->count; i++) {
        AEListItem* item = (AEListItem*)currentPos;
        if (item->keyword == theAEKeyword) {
            /* Replace existing item */
            Size oldItemSize = sizeof(AEListItem) + item->dataSize;
            Size newItemSize = sizeof(AEListItem) + dataSize;
            Size sizeDiff = newItemSize - oldItemSize;

            if (sizeDiff != 0) {
                /* Resize handle if necessary */
                Size currentSize = AEGetHandleSize(theAERecord->dataHandle);
                OSErr err = AESetHandleSize(theAERecord->dataHandle, currentSize + sizeDiff);
                if (err != noErr) return err;

                /* Refresh pointers after resize */
                header = (AEListHeader*)AEGetHandleData(theAERecord->dataHandle);
                dataPtr_base = (char*)header + sizeof(AEListHeader);
                currentPos = dataPtr_base;
                for (int32_t j = 0; j < i; j++) {
                    AEListItem* temp = (AEListItem*)currentPos;
                    currentPos += sizeof(AEListItem) + temp->dataSize;
                }
                item = (AEListItem*)currentPos;

                /* Move data after this item */
                char* nextPos = currentPos + oldItemSize;
                Size moveSize = (dataPtr_base + header->dataSize - sizeof(AEListHeader)) - nextPos;
                if (moveSize > 0) {
                    memmove(currentPos + newItemSize, nextPos, moveSize);
                }
            }

            /* Update item */
            item->keyword = theAEKeyword;
            item->descriptorType = typeCode;
            item->dataSize = dataSize;

            if (dataSize > 0 && dataPtr) {
                memcpy((char*)item + sizeof(AEListItem), dataPtr, dataSize);
            }

            header->dataSize += sizeDiff;
            return noErr;
        }
        currentPos += sizeof(AEListItem) + item->dataSize;
    }

    /* Key not found, add new item */
    Size itemSize = sizeof(AEListItem) + dataSize;
    Size currentSize = AEGetHandleSize(theAERecord->dataHandle);
    Size newSize = currentSize + itemSize;

    OSErr err = AESetHandleSize(theAERecord->dataHandle, newSize);
    if (err != noErr) return err;

    /* Refresh header pointer after resize */
    header = (AEListHeader*)AEGetHandleData(theAERecord->dataHandle);

    /* Add new item at end */
    char* newItemPos = (char*)header + header->dataSize;
    AEListItem* newItem = (AEListItem*)newItemPos;
    newItem->keyword = theAEKeyword;
    newItem->descriptorType = typeCode;
    newItem->dataSize = dataSize;

    if (dataSize > 0 && dataPtr) {
        memcpy((char*)newItem + sizeof(AEListItem), dataPtr, dataSize);
    }

    header->count++;
    header->dataSize += itemSize;

    return noErr;
}

OSErr AEPutKeyDesc(const AERecord* theAERecord, AEKeyword theAEKeyword, const AEDesc* theAEDesc) {
    if (!theAEDesc) return errAENotAEDesc;

    Size dataSize = 0;
    void* dataPtr = NULL;

    if (theAEDesc->dataHandle) {
        dataSize = AEGetHandleSize(theAEDesc->dataHandle);
        dataPtr = AEGetHandleData(theAEDesc->dataHandle);
    }

    return AEPutKeyPtr(theAERecord, theAEKeyword, theAEDesc->descriptorType, dataPtr, dataSize);
}

OSErr AEGetKeyPtr(const AERecord* theAERecord, AEKeyword theAEKeyword, DescType desiredType, DescType* typeCode, void* dataPtr, Size maximumSize, Size* actualSize) {
    if (!theAERecord || !actualSize) return errAENotAEDesc;
    if (!theAERecord->dataHandle) return errAECorruptData;

    AEListHeader* header = (AEListHeader*)AEGetHandleData(theAERecord->dataHandle);
    if (!header->isRecord) {
        return errAEWrongDataType;
    }

    /* Find the key */
    char* dataPtr_base = (char*)header + sizeof(AEListHeader);
    char* currentPos = dataPtr_base;

    for (int32_t i = 0; i < header->count; i++) {
        AEListItem* item = (AEListItem*)currentPos;
        if (item->keyword == theAEKeyword) {
            /* Found the key */
            if (typeCode) *typeCode = item->descriptorType;
            *actualSize = item->dataSize;

            if (dataPtr && maximumSize > 0) {
                Size copySize = (item->dataSize < maximumSize) ? item->dataSize : maximumSize;
                memcpy(dataPtr, (char*)item + sizeof(AEListItem), copySize);
            }

            /* Handle type coercion if needed */
            if (desiredType != typeWildCard && desiredType != item->descriptorType) {
                /* TODO: Implement coercion */
                return errAECoercionFail;
            }

            return noErr;
        }
        currentPos += sizeof(AEListItem) + item->dataSize;
    }

    return errAEDescNotFound;
}

OSErr AEGetKeyDesc(const AERecord* theAERecord, AEKeyword theAEKeyword, DescType desiredType, AEDesc* result) {
    if (!result) return errAENotAEDesc;

    /* First get the size */
    DescType actualType;
    Size actualSize;
    OSErr err = AEGetKeyPtr(theAERecord, theAEKeyword, desiredType, &actualType, NULL, 0, &actualSize);
    if (err != noErr) return err;

    /* Create a temporary buffer */
    void* tempData = malloc(actualSize);
    if (!tempData) return memFullErr;

    /* Get the actual data */
    err = AEGetKeyPtr(theAERecord, theAEKeyword, desiredType, &actualType, tempData, actualSize, &actualSize);
    if (err != noErr) {
        free(tempData);
        return err;
    }

    /* Create the result descriptor */
    err = AECreateDesc(actualType, tempData, actualSize, result);
    free(tempData);

    return err;
}

/* ========================================================================
 * Apple Event Parameter Operations
 * ======================================================================== */

/* Apple Events are just records with special attributes */
OSErr AEPutParamPtr(const AppleEvent* theAppleEvent, AEKeyword theAEKeyword, DescType typeCode, const void* dataPtr, Size dataSize) {
    return AEPutKeyPtr((const AERecord*)theAppleEvent, theAEKeyword, typeCode, dataPtr, dataSize);
}

OSErr AEPutParamDesc(const AppleEvent* theAppleEvent, AEKeyword theAEKeyword, const AEDesc* theAEDesc) {
    return AEPutKeyDesc((const AERecord*)theAppleEvent, theAEKeyword, theAEDesc);
}

OSErr AEGetParamPtr(const AppleEvent* theAppleEvent, AEKeyword theAEKeyword, DescType desiredType, DescType* typeCode, void* dataPtr, Size maximumSize, Size* actualSize) {
    return AEGetKeyPtr((const AERecord*)theAppleEvent, theAEKeyword, desiredType, typeCode, dataPtr, maximumSize, actualSize);
}

OSErr AEGetParamDesc(const AppleEvent* theAppleEvent, AEKeyword theAEKeyword, DescType desiredType, AEDesc* result) {
    return AEGetKeyDesc((const AERecord*)theAppleEvent, theAEKeyword, desiredType, result);
}

/* ========================================================================
 * Apple Event Creation
 * ======================================================================== */

OSErr AECreateAppleEvent(AEEventClass theAEEventClass, AEEventID theAEEventID, const AEAddressDesc* target, int16_t returnID, int32_t transactionID, AppleEvent* result) {
    if (!result) return errAENotAEDesc;

    /* Create a record for the Apple Event */
    OSErr err = AECreateList(NULL, 0, true, result);
    if (err != noErr) return err;

    /* Add required attributes */
    err = AEPutKeyPtr(result, keyEventClassAttr, typeType, &theAEEventClass, sizeof(theAEEventClass));
    if (err != noErr) goto cleanup;

    err = AEPutKeyPtr(result, keyEventIDAttr, typeType, &theAEEventID, sizeof(theAEEventID));
    if (err != noErr) goto cleanup;

    if (target) {
        err = AEPutKeyDesc(result, keyAddressAttr, target);
        if (err != noErr) goto cleanup;
    }

    if (returnID != kAutoGenerateReturnID) {
        err = AEPutKeyPtr(result, keyReturnIDAttr, typeShortInteger, &returnID, sizeof(returnID));
        if (err != noErr) goto cleanup;
    } else {
        /* Generate a unique return ID */
        static int16_t nextReturnID = 1;
        int16_t generatedID = nextReturnID++;
        err = AEPutKeyPtr(result, keyReturnIDAttr, typeShortInteger, &generatedID, sizeof(generatedID));
        if (err != noErr) goto cleanup;
    }

    if (transactionID != kAnyTransactionID) {
        err = AEPutKeyPtr(result, keyTransactionIDAttr, typeLongInteger, &transactionID, sizeof(transactionID));
        if (err != noErr) goto cleanup;
    }

    /* Add source attribute */
    AEEventSource source = kAESameProcess;  /* TODO: Determine actual source */
    err = AEPutKeyPtr(result, keyEventSourceAttr, typeEnumerated, &source, sizeof(source));
    if (err != noErr) goto cleanup;

    return noErr;

cleanup:
    AEDisposeDesc(result);
    return err;
}

/* ========================================================================
 * Current Event Management
 * ======================================================================== */

OSErr AEGetTheCurrentEvent(AppleEvent* theAppleEvent) {
    if (!theAppleEvent) return errAENotAEDesc;
    if (!g_aeMgrState.currentEvent) return errAEEventNotHandled;

    return AEDuplicateDesc(g_aeMgrState.currentEvent, theAppleEvent);
}

OSErr AESetTheCurrentEvent(const AppleEvent* theAppleEvent) {
    g_aeMgrState.currentEvent = theAppleEvent;
    return noErr;
}

/* ========================================================================
 * Interaction Management
 * ======================================================================== */

OSErr AEGetInteractionAllowed(AEInteractAllowed* level) {
    if (!level) return errAENotAEDesc;
    *level = g_aeMgrState.interactionLevel;
    return noErr;
}

OSErr AESetInteractionAllowed(AEInteractAllowed level) {
    g_aeMgrState.interactionLevel = level;
    return noErr;
}

/* ========================================================================
 * Utility Functions
 * ======================================================================== */

OSErr AECreateProcessDesc(const ProcessSerialNumber* psn, AEAddressDesc* addressDesc) {
    if (!psn || !addressDesc) return errAENotAEDesc;

    return AECreateDesc(typeProcessSerialNumber, psn, sizeof(ProcessSerialNumber), addressDesc);
}

OSErr AECreateApplicationDesc(const char* applicationName, AEAddressDesc* addressDesc) {
    if (!applicationName || !addressDesc) return errAENotAEDesc;

    return AECreateDesc(typeApplSignature, applicationName, strlen(applicationName), addressDesc);
}