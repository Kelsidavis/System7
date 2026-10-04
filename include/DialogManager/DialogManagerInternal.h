#ifndef DIALOGMANAGERINTERNAL_H
#define DIALOGMANAGERINTERNAL_H

#include "../SystemTypes.h"
#include "DialogManager.h"

// Internal dialog structures
typedef struct DialogItemEx {
    Handle handle;
    Rect bounds;
    short type;
    short index;
    void* data;
    Boolean visible;
    Boolean enabled;
    SInt32 refCon;
    void* userData;
    void* platformData;
} DialogItemEx;

/* Global dialog manager parameters */
typedef struct DialogGlobals {
    ResumeProcPtr resumeProc;
    void* soundProc;
    SInt16 alertStage;
    SInt16 dialogFont;
    SInt16 spareFlags;
    DialogPtr frontModal;
    SInt16 defaultItem;
    SInt16 cancelItem;
    Boolean tracksCursor;
    unsigned char paramText[4][256];
    SInt16 alertStageCount;
    SInt16 alertButtonSpacing;
    SInt16 defaultButtonOutlineWidth;
    SInt16 buttonFrameInset;
    void* dialogTextProc;
    void* alertSoundProc;
    ModalFilterProcPtr modalFilterProc;
} DialogGlobals;

/* Extended DialogManagerState with all fields needed by DialogManagerCore.c */
typedef struct DialogManagerState {
    /* Basic fields */
    DialogPtr currentDialog;
    short modalDepth;
    Boolean inProgress;
    Handle itemList;
    short itemCount;

    /* Extended fields */
    DialogGlobals globals;
    Boolean initialized;
    SInt16 modalLevel;
    Boolean systemModal;
    Boolean useNativeDialogs;
    Boolean useAccessibility;
    float scaleFactor;
    void* platformContext;
    DialogPtr modalStack[16];

} DialogManagerState;

#endif
