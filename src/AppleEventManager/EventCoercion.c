/*
 * Copyright (c) 2024 System7 Project
 * MIT License - See LICENSE file
 */
/*
 * EventCoercion.c
 *
 * Apple Event type coercion handlers
 * Provides automatic conversion between descriptor types
 *
 * Based on Mac OS 7.1 Apple Event Manager
 */

#include "AppleEventManager/AppleEventManager.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* External access to AEMgrState */
extern pthread_mutex_t g_aeMgrMutex;
extern AECoercionHandlerEntry* g_coercionHandlers;

/* ========================================================================
 * Text Coercion Functions
 * ======================================================================== */

static OSErr TextToIntegerCoercion(const AEDesc* fromDesc, DescType toType, long refcon, AEDesc* toDesc) {
    (void)refcon;

    if (!fromDesc || !toDesc) return errAENotAEDesc;
    if (!fromDesc->dataHandle) return errAECorruptData;

    Size textSize = AEGetHandleSize(fromDesc->dataHandle);
    char* text = (char*)AEGetHandleData(fromDesc->dataHandle);

    if (!text || textSize == 0) return errAECoercionFail;

    /* Create temporary null-terminated string */
    char* tempStr = malloc(textSize + 1);
    if (!tempStr) return memFullErr;
    memcpy(tempStr, text, textSize);
    tempStr[textSize] = '\0';

    /* Convert to integer */
    char* endPtr;
    long value = strtol(tempStr, &endPtr, 10);
    free(tempStr);

    if (endPtr == tempStr) {
        return errAECoercionFail;  /* No conversion performed */
    }

    /* Create appropriate integer descriptor */
    if (toType == typeShortInteger) {
        short shortVal = (short)value;
        return AECreateDesc(typeShortInteger, &shortVal, sizeof(short), toDesc);
    } else if (toType == typeLongInteger) {
        return AECreateDesc(typeLongInteger, &value, sizeof(long), toDesc);
    }

    return errAECoercionFail;
}

static OSErr IntegerToTextCoercion(const AEDesc* fromDesc, DescType toType, long refcon, AEDesc* toDesc) {
    (void)refcon;
    (void)toType;

    if (!fromDesc || !toDesc) return errAENotAEDesc;
    if (!fromDesc->dataHandle) return errAECorruptData;

    char buffer[32];

    if (fromDesc->descriptorType == typeShortInteger) {
        short* value = (short*)AEGetHandleData(fromDesc->dataHandle);
        if (!value) return errAECorruptData;
        snprintf(buffer, sizeof(buffer), "%d", (int)*value);
    } else if (fromDesc->descriptorType == typeLongInteger) {
        long* value = (long*)AEGetHandleData(fromDesc->dataHandle);
        if (!value) return errAECorruptData;
        snprintf(buffer, sizeof(buffer), "%ld", *value);
    } else {
        return errAECoercionFail;
    }

    return AECreateDesc(typeText, buffer, strlen(buffer), toDesc);
}

static OSErr TextToBooleanCoercion(const AEDesc* fromDesc, DescType toType, long refcon, AEDesc* toDesc) {
    (void)refcon;
    (void)toType;

    if (!fromDesc || !toDesc) return errAENotAEDesc;
    if (!fromDesc->dataHandle) return errAECorruptData;

    Size textSize = AEGetHandleSize(fromDesc->dataHandle);
    char* text = (char*)AEGetHandleData(fromDesc->dataHandle);

    if (!text || textSize == 0) return errAECoercionFail;

    Boolean value = false;

    /* Check for common boolean text values */
    if (textSize == 4 && strncmp(text, "true", 4) == 0) {
        value = true;
    } else if (textSize == 5 && strncmp(text, "false", 5) == 0) {
        value = false;
    } else if (textSize == 1) {
        if (text[0] == '1' || text[0] == 'T' || text[0] == 't' || text[0] == 'Y' || text[0] == 'y') {
            value = true;
        } else if (text[0] == '0' || text[0] == 'F' || text[0] == 'f' || text[0] == 'N' || text[0] == 'n') {
            value = false;
        } else {
            return errAECoercionFail;
        }
    } else {
        return errAECoercionFail;
    }

    return AECreateDesc(typeBoolean, &value, sizeof(Boolean), toDesc);
}

static OSErr BooleanToTextCoercion(const AEDesc* fromDesc, DescType toType, long refcon, AEDesc* toDesc) {
    (void)refcon;
    (void)toType;

    if (!fromDesc || !toDesc) return errAENotAEDesc;
    if (!fromDesc->dataHandle) return errAECorruptData;

    Boolean* value = (Boolean*)AEGetHandleData(fromDesc->dataHandle);
    if (!value) return errAECorruptData;

    const char* text = (*value) ? "true" : "false";
    return AECreateDesc(typeText, text, strlen(text), toDesc);
}

/* ========================================================================
 * Numeric Coercion Functions
 * ======================================================================== */

static OSErr ShortToLongCoercion(const AEDesc* fromDesc, DescType toType, long refcon, AEDesc* toDesc) {
    (void)refcon;
    (void)toType;

    if (!fromDesc || !toDesc) return errAENotAEDesc;
    if (!fromDesc->dataHandle) return errAECorruptData;

    short* shortVal = (short*)AEGetHandleData(fromDesc->dataHandle);
    if (!shortVal) return errAECorruptData;

    long longVal = *shortVal;
    return AECreateDesc(typeLongInteger, &longVal, sizeof(long), toDesc);
}

static OSErr LongToShortCoercion(const AEDesc* fromDesc, DescType toType, long refcon, AEDesc* toDesc) {
    (void)refcon;
    (void)toType;

    if (!fromDesc || !toDesc) return errAENotAEDesc;
    if (!fromDesc->dataHandle) return errAECorruptData;

    long* longVal = (long*)AEGetHandleData(fromDesc->dataHandle);
    if (!longVal) return errAECorruptData;

    /* Check for overflow */
    if (*longVal > 32767 || *longVal < -32768) {
        return errAECoercionFail;
    }

    short shortVal = (short)*longVal;
    return AECreateDesc(typeShortInteger, &shortVal, sizeof(short), toDesc);
}

static OSErr FloatToLongCoercion(const AEDesc* fromDesc, DescType toType, long refcon, AEDesc* toDesc) {
    (void)refcon;
    (void)toType;

    if (!fromDesc || !toDesc) return errAENotAEDesc;
    if (!fromDesc->dataHandle) return errAECorruptData;

    float* floatVal = (float*)AEGetHandleData(fromDesc->dataHandle);
    if (!floatVal) return errAECorruptData;

    long longVal = (long)*floatVal;
    return AECreateDesc(typeLongInteger, &longVal, sizeof(long), toDesc);
}

static OSErr LongToFloatCoercion(const AEDesc* fromDesc, DescType toType, long refcon, AEDesc* toDesc) {
    (void)refcon;
    (void)toType;

    if (!fromDesc || !toDesc) return errAENotAEDesc;
    if (!fromDesc->dataHandle) return errAECorruptData;

    long* longVal = (long*)AEGetHandleData(fromDesc->dataHandle);
    if (!longVal) return errAECorruptData;

    float floatVal = (float)*longVal;
    return AECreateDesc(typeFloat, &floatVal, sizeof(float), toDesc);
}

/* ========================================================================
 * File and Alias Coercion Functions
 * ======================================================================== */

static OSErr AliasToFSSpecCoercion(const AEDesc* fromDesc, DescType toType, long refcon, AEDesc* toDesc) {
    (void)refcon;
    (void)toType;

    if (!fromDesc || !toDesc) return errAENotAEDesc;
    if (!fromDesc->dataHandle) return errAECorruptData;

    /* For now, just copy the alias data as FSSpec */
    /* In a full implementation, this would resolve the alias */
    Size dataSize = AEGetHandleSize(fromDesc->dataHandle);
    void* data = AEGetHandleData(fromDesc->dataHandle);

    return AECreateDesc(typeFSS, data, dataSize, toDesc);
}

static OSErr FSSpecToAliasCoercion(const AEDesc* fromDesc, DescType toType, long refcon, AEDesc* toDesc) {
    (void)refcon;
    (void)toType;

    if (!fromDesc || !toDesc) return errAENotAEDesc;
    if (!fromDesc->dataHandle) return errAECorruptData;

    /* For now, just copy the FSSpec data as alias */
    /* In a full implementation, this would create an alias record */
    Size dataSize = AEGetHandleSize(fromDesc->dataHandle);
    void* data = AEGetHandleData(fromDesc->dataHandle);

    return AECreateDesc(typeAlias, data, dataSize, toDesc);
}

/* ========================================================================
 * Coercion Handler Management
 * ======================================================================== */

OSErr AEInstallCoercionHandler(DescType fromType, DescType toType, AECoercionHandlerUPP handler, long handlerRefcon, Boolean fromTypeIsDesc, Boolean isSysHandler) {
    (void)fromTypeIsDesc;  /* Not used in this implementation */

    if (!handler) return errAENotAEDesc;

    pthread_mutex_lock(&g_aeMgrMutex);

    /* Check if handler already exists */
    AECoercionHandlerEntry* entry = g_coercionHandlers;
    while (entry) {
        if (entry->fromType == fromType && entry->toType == toType && entry->isSysHandler == isSysHandler) {
            /* Replace existing handler */
            entry->handler = handler;
            entry->handlerRefcon = handlerRefcon;
            pthread_mutex_unlock(&g_aeMgrMutex);
            return noErr;
        }
        entry = entry->next;
    }

    /* Add new handler */
    AECoercionHandlerEntry* newEntry = malloc(sizeof(AECoercionHandlerEntry));
    if (!newEntry) {
        pthread_mutex_unlock(&g_aeMgrMutex);
        return memFullErr;
    }

    newEntry->fromType = fromType;
    newEntry->toType = toType;
    newEntry->handler = handler;
    newEntry->handlerRefcon = handlerRefcon;
    newEntry->isSysHandler = isSysHandler;
    newEntry->next = g_coercionHandlers;
    g_coercionHandlers = newEntry;

    pthread_mutex_unlock(&g_aeMgrMutex);
    return noErr;
}

OSErr AERemoveCoercionHandler(DescType fromType, DescType toType, AECoercionHandlerUPP handler, Boolean isSysHandler) {
    (void)handler;  /* We identify by types only */

    pthread_mutex_lock(&g_aeMgrMutex);

    AECoercionHandlerEntry** current = &g_coercionHandlers;
    while (*current) {
        if ((*current)->fromType == fromType && (*current)->toType == toType && (*current)->isSysHandler == isSysHandler) {
            AECoercionHandlerEntry* toRemove = *current;
            *current = (*current)->next;
            free(toRemove);
            pthread_mutex_unlock(&g_aeMgrMutex);
            return noErr;
        }
        current = &(*current)->next;
    }

    pthread_mutex_unlock(&g_aeMgrMutex);
    return errAEHandlerNotFound;
}

OSErr AEGetCoercionHandler(DescType fromType, DescType toType, AECoercionHandlerUPP* handler, long* handlerRefcon, Boolean* fromTypeIsDesc, Boolean isSysHandler) {
    if (!handler) return errAENotAEDesc;

    pthread_mutex_lock(&g_aeMgrMutex);

    AECoercionHandlerEntry* entry = g_coercionHandlers;
    while (entry) {
        if (entry->fromType == fromType && entry->toType == toType && entry->isSysHandler == isSysHandler) {
            *handler = entry->handler;
            if (handlerRefcon) *handlerRefcon = entry->handlerRefcon;
            if (fromTypeIsDesc) *fromTypeIsDesc = false;
            pthread_mutex_unlock(&g_aeMgrMutex);
            return noErr;
        }
        entry = entry->next;
    }

    pthread_mutex_unlock(&g_aeMgrMutex);
    return errAEHandlerNotFound;
}

OSErr AECoercePtr(DescType typeCode, const void* dataPtr, Size dataSize, DescType toType, AEDesc* result) {
    if (!result) return errAENotAEDesc;

    /* If types are the same, just create the descriptor */
    if (typeCode == toType) {
        return AECreateDesc(toType, dataPtr, dataSize, result);
    }

    /* Create temporary descriptor */
    AEDesc tempDesc;
    OSErr err = AECreateDesc(typeCode, dataPtr, dataSize, &tempDesc);
    if (err != noErr) return err;

    /* Coerce the descriptor */
    err = AECoerceDesc(&tempDesc, toType, result);
    AEDisposeDesc(&tempDesc);

    return err;
}

OSErr AECoerceDesc(const AEDesc* theAEDesc, DescType toType, AEDesc* result) {
    if (!theAEDesc || !result) return errAENotAEDesc;

    /* If types are the same, just duplicate */
    if (theAEDesc->descriptorType == toType) {
        return AEDuplicateDesc(theAEDesc, result);
    }

    /* Look for a coercion handler */
    pthread_mutex_lock(&g_aeMgrMutex);

    AECoercionHandlerEntry* entry = g_coercionHandlers;
    while (entry) {
        if (entry->fromType == theAEDesc->descriptorType && entry->toType == toType) {
            /* Found a handler */
            AECoercionHandlerUPP handler = entry->handler;
            long refcon = entry->handlerRefcon;
            pthread_mutex_unlock(&g_aeMgrMutex);

            return handler(theAEDesc, toType, refcon, result);
        }
        entry = entry->next;
    }

    pthread_mutex_unlock(&g_aeMgrMutex);

    /* No handler found */
    return errAECoercionFail;
}

/* ========================================================================
 * Initialize Built-in Coercion Handlers
 * ======================================================================== */

OSErr InitBuiltinCoercionHandlers(void) {
    OSErr err;

    /* Text conversions */
    err = AEInstallCoercionHandler(typeText, typeLongInteger, TextToIntegerCoercion, 0, false, true);
    if (err != noErr) return err;

    err = AEInstallCoercionHandler(typeText, typeShortInteger, TextToIntegerCoercion, 0, false, true);
    if (err != noErr) return err;

    err = AEInstallCoercionHandler(typeLongInteger, typeText, IntegerToTextCoercion, 0, false, true);
    if (err != noErr) return err;

    err = AEInstallCoercionHandler(typeShortInteger, typeText, IntegerToTextCoercion, 0, false, true);
    if (err != noErr) return err;

    err = AEInstallCoercionHandler(typeText, typeBoolean, TextToBooleanCoercion, 0, false, true);
    if (err != noErr) return err;

    err = AEInstallCoercionHandler(typeBoolean, typeText, BooleanToTextCoercion, 0, false, true);
    if (err != noErr) return err;

    /* Numeric conversions */
    err = AEInstallCoercionHandler(typeShortInteger, typeLongInteger, ShortToLongCoercion, 0, false, true);
    if (err != noErr) return err;

    err = AEInstallCoercionHandler(typeLongInteger, typeShortInteger, LongToShortCoercion, 0, false, true);
    if (err != noErr) return err;

    err = AEInstallCoercionHandler(typeFloat, typeLongInteger, FloatToLongCoercion, 0, false, true);
    if (err != noErr) return err;

    err = AEInstallCoercionHandler(typeLongInteger, typeFloat, LongToFloatCoercion, 0, false, true);
    if (err != noErr) return err;

    /* File/Alias conversions */
    err = AEInstallCoercionHandler(typeAlias, typeFSS, AliasToFSSpecCoercion, 0, false, true);
    if (err != noErr) return err;

    err = AEInstallCoercionHandler(typeFSS, typeAlias, FSSpecToAliasCoercion, 0, false, true);
    if (err != noErr) return err;

    return noErr;
}