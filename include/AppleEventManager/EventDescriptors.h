/*
 * EventDescriptors.h
 *
 * Apple Event descriptor types and manipulation functions
 * Provides comprehensive support for Apple Event data structures
 *
 * Based on Mac OS 7.1 Apple Event descriptor system
 */

#ifndef EVENT_DESCRIPTORS_H
#define EVENT_DESCRIPTORS_H

#include "AppleEventManager.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * Extended Descriptor Types and Structures
 * ======================================================================== */

/* Array types for efficient data handling */
typedef enum {
    kAEDataArray = 0,
    kAEPackedArray = 1,
    kAEHandleArray = 2,
    kAEDescArray = 3,
    kAEKeyDescArray = 4
} AEArrayType;

/* Key-descriptor pair for records */
typedef struct AEKeyDesc {
    AEKeyword descKey;
    AEDesc descContent;
} AEKeyDesc;

/* Array data union for different array types */
typedef union AEArrayData {
    int16_t* AEDataArray;
    char* AEPackedArray;
    Handle* AEHandleArray;
    AEDesc* AEDescArray;
    AEKeyDesc* AEKeyDescArray;
} AEArrayData;

typedef AEArrayData* AEArrayDataPointer;

/* Internal descriptor list structure */
typedef struct AEDescListInfo {
    int32_t count;
    int32_t dataSize;
    bool isRecord;
    void* listData;
} AEDescListInfo;

/* ========================================================================
 * Descriptor Creation and Manipulation Functions
 * ======================================================================== */

/* Basic descriptor operations */
OSErr AECreateDescFromData(DescType typeCode, const void* dataPtr, Size dataSize, AEDesc* result);
OSErr AEGetDescData(const AEDesc* theAEDesc, void* dataPtr, Size maximumSize, Size* actualSize);
OSErr AEGetDescDataSize(const AEDesc* theAEDesc, Size* dataSize);
OSErr AEReplaceDescData(DescType typeCode, const void* dataPtr, Size dataSize, AEDesc* theAEDesc);

/* Descriptor inspection */
DescType AEGetDescType(const AEDesc* theAEDesc);
Size AEGetDescSize(const AEDesc* theAEDesc);
bool AEIsNullDesc(const AEDesc* theAEDesc);
bool AEIsListDesc(const AEDesc* theAEDesc);
bool AEIsRecordDesc(const AEDesc* theAEDesc);

/* Descriptor comparison */
bool AECompareDesc(const AEDesc* desc1, const AEDesc* desc2);
OSErr AEDescriptorsEqual(const AEDesc* desc1, const AEDesc* desc2, bool* equal);

/* ========================================================================
 * Array Support Functions
 * ======================================================================== */

OSErr AEGetArray(const AEDescList* theAEDescList, AEArrayType arrayType, AEArrayDataPointer arrayPtr, Size maximumSize, DescType* itemType, Size* itemSize, int32_t* itemCount);
OSErr AEPutArray(const AEDescList* theAEDescList, AEArrayType arrayType, const AEArrayDataPointer arrayPtr, DescType itemType, Size itemSize, int32_t itemCount);

/* Array creation helpers */
OSErr AECreateArrayFromData(AEArrayType arrayType, DescType itemType, const void* arrayData, Size itemSize, int32_t itemCount, AEDescList* resultList);
OSErr AECreateStringArray(const char** strings, int32_t stringCount, AEDescList* resultList);
OSErr AECreateIntegerArray(const int32_t* integers, int32_t integerCount, AEDescList* resultList);

/* ========================================================================
 * List and Record Utility Functions
 * ======================================================================== */

/* List utilities */
OSErr AEAppendDesc(AEDescList* theList, const AEDesc* theDesc);
OSErr AEInsertDesc(AEDescList* theList, int32_t index, const AEDesc* theDesc);
OSErr AEGetListInfo(const AEDescList* theList, AEDescListInfo* listInfo);

/* Record utilities */
OSErr AEPutKeyDesc_Safe(AERecord* theRecord, AEKeyword keyword, const AEDesc* theDesc);
OSErr AEGetKeywordList(const AERecord* theRecord, AEKeyword** keywords, int32_t* keywordCount);
bool AEHasKey(const AERecord* theRecord, AEKeyword keyword);

/* ========================================================================
 * Data Type Coercion Support
 * ======================================================================== */

/* Built-in coercion functions */
OSErr AECoerceToText(const AEDesc* fromDesc, char** textData, Size* textSize);
OSErr AECoerceToInteger(const AEDesc* fromDesc, int32_t* integerValue);
OSErr AECoerceToBoolean(const AEDesc* fromDesc, bool* booleanValue);
OSErr AECoerceToFloat(const AEDesc* fromDesc, double* floatValue);

/* Coercion from text */
OSErr AECoerceFromText(const char* textData, Size textSize, DescType toType, AEDesc* result);

/* ========================================================================
 * Advanced Descriptor Functions
 * ======================================================================== */

/* Descriptor serialization */
OSErr AEFlattenDesc(const AEDesc* theDesc, void** flatData, Size* flatSize);
OSErr AEUnflattenDesc(const void* flatData, Size flatSize, AEDesc* theDesc);

/* Descriptor streaming */
typedef struct AEDescStream AEDescStream;
OSErr AEStreamCreateForWriting(AEDescStream** stream);
OSErr AEStreamCreateForReading(const void* data, Size dataSize, AEDescStream** stream);
OSErr AEStreamWriteDesc(AEDescStream* stream, const AEDesc* theDesc);
OSErr AEStreamReadDesc(AEDescStream* stream, AEDesc* theDesc);
OSErr AEStreamClose(AEDescStream* stream);

/* Descriptor validation */
OSErr AEValidateDesc(const AEDesc* theDesc);
OSErr AEValidateDescList(const AEDescList* theList);
OSErr AEValidateRecord(const AERecord* theRecord);

/* ========================================================================
 * Memory Management Helpers
 * ======================================================================== */

/* Handle allocation and management */
Handle AEAllocateHandle(Size size);
void AEDisposeHandle(Handle h);
OSErr AESetHandleSize(Handle h, Size newSize);
Size AEGetHandleSize(Handle h);
void AEHLock(Handle h);
void AEHUnlock(Handle h);

/* Bulk descriptor operations */
OSErr AEDisposeDescArray(AEDesc* descArray, int32_t count);
OSErr AEDuplicateDescArray(const AEDesc* sourceArray, int32_t count, AEDesc** destArray);

/* ========================================================================
 * Debugging and Inspection Functions
 * ======================================================================== */

#ifdef DEBUG
/* Debug output functions */
void AEPrintDesc(const AEDesc* theDesc, const char* label);
void AEPrintDescList(const AEDescList* theList, const char* label);
void AEPrintRecord(const AERecord* theRecord, const char* label);

/* Descriptor statistics */
typedef struct AEDescStats {
    int32_t totalDescs;
    int32_t totalDataSize;
    int32_t listCount;
    int32_t recordCount;
    int32_t nullDescCount;
} AEDescStats;

OSErr AEGetDescriptorStats(AEDescStats* stats);
void AEResetDescriptorStats(void);
#endif

#ifdef __cplusplus
}
#endif

#endif /* EVENT_DESCRIPTORS_H */