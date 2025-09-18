/*
 * AppleEventManager.h
 *
 * Main Apple Event Manager API for System 7.1 Portable
 * Provides complete inter-application communication, AppleScript support, and automation
 *
 * Based on Mac OS 7.1 Apple Event Manager with modern portable extensions
 */

#ifndef APPLE_EVENT_MANAGER_H
#define APPLE_EVENT_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * Apple Event Manager Core Types and Constants
 * ======================================================================== */

/* Fundamental Apple Event types */
typedef uint32_t AEKeyword;
typedef uint32_t AEEventClass;
typedef uint32_t AEEventID;
typedef uint32_t DescType;
typedef int16_t OSErr;
typedef int32_t Size;
typedef void* Handle;
typedef void* Ptr;

/* Core Apple Event descriptor structure */
typedef struct AEDesc {
    DescType descriptorType;
    Handle dataHandle;
} AEDesc;

/* Apple Event type aliases */
typedef AEDesc AEAddressDesc;
typedef AEDesc AEDescList;
typedef AEDescList AERecord;
typedef AERecord AppleEvent;

/* Send mode and priority types */
typedef int32_t AESendMode;
typedef int16_t AESendPriority;

/* Interaction and source types */
typedef enum {
    kAEInteractWithSelf = 0,
    kAEInteractWithLocal = 1,
    kAEInteractWithAll = 2
} AEInteractAllowed;

typedef enum {
    kAEUnknownSource = 0,
    kAEDirectCall = 1,
    kAESameProcess = 2,
    kAELocalProcess = 3,
    kAERemoteProcess = 4
} AEEventSource;

/* Process Serial Number for process targeting */
typedef struct ProcessSerialNumber {
    uint32_t highLongOfPSN;
    uint32_t lowLongOfPSN;
} ProcessSerialNumber;

/* ========================================================================
 * Apple Event Manager Constants
 * ======================================================================== */

/* Standard Apple Event data types */
#define typeBoolean 'bool'
#define typeChar 'TEXT'
#define typeSMInt 'shor'
#define typeInteger 'long'
#define typeSMFloat 'sing'
#define typeFloat 'doub'
#define typeLongInteger 'long'
#define typeShortInteger 'shor'
#define typeLongFloat 'doub'
#define typeShortFloat 'sing'
#define typeExtended 'exte'
#define typeComp 'comp'
#define typeMagnitude 'magn'
#define typeAEList 'list'
#define typeAERecord 'reco'
#define typeTrue 'true'
#define typeFalse 'fals'
#define typeAlias 'alis'
#define typeEnumerated 'enum'
#define typeType 'type'
#define typeAppParameters 'appa'
#define typeProperty 'prop'
#define typeFSS 'fss '
#define typeKeyword 'keyw'
#define typeSectionH 'sect'
#define typeWildCard '****'
#define typeNull 'null'
#define typeApplSignature 'sign'
#define typeSessionID 'ssid'
#define typeTargetID 'targ'
#define typeProcessSerialNumber 'psn '
#define typeQDRectangle 'qdrt'
#define typeFixed 'fixd'

/* Core Apple Event classes and IDs */
#define kCoreEventClass 'aevt'
#define kAEOpenApplication 'oapp'
#define kAEOpenDocuments 'odoc'
#define kAEPrintDocuments 'pdoc'
#define kAEQuitApplication 'quit'
#define kAEAnswer 'ansr'

/* System Apple Events */
#define kAECreatorType 'crea'
#define kAEQuitAll 'quia'
#define kAEShutDown 'shut'
#define kAERestart 'rest'
#define kAEApplicationDied 'obit'

/* Standard keywords */
#define keyDirectObject '----'
#define keyErrorNumber 'errn'
#define keyErrorString 'errs'
#define keyProcessSerialNumber 'psn '

/* Special handler keywords */
#define keyPreDispatch 'phac'
#define keySelectProc 'selh'

/* Attribute keywords */
#define keyTransactionIDAttr 'tran'
#define keyReturnIDAttr 'rtid'
#define keyEventClassAttr 'evcl'
#define keyEventIDAttr 'evid'
#define keyAddressAttr 'addr'
#define keyOptionalKeywordAttr 'optk'
#define keyTimeoutAttr 'timo'
#define keyInteractLevelAttr 'inte'
#define keyEventSourceAttr 'esrc'
#define keyMissedKeywordAttr 'miss'

/* Send modes */
#define kAENoReply 0x00000001
#define kAEQueueReply 0x00000002
#define kAEWaitReply 0x00000003

/* Interaction levels */
#define kAENeverInteract 0x00000010
#define kAECanInteract 0x00000020
#define kAEAlwaysInteract 0x00000030
#define kAECanSwitchLayer 0x00000040
#define kAEDontReconnect 0x00000080

/* Timeout constants */
#define kAEDefaultTimeout -1
#define kNoTimeOut -2

/* Transaction and return ID constants */
#define kAnyTransactionID 0
#define kAutoGenerateReturnID -1

/* Dispatch constants */
#define kAENoDispatch 0
#define kAEUseStandardDispatch -1

/* Priority constants */
#define kAENormalPriority 0x00000000

/* Error codes */
#define errAECoercionFail -1700
#define errAEDescNotFound -1701
#define errAECorruptData -1702
#define errAEWrongDataType -1703
#define errAENotAEDesc -1704
#define errAEBadListItem -1705
#define errAENewerVersion -1706
#define errAENotAppleEvent -1707
#define errAEEventNotHandled -1708
#define errAEReplyNotValid -1709
#define errAEUnknownSendMode -1710
#define errAEWaitCanceled -1711
#define errAETimeout -1712
#define errAENoUserInteraction -1713
#define errAENotASpecialFunction -1714
#define errAEParamMissed -1715
#define errAEUnknownAddressType -1716
#define errAEHandlerNotFound -1717
#define errAEReplyNotArrived -1718
#define errAEIllegalIndex -1719

#define noErr 0

/* ========================================================================
 * Function Pointer Types
 * ======================================================================== */

typedef OSErr (*EventHandlerProcPtr)(const AppleEvent* theAppleEvent, AppleEvent* reply, int32_t handlerRefcon);
typedef bool (*IdleProcPtr)(void);
typedef bool (*EventFilterProcPtr)(void* eventRecord, int32_t returnID);
typedef OSErr (*CoercionHandlerProcPtr)(const AEDesc* fromDesc, DescType toType, int32_t handlerRefcon, AEDesc* toDesc);

/* ========================================================================
 * Apple Event Manager Core Functions
 * ======================================================================== */

/* Descriptor creation and manipulation */
OSErr AECreateDesc(DescType typeCode, const void* dataPtr, Size dataSize, AEDesc* result);
OSErr AECoercePtr(DescType typeCode, const void* dataPtr, Size dataSize, DescType toType, AEDesc* result);
OSErr AECoerceDesc(const AEDesc* theAEDesc, DescType toType, AEDesc* result);
OSErr AEDisposeDesc(AEDesc* theAEDesc);
OSErr AEDuplicateDesc(const AEDesc* theAEDesc, AEDesc* result);

/* List and record operations */
OSErr AECreateList(const void* factoringPtr, Size factoredSize, bool isRecord, AEDescList* resultList);
OSErr AECountItems(const AEDescList* theAEDescList, int32_t* theCount);
OSErr AEPutPtr(const AEDescList* theAEDescList, int32_t index, DescType typeCode, const void* dataPtr, Size dataSize);
OSErr AEPutDesc(const AEDescList* theAEDescList, int32_t index, const AEDesc* theAEDesc);
OSErr AEGetNthPtr(const AEDescList* theAEDescList, int32_t index, DescType desiredType, AEKeyword* theAEKeyword, DescType* typeCode, void* dataPtr, Size maximumSize, Size* actualSize);
OSErr AEGetNthDesc(const AEDescList* theAEDescList, int32_t index, DescType desiredType, AEKeyword* theAEKeyword, AEDesc* result);
OSErr AESizeOfNthItem(const AEDescList* theAEDescList, int32_t index, DescType* typeCode, Size* dataSize);
OSErr AEDeleteItem(const AEDescList* theAEDescList, int32_t index);

/* Record operations */
OSErr AEPutKeyPtr(const AERecord* theAERecord, AEKeyword theAEKeyword, DescType typeCode, const void* dataPtr, Size dataSize);
OSErr AEPutKeyDesc(const AERecord* theAERecord, AEKeyword theAEKeyword, const AEDesc* theAEDesc);
OSErr AEGetKeyPtr(const AERecord* theAERecord, AEKeyword theAEKeyword, DescType desiredType, DescType* typeCode, void* dataPtr, Size maximumSize, Size* actualSize);
OSErr AEGetKeyDesc(const AERecord* theAERecord, AEKeyword theAEKeyword, DescType desiredType, AEDesc* result);
OSErr AESizeOfKeyDesc(const AERecord* theAERecord, AEKeyword theAEKeyword, DescType* typeCode, Size* dataSize);
OSErr AEDeleteKeyDesc(const AERecord* theAERecord, AEKeyword theAEKeyword);

/* Apple Event parameter operations */
OSErr AEPutParamPtr(const AppleEvent* theAppleEvent, AEKeyword theAEKeyword, DescType typeCode, const void* dataPtr, Size dataSize);
OSErr AEPutParamDesc(const AppleEvent* theAppleEvent, AEKeyword theAEKeyword, const AEDesc* theAEDesc);
OSErr AEGetParamPtr(const AppleEvent* theAppleEvent, AEKeyword theAEKeyword, DescType desiredType, DescType* typeCode, void* dataPtr, Size maximumSize, Size* actualSize);
OSErr AEGetParamDesc(const AppleEvent* theAppleEvent, AEKeyword theAEKeyword, DescType desiredType, AEDesc* result);
OSErr AESizeOfParam(const AppleEvent* theAppleEvent, AEKeyword theAEKeyword, DescType* typeCode, Size* dataSize);
OSErr AEDeleteParam(const AppleEvent* theAppleEvent, AEKeyword theAEKeyword);

/* Apple Event attribute operations */
OSErr AEGetAttributePtr(const AppleEvent* theAppleEvent, AEKeyword theAEKeyword, DescType desiredType, DescType* typeCode, void* dataPtr, Size maximumSize, Size* actualSize);
OSErr AEGetAttributeDesc(const AppleEvent* theAppleEvent, AEKeyword theAEKeyword, DescType desiredType, AEDesc* result);
OSErr AESizeOfAttribute(const AppleEvent* theAppleEvent, AEKeyword theAEKeyword, DescType* typeCode, Size* dataSize);
OSErr AEPutAttributePtr(const AppleEvent* theAppleEvent, AEKeyword theAEKeyword, DescType typeCode, const void* dataPtr, Size dataSize);
OSErr AEPutAttributeDesc(const AppleEvent* theAppleEvent, AEKeyword theAEKeyword, const AEDesc* theAEDesc);

/* Apple Event creation, sending, and processing */
OSErr AECreateAppleEvent(AEEventClass theAEEventClass, AEEventID theAEEventID, const AEAddressDesc* target, int16_t returnID, int32_t transactionID, AppleEvent* result);
OSErr AESend(const AppleEvent* theAppleEvent, AppleEvent* reply, AESendMode sendMode, AESendPriority sendPriority, int32_t timeOutInTicks, IdleProcPtr idleProc, EventFilterProcPtr filterProc);
OSErr AEProcessAppleEvent(const void* theEventRecord);
OSErr AEResetTimer(const AppleEvent* reply);

/* Advanced event handling */
OSErr AESuspendTheCurrentEvent(const AppleEvent* theAppleEvent);
OSErr AEResumeTheCurrentEvent(const AppleEvent* theAppleEvent, const AppleEvent* reply, EventHandlerProcPtr dispatcher, int32_t handlerRefcon);
OSErr AEGetTheCurrentEvent(AppleEvent* theAppleEvent);
OSErr AESetTheCurrentEvent(const AppleEvent* theAppleEvent);

/* User interaction control */
OSErr AEGetInteractionAllowed(AEInteractAllowed* level);
OSErr AESetInteractionAllowed(AEInteractAllowed level);
OSErr AEInteractWithUser(int32_t timeOutInTicks, void* nmReqPtr, IdleProcPtr idleProc);

/* Event handler management */
OSErr AEInstallEventHandler(AEEventClass theAEEventClass, AEEventID theAEEventID, EventHandlerProcPtr handler, int32_t handlerRefcon, bool isSysHandler);
OSErr AERemoveEventHandler(AEEventClass theAEEventClass, AEEventID theAEEventID, EventHandlerProcPtr handler, bool isSysHandler);
OSErr AEGetEventHandler(AEEventClass theAEEventClass, AEEventID theAEEventID, EventHandlerProcPtr* handler, int32_t* handlerRefcon, bool isSysHandler);

/* Coercion handler management */
OSErr AEInstallCoercionHandler(DescType fromType, DescType toType, CoercionHandlerProcPtr handler, int32_t handlerRefcon, bool fromTypeIsDesc, bool isSysHandler);
OSErr AERemoveCoercionHandler(DescType fromType, DescType toType, CoercionHandlerProcPtr handler, bool isSysHandler);
OSErr AEGetCoercionHandler(DescType fromType, DescType toType, CoercionHandlerProcPtr* handler, int32_t* handlerRefcon, bool* fromTypeIsDesc, bool isSysHandler);

/* Special handler management */
OSErr AEInstallSpecialHandler(AEKeyword functionClass, void* handler, bool isSysHandler);
OSErr AERemoveSpecialHandler(AEKeyword functionClass, void* handler, bool isSysHandler);
OSErr AEGetSpecialHandler(AEKeyword functionClass, void** handler, bool isSysHandler);

/* ========================================================================
 * Apple Event Manager Initialization and Cleanup
 * ======================================================================== */

OSErr AEManagerInit(void);
void AEManagerCleanup(void);

/* Utility functions for process targeting */
OSErr AECreateProcessDesc(const ProcessSerialNumber* psn, AEAddressDesc* addressDesc);
OSErr AECreateApplicationDesc(const char* applicationName, AEAddressDesc* addressDesc);

#ifdef __cplusplus
}
#endif

#endif /* APPLE_EVENT_MANAGER_H */