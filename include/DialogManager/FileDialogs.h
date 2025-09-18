/*
 * FileDialogs.h - Standard File Package API
 *
 * This header defines the Standard File Package functionality for
 * file and folder selection dialogs, maintaining Mac System 7.1
 * compatibility while providing modern platform integration.
 */

#ifndef FILE_DIALOGS_H
#define FILE_DIALOGS_H

#include "DialogTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Standard File Package constants */
enum {
    sfGetFile       = 0,    /* Get file dialog */
    sfPutFile       = 1,    /* Put file (save) dialog */
    sfGetFolder     = 2,    /* Get folder dialog */
    sfGetDiskOrVol  = 3     /* Get disk or volume dialog */
};

/* File type constants */
typedef uint32_t OSType;
#define kFileTypeAny    '????'
#define kFileTypeText   'TEXT'
#define kFileTypeAppl   'APPL'

/* Standard File Package structures */

/* File system specification */
typedef struct FSSpec {
    int16_t     vRefNum;        /* Volume reference number */
    int32_t     parID;          /* Parent directory ID */
    Str255      name;           /* File or directory name */
} FSSpec;

/* Standard file reply record */
typedef struct StandardFileReply {
    bool        sfGood;         /* true if user clicked OK */
    bool        sfReplacing;    /* true if replacing existing file */
    OSType      sfType;         /* File type */
    FSSpec      sfFile;         /* File system specification */
    int16_t     sfScript;       /* Script code for name */
    int16_t     sfFlags;        /* Flags */
    bool        sfIsFolder;     /* true if selection is a folder */
    bool        sfIsVolume;     /* true if selection is a volume */
} StandardFileReply;

/* Custom file filter procedure */
typedef bool (*FileFilterProcPtr)(const FSSpec* theFile, void* yourDataPtr);

/* Custom dialog hook procedure */
typedef int16_t (*DlgHookProcPtr)(int16_t item, DialogPtr theDialog, void* yourDataPtr);

/* Standard File Package functions */

/*
 * SFGetFile - Display Get File dialog
 *
 * This function displays the standard Get File dialog, allowing
 * the user to select an existing file.
 *
 * Parameters:
 *   where       - Point where dialog should appear (use (-1,-1) for default)
 *   prompt      - Prompt string (Pascal string, or NULL)
 *   fileFilter  - File filter procedure (or NULL for no filtering)
 *   numTypes    - Number of file types to show (0 for all types)
 *   typeList    - Array of file types to show (or NULL)
 *   dlgHook     - Dialog hook procedure (or NULL)
 *   reply       - Returns user's selection
 */
void SFGetFile(Point where, const unsigned char* prompt, FileFilterProcPtr fileFilter,
               int16_t numTypes, const OSType* typeList, DlgHookProcPtr dlgHook,
               StandardFileReply* reply);

/*
 * SFPutFile - Display Put File dialog
 *
 * This function displays the standard Put File dialog, allowing
 * the user to specify a file name and location for saving.
 *
 * Parameters:
 *   where       - Point where dialog should appear (use (-1,-1) for default)
 *   prompt      - Prompt string (Pascal string, or NULL)
 *   origName    - Default file name (Pascal string)
 *   dlgHook     - Dialog hook procedure (or NULL)
 *   reply       - Returns user's selection
 */
void SFPutFile(Point where, const unsigned char* prompt, const unsigned char* origName,
               DlgHookProcPtr dlgHook, StandardFileReply* reply);

/*
 * StandardGetFile - Enhanced Get File dialog
 *
 * This is an enhanced version of SFGetFile with additional options.
 *
 * Parameters:
 *   fileFilter  - File filter procedure
 *   numTypes    - Number of file types
 *   typeList    - Array of file types
 *   reply       - Returns user's selection
 */
void StandardGetFile(FileFilterProcPtr fileFilter, int16_t numTypes,
                     const OSType* typeList, StandardFileReply* reply);

/*
 * StandardPutFile - Enhanced Put File dialog
 *
 * This is an enhanced version of SFPutFile with additional options.
 *
 * Parameters:
 *   prompt      - Prompt string (Pascal string)
 *   defaultName - Default file name (Pascal string)
 *   reply       - Returns user's selection
 */
void StandardPutFile(const unsigned char* prompt, const unsigned char* defaultName,
                     StandardFileReply* reply);

/*
 * CustomGetFile - Customizable Get File dialog
 *
 * This function provides maximum customization of the Get File dialog.
 *
 * Parameters:
 *   fileFilter  - File filter procedure
 *   numTypes    - Number of file types
 *   typeList    - Array of file types
 *   reply       - Returns user's selection
 *   dlgID       - Custom dialog resource ID
 *   where       - Dialog position
 *   dlgHook     - Dialog hook procedure
 *   modalFilter - Modal filter procedure
 *   activateList - Activate procedure
 *   activateProc - Activate procedure data
 *   yourDataPtr - Your data pointer
 */
void CustomGetFile(FileFilterProcPtr fileFilter, int16_t numTypes,
                   const OSType* typeList, StandardFileReply* reply,
                   int16_t dlgID, Point where, DlgHookProcPtr dlgHook,
                   ModalFilterProcPtr modalFilter, void* activateList,
                   void* activateProc, void* yourDataPtr);

/*
 * CustomPutFile - Customizable Put File dialog
 *
 * This function provides maximum customization of the Put File dialog.
 *
 * Parameters:
 *   prompt      - Prompt string
 *   defaultName - Default file name
 *   reply       - Returns user's selection
 *   dlgID       - Custom dialog resource ID
 *   where       - Dialog position
 *   dlgHook     - Dialog hook procedure
 *   modalFilter - Modal filter procedure
 *   activateList - Activate procedure
 *   activateProc - Activate procedure data
 *   yourDataPtr - Your data pointer
 */
void CustomPutFile(const unsigned char* prompt, const unsigned char* defaultName,
                   StandardFileReply* reply, int16_t dlgID, Point where,
                   DlgHookProcPtr dlgHook, ModalFilterProcPtr modalFilter,
                   void* activateList, void* activateProc, void* yourDataPtr);

/* Modern file dialog extensions */

/*
 * ShowOpenFileDialog - Modern open file dialog
 *
 * This function provides a modern interface for opening files,
 * using native platform dialogs when available.
 *
 * Parameters:
 *   title       - Dialog title (C string)
 *   defaultPath - Default directory path (C string, or NULL)
 *   fileTypes   - Comma-separated list of file extensions (C string, or NULL)
 *   multiSelect - Allow multiple file selection
 *   selectedPath - Buffer for selected file path
 *   pathSize    - Size of selectedPath buffer
 *
 * Returns:
 *   true if user selected a file, false if cancelled
 */
bool ShowOpenFileDialog(const char* title, const char* defaultPath,
                       const char* fileTypes, bool multiSelect,
                       char* selectedPath, size_t pathSize);

/*
 * ShowSaveFileDialog - Modern save file dialog
 *
 * Parameters:
 *   title       - Dialog title (C string)
 *   defaultPath - Default directory path (C string, or NULL)
 *   defaultName - Default file name (C string, or NULL)
 *   fileTypes   - Comma-separated list of file extensions (C string, or NULL)
 *   selectedPath - Buffer for selected file path
 *   pathSize    - Size of selectedPath buffer
 *
 * Returns:
 *   true if user specified a file, false if cancelled
 */
bool ShowSaveFileDialog(const char* title, const char* defaultPath,
                       const char* defaultName, const char* fileTypes,
                       char* selectedPath, size_t pathSize);

/*
 * ShowFolderDialog - Modern folder selection dialog
 *
 * Parameters:
 *   title       - Dialog title (C string)
 *   defaultPath - Default directory path (C string, or NULL)
 *   selectedPath - Buffer for selected folder path
 *   pathSize    - Size of selectedPath buffer
 *
 * Returns:
 *   true if user selected a folder, false if cancelled
 */
bool ShowFolderDialog(const char* title, const char* defaultPath,
                     char* selectedPath, size_t pathSize);

/* Multiple file selection */

/*
 * ShowMultipleFileDialog - Select multiple files
 *
 * This function allows selection of multiple files at once.
 *
 * Parameters:
 *   title       - Dialog title (C string)
 *   defaultPath - Default directory path (C string, or NULL)
 *   fileTypes   - File type filter (C string, or NULL)
 *   maxFiles    - Maximum number of files to select
 *   selectedFiles - Array of buffers for selected file paths
 *   pathSize    - Size of each path buffer
 *   fileCount   - Returns the number of files selected
 *
 * Returns:
 *   true if user selected files, false if cancelled
 */
bool ShowMultipleFileDialog(const char* title, const char* defaultPath,
                           const char* fileTypes, int16_t maxFiles,
                           char selectedFiles[][256], size_t pathSize,
                           int16_t* fileCount);

/* File dialog configuration */

/*
 * SetFileDialogPosition - Set default position for file dialogs
 *
 * Parameters:
 *   position - Position specification (0 = center screen, 1 = remember last)
 */
void SetFileDialogPosition(int16_t position);

/*
 * SetFileDialogDefaultPath - Set default path for file dialogs
 *
 * Parameters:
 *   path - Default path (C string)
 */
void SetFileDialogDefaultPath(const char* path);

/*
 * GetFileDialogDefaultPath - Get default path for file dialogs
 *
 * Parameters:
 *   path     - Buffer for default path
 *   pathSize - Size of path buffer
 */
void GetFileDialogDefaultPath(char* path, size_t pathSize);

/* Platform integration */

/*
 * SetUseNativeFileDialogs - Enable/disable native file dialogs
 *
 * Parameters:
 *   useNative - true to use native platform file dialogs when available
 */
void SetUseNativeFileDialogs(bool useNative);

/*
 * GetUseNativeFileDialogs - Check if native file dialogs are enabled
 *
 * Returns:
 *   true if native file dialogs are enabled
 */
bool GetUseNativeFileDialogs(void);

/* File preview support */

/*
 * SetFilePreviewEnabled - Enable/disable file preview
 *
 * Parameters:
 *   enabled - true to enable file preview in dialogs
 */
void SetFilePreviewEnabled(bool enabled);

/*
 * GetFilePreviewEnabled - Check if file preview is enabled
 *
 * Returns:
 *   true if file preview is enabled
 */
bool GetFilePreviewEnabled(void);

/* File type management */

/*
 * RegisterFileType - Register a file type with description
 *
 * Parameters:
 *   fileType    - File type code
 *   extension   - File extension (C string)
 *   description - File type description (C string)
 */
void RegisterFileType(OSType fileType, const char* extension, const char* description);

/*
 * GetFileTypeDescription - Get description for file type
 *
 * Parameters:
 *   fileType    - File type code
 *   description - Buffer for description
 *   descSize    - Size of description buffer
 *
 * Returns:
 *   true if description was found
 */
bool GetFileTypeDescription(OSType fileType, char* description, size_t descSize);

/* File dialog accessibility */

/*
 * SetFileDialogAccessibility - Enable accessibility for file dialogs
 *
 * Parameters:
 *   enabled - true to enable accessibility features
 */
void SetFileDialogAccessibility(bool enabled);

/* Utility functions */

/*
 * FSSpecToPath - Convert FSSpec to path string
 *
 * Parameters:
 *   spec     - File system specification
 *   path     - Buffer for path string
 *   pathSize - Size of path buffer
 *
 * Returns:
 *   true if conversion was successful
 */
bool FSSpecToPath(const FSSpec* spec, char* path, size_t pathSize);

/*
 * PathToFSSpec - Convert path string to FSSpec
 *
 * Parameters:
 *   path - Path string (C string)
 *   spec - Buffer for file system specification
 *
 * Returns:
 *   true if conversion was successful
 */
bool PathToFSSpec(const char* path, FSSpec* spec);

/*
 * ValidateFileSelection - Validate user's file selection
 *
 * Parameters:
 *   reply - Standard file reply to validate
 *
 * Returns:
 *   true if selection is valid
 */
bool ValidateFileSelection(const StandardFileReply* reply);

/* Internal file dialog functions */
void InitFileDialogs(void);
void CleanupFileDialogs(void);
OSErr CreateStandardFileDialog(int16_t dialogType, DialogPtr* dialog);
bool ProcessFileDialogEvent(DialogPtr dialog, const EventRecord* event);
void UpdateFileList(DialogPtr dialog);
void UpdateFilePreview(DialogPtr dialog, const FSSpec* selectedFile);

#ifdef __cplusplus
}
#endif

#endif /* FILE_DIALOGS_H */