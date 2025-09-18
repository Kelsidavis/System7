/*
 * Copyright (c) 2024 System7 Project
 * MIT License - See LICENSE file
 */
/*
 * EventDescriptors.c
 *
 * Apple Event descriptor handling and manipulation functions
 * Provides comprehensive support for Apple Event data structures
 *
 * Based on Mac OS 7.1 Apple Event descriptor system
 */

#include "AppleEventManager/EventDescriptors.h"
#include "AppleEventManager/AppleEventManager.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <math.h>

/* ========================================================================
 * Extended Descriptor Functions
 * ======================================================================== */

OSErr AECreateDescFromData(DescType typeCode, const void* dataPtr, Size dataSize, AEDesc* result) {
    return AECreateDesc(typeCode, dataPtr, dataSize, result);
}

OSErr AEGetDescData(const AEDesc* theAEDesc, void* dataPtr, Size maximumSize, Size* actualSize) {
    if (!theAEDesc || !actualSize) return errAENotAEDesc;

    *actualSize = 0;
    if (!theAEDesc->dataHandle) {
        return noErr;
    }

    Size dataSize = AEGetHandleSize(theAEDesc->dataHandle);
    *actualSize = dataSize;

    if (dataPtr && maximumSize > 0) {
        Size copySize = (dataSize < maximumSize) ? dataSize : maximumSize;
        memcpy(dataPtr, AEGetHandleData(theAEDesc->dataHandle), copySize);
    }

    return noErr;
}

OSErr AEGetDescDataSize(const AEDesc* theAEDesc, Size* dataSize) {
    if (!theAEDesc || !dataSize) return errAENotAEDesc;

    if (!theAEDesc->dataHandle) {
        *dataSize = 0;
    } else {
        *dataSize = AEGetHandleSize(theAEDesc->dataHandle);
    }

    return noErr;
}

OSErr AEReplaceDescData(DescType typeCode, const void* dataPtr, Size dataSize, AEDesc* theAEDesc) {
    if (!theAEDesc) return errAENotAEDesc;

    /* Dispose old data */
    if (theAEDesc->dataHandle) {
        AEDisposeHandle(theAEDesc->dataHandle);
        theAEDesc->dataHandle = NULL;
    }

    /* Set new type */
    theAEDesc->descriptorType = typeCode;

    /* Create new data if provided */
    if (dataSize > 0 && dataPtr) {
        theAEDesc->dataHandle = AEAllocateHandle(dataSize);
        if (!theAEDesc->dataHandle) {
            return memFullErr;
        }
        memcpy(AEGetHandleData(theAEDesc->dataHandle), dataPtr, dataSize);
    }

    return noErr;
}

/* ========================================================================
 * Descriptor Inspection Functions
 * ======================================================================== */

DescType AEGetDescType(const AEDesc* theAEDesc) {
    if (!theAEDesc) return typeNull;
    return theAEDesc->descriptorType;
}

Size AEGetDescSize(const AEDesc* theAEDesc) {
    if (!theAEDesc || !theAEDesc->dataHandle) return 0;
    return AEGetHandleSize(theAEDesc->dataHandle);
}

bool AEIsNullDesc(const AEDesc* theAEDesc) {
    if (!theAEDesc) return true;
    return (theAEDesc->descriptorType == typeNull || theAEDesc->dataHandle == NULL);
}

bool AEIsListDesc(const AEDesc* theAEDesc) {
    if (!theAEDesc) return false;
    return (theAEDesc->descriptorType == typeAEList);
}

bool AEIsRecordDesc(const AEDesc* theAEDesc) {
    if (!theAEDesc) return false;
    return (theAEDesc->descriptorType == typeAERecord);
}

/* ========================================================================
 * Descriptor Comparison Functions
 * ======================================================================== */

bool AECompareDesc(const AEDesc* desc1, const AEDesc* desc2) {
    if (!desc1 || !desc2) return false;
    if (desc1->descriptorType != desc2->descriptorType) return false;

    Size size1 = AEGetDescSize(desc1);
    Size size2 = AEGetDescSize(desc2);
    if (size1 != size2) return false;

    if (size1 == 0) return true;

    return memcmp(AEGetHandleData(desc1->dataHandle),
                  AEGetHandleData(desc2->dataHandle),
                  size1) == 0;
}

OSErr AEDescriptorsEqual(const AEDesc* desc1, const AEDesc* desc2, bool* equal) {
    if (!equal) return errAENotAEDesc;
    *equal = AECompareDesc(desc1, desc2);
    return noErr;
}

/* ========================================================================
 * Array Creation Helper Functions
 * ======================================================================== */

OSErr AECreateStringArray(const char** strings, int32_t stringCount, AEDescList* resultList) {
    if (!strings || !resultList) return errAENotAEDesc;
    if (stringCount < 0) return errAEIllegalIndex;

    OSErr err = AECreateList(NULL, 0, false, resultList);
    if (err != noErr) return err;

    for (int32_t i = 0; i < stringCount; i++) {
        if (strings[i]) {
            err = AEPutPtr(resultList, i + 1, typeChar, strings[i], strlen(strings[i]));
            if (err != noErr) {
                AEDisposeDesc(resultList);
                return err;
            }
        }
    }

    return noErr;
}

OSErr AECreateIntegerArray(const int32_t* integers, int32_t integerCount, AEDescList* resultList) {
    if (!integers || !resultList) return errAENotAEDesc;
    if (integerCount < 0) return errAEIllegalIndex;

    OSErr err = AECreateList(NULL, 0, false, resultList);
    if (err != noErr) return err;

    for (int32_t i = 0; i < integerCount; i++) {
        err = AEPutPtr(resultList, i + 1, typeLongInteger, &integers[i], sizeof(int32_t));
        if (err != noErr) {
            AEDisposeDesc(resultList);
            return err;
        }
    }

    return noErr;
}

/* ========================================================================
 * List Utility Functions
 * ======================================================================== */

OSErr AEAppendDesc(AEDescList* theList, const AEDesc* theDesc) {
    if (!theList || !theDesc) return errAENotAEDesc;

    int32_t count;
    OSErr err = AECountItems(theList, &count);
    if (err != noErr) return err;

    return AEPutDesc(theList, count + 1, theDesc);
}

OSErr AEInsertDesc(AEDescList* theList, int32_t index, const AEDesc* theDesc) {
    if (!theList || !theDesc) return errAENotAEDesc;
    return AEPutDesc(theList, index, theDesc);
}

OSErr AEGetListInfo(const AEDescList* theList, AEDescListInfo* listInfo) {
    if (!theList || !listInfo) return errAENotAEDesc;

    OSErr err = AECountItems(theList, &listInfo->count);
    if (err != noErr) return err;

    listInfo->dataSize = AEGetDescSize(theList);
    listInfo->isRecord = AEIsRecordDesc(theList);
    listInfo->listData = theList->dataHandle ? AEGetHandleData(theList->dataHandle) : NULL;

    return noErr;
}

/* ========================================================================
 * Record Utility Functions
 * ======================================================================== */

OSErr AEPutKeyDesc_Safe(AERecord* theRecord, AEKeyword keyword, const AEDesc* theDesc) {
    if (!theRecord || !theDesc) return errAENotAEDesc;

    /* Check if it's actually a record */
    if (!AEIsRecordDesc(theRecord)) {
        return errAEWrongDataType;
    }

    return AEPutKeyDesc(theRecord, keyword, theDesc);
}

bool AEHasKey(const AERecord* theRecord, AEKeyword keyword) {
    if (!theRecord) return false;

    AEDesc tempDesc;
    OSErr err = AEGetKeyDesc(theRecord, keyword, typeWildCard, &tempDesc);
    if (err == noErr) {
        AEDisposeDesc(&tempDesc);
        return true;
    }
    return false;
}

/* ========================================================================
 * Built-in Coercion Functions
 * ======================================================================== */

OSErr AECoerceToText(const AEDesc* fromDesc, char** textData, Size* textSize) {
    if (!fromDesc || !textData || !textSize) return errAENotAEDesc;

    *textData = NULL;
    *textSize = 0;

    /* Handle text types directly */
    if (fromDesc->descriptorType == typeChar) {
        Size dataSize = AEGetDescSize(fromDesc);
        *textData = malloc(dataSize + 1);
        if (!*textData) return memFullErr;

        if (dataSize > 0) {
            memcpy(*textData, AEGetHandleData(fromDesc->dataHandle), dataSize);
        }
        (*textData)[dataSize] = '\0';
        *textSize = dataSize;
        return noErr;
    }

    /* Convert other types to text */
    char buffer[256];
    const char* resultText = NULL;
    Size resultSize = 0;

    switch (fromDesc->descriptorType) {
        case typeLongInteger:
        case typeInteger: {
            if (AEGetDescSize(fromDesc) >= sizeof(int32_t)) {
                int32_t value = *(int32_t*)AEGetHandleData(fromDesc->dataHandle);
                snprintf(buffer, sizeof(buffer), "%d", value);
                resultText = buffer;
                resultSize = strlen(buffer);
            }
            break;
        }

        case typeShortInteger: {
            if (AEGetDescSize(fromDesc) >= sizeof(int16_t)) {
                int16_t value = *(int16_t*)AEGetHandleData(fromDesc->dataHandle);
                snprintf(buffer, sizeof(buffer), "%d", value);
                resultText = buffer;
                resultSize = strlen(buffer);
            }
            break;
        }

        case typeFloat:
        case typeLongFloat: {
            if (AEGetDescSize(fromDesc) >= sizeof(double)) {
                double value = *(double*)AEGetHandleData(fromDesc->dataHandle);
                snprintf(buffer, sizeof(buffer), "%.6g", value);
                resultText = buffer;
                resultSize = strlen(buffer);
            }
            break;
        }

        case typeShortFloat: {
            if (AEGetDescSize(fromDesc) >= sizeof(float)) {
                float value = *(float*)AEGetHandleData(fromDesc->dataHandle);
                snprintf(buffer, sizeof(buffer), "%.6g", value);
                resultText = buffer;
                resultSize = strlen(buffer);
            }
            break;
        }

        case typeBoolean:
        case typeTrue:
        case typeFalse: {
            if (fromDesc->descriptorType == typeTrue) {
                resultText = "true";
                resultSize = 4;
            } else if (fromDesc->descriptorType == typeFalse) {
                resultText = "false";
                resultSize = 5;
            } else if (AEGetDescSize(fromDesc) >= 1) {
                bool value = *(bool*)AEGetHandleData(fromDesc->dataHandle);
                resultText = value ? "true" : "false";
                resultSize = value ? 4 : 5;
            }
            break;
        }

        case typeType: {
            if (AEGetDescSize(fromDesc) >= sizeof(DescType)) {
                DescType value = *(DescType*)AEGetHandleData(fromDesc->dataHandle);
                /* Convert four-char code to string */
                buffer[0] = (value >> 24) & 0xFF;
                buffer[1] = (value >> 16) & 0xFF;
                buffer[2] = (value >> 8) & 0xFF;
                buffer[3] = value & 0xFF;
                buffer[4] = '\0';
                resultText = buffer;
                resultSize = 4;
            }
            break;
        }

        default:
            return errAECoercionFail;
    }

    if (resultText && resultSize > 0) {
        *textData = malloc(resultSize + 1);
        if (!*textData) return memFullErr;

        memcpy(*textData, resultText, resultSize);
        (*textData)[resultSize] = '\0';
        *textSize = resultSize;
        return noErr;
    }

    return errAECoercionFail;
}

OSErr AECoerceToInteger(const AEDesc* fromDesc, int32_t* integerValue) {
    if (!fromDesc || !integerValue) return errAENotAEDesc;

    *integerValue = 0;

    switch (fromDesc->descriptorType) {
        case typeLongInteger:
        case typeInteger: {
            if (AEGetDescSize(fromDesc) >= sizeof(int32_t)) {
                *integerValue = *(int32_t*)AEGetHandleData(fromDesc->dataHandle);
                return noErr;
            }
            break;
        }

        case typeShortInteger: {
            if (AEGetDescSize(fromDesc) >= sizeof(int16_t)) {
                *integerValue = (int32_t)(*(int16_t*)AEGetHandleData(fromDesc->dataHandle));
                return noErr;
            }
            break;
        }

        case typeFloat:
        case typeLongFloat: {
            if (AEGetDescSize(fromDesc) >= sizeof(double)) {
                double value = *(double*)AEGetHandleData(fromDesc->dataHandle);
                *integerValue = (int32_t)round(value);
                return noErr;
            }
            break;
        }

        case typeShortFloat: {
            if (AEGetDescSize(fromDesc) >= sizeof(float)) {
                float value = *(float*)AEGetHandleData(fromDesc->dataHandle);
                *integerValue = (int32_t)round(value);
                return noErr;
            }
            break;
        }

        case typeBoolean:
        case typeTrue:
        case typeFalse: {
            if (fromDesc->descriptorType == typeTrue) {
                *integerValue = 1;
                return noErr;
            } else if (fromDesc->descriptorType == typeFalse) {
                *integerValue = 0;
                return noErr;
            } else if (AEGetDescSize(fromDesc) >= 1) {
                bool value = *(bool*)AEGetHandleData(fromDesc->dataHandle);
                *integerValue = value ? 1 : 0;
                return noErr;
            }
            break;
        }

        case typeChar: {
            /* Try to parse text as integer */
            char* textData;
            Size textSize;
            OSErr err = AECoerceToText(fromDesc, &textData, &textSize);
            if (err == noErr && textData) {
                char* endPtr;
                long value = strtol(textData, &endPtr, 10);
                if (endPtr != textData && *endPtr == '\0') {
                    *integerValue = (int32_t)value;
                    free(textData);
                    return noErr;
                }
                free(textData);
            }
            break;
        }
    }

    return errAECoercionFail;
}

OSErr AECoerceToBoolean(const AEDesc* fromDesc, bool* booleanValue) {
    if (!fromDesc || !booleanValue) return errAENotAEDesc;

    *booleanValue = false;

    switch (fromDesc->descriptorType) {
        case typeBoolean: {
            if (AEGetDescSize(fromDesc) >= 1) {
                *booleanValue = *(bool*)AEGetHandleData(fromDesc->dataHandle);
                return noErr;
            }
            break;
        }

        case typeTrue:
            *booleanValue = true;
            return noErr;

        case typeFalse:
            *booleanValue = false;
            return noErr;

        case typeLongInteger:
        case typeInteger: {
            if (AEGetDescSize(fromDesc) >= sizeof(int32_t)) {
                int32_t value = *(int32_t*)AEGetHandleData(fromDesc->dataHandle);
                *booleanValue = (value != 0);
                return noErr;
            }
            break;
        }

        case typeShortInteger: {
            if (AEGetDescSize(fromDesc) >= sizeof(int16_t)) {
                int16_t value = *(int16_t*)AEGetHandleData(fromDesc->dataHandle);
                *booleanValue = (value != 0);
                return noErr;
            }
            break;
        }

        case typeChar: {
            /* Check for common boolean text values */
            char* textData;
            Size textSize;
            OSErr err = AECoerceToText(fromDesc, &textData, &textSize);
            if (err == noErr && textData) {
                /* Convert to lowercase for comparison */
                for (Size i = 0; i < textSize; i++) {
                    textData[i] = tolower(textData[i]);
                }

                if (strcmp(textData, "true") == 0 || strcmp(textData, "yes") == 0 || strcmp(textData, "1") == 0) {
                    *booleanValue = true;
                    free(textData);
                    return noErr;
                } else if (strcmp(textData, "false") == 0 || strcmp(textData, "no") == 0 || strcmp(textData, "0") == 0) {
                    *booleanValue = false;
                    free(textData);
                    return noErr;
                }
                free(textData);
            }
            break;
        }
    }

    return errAECoercionFail;
}

OSErr AECoerceToFloat(const AEDesc* fromDesc, double* floatValue) {
    if (!fromDesc || !floatValue) return errAENotAEDesc;

    *floatValue = 0.0;

    switch (fromDesc->descriptorType) {
        case typeFloat:
        case typeLongFloat: {
            if (AEGetDescSize(fromDesc) >= sizeof(double)) {
                *floatValue = *(double*)AEGetHandleData(fromDesc->dataHandle);
                return noErr;
            }
            break;
        }

        case typeShortFloat: {
            if (AEGetDescSize(fromDesc) >= sizeof(float)) {
                float value = *(float*)AEGetHandleData(fromDesc->dataHandle);
                *floatValue = (double)value;
                return noErr;
            }
            break;
        }

        case typeLongInteger:
        case typeInteger: {
            if (AEGetDescSize(fromDesc) >= sizeof(int32_t)) {
                int32_t value = *(int32_t*)AEGetHandleData(fromDesc->dataHandle);
                *floatValue = (double)value;
                return noErr;
            }
            break;
        }

        case typeShortInteger: {
            if (AEGetDescSize(fromDesc) >= sizeof(int16_t)) {
                int16_t value = *(int16_t*)AEGetHandleData(fromDesc->dataHandle);
                *floatValue = (double)value;
                return noErr;
            }
            break;
        }

        case typeChar: {
            /* Try to parse text as float */
            char* textData;
            Size textSize;
            OSErr err = AECoerceToText(fromDesc, &textData, &textSize);
            if (err == noErr && textData) {
                char* endPtr;
                double value = strtod(textData, &endPtr);
                if (endPtr != textData && *endPtr == '\0') {
                    *floatValue = value;
                    free(textData);
                    return noErr;
                }
                free(textData);
            }
            break;
        }
    }

    return errAECoercionFail;
}

/* ========================================================================
 * Memory Management Helpers
 * ======================================================================== */

OSErr AEDisposeDescArray(AEDesc* descArray, int32_t count) {
    if (!descArray || count < 0) return errAENotAEDesc;

    for (int32_t i = 0; i < count; i++) {
        AEDisposeDesc(&descArray[i]);
    }

    return noErr;
}

OSErr AEDuplicateDescArray(const AEDesc* sourceArray, int32_t count, AEDesc** destArray) {
    if (!sourceArray || !destArray || count < 0) return errAENotAEDesc;

    *destArray = malloc(count * sizeof(AEDesc));
    if (!*destArray) return memFullErr;

    for (int32_t i = 0; i < count; i++) {
        OSErr err = AEDuplicateDesc(&sourceArray[i], &(*destArray)[i]);
        if (err != noErr) {
            /* Clean up partial allocation */
            for (int32_t j = 0; j < i; j++) {
                AEDisposeDesc(&(*destArray)[j]);
            }
            free(*destArray);
            *destArray = NULL;
            return err;
        }
    }

    return noErr;
}

/* ========================================================================
 * Descriptor Validation Functions
 * ======================================================================== */

OSErr AEValidateDesc(const AEDesc* theDesc) {
    if (!theDesc) return errAENotAEDesc;

    /* Check for null descriptor */
    if (theDesc->descriptorType == typeNull) {
        if (theDesc->dataHandle != NULL) {
            return errAECorruptData;
        }
        return noErr;
    }

    /* Check for valid handle */
    if (theDesc->dataHandle == NULL) {
        /* Only typeNull should have NULL handle */
        if (theDesc->descriptorType != typeNull) {
            return errAECorruptData;
        }
    }

    /* Validate list/record structure */
    if (theDesc->descriptorType == typeAEList || theDesc->descriptorType == typeAERecord) {
        /* TODO: Validate internal list structure */
    }

    return noErr;
}

OSErr AEValidateDescList(const AEDescList* theList) {
    if (!theList) return errAENotAEDesc;

    OSErr err = AEValidateDesc((const AEDesc*)theList);
    if (err != noErr) return err;

    /* Additional list-specific validation */
    if (!AEIsListDesc(theList) && !AEIsRecordDesc(theList)) {
        return errAEWrongDataType;
    }

    return noErr;
}

OSErr AEValidateRecord(const AERecord* theRecord) {
    if (!theRecord) return errAENotAEDesc;

    OSErr err = AEValidateDesc((const AEDesc*)theRecord);
    if (err != noErr) return err;

    /* Additional record-specific validation */
    if (!AEIsRecordDesc(theRecord)) {
        return errAEWrongDataType;
    }

    return noErr;
}

/* ========================================================================
 * Debug Functions
 * ======================================================================== */

#ifdef DEBUG
void AEPrintDesc(const AEDesc* theDesc, const char* label) {
    if (!theDesc) {
        printf("%s: NULL descriptor\n", label ? label : "AEDesc");
        return;
    }

    printf("%s: type='%.4s', size=%ld\n",
           label ? label : "AEDesc",
           (char*)&theDesc->descriptorType,
           (long)AEGetDescSize(theDesc));

    /* Print some data for common types */
    if (theDesc->descriptorType == typeChar && theDesc->dataHandle) {
        Size size = AEGetDescSize(theDesc);
        char* data = (char*)AEGetHandleData(theDesc->dataHandle);
        printf("  Text: \"%.*s\"\n", (int)size, data);
    } else if ((theDesc->descriptorType == typeLongInteger || theDesc->descriptorType == typeInteger)
               && AEGetDescSize(theDesc) >= sizeof(int32_t)) {
        int32_t value = *(int32_t*)AEGetHandleData(theDesc->dataHandle);
        printf("  Integer: %d\n", value);
    }
}

void AEPrintDescList(const AEDescList* theList, const char* label) {
    if (!theList) {
        printf("%s: NULL list\n", label ? label : "AEDescList");
        return;
    }

    int32_t count;
    OSErr err = AECountItems(theList, &count);
    if (err != noErr) {
        printf("%s: Error getting count (%d)\n", label ? label : "AEDescList", err);
        return;
    }

    printf("%s: %s with %d items\n",
           label ? label : "AEDescList",
           AEIsRecordDesc(theList) ? "Record" : "List",
           count);

    /* Print first few items */
    for (int32_t i = 1; i <= count && i <= 5; i++) {
        AEDesc item;
        AEKeyword keyword;
        err = AEGetNthDesc(theList, i, typeWildCard, &keyword, &item);
        if (err == noErr) {
            char itemLabel[64];
            snprintf(itemLabel, sizeof(itemLabel), "  Item %d", i);
            AEPrintDesc(&item, itemLabel);
            AEDisposeDesc(&item);
        }
    }

    if (count > 5) {
        printf("  ... and %d more items\n", count - 5);
    }
}
#endif