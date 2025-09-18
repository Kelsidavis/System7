/*
 * DialogResources.h - Dialog Resource Management API
 *
 * This header defines the resource management functionality for
 * dialog templates (DLOG), dialog item lists (DITL), and alert
 * templates (ALRT), maintaining Mac System 7.1 compatibility.
 */

#ifndef DIALOG_RESOURCES_H
#define DIALOG_RESOURCES_H

#include "DialogTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Resource type constants */
#define kDialogResourceType     'DLOG'  /* Dialog template resource */
#define kDialogItemResourceType 'DITL'  /* Dialog item list resource */
#define kAlertResourceType      'ALRT'  /* Alert template resource */
#define kIconResourceType       'ICON'  /* Icon resource */
#define kPictureResourceType    'PICT'  /* Picture resource */
#define kStringResourceType     'STR '  /* String resource */
#define kStringListResourceType 'STR#'  /* String list resource */

/* Resource loading functions */

/*
 * LoadDialogTemplate - Load DLOG resource
 *
 * This function loads a dialog template from a DLOG resource,
 * parsing the resource data into a DialogTemplate structure.
 *
 * Parameters:
 *   dialogID - Resource ID of the DLOG resource
 *   template - Returns pointer to loaded template (caller must dispose)
 *
 * Returns:
 *   Error code or noErr
 */
OSErr LoadDialogTemplate(int16_t dialogID, DialogTemplate** template);

/*
 * LoadDialogItemList - Load DITL resource
 *
 * This function loads a dialog item list from a DITL resource,
 * parsing the resource data into a usable item list structure.
 *
 * Parameters:
 *   itemListID - Resource ID of the DITL resource
 *   itemList   - Returns handle to loaded item list
 *
 * Returns:
 *   Error code or noErr
 */
OSErr LoadDialogItemList(int16_t itemListID, Handle* itemList);

/*
 * LoadAlertTemplate - Load ALRT resource
 *
 * This function loads an alert template from an ALRT resource.
 *
 * Parameters:
 *   alertID  - Resource ID of the ALRT resource
 *   template - Returns pointer to loaded template (caller must dispose)
 *
 * Returns:
 *   Error code or noErr
 */
OSErr LoadAlertTemplate(int16_t alertID, AlertTemplate** template);

/* Resource creation functions */

/*
 * CreateDialogTemplate - Create dialog template structure
 *
 * This function creates a dialog template structure that can be
 * used to create dialogs without resource files.
 *
 * Parameters:
 *   bounds    - Dialog bounds rectangle
 *   procID    - Window procedure ID
 *   visible   - Initial visibility
 *   goAway    - Has close box
 *   refCon    - Reference constant
 *   title     - Dialog title (Pascal string)
 *   itemsID   - Dialog item list resource ID
 *
 * Returns:
 *   Pointer to created template, or NULL on failure
 */
DialogTemplate* CreateDialogTemplate(const Rect* bounds, int16_t procID,
                                     bool visible, bool goAway, int32_t refCon,
                                     const unsigned char* title, int16_t itemsID);

/*
 * CreateAlertTemplate - Create alert template structure
 *
 * Parameters:
 *   bounds   - Alert bounds rectangle
 *   itemsID  - Dialog item list resource ID
 *   stages   - Alert stage configuration
 *
 * Returns:
 *   Pointer to created template, or NULL on failure
 */
AlertTemplate* CreateAlertTemplate(const Rect* bounds, int16_t itemsID,
                                  StageList stages);

/* Dialog item list creation */

/*
 * CreateDialogItemList - Create dialog item list
 *
 * This function creates a dialog item list structure that can be
 * used to create dialogs without DITL resources.
 *
 * Parameters:
 *   itemCount - Number of items in the list
 *
 * Returns:
 *   Handle to created item list, or NULL on failure
 */
Handle CreateDialogItemList(int16_t itemCount);

/*
 * AddDialogItem - Add item to dialog item list
 *
 * Parameters:
 *   itemList - Handle to dialog item list
 *   itemNo   - Item number (1-based)
 *   itemType - Type of item
 *   bounds   - Item bounds rectangle
 *   title    - Item title/text (Pascal string, or NULL)
 *
 * Returns:
 *   Error code or noErr
 */
OSErr AddDialogItem(Handle itemList, int16_t itemNo, int16_t itemType,
                   const Rect* bounds, const unsigned char* title);

/*
 * AddButtonItem - Add button to dialog item list
 *
 * Parameters:
 *   itemList - Handle to dialog item list
 *   itemNo   - Item number (1-based)
 *   bounds   - Button bounds rectangle
 *   title    - Button title (Pascal string)
 *   isDefault - true if this is the default button
 *
 * Returns:
 *   Error code or noErr
 */
OSErr AddButtonItem(Handle itemList, int16_t itemNo, const Rect* bounds,
                   const unsigned char* title, bool isDefault);

/*
 * AddTextItem - Add text item to dialog item list
 *
 * Parameters:
 *   itemList  - Handle to dialog item list
 *   itemNo    - Item number (1-based)
 *   bounds    - Text bounds rectangle
 *   text      - Text content (Pascal string)
 *   isEditable - true for editable text, false for static text
 *
 * Returns:
 *   Error code or noErr
 */
OSErr AddTextItem(Handle itemList, int16_t itemNo, const Rect* bounds,
                 const unsigned char* text, bool isEditable);

/*
 * AddIconItem - Add icon item to dialog item list
 *
 * Parameters:
 *   itemList - Handle to dialog item list
 *   itemNo   - Item number (1-based)
 *   bounds   - Icon bounds rectangle
 *   iconID   - Icon resource ID
 *
 * Returns:
 *   Error code or noErr
 */
OSErr AddIconItem(Handle itemList, int16_t itemNo, const Rect* bounds,
                 int16_t iconID);

/*
 * AddUserItem - Add user item to dialog item list
 *
 * Parameters:
 *   itemList - Handle to dialog item list
 *   itemNo   - Item number (1-based)
 *   bounds   - Item bounds rectangle
 *   userProc - User item procedure
 *
 * Returns:
 *   Error code or noErr
 */
OSErr AddUserItem(Handle itemList, int16_t itemNo, const Rect* bounds,
                 UserItemProcPtr userProc);

/* Resource disposal functions */

/*
 * DisposeDialogTemplate - Dispose dialog template
 *
 * Parameters:
 *   template - Dialog template to dispose
 */
void DisposeDialogTemplate(DialogTemplate* template);

/*
 * DisposeAlertTemplate - Dispose alert template
 *
 * Parameters:
 *   template - Alert template to dispose
 */
void DisposeAlertTemplate(AlertTemplate* template);

/*
 * DisposeDialogItemList - Dispose dialog item list
 *
 * Parameters:
 *   itemList - Dialog item list handle to dispose
 */
void DisposeDialogItemList(Handle itemList);

/* Resource validation functions */

/*
 * ValidateDialogTemplate - Validate dialog template
 *
 * This function checks if a dialog template is properly formatted
 * and contains valid data.
 *
 * Parameters:
 *   template - Dialog template to validate
 *
 * Returns:
 *   true if template is valid
 */
bool ValidateDialogTemplate(const DialogTemplate* template);

/*
 * ValidateDialogItemList - Validate dialog item list
 *
 * Parameters:
 *   itemList - Dialog item list to validate
 *
 * Returns:
 *   true if item list is valid
 */
bool ValidateDialogItemList(Handle itemList);

/*
 * ValidateAlertTemplate - Validate alert template
 *
 * Parameters:
 *   template - Alert template to validate
 *
 * Returns:
 *   true if template is valid
 */
bool ValidateAlertTemplate(const AlertTemplate* template);

/* Resource conversion functions */

/*
 * ConvertDialogTemplateToNative - Convert template to native format
 *
 * This function converts a Mac dialog template to a platform-native
 * dialog template when supported.
 *
 * Parameters:
 *   template     - Mac dialog template
 *   nativeTemplate - Returns platform-native template
 *
 * Returns:
 *   true if conversion was successful
 */
bool ConvertDialogTemplateToNative(const DialogTemplate* template,
                                  void** nativeTemplate);

/*
 * ConvertNativeToDialogTemplate - Convert native template to Mac format
 *
 * Parameters:
 *   nativeTemplate - Platform-native dialog template
 *   template       - Returns Mac dialog template
 *
 * Returns:
 *   true if conversion was successful
 */
bool ConvertNativeToDialogTemplate(const void* nativeTemplate,
                                  DialogTemplate** template);

/* Resource caching */

/*
 * SetDialogResourceCaching - Enable/disable resource caching
 *
 * Parameters:
 *   enabled - true to enable caching of dialog resources
 */
void SetDialogResourceCaching(bool enabled);

/*
 * FlushDialogResourceCache - Flush cached dialog resources
 */
void FlushDialogResourceCache(void);

/*
 * PreloadDialogResources - Preload dialog resources
 *
 * This function preloads dialog resources into the cache for
 * faster access during runtime.
 *
 * Parameters:
 *   resourceIDs - Array of resource IDs to preload
 *   count       - Number of resource IDs
 */
void PreloadDialogResources(const int16_t* resourceIDs, int16_t count);

/* Resource localization */

/*
 * SetDialogResourceLanguage - Set language for resource loading
 *
 * Parameters:
 *   languageCode - Language code (e.g., "en", "fr", "de")
 */
void SetDialogResourceLanguage(const char* languageCode);

/*
 * GetDialogResourceLanguage - Get current resource language
 *
 * Parameters:
 *   languageCode - Buffer for language code
 *   codeSize     - Size of languageCode buffer
 */
void GetDialogResourceLanguage(char* languageCode, size_t codeSize);

/*
 * LoadLocalizedDialogTemplate - Load localized dialog template
 *
 * This function loads a dialog template, automatically selecting
 * the appropriate localized version based on the current language.
 *
 * Parameters:
 *   dialogID - Base resource ID
 *   template - Returns pointer to loaded template
 *
 * Returns:
 *   Error code or noErr
 */
OSErr LoadLocalizedDialogTemplate(int16_t dialogID, DialogTemplate** template);

/* Modern resource loading */

/*
 * LoadDialogFromJSON - Load dialog from JSON description
 *
 * This function loads a dialog template from a JSON description,
 * providing a modern alternative to resource files.
 *
 * Parameters:
 *   jsonData - JSON description of dialog
 *   template - Returns pointer to created template
 *
 * Returns:
 *   Error code or noErr
 */
OSErr LoadDialogFromJSON(const char* jsonData, DialogTemplate** template);

/*
 * SaveDialogToJSON - Save dialog template as JSON
 *
 * Parameters:
 *   template - Dialog template to save
 *   jsonData - Buffer for JSON data
 *   dataSize - Size of jsonData buffer
 *
 * Returns:
 *   Error code or noErr
 */
OSErr SaveDialogToJSON(const DialogTemplate* template, char* jsonData,
                      size_t dataSize);

/*
 * LoadDialogFromXML - Load dialog from XML description
 *
 * Parameters:
 *   xmlData  - XML description of dialog
 *   template - Returns pointer to created template
 *
 * Returns:
 *   Error code or noErr
 */
OSErr LoadDialogFromXML(const char* xmlData, DialogTemplate** template);

/* Resource file management */

/*
 * SetDialogResourceFile - Set resource file for dialog loading
 *
 * Parameters:
 *   refNum - Resource file reference number
 */
void SetDialogResourceFile(int16_t refNum);

/*
 * GetDialogResourceFile - Get current dialog resource file
 *
 * Returns:
 *   Resource file reference number
 */
int16_t GetDialogResourceFile(void);

/*
 * SearchDialogResourcePath - Set search path for dialog resources
 *
 * Parameters:
 *   path - Search path for dialog resource files
 */
void SetDialogResourcePath(const char* path);

/* Resource debugging */

/*
 * DumpDialogTemplate - Dump dialog template to debug output
 *
 * This function prints the contents of a dialog template to
 * the debug output for troubleshooting.
 *
 * Parameters:
 *   template - Dialog template to dump
 */
void DumpDialogTemplate(const DialogTemplate* template);

/*
 * DumpDialogItemList - Dump dialog item list to debug output
 *
 * Parameters:
 *   itemList - Dialog item list to dump
 */
void DumpDialogItemList(Handle itemList);

/* Internal resource functions */
void InitDialogResources(void);
void CleanupDialogResources(void);
OSErr ParseDLOGResource(Handle resourceData, DialogTemplate** template);
OSErr ParseDITLResource(Handle resourceData, Handle* itemList);
OSErr ParseALRTResource(Handle resourceData, AlertTemplate** template);
void* GetDialogResourceData(int16_t resourceID, OSType resourceType, size_t* dataSize);

#ifdef __cplusplus
}
#endif

#endif /* DIALOG_RESOURCES_H */