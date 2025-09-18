/*
 * StandardFilePackage.h
 * System 7.1 Portable Standard File Package Implementation
 *
 * Implements Mac OS Standard File Package (Pack 3) for file dialog functionality.
 * Critical for applications that need to open/save files through standard dialogs.
 * Provides platform-independent file selection interface.
 */

#ifndef __STANDARD_FILE_PACKAGE_H__
#define __STANDARD_FILE_PACKAGE_H__

#include "PackageTypes.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Standard File selector constants */
#define sfSelPutFile        1
#define sfSelGetFile        2
#define sfSelPPutFile       3
#define sfSelPGetFile       4
#define sfSelStandardPut    5
#define sfSelStandardGet    6
#define sfSelCustomPut      7
#define sfSelCustomGet      8

/* Dialog ID constants */
#define putDlgID            -3999
#define getDlgID            -4000
#define sfPutDialogID       -6043
#define sfGetDialogID       -6042

/* Dialog item constants */
#define putSave             1
#define putCancel           2
#define putEject            5
#define putDrive            6
#define putName             7

#define getOpen             1
#define getCancel           3
#define getEject            5
#define getDrive            6
#define getNmList           7
#define getScroll           8

/* System 7.0+ dialog items */
#define sfItemOpenButton        1
#define sfItemCancelButton      2
#define sfItemBalloonHelp       3
#define sfItemVolumeUser        4
#define sfItemEjectButton       5
#define sfItemDesktopButton     6
#define sfItemFileListUser      7
#define sfItemPopUpMenuUser     8
#define sfItemDividerLinePict   9
#define sfItemFileNameTextEdit  10
#define sfItemPromptStaticText  11
#define sfItemNewFolderUser     12

/* Hook event constants */
#define sfHookFirstCall         -1
#define sfHookCharOffset        0x1000
#define sfHookNullEvent         100
#define sfHookRebuildList       101
#define sfHookFolderPopUp       102
#define sfHookOpenFolder        103
#define sfHookOpenAlias         104
#define sfHookGoToDesktop       105
#define sfHookGoToAliasTarget   106
#define sfHookGoToParent        107
#define sfHookGoToNextDrive     108
#define sfHookGoToPrevDrive     109
#define sfHookChangeSelection   110
#define sfHookSetActiveOffset   200
#define sfHookLastCall          -2

/* Dialog reference constants */
#define sfMainDialogRefCon      'stdf'
#define sfNewFolderDialogRefCon 'nfdr'
#define sfReplaceDialogRefCon   'rplc'
#define sfStatWarnDialogRefCon  'stat'
#define sfLockWarnDialogRefCon  'lock'
#define sfErrorDialogRefCon     'err '

/* File types */
typedef OSType SFTypeList[4];

/* Standard File reply structures */
typedef struct {
    Boolean good;           /* true if user selected a file */
    Boolean copy;           /* true if user wants to make a copy */
    OSType  fType;          /* file type */
    int16_t vRefNum;        /* volume reference number */
    int16_t version;        /* file version */
    Str63   fName;          /* file name */
} SFReply;

typedef struct {
    Boolean     sfGood;         /* true if user selected a file */
    Boolean     sfReplacing;    /* true if replacing existing file */
    OSType      sfType;         /* file type */
    FSSpec      sfFile;         /* file specification */
    ScriptCode  sfScript;       /* script code for file name */
    int16_t     sfFlags;        /* flags */
    Boolean     sfIsFolder;     /* true if selection is a folder */
    Boolean     sfIsVolume;     /* true if selection is a volume */
    int32_t     sfReserved1;    /* reserved */
    int16_t     sfReserved2;    /* reserved */
} StandardFileReply;

/* Filter and hook procedure types */
typedef int16_t (*DlgHookProcPtr)(int16_t item, DialogPtr theDialog);
typedef Boolean (*FileFilterProcPtr)(void *paramBlock);
typedef Boolean (*ModalFilterProcPtr)(DialogPtr theDialog, EventRecord *theEvent, int16_t *itemHit);

/* Enhanced procedure types with user data */
typedef int16_t (*DlgHookYDProcPtr)(int16_t item, DialogPtr theDialog, void *yourDataPtr);
typedef Boolean (*ModalFilterYDProcPtr)(DialogPtr theDialog, EventRecord *theEvent, int16_t *itemHit, void *yourDataPtr);
typedef Boolean (*FileFilterYDProcPtr)(void *paramBlock, void *yourDataPtr);
typedef void (*ActivateYDProcPtr)(DialogPtr theDialog, int16_t itemNo, Boolean activating, void *yourDataPtr);

/* File dialog configuration */
typedef struct {
    Point               where;          /* Dialog position */
    ConstStr255Param    prompt;         /* Dialog prompt */
    ConstStr255Param    defaultName;    /* Default file name */
    DlgHookProcPtr      dlgHook;        /* Dialog hook procedure */
    ModalFilterProcPtr  filterProc;     /* Modal filter procedure */
    FileFilterProcPtr   fileFilter;    /* File filter procedure */
    int16_t             numTypes;       /* Number of file types */
    SFTypeList          typeList;       /* File type list */
    int16_t             dlgID;          /* Custom dialog ID */
    void               *userData;       /* User data pointer */
} SFDialogConfig;

/* Standard File Package API Functions */

/* Package initialization and management */
int32_t InitStandardFilePackage(void);
void CleanupStandardFilePackage(void);
int32_t StandardFileDispatch(int16_t selector, void *params);

/* Classic Standard File functions */
void SFPutFile(Point where, ConstStr255Param prompt, ConstStr255Param origName,
               DlgHookProcPtr dlgHook, SFReply *reply);

void SFGetFile(Point where, ConstStr255Param prompt, FileFilterProcPtr fileFilter,
               int16_t numTypes, SFTypeList typeList, DlgHookProcPtr dlgHook, SFReply *reply);

void SFPPutFile(Point where, ConstStr255Param prompt, ConstStr255Param origName,
                DlgHookProcPtr dlgHook, SFReply *reply, int16_t dlgID, ModalFilterProcPtr filterProc);

void SFPGetFile(Point where, ConstStr255Param prompt, FileFilterProcPtr fileFilter,
                int16_t numTypes, SFTypeList typeList, DlgHookProcPtr dlgHook, SFReply *reply,
                int16_t dlgID, ModalFilterProcPtr filterProc);

/* System 7.0+ Standard File functions */
void StandardPutFile(ConstStr255Param prompt, ConstStr255Param defaultName, StandardFileReply *reply);

void StandardGetFile(FileFilterProcPtr fileFilter, int16_t numTypes, SFTypeList typeList,
                     StandardFileReply *reply);

void CustomPutFile(ConstStr255Param prompt, ConstStr255Param defaultName, StandardFileReply *reply,
                   int16_t dlgID, Point where, DlgHookYDProcPtr dlgHook,
                   ModalFilterYDProcPtr filterProc, int16_t *activeList,
                   ActivateYDProcPtr activateProc, void *yourDataPtr);

void CustomGetFile(FileFilterYDProcPtr fileFilter, int16_t numTypes, SFTypeList typeList,
                   StandardFileReply *reply, int16_t dlgID, Point where, DlgHookYDProcPtr dlgHook,
                   ModalFilterYDProcPtr filterProc, int16_t *activeList,
                   ActivateYDProcPtr activateProc, void *yourDataPtr);

/* C-style interface functions */
void sfputfile(Point *where, char *prompt, char *origName, DlgHookProcPtr dlgHook, SFReply *reply);
void sfgetfile(Point *where, char *prompt, FileFilterProcPtr fileFilter, int16_t numTypes,
               SFTypeList typeList, DlgHookProcPtr dlgHook, SFReply *reply);
void sfpputfile(Point *where, char *prompt, char *origName, DlgHookProcPtr dlgHook, SFReply *reply,
                int16_t dlgID, ModalFilterProcPtr filterProc);
void sfpgetfile(Point *where, char *prompt, FileFilterProcPtr fileFilter, int16_t numTypes,
                SFTypeList typeList, DlgHookProcPtr dlgHook, SFReply *reply,
                int16_t dlgID, ModalFilterProcPtr filterProc);

/* Platform-independent file dialog interface */
typedef struct {
    Boolean (*ShowOpenDialog)(const char *title, const char *defaultPath, const char *filter,
                              char *selectedPath, int maxPathLen);
    Boolean (*ShowSaveDialog)(const char *title, const char *defaultPath, const char *defaultName,
                              const char *filter, char *selectedPath, int maxPathLen);
    Boolean (*ShowFolderDialog)(const char *title, const char *defaultPath,
                               char *selectedPath, int maxPathLen);
    void (*SetDialogParent)(void *parentWindow);
    void (*SetDialogOptions)(int options);
} PlatformFileDialogs;

/* File system navigation */
typedef struct {
    FSSpec      spec;           /* File specification */
    OSType      fileType;       /* File type */
    OSType      creator;        /* File creator */
    int32_t     dataSize;       /* Data fork size */
    int32_t     rsrcSize;       /* Resource fork size */
    uint32_t    modDate;        /* Modification date */
    uint32_t    crDate;         /* Creation date */
    Boolean     isFolder;       /* true if folder */
    Boolean     isAlias;        /* true if alias */
    Boolean     isVisible;      /* true if visible */
    Boolean     isLocked;       /* true if locked */
    Str255      displayName;    /* Display name */
} FileInfo;

typedef struct {
    FSSpec      currentDir;     /* Current directory */
    int16_t     fileCount;      /* Number of files */
    FileInfo    *files;         /* File list */
    int16_t     selectedIndex;  /* Selected file index */
    Boolean     showInvisible;  /* Show invisible files */
    SFTypeList  filterTypes;    /* File type filter */
    int16_t     numFilterTypes; /* Number of filter types */
} DirectoryListing;

/* Directory navigation functions */
OSErr SF_GetDirectoryListing(const FSSpec *directory, DirectoryListing *listing);
void SF_FreeDirectoryListing(DirectoryListing *listing);
OSErr SF_ChangeDirectory(FSSpec *currentDir, const char *newPath);
OSErr SF_GetParentDirectory(const FSSpec *currentDir, FSSpec *parentDir);
Boolean SF_IsValidFilename(const char *filename);
OSErr SF_CreateNewFolder(const FSSpec *parentDir, const char *folderName, FSSpec *newFolder);

/* File type and filter management */
Boolean SF_FileMatchesFilter(const FileInfo *fileInfo, const SFTypeList typeList, int16_t numTypes);
void SF_BuildTypeFilter(const SFTypeList typeList, int16_t numTypes, char *filterString, int maxLen);
OSErr SF_GetFileInfo(const FSSpec *fileSpec, FileInfo *fileInfo);
OSErr SF_SetFileInfo(const FSSpec *fileSpec, const FileInfo *fileInfo);

/* Dialog management */
typedef struct {
    DialogPtr       dialog;         /* Dialog pointer */
    WindowPtr       window;         /* Window pointer */
    ControlHandle   fileList;       /* File list control */
    ControlHandle   volumePopup;    /* Volume popup */
    ControlHandle   okButton;       /* OK button */
    ControlHandle   cancelButton;   /* Cancel button */
    ControlHandle   ejectButton;    /* Eject button */
    ControlHandle   desktopButton;  /* Desktop button */
    ControlHandle   newFolderButton; /* New folder button */
    DirectoryListing listing;       /* Current directory listing */
    SFDialogConfig  config;         /* Dialog configuration */
    Boolean         isOpen;         /* true for open dialog */
    void           *userData;       /* User data */
} SFDialogState;

SFDialogState *SF_CreateDialog(const SFDialogConfig *config, Boolean isOpen);
void SF_DestroyDialog(SFDialogState *dialogState);
Boolean SF_RunDialog(SFDialogState *dialogState, StandardFileReply *reply);
void SF_UpdateFileList(SFDialogState *dialogState);
void SF_HandleDialogEvent(SFDialogState *dialogState, const EventRecord *event);

/* Volume and drive management */
typedef struct {
    int16_t     vRefNum;        /* Volume reference number */
    Str255      name;           /* Volume name */
    int32_t     totalBytes;     /* Total bytes */
    int32_t     freeBytes;      /* Free bytes */
    Boolean     isEjectable;    /* true if ejectable */
    Boolean     isNetwork;      /* true if network volume */
    Boolean     isReadOnly;     /* true if read-only */
} VolumeInfo;

int16_t SF_GetVolumeList(VolumeInfo *volumes, int16_t maxVolumes);
OSErr SF_EjectVolume(int16_t vRefNum);
OSErr SF_MountVolume(void);
void SF_GoToDesktop(FSSpec *currentDir);
OSErr SF_ResolveAlias(const FSSpec *aliasFile, FSSpec *target);

/* Configuration and preferences */
void SF_SetDefaultDirectory(const FSSpec *directory);
void SF_GetDefaultDirectory(FSSpec *directory);
void SF_SetFileDialogPreferences(int32_t preferences);
int32_t SF_GetFileDialogPreferences(void);
void SF_SetPlatformFileDialogs(const PlatformFileDialogs *dialogs);

/* Error handling */
const char *SF_GetErrorString(OSErr error);
void SF_SetErrorHandler(void (*handler)(OSErr error, const char *message));

/* Utility functions */
void SF_FSSpecToPath(const FSSpec *spec, char *path, int maxLen);
OSErr SF_PathToFSSpec(const char *path, FSSpec *spec);
Boolean SF_FSSpecEqual(const FSSpec *spec1, const FSSpec *spec2);
OSErr SF_GetFullPath(const FSSpec *spec, char *fullPath, int maxLen);

/* Thread safety */
void SF_LockFileDialogs(void);
void SF_UnlockFileDialogs(void);

/* Memory management */
void *SF_AllocMem(size_t size);
void SF_FreeMem(void *ptr);
void *SF_ReallocMem(void *ptr, size_t newSize);

#ifdef __cplusplus
}
#endif

#endif /* __STANDARD_FILE_PACKAGE_H__ */