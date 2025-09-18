/*
 * ObjectModel.h
 *
 * Object model and accessibility support for Apple Events
 * Provides object specifier resolution and accessibility framework
 *
 * Based on Mac OS 7.1 Object Support Library with modern extensions
 */

#ifndef OBJECT_MODEL_H
#define OBJECT_MODEL_H

#include "AppleEventManager.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * Object Model Core Types
 * ======================================================================== */

/* Object specifier types */
#define typeObjectSpecifier 'obj '
#define typeAbsoluteOrdinal 'abso'
#define typeIndexDescriptor 'inde'
#define typeMiddleDescriptor 'midd'
#define typeRandomDescriptor 'rand'
#define typeRangeDescriptor 'rang'
#define typeLogicalDescriptor 'logi'
#define typeCompDescriptor 'ccmp'
#define typeWhoseDescriptor 'whos'

/* Object classes */
#define cApplication 'capp'
#define cDocument 'docu'
#define cFile 'file'
#define cInsertionPoint 'cins'
#define cObject 'cobj'
#define cProperty 'prop'
#define cSelection 'csel'
#define cText 'ctxt'
#define cWindow 'cwin'
#define cWord 'cwor'
#define cChar 'cha '
#define cParagraph 'cpar'
#define cLine 'clin'

/* Form types */
#define formAbsolutePosition 'indx'
#define formRelativePosition 'rele'
#define formName 'name'
#define formUniqueID 'ID  '
#define formPropertyID 'prop'
#define formRange 'rang'
#define formTest 'test'

/* Logical operators */
#define kAEAND 'AND '
#define kAEOR 'OR  '
#define kAENOT 'NOT '

/* Comparison operators */
#define kAEEquals 'equal'
#define kAENotEquals 'ne  '
#define kAEGreaterThan 'gt  '
#define kAEGreaterThanEquals 'ge  '
#define kAELessThan 'lt  '
#define kAELessThanEquals 'le  '
#define kAEBeginsWith 'bgwt'
#define kAEEndsWith 'ends'
#define kAEContains 'cont'

/* Position constants */
#define kAEFirst 'firs'
#define kAELast 'last'
#define kAEMiddle 'midd'
#define kAEAny 'any '
#define kAEAll 'all '

/* ========================================================================
 * Object Accessor and Resolution Types
 * ======================================================================== */

/* Object accessor callback */
typedef OSErr (*OSLAccessorProcPtr)(DescType desiredClass, const AEDesc* container, DescType containerClass, DescType form, const AEDesc* selectionData, AEDesc* value, int32_t accessorRefcon);

/* Object counting callback */
typedef OSErr (*OSLCountProcPtr)(DescType desiredClass, DescType containerClass, const AEDesc* container, int32_t* result);

/* Object disposal callback */
typedef OSErr (*OSLDisposeProcPtr)(AEDesc* unneededToken);

/* Object comparison callback */
typedef OSErr (*OSLCompareProcPtr)(DescType oper, const AEDesc* obj1, const AEDesc* obj2, bool* result);

/* Mark token callback */
typedef OSErr (*OSLMarkProcPtr)(const AEDesc* dToken, const AEDesc* markToken, int32_t index);

/* Object adjustment callback */
typedef OSErr (*OSLAdjustProcPtr)(DescType newStart, DescType newStop, const AEDesc* currentRangeToken, AEDesc* result);

/* Error callback */
typedef OSErr (*OSLErrorProcPtr)(OSErr errNum, int32_t data);

/* ========================================================================
 * Object Specifier Structure
 * ======================================================================== */

typedef struct ObjectSpecifier {
    DescType objectClass;
    AEDesc container;
    DescType keyForm;
    AEDesc keyData;
} ObjectSpecifier;

/* Range specifier */
typedef struct RangeSpecifier {
    AEDesc rangeStart;
    AEDesc rangeStop;
} RangeSpecifier;

/* Logical specifier */
typedef struct LogicalSpecifier {
    DescType logicalOperator;
    AEDescList operands;
} LogicalSpecifier;

/* Comparison specifier */
typedef struct ComparisonSpecifier {
    DescType comparisonOperator;
    AEDesc operand1;
    AEDesc operand2;
} ComparisonSpecifier;

/* ========================================================================
 * Object Support Library (OSL) Functions
 * ======================================================================== */

/* OSL initialization */
OSErr OSLInit(void);
void OSLCleanup(void);

/* Object resolution */
OSErr OSLResolve(const AEDesc* objectSpecifier, AEDesc* theToken);
OSErr OSLGetContainerToken(const AEDesc* objectSpecifier, AEDesc* containerToken);
OSErr OSLGetKeyData(const AEDesc* objectSpecifier, AEDesc* keyData);

/* Accessor registration */
OSErr OSLInstallAccessor(DescType desiredClass, DescType containerType, OSLAccessorProcPtr theAccessor, int32_t accessorRefcon, bool isSysHandler);
OSErr OSLRemoveAccessor(DescType desiredClass, DescType containerType, OSLAccessorProcPtr theAccessor, bool isSysHandler);
OSErr OSLGetAccessor(DescType desiredClass, DescType containerType, OSLAccessorProcPtr* theAccessor, int32_t* accessorRefcon, bool isSysHandler);

/* Object counting */
OSErr OSLInstallCountProc(DescType desiredClass, DescType containerType, OSLCountProcPtr theCountProc, bool isSysHandler);
OSErr OSLRemoveCountProc(DescType desiredClass, DescType containerType, OSLCountProcPtr theCountProc, bool isSysHandler);

/* Object comparison */
OSErr OSLInstallCompareProc(DescType comparisonOperator, DescType operandType, OSLCompareProcPtr theCompareProc, bool isSysHandler);
OSErr OSLRemoveCompareProc(DescType comparisonOperator, DescType operandType, OSLCompareProcPtr theCompareProc, bool isSysHandler);

/* ========================================================================
 * Object Specifier Creation and Manipulation
 * ======================================================================== */

/* Object specifier creation */
OSErr CreateObjSpecifier(DescType desiredClass, const AEDesc* theContainer, DescType keyForm, const AEDesc* keyData, bool disposeInputs, AEDesc* objSpecifier);
OSErr CreateRangeSpecifier(const AEDesc* rangeStart, const AEDesc* rangeStop, bool disposeInputs, AEDesc* rangeSpecifier);
OSErr CreateLogicalSpecifier(DescType theLogicalOperator, const AEDescList* theLogicalTerms, bool disposeInputs, AEDesc* logicalSpecifier);
OSErr CreateComparisonSpecifier(DescType theComparisonOperator, const AEDesc* theObject, const AEDesc* theDescOrObject, bool disposeInputs, AEDesc* comparisonSpecifier);

/* Object specifier parsing */
OSErr ParseObjSpecifier(const AEDesc* objSpecifier, ObjectSpecifier* parsedSpec);
OSErr ParseRangeSpecifier(const AEDesc* rangeSpecifier, RangeSpecifier* parsedRange);
OSErr ParseLogicalSpecifier(const AEDesc* logicalSpecifier, LogicalSpecifier* parsedLogical);
OSErr ParseComparisonSpecifier(const AEDesc* comparisonSpecifier, ComparisonSpecifier* parsedComparison);

/* Object specifier validation */
OSErr ValidateObjSpecifier(const AEDesc* objSpecifier);
bool IsValidObjectClass(DescType objectClass);
bool IsValidKeyForm(DescType keyForm);

/* ========================================================================
 * Property and Element Access
 * ======================================================================== */

/* Property access */
OSErr GetObjectProperty(const AEDesc* objectToken, DescType propertyType, AEDesc* propertyValue);
OSErr SetObjectProperty(const AEDesc* objectToken, DescType propertyType, const AEDesc* propertyValue);

/* Element access */
OSErr GetObjectElements(const AEDesc* containerToken, DescType elementClass, AEDescList* elementList);
OSErr CountObjectElements(const AEDesc* containerToken, DescType elementClass, int32_t* count);

/* Element creation and deletion */
OSErr CreateNewElement(const AEDesc* containerToken, DescType elementClass, const AEDesc* properties, AEDesc* newElementToken);
OSErr DeleteElement(const AEDesc* elementToken);

/* ========================================================================
 * Application Object Model Support
 * ======================================================================== */

/* Application object hierarchy */
typedef struct AppObjectInfo {
    DescType objectClass;
    char* className;
    char* objectName;
    DescType* supportedProperties;
    int32_t propertyCount;
    DescType* supportedElements;
    int32_t elementCount;
} AppObjectInfo;

/* Application object registration */
OSErr RegisterApplicationObject(DescType objectClass, const AppObjectInfo* objectInfo);
OSErr UnregisterApplicationObject(DescType objectClass);
OSErr GetApplicationObjectInfo(DescType objectClass, AppObjectInfo** objectInfo);

/* Standard application objects */
OSErr GetApplicationObject(AEDesc* applicationToken);
OSErr GetDocumentList(AEDescList* documentList);
OSErr GetWindowList(AEDescList* windowList);

/* ========================================================================
 * Accessibility Support
 * ======================================================================== */

/* Accessibility object types */
typedef enum {
    kAXApplicationRole = 'axap',
    kAXWindowRole = 'axwn',
    kAXButtonRole = 'axbt',
    kAXTextFieldRole = 'axtf',
    kAXStaticTextRole = 'axst',
    kAXMenuRole = 'axmn',
    kAXMenuItemRole = 'axmi'
} AXRole;

/* Accessibility attributes */
#define kAXRoleAttribute 'axrl'
#define kAXTitleAttribute 'axti'
#define kAXValueAttribute 'axvl'
#define kAXPositionAttribute 'axps'
#define kAXSizeAttribute 'axsz'
#define kAXEnabledAttribute 'axen'
#define kAXFocusedAttribute 'axfc'
#define kAXChildrenAttribute 'axch'
#define kAXParentAttribute 'axpr'

/* Accessibility functions */
OSErr AXGetObjectAttribute(const AEDesc* objectToken, AEKeyword attribute, AEDesc* attributeValue);
OSErr AXSetObjectAttribute(const AEDesc* objectToken, AEKeyword attribute, const AEDesc* attributeValue);
OSErr AXGetObjectAttributeNames(const AEDesc* objectToken, AEDescList* attributeNames);

/* Accessibility actions */
#define kAXPressAction 'axpr'
#define kAXShowMenuAction 'axsm'
#define kAXPickAction 'axpk'

OSErr AXPerformObjectAction(const AEDesc* objectToken, AEKeyword action, const AEDescList* parameters);
OSErr AXGetObjectActionNames(const AEDesc* objectToken, AEDescList* actionNames);

/* ========================================================================
 * Object Model Utilities
 * ======================================================================== */

/* Token management */
OSErr CreateToken(DescType tokenType, const void* tokenData, Size tokenSize, AEDesc* token);
OSErr ExtractTokenData(const AEDesc* token, DescType expectedType, void** tokenData, Size* tokenSize);
OSErr DisposeToken(AEDesc* token);

/* Object hierarchy navigation */
OSErr GetObjectContainer(const AEDesc* objectToken, AEDesc* containerToken);
OSErr GetObjectChildren(const AEDesc* objectToken, DescType childType, AEDescList* childTokens);
OSErr GetObjectSibling(const AEDesc* objectToken, bool next, AEDesc* siblingToken);

/* Object information */
OSErr GetObjectClass(const AEDesc* objectToken, DescType* objectClass);
OSErr GetObjectName(const AEDesc* objectToken, char** objectName);
OSErr GetObjectID(const AEDesc* objectToken, int32_t* objectID);

/* ========================================================================
 * Advanced Object Model Features
 * ======================================================================== */

/* Object model caching */
OSErr EnableObjectCaching(bool enable);
OSErr FlushObjectCache(void);
OSErr CacheObjectToken(const AEDesc* objectSpecifier, const AEDesc* objectToken);

/* Object model validation */
OSErr ValidateObjectModel(void);
OSErr CheckObjectConsistency(const AEDesc* objectToken, bool* isConsistent);

/* Object model debugging */
#ifdef DEBUG
void PrintObjectSpecifier(const AEDesc* objSpecifier, const char* label);
void PrintObjectToken(const AEDesc* token, const char* label);
OSErr DumpObjectHierarchy(const AEDesc* rootObject, int32_t maxDepth);
#endif

/* ========================================================================
 * Object Model Statistics
 * ======================================================================== */

typedef struct OSLStats {
    int32_t accessorCount;
    int32_t resolutionCount;
    int32_t resolutionErrors;
    int32_t cacheHits;
    int32_t cacheMisses;
    int32_t tokensCreated;
    int32_t tokensDisposed;
} OSLStats;

OSErr OSLGetStatistics(OSLStats* stats);
void OSLResetStatistics(void);

#ifdef __cplusplus
}
#endif

#endif /* OBJECT_MODEL_H */