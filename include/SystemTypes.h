#ifndef SYSTEM_TYPES_H
#define SYSTEM_TYPES_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
/* #include <stdio.h> - not included for bare metal */

/* Pascal calling convention keyword - not used in x86 reimplementation */
#define pascal

// Stub FILE type for bare metal compatibility
// Only define if stdio.h hasn't been included
#if !defined(_STDIO_H) && !defined(_FILE_DEFINED)
#define _FILE_DEFINED
typedef struct FILE FILE;
#endif

// Base types
typedef uint8_t  UInt8;
typedef uint16_t UInt16;
typedef uint32_t UInt32;

// Bits16 type for cursor data
typedef UInt16 Bits16[16];
typedef uint64_t UInt64;
typedef int8_t   SInt8;
typedef int16_t  SInt16;
typedef int32_t  SInt32;
typedef int64_t  SInt64;

typedef SInt16   OSErr;
typedef SInt32   OSStatus;
typedef UInt8    Boolean;
typedef UInt8    Style;
typedef char*    Ptr;
typedef Ptr*     Handle;
typedef SInt32   Size;
typedef UInt32   FourCharCode;
typedef FourCharCode OSType;
typedef UInt32   ResType;  /* 4-character resource type like 'PAT ' or 'ppat' */
typedef SInt16   ResID;

#define true  1
#define false 0
#define nil   NULL
#define noErr 0

// String types - MUST come before FSSpec
typedef unsigned char Str255[256];
typedef unsigned char Str63[64];
typedef unsigned char Str32[33];
typedef unsigned char Str31[32];
typedef Str63 StrFileName;

// Forward declarations
typedef struct Point Point;
typedef struct Rect Rect;
typedef struct Region **RgnHandle;
typedef struct GrafPort GrafPort;
typedef GrafPort *GrafPtr;
typedef struct WindowRecord* WindowPtr;
typedef WindowPtr DialogPtr;
typedef struct ControlRecord Control;
typedef Control **ControlHandle;
typedef struct Menu **MenuHandle;
typedef struct ListRec **ListHandle;
typedef struct PixPat **PixPatHandle;

// Pointer aliases
typedef void* VoidPtr;
typedef char* StringPtr;
typedef unsigned char* UCharPtr;
typedef const char* ConstStr255Param;
typedef unsigned char** StringHandle;

// Point and Rect
struct Point {
    SInt16 v;
    SInt16 h;
};

struct Rect {
    SInt16 top;
    SInt16 left;
    SInt16 bottom;
    SInt16 right;
};

// RGB Color
typedef struct RGBColor {
    UInt16 red;
    UInt16 green;
    UInt16 blue;
} RGBColor;

// Pattern
typedef struct Pattern {
    UInt8 pat[8];
} Pattern;

// Event Record
typedef struct EventRecord {
    UInt16   what;
    UInt32   message;
    UInt32   when;
    Point    where;
    UInt16   modifiers;
} EventRecord;

// BitMap
typedef struct BitMap {
    Ptr      baseAddr;
    SInt16   rowBytes;
    Rect     bounds;
} BitMap;

// PixMap
typedef struct PixMap {
    Ptr      baseAddr;
    SInt16   rowBytes;
    Rect     bounds;
    SInt16   pmVersion;
    SInt16   packType;
    UInt32   packSize;
    UInt32   hRes;
    UInt32   vRes;
    SInt16   pixelType;
    SInt16   pixelSize;
    SInt16   cmpCount;
    SInt16   cmpSize;
    UInt32   planeBytes;
    Handle   pmTable;
    UInt32   pmReserved;
} PixMap;

typedef PixMap* PixMapPtr;
typedef PixMapPtr* PixMapHandle;

// GrafPort
struct GrafPort {
    SInt16   device;
    BitMap   portBits;
    Rect     portRect;
    RgnHandle visRgn;
    RgnHandle clipRgn;
    Pattern  bkPat;
    Pattern  fillPat;
    Point    pnLoc;
    Point    pnSize;
    SInt16   pnMode;
    Pattern  pnPat;
    SInt16   pnVis;
    SInt16   txFont;
    UInt8    txFace;
    SInt16   txMode;
    SInt16   txSize;
    SInt32   spExtra;
    SInt32   fgColor;
    SInt32   bkColor;
    SInt16   colrBit;
    SInt16   patStretch;
    Handle   picSave;
    Handle   rgnSave;
    Handle   polySave;
    GrafPtr  grafProcs;
};

// CGrafPort
typedef struct CGrafPort {
    SInt16        device;
    PixMapHandle  portPixMap;
    SInt16        portVersion;
    Handle        grafVars;
    SInt16        chExtra;
    SInt16        pnLocHFrac;
    Rect          portRect;
    RgnHandle     visRgn;
    RgnHandle     clipRgn;
    PixPatHandle  bkPixPat;
    RGBColor      rgbFgColor;
    RGBColor      rgbBkColor;
    Point         pnLoc;
    Point         pnSize;
    SInt16        pnMode;
    PixPatHandle  pnPixPat;
    PixPatHandle  fillPixPat;
    SInt16        pnVis;
    SInt16        txFont;
    UInt8         txFace;
    SInt16        txMode;
    SInt16        txSize;
    SInt32        spExtra;
    SInt32        fgColor;
    SInt32        bkColor;
    SInt16        colrBit;
    SInt16        patStretch;
    Handle        picSave;
    Handle        rgnSave;
    Handle        polySave;
    GrafPtr       grafProcs;
} CGrafPort;

typedef CGrafPort* CGrafPtr;
typedef CGrafPtr GWorldPtr;

// GrafVars - internal graphics state
typedef struct GrafVars {
    RGBColor rgbOpColor;
    RGBColor rgbHiliteColor;
    Handle   pmFgColor;
    SInt16   pmFgIndex;
    Handle   pmBkColor;
    SInt16   pmBkIndex;
    SInt16   pmFlags;
} GrafVars;

// Finder Info structures
typedef struct FInfo {
    OSType  fdType;      /* File type */
    OSType  fdCreator;   /* File creator */
    UInt16  fdFlags;     /* Finder flags */
    Point   fdLocation;  /* File location */
    SInt16  fdFldr;      /* Folder ID */
} FInfo;

typedef struct DInfo {
    Rect    frRect;      /* Folder rect */
    UInt16  frFlags;     /* Folder flags */
    Point   frLocation;  /* Folder location */
    SInt16  frView;      /* Folder view */
} DInfo;

// File System Spec
typedef struct FSSpec {
    SInt16   vRefNum;
    SInt32   parID;
    Str255   name;
} FSSpec;

typedef FSSpec* FSSpecPtr;

// Parameter blocks
typedef struct ParamBlockRec ParamBlockRec;
typedef ParamBlockRec* ParmBlkPtr;

typedef struct IOParam {
    struct IOParam* qLink;
    SInt16   qType;
    SInt16   ioTrap;
    Ptr      ioCmdAddr;
    void*    ioCompletion;
    OSErr    ioResult;
    StringPtr ioNamePtr;
    SInt16   ioVRefNum;
    SInt16   ioRefNum;
    SInt8    ioVersNum;
    SInt8    ioPermssn;
    Ptr      ioMisc;
    Ptr      ioBuffer;
    SInt32   ioReqCount;
    SInt32   ioActCount;
    SInt16   ioPosMode;
    SInt32   ioPosOffset;
} IOParam;

// HParamBlockRec definition
typedef struct HParamBlockRec {
    struct HParamBlockRec* qLink;
    SInt16   qType;
    SInt16   ioTrap;
    Ptr      ioCmdAddr;
    void*    ioCompletion;
    OSErr    ioResult;
    StringPtr ioNamePtr;
    SInt16   ioVRefNum;

    union {
        struct {
            SInt16   ioVolIndex;
            UInt32   ioVAlBlkSiz;   /* Allocation block size */
            UInt32   ioVNmAlBlks;   /* Number of allocation blocks */
        } volumeParam;

        struct {
            SInt16   ioRefNum;
            SInt16   ioFDirIndex;
            SInt8    ioFlAttrib;
            SInt8    ioFlVersNum;
            FInfo    ioFlFndrInfo;
            SInt32   ioDirID;
            SInt16   ioFlStBlk;
            SInt32   ioFlLgLen;
            SInt32   ioFlPyLen;
            SInt16   ioFlRStBlk;
            SInt32   ioFlRLgLen;
            SInt32   ioFlRPyLen;
            UInt32   ioFlCrDat;
            UInt32   ioFlMdDat;
        } hFileInfo;

        struct {
            SInt32   ioDrDirID;
            UInt16   ioDrNmFls;
            SInt8    filler3[9];
            UInt32   ioDrCrDat;
            UInt32   ioDrMdDat;
            UInt32   ioDrBkDat;
            DInfo    ioDrFndrInfo;
            SInt16   ioDrParID;
        } dirInfo;
    } u;
} HParamBlockRec;

// CInfoPBRec definition
typedef struct CInfoPBRec {
    struct CInfoPBRec* qLink;
    SInt16   qType;
    SInt16   ioTrap;
    Ptr      ioCmdAddr;
    void*    ioCompletion;
    OSErr    ioResult;
    StringPtr ioNamePtr;
    SInt16   ioVRefNum;

    union {
        struct {
            SInt16   ioFRefNum;
            SInt16   ioFDirIndex;
            SInt8    ioFlAttrib;
            SInt8    ioFlVersNum;
            FInfo    ioFlFndrInfo;
            SInt32   ioDirID;
            SInt16   ioFlStBlk;
            SInt32   ioFlLgLen;
            SInt32   ioFlPyLen;
            SInt16   ioFlRStBlk;
            SInt32   ioFlRLgLen;
            SInt32   ioFlRPyLen;
            UInt32   ioFlCrDat;
            UInt32   ioFlMdDat;
        } hFileInfo;

        struct {
            SInt32   ioDrDirID;
            UInt16   ioDrNmFls;
            SInt8    filler[9];
            UInt32   ioDrCrDat;
            UInt32   ioDrMdDat;
            UInt32   ioDrBkDat;
            DInfo    ioDrFndrInfo;
            SInt32   ioDrParID;
        } dirInfo;
    } u;
} CInfoPBRec;

typedef struct FileParam {
    struct FileParam* qLink;
    SInt16   qType;
    SInt16   ioTrap;
    Ptr      ioCmdAddr;
    void*    ioCompletion;
    OSErr    ioResult;
    StringPtr ioNamePtr;
    SInt16   ioVRefNum;
    SInt16   ioFRefNum;
    SInt8    ioFVersNum;
    SInt8    filler1;
    SInt16   ioFDirIndex;
    SInt8    ioFlAttrib;
    SInt8    ioFlVersNum;
    UInt32   ioFlFndrInfo[4];
    UInt32   ioFlNum;
    UInt16   ioFlStBlk;
    SInt32   ioFlLgLen;
    SInt32   ioFlPyLen;
    UInt16   ioFlRStBlk;
    SInt32   ioFlRLgLen;
    SInt32   ioFlRPyLen;
    UInt32   ioFlCrDat;
    UInt32   ioFlMdDat;
} FileParam;

typedef struct VolumeParam {
    struct VolumeParam* qLink;
    SInt16   qType;
    SInt16   ioTrap;
    Ptr      ioCmdAddr;
    void*    ioCompletion;
    OSErr    ioResult;
    StringPtr ioNamePtr;
    SInt16   ioVRefNum;
    SInt32   filler2;
    SInt16   ioVolIndex;
    UInt32   ioVCrDate;
    UInt32   ioVLsBkUp;
    UInt16   ioVAtrb;
    UInt16   ioVNmFls;
    UInt16   ioVDirSt;
    UInt16   ioVBlLn;
    UInt16   ioVNmAlBlks;
    SInt32   ioVAlBlkSiz;
    SInt32   ioVClpSiz;
    UInt16   ioAlBlSt;
    UInt32   ioVNxtFNum;
    UInt16   ioVFrBlk;
} VolumeParam;

typedef struct CntrlParam {
    struct CntrlParam* qLink;
    SInt16   qType;
    SInt16   ioTrap;
    Ptr      ioCmdAddr;
    void*    ioCompletion;
    OSErr    ioResult;
    StringPtr ioNamePtr;
    SInt16   ioVRefNum;
    SInt16   ioCRefNum;
    SInt16   csCode;
    SInt16   csParam[11];
} CntrlParam;

struct ParamBlockRec {
    struct ParamBlockRec* qLink;
    SInt16   qType;
    SInt16   ioTrap;
    Ptr      ioCmdAddr;
    void*    ioCompletion;
    OSErr    ioResult;
    StringPtr ioNamePtr;
    SInt16   ioVRefNum;
    SInt16   ioRefNum;       // File reference number
    SInt8    ioVersNum;      // Version number
    SInt8    ioPermssn;      // Permission
    void*    ioMisc;         // Miscellaneous
    void*    ioBuffer;       // Buffer pointer
    SInt32   ioReqCount;     // Requested count
    SInt32   ioActCount;     // Actual count
    SInt16   ioPosMode;      // Position mode
    SInt32   ioPosOffset;    // Position offset
    union {
        IOParam      ioParam;
        FileParam    fileParam;
        VolumeParam  volumeParam;
        CntrlParam   cntrlParam;
    } u;
};

// Pointer types for param blocks
typedef IOParam* IOParamPtr;
typedef FileParam* FileParamPtr;
typedef VolumeParam* VolumeParamPtr;
typedef CntrlParam* CntrlParamPtr;

// All other needed types
typedef struct ResourceMap {
    UInt32   resourceMapOffset;
    UInt32   resourceDataOffset;
    UInt32   resourceDataLength;
    UInt32   resourceMapLength;
} ResourceMap;

typedef struct WindowRecord {
    GrafPort      port;
    SInt16        windowKind;
    Boolean       visible;
    Boolean       hilited;
    Boolean       goAwayFlag;
    Boolean       spareFlag;
    RgnHandle     strucRgn;
    RgnHandle     contRgn;
    RgnHandle     updateRgn;
    Handle        windowDefProc;
    Handle        dataHandle;
    StringHandle  titleHandle;
    SInt16        titleWidth;
    ControlHandle controlList;
    struct WindowRecord* nextWindow;
    Handle        windowPic;
    SInt32        refCon;
    RgnHandle     visRgn;
    GWorldPtr     offscreenGWorld;  /* Offscreen buffer for double-buffering */
} WindowRecord;

typedef WindowRecord* WindowPeek;

/* Window Manager constants */
#define kUpdateTitle    1
#define kUpdateAll      2
#define kUpdateFrame    4
#define kUpdateContent  8
/* [WM-054] wDraw and other WDEF messages now in WindowManager/WindowWDEF.h */
#define wNoHit         0
#define wInContent     1
#define wInDrag        2
#define wInGrow        3
#define wInGoAway      4
#define wInZoomIn      5
#define wInZoomOut     6

/* FindWindow part codes */
#define inDesk         0
#define inMenuBar      1
#define inSysWindow    2
#define inContent      3
#define inDrag         4
#define inGrow         5
#define inGoAway       6
#define inZoomIn       7
#define inZoomOut      8

/* Window update flags */
typedef unsigned short WindowUpdateFlags;

/* Window definition procedure pointer */
typedef long (*WindowDefProcPtr)(short varCode, WindowPtr theWindow, short message, long param);

typedef struct ControlRecord {
    struct ControlRecord** nextControl;
    WindowPtr     contrlOwner;
    Rect          contrlRect;
    UInt8         contrlVis;
    UInt8         contrlHilite;
    SInt16        contrlValue;
    SInt16        contrlMin;
    SInt16        contrlMax;
    Handle        contrlDefProc;
    Handle        contrlData;
    void*         contrlAction;
    SInt32        contrlRfCon;
    Str255        contrlTitle;
} ControlRecord;

typedef ControlRecord* ControlPtr;

typedef struct MenuInfo {
    SInt16   menuID;
    SInt16   menuWidth;
    SInt16   menuHeight;
    Handle   menuProc;
    SInt32   enableFlags;
    Str255   menuData;
} MenuInfo;

typedef MenuInfo* MenuPtr;

typedef struct DialogRecord {
    WindowRecord  window;
    Handle        items;
    Handle        textH;
    SInt16        editField;
    SInt16        editOpen;
    SInt16        aDefItem;
} DialogRecord;

typedef DialogRecord* DialogPeek;

typedef struct DialogTemplate {
    Rect     boundsRect;
    SInt16   procID;
    Boolean  visible;
    Boolean  filler1;
    Boolean  goAwayFlag;
    Boolean  filler2;
    SInt32   refCon;
    SInt16   itemsID;
    Str255   title;
} DialogTemplate;

typedef struct Cell {
    SInt16 h;
    SInt16 v;
} Cell;

typedef struct ListRec {
    Rect          rView;
    BitMap        port;
    Point         indent;
    Point         cellSize;
    Rect          visible;
    ControlHandle vScroll;
    ControlHandle hScroll;
    SInt8         selFlags;
    Boolean       lActive;
    SInt8         lReserved;
    SInt8         listFlags;
    SInt32        clikTime;
    Point         clikLoc;
    Point         mouseLoc;
    void*         lClickLoop;
    Cell          lastClick;
    SInt32        refCon;
    Handle        listDefProc;
    Handle        userHandle;
    Rect          dataBounds;
    Handle        cells;
    SInt16        maxIndex;
    SInt16        cellArray[1];
} ListRec;

typedef ListRec* ListPtr;

typedef struct TERec {
    Rect     destRect;
    Rect     viewRect;
    Rect     selRect;
    SInt16   lineHeight;
    SInt16   fontAscent;
    Point    selPoint;
    SInt16   selStart;
    SInt16   selEnd;
    SInt16   active;
    Handle   hText;
    SInt16   recalBack;
    SInt16   recalLines;
    SInt16   clikLoop;
    SInt32   clickTime;
    SInt16   clickLoc;
    SInt32   caretTime;
    SInt16   caretState;
    SInt16   just;
    SInt16   teLength;
    Handle   hDispatchRec;
    SInt16   clikStuff;
    SInt16   crOnly;
    SInt16   txFont;
    UInt8    filler;        /* Padding byte */
    void*    wordBreak;     /* Word break hook */
    UInt8    txFace;
    SInt16   txMode;
    SInt16   txSize;
    GrafPtr  inPort;
    void*    highHook;
    void*    caretHook;
    SInt16   nLines;
    SInt16   lineStarts[16001];
} TERec;

typedef TERec* TEPtr;
typedef TEPtr* TEHandle;

typedef struct ScrapStuff {
    SInt32   scrapSize;
    Handle   scrapHandle;
    SInt16   scrapCount;
    SInt16   scrapState;
    StringPtr scrapName;
} ScrapStuff;

typedef ScrapStuff* PScrapStuff;

typedef struct TPrint {
    SInt16   iPrVersion;
    SInt16   prInfo[13];
    Rect     rPaper;
    Rect     prStl[6];
    SInt16   prInfoPT;
    SInt16   iPageV;
    SInt16   iPageH;
    SInt8    bPort;
    SInt8    feed;
} TPrint;

typedef TPrint* TPPrint;
typedef TPPrint* THPrint;

typedef struct TPrStatus {
    SInt16   iTotPages;
    SInt16   iCurPage;
    SInt16   iTotCopies;
    SInt16   iCurCopy;
    SInt16   iTotBands;
    SInt16   iCurBand;
    Boolean  fPgDirty;
    Boolean  fImaging;
    Handle   hPrint;
    GrafPtr  pPrPort;
    Handle   hPic;
} TPrStatus;

typedef struct SndCommand {
    UInt16   cmd;
    SInt16   param1;
    SInt32   param2;
} SndCommand;

typedef struct SndChannel {
    struct SndChannel* nextChan;
    Ptr      firstMod;
    void*    callBack;
    SInt32   userInfo;
    SInt32   wait;
    SndCommand cmdInProgress;
    SInt16   flags;
    SInt16   qLength;
    SInt16   qHead;
    SInt16   qTail;
    SndCommand queue[128];
} SndChannel;

typedef SndChannel* SndChannelPtr;

typedef struct DriverHeader {
    SInt16   drvrFlags;
    SInt16   drvrDelay;
    SInt16   drvrEMask;
    SInt16   drvrMenu;
    SInt16   drvrOpen;
    SInt16   drvrPrime;
    SInt16   drvrCtl;
    SInt16   drvrStatus;
    SInt16   drvrClose;
    UInt8    drvrName[32];
} DriverHeader;

typedef DriverHeader* DriverHeaderPtr;

// Queue types (needed for DCtlEntry)
typedef struct QElem {
    struct QElem* qLink;
    SInt16 qType;
    char qData[1];
} QElem;

typedef QElem* QElemPtr;

typedef struct QHdr {
    SInt16 qFlags;
    QElemPtr qHead;
    QElemPtr qTail;
} QHdr;

typedef QHdr* QHdrPtr;

typedef struct DCtlEntry {
    Ptr      dCtlDriver;
    SInt16   dCtlFlags;
    QHdr     dCtlQHdr;
    SInt32   dCtlPosition;
    Handle   dCtlStorage;
    SInt16   dCtlRefNum;
    SInt32   dCtlCurTicks;
    WindowPtr dCtlWindow;
    SInt16   dCtlDelay;
    SInt16   dCtlEMask;
    SInt16   dCtlMenu;
} DCtlEntry;

typedef DCtlEntry* DCtlPtr;
typedef DCtlPtr* DCtlHandle;

typedef struct AuxDCE {
    Ptr      dCtlDriver;
    SInt16   dCtlFlags;
    SInt16   dCtlQHdr;
    SInt32   dCtlPosition;
    Handle   dCtlStorage;
    SInt16   dCtlRefNum;
    SInt32   dCtlCurTicks;
    GrafPtr  dCtlWindow;
    SInt16   dCtlDelay;
    SInt16   dCtlEMask;
    SInt16   dCtlMenu;
    SInt8    dCtlSlot;
    SInt8    dCtlSlotId;
    SInt32   dCtlDevBase;
    Ptr      dCtlOwner;
    SInt8    dCtlExtDev;
    SInt8    fillByte;
} AuxDCE;

typedef AuxDCE* AuxDCEPtr;
typedef AuxDCEPtr* AuxDCEHandle;

// Component Manager
typedef struct Component* ComponentInstance;
typedef struct ComponentRecord* Component;

typedef struct ComponentDescription {
    OSType   componentType;
    OSType   componentSubType;
    OSType   componentManufacturer;
    UInt32   componentFlags;
    UInt32   componentFlagsMask;
} ComponentDescription;

typedef struct ComponentResource {
    ComponentDescription cd;
    ResType  component_type;
    ResID    component_id;
    ResType  component_icon;
} ComponentResource;

typedef ComponentResource* ComponentResourcePtr;
typedef ComponentResourcePtr* ComponentResourceHandle;

// Additional Component Manager types
typedef struct ComponentParameters {
    UInt8    flags;
    UInt8    paramSize;
    SInt16   what;
    long     params[1];
} ComponentParameters;

typedef long (*ComponentRoutine)(ComponentParameters* params, Handle storage);
typedef ComponentRoutine ComponentFunction;
typedef struct ComponentMutex* ComponentMutex;

typedef struct ComponentRegistryEntry {
    Component               component;
    ComponentDescription    description;
    ComponentRoutine        entryPoint;
    Handle                  storage;
    SInt32                  refCount;
    Boolean                 registered;
    struct ComponentRegistryEntry* next;
} ComponentRegistryEntry;

// Apple Events
typedef FourCharCode DescType;
typedef FourCharCode AEKeyword;

typedef struct AEDesc {
    DescType descriptorType;
    Handle   dataHandle;
} AEDesc;

typedef AEDesc AEAddressDesc;
typedef AEDesc AEDescList;
typedef AEDescList AERecord;
typedef AERecord AppleEvent;

// Apple Event types
typedef FourCharCode AEEventClass;
typedef FourCharCode AEEventID;
typedef FourCharCode AEReturnID;
typedef FourCharCode AETransactionID;

typedef struct AEKeyDesc {
    AEKeyword   descKey;
    AEDesc      descContent;
} AEKeyDesc;

typedef SInt8 AEArrayType;
typedef void* AEArrayDataPointer;

typedef struct AEDescListInfo {
    Size        dataSize;
    SInt32      recordCount;
    Boolean     isRecord;
} AEDescListInfo;

typedef struct AEDescStream {
    void*       streamData;
    Size        streamSize;
    Size        streamPos;
} AEDescStream;

typedef OSErr (*AEHandlerEnumProc)(AEEventClass theAEEventClass, AEEventID theAEEventID, SInt32 refCon);
typedef SInt8 AEHandlerResult;

typedef struct AEDispatchContext {
    AppleEvent* theAppleEvent;
    AppleEvent* reply;
    SInt32      handlerRefcon;
} AEDispatchContext;

typedef OSErr (*AEPreDispatchProc)(const AppleEvent* theAppleEvent, AppleEvent* reply, SInt32 refCon);
typedef OSErr (*AEPostDispatchProc)(const AppleEvent* theAppleEvent, AppleEvent* reply, SInt32 refCon);

// Apple Event handler types
typedef OSErr (*AEEventHandlerProcPtr)(const AppleEvent* theAppleEvent, AppleEvent* reply, SInt32 handlerRefcon);
typedef OSErr (*AECoerceDescProcPtr)(const AEDesc* fromDesc, DescType toType, SInt32 handlerRefcon, AEDesc* toDesc);

// More Apple Event types
typedef OSErr (*AECoercionEnumProc)(DescType fromType, DescType toType, SInt32 refCon);
typedef OSErr (*AESpecialHandlerEnumProc)(AEKeyword theKeyword, SInt32 refCon);
typedef Boolean (*AEEventFilterProc)(const AppleEvent* theAppleEvent, AppleEvent* reply, SInt32 refCon);
typedef OSErr (*AEErrorHandlerProc)(OSErr errorCode, const AppleEvent* theAppleEvent, AppleEvent* reply, SInt32 refCon);


// Apple Event constants
enum {
    kAEInteractWithLocal = 0,
    kAEInteractWithSelf = 1,
    kAEInteractWithAll = 2,
    kAESameProcess = 0,
    kAELocalProcess = 1,
    kAERemoteProcess = 2
};

typedef SInt8 AEEventSource;

// Thread support types (stub for portability)
typedef struct pthread_mutex_t { void* dummy; } pthread_mutex_t;

// Color Manager types
typedef struct CMCMYKColor {
    UInt16 cyan;
    UInt16 magenta;
    UInt16 yellow;
    UInt16 black;
} CMCMYKColor;

typedef struct CMHSVColor {
    UInt16 hue;
    UInt16 saturation;
    UInt16 value;
} CMHSVColor;

typedef struct CMHLSColor {
    UInt16 hue;
    UInt16 lightness;
    UInt16 saturation;
} CMHLSColor;

typedef struct CMXYZColor {
    UInt16 X;
    UInt16 Y;
    UInt16 Z;
} CMXYZColor;

typedef struct CMLABColor {
    UInt16 L;
    SInt16 a;
    SInt16 b;
} CMLABColor;

typedef OSErr CMError;
typedef struct OpaqueCMProfileRef* CMProfileRef;
typedef struct OpaqueCMWorldRef* CMWorldRef;
typedef struct OpaqueCMBitmapRef* CMBitmapRef;

// Duplicate removed - AEKeyDesc defined above

// Process Manager
typedef struct ProcessSerialNumber {
    UInt32 highLongOfPSN;
    UInt32 lowLongOfPSN;
} ProcessSerialNumber;

typedef struct ProcessInfoRec {
    UInt32               processInfoLength;
    StringPtr            processName;
    ProcessSerialNumber  processNumber;
    UInt32               processType;
    OSType               processSignature;
    UInt32               processMode;
    Ptr                  processLocation;
    UInt32               processSize;
    UInt32               processFreeMem;
    ProcessSerialNumber  processLauncher;
    UInt32               processLaunchDate;
    UInt32               processActiveTime;
    void*                processAppSpec;
} ProcessInfoRec;

// Alias Manager
typedef Handle AliasHandle;

typedef struct AliasRecord {
    OSType   userType;
    UInt16   aliasSize;
    UInt16   version;      /* Alias version (2 for System 7) */
    UInt16   aliasKind;    /* Type of alias */
} AliasRecord;

typedef AliasRecord* AliasPtr;

// Notification Manager
typedef struct NMRec {
    struct NMRec* qLink;
    SInt16   qType;
    SInt16   nmFlags;
    SInt32   nmPrivate;
    SInt16   nmReserved;
    SInt16   nmMark;
    Handle   nmIcon;
    Handle   nmSound;
    StringPtr nmStr;
    void*    nmResp;
    SInt32   nmRefCon;
} NMRec;

typedef NMRec* NMRecPtr;

// Time Manager
typedef struct TMTask {
    struct TMTask* qLink;
    SInt16   qType;
    Ptr      tmAddr;
    SInt32   tmCount;
    SInt32   tmWakeUp;
    SInt32   tmReserved;
} TMTask;

typedef TMTask* TMTaskPtr;

// Edition Manager
typedef struct EditionContainerSpec {
    FSSpec   theFile;
    OSType   theFileType;
    SInt16   thePart;
} EditionContainerSpec;

typedef struct SectionRecord {
    UInt32   version;
    SInt32   kind;
    UInt32   mode;
    UInt32   mdDate;
    SInt32   sectionID;
    SInt32   refCon;
    Handle   alias;
} SectionRecord;

typedef SectionRecord* SectionPtr;
typedef SectionPtr* SectionHandle;

// Standard File
typedef struct StandardFileReply {
    Boolean  sfGood;
    Boolean  sfReplacing;
    OSType   sfType;
    FSSpec   sfFile;
    UInt16   sfScript;
    SInt16   sfFlags;
    Boolean  sfIsFolder;
    Boolean  sfIsVolume;
    SInt32   sfReserved1;
    SInt16   sfReserved2;
} StandardFileReply;

typedef struct SFReply {
    Boolean  good;
    Boolean  copy;
    OSType   fType;
    SInt16   vRefNum;
    SInt16   version;
    Str63    fName;
} SFReply;

// HFS Types
typedef struct HFSCatalogFile {
    SInt8    recordType;
    SInt8    flags;
    UInt8    fileType;
    UInt8    fileUsrWds;
    UInt32   fileNum;
    UInt16   dataStartBlock;
    SInt32   dataLogicalSize;
    SInt32   dataPhysicalSize;
    UInt16   rsrcStartBlock;
    SInt32   rsrcLogicalSize;
    SInt32   rsrcPhysicalSize;
    UInt32   createDate;
    UInt32   modifyDate;
} HFSCatalogFile;

typedef struct HFSCatalogFolder {
    SInt8    recordType;
    SInt8    flags;
    UInt16   valence;
    UInt32   folderID;
    UInt32   createDate;
    UInt32   modifyDate;
} HFSCatalogFolder;

typedef struct HFSCatalogThread {
    SInt8    recordType;
    SInt8    reserved[9];
    UInt32   parentID;
    Str31    nodeName;
} HFSCatalogThread;

// Memory Manager
typedef struct Zone {
    Ptr      bkLim;
    Ptr      purgePtr;
    Ptr      hFstFree;
    SInt32   zcbFree;
    void*    gzProc;
    SInt16   moreMast;
    SInt16   flags;
    SInt16   cntRel;
    SInt16   maxRel;
    SInt16   cntNRel;
    SInt16   maxNRel;
    SInt16   cntEmpty;
    SInt16   cntHandles;
    SInt32   minCBFree;
    void*    purgeProc;
    Ptr      sparePtr;
    Ptr      allocPtr;
    SInt16   heapData;
} Zone;

typedef Zone* THz;

typedef CInfoPBRec* CInfoPBPtr;

// Error codes
enum {
    fnfErr             = -43,
    paramErr           = -50,
    memFullErr         = -108,
    nilHandleErr       = -109,
    memWZErr           = -111,
    memPurErr          = -112,
    memAdrErr          = -110,
    memAZErr           = -113,
    memPCErr           = -114,
    memBCErr           = -115,
    memSCErr           = -116,
    memLockedErr       = -117,
    resNotFound        = -192,
    resFNotFound       = -193,
    addResFailed       = -194,
    rmvResFailed       = -196,
    resAttrErr         = -198,
    mapReadErr         = -199,
    CantDecompress     = -186,
    badExtResource     = -185,
    noMemForPictPlaybackErr = -145,
    rgnTooBigError     = -147,
    pixMapTooDeepErr   = -148,
    nsStackErr         = -149,
    cMatchErr          = -150,
    cTempMemErr        = -151,
    cNoMemErr          = -152,
    cRangeErr          = -153,
    cProtectErr        = -154,
    cDevErr            = -155,
    cResErr            = -156,
    rgnTooBigErr       = -500,
    updPixMemErr       = -125,
    pictInfoVersionErr = -11000,
    pictInfoIDErr      = -11001,
    pictInfoVerbErr    = -11002,
    cantLoadPickMethodErr = -11003,
    colorsRequestedErr = -11004,
    pictureDataErr     = -11005
};

// Function pointer types
typedef void (*ProcPtr)(void);
typedef ProcPtr UniversalProcPtr;
typedef UniversalProcPtr RoutineDescriptor;
typedef RoutineDescriptor* RoutineDescriptorPtr;
typedef RoutineDescriptorPtr* RoutineDescriptorHandle;
typedef OSErr (*IOCompletionProcPtr)(ParmBlkPtr paramBlock);
typedef void (*DeferredTaskProcPtr)(long dtParam);
typedef void (*TimerProcPtr)(TMTaskPtr tmTaskPtr);
typedef void (*ControlActionProcPtr)(ControlHandle theControl, SInt16 partCode);
typedef Boolean (*ModalFilterProcPtr)(DialogPtr theDialog, EventRecord* theEvent, SInt16* itemHit);
typedef void (*UserItemProcPtr)(WindowPtr theWindow, SInt16 itemNo);
typedef SInt16 (*FileFilterProcPtr)(ParmBlkPtr paramBlock);
typedef Boolean (*FileFilterYDProcPtr)(StringPtr fileName);


// Apple Event internal structures
typedef struct AEHandlerTableEntry {
    AEEventClass    eventClass;
    AEEventID       eventID;
    AEEventHandlerProcPtr handler;
    SInt32          refCon;
    Boolean         isSysHandler;
    struct AEHandlerTableEntry* next;
} AEHandlerTableEntry;

typedef struct AECoercionHandlerEntry {
    DescType        fromType;
    DescType        toType;
    AECoerceDescProcPtr handler;
    SInt32          refCon;
    Boolean         isSystemHandler;
    struct AECoercionHandlerEntry* next;
} AECoercionHandlerEntry;

typedef struct AESpecialHandlerEntry {
    AEKeyword       keyword;
    UniversalProcPtr handler;
    Boolean         isSysHandler;
    struct AESpecialHandlerEntry* next;
} AESpecialHandlerEntry;

typedef struct AEHandlerStats {
    UInt32  callCount;
    UInt32  successCount;
    UInt32  errorCount;
    UInt32  totalTime;
} AEHandlerStats;

typedef struct AEHandlerPerfInfo {
    UInt32  minTime;
    UInt32  maxTime;
    UInt32  avgTime;
} AEHandlerPerfInfo;

// Universal headers (moved to earlier in file)

// Gestalt
typedef FourCharCode GestaltSelector;

enum {
    gestaltSystemVersion = 0x73797376,  // 'sysv'
    gestaltProcessorType = 0x70726f63,  // 'proc'
    gestaltPhysicalRAMSize = 0x72616d20, // 'ram '
    gestaltLogicalRAMSize = 0x6c72616d,  // 'lram'
    gestaltQuickdrawVersion = 0x71642020, // 'qd  '
    gestaltOSTable = 0x6f737474,        // 'ostt'
    gestaltToolboxTable = 0x74627474,   // 'tbtt'
    gestaltFPUType = 0x66707520,        // 'fpu '
    gestaltMMUType = 0x6d6d7520,        // 'mmu '
    gestaltAppleTalkVersion = 0x61746c6b, // 'atlk'
    gestaltVMAttr = 0x766d2020          // 'vm  '
};

// Resource Manager
enum {
    resSysHeap = 64,
    resPurgeable = 32,
    resLocked = 16,
    resProtected = 8,
    resPreload = 4,
    resChanged = 2
};

// Script Manager
typedef SInt16 ScriptCode;
typedef SInt16 LangCode;

// ADB Manager
typedef struct ADBAddress {
    UInt8 addr;
    UInt8 devType;
} ADBAddress;

typedef struct ADBDevice {
    ADBAddress address;
    void* serviceRoutine;
    void* dataArea;
} ADBDevice;

typedef struct ADBDataBlock {
    SInt8    devType;
    SInt8    origADBAddr;
    void*    dbServiceRtPtr;
    void*    dbDataAreaAddr;
} ADBDataBlock;

typedef void (*ADBCompletionProcPtr)(UInt8* dataBuffer, UInt8 command, SInt32 data);

// Device Manager types
typedef enum {
    kIOOperationRead = 1,
    kIOOperationWrite = 2,
    kIOOperationControl = 3,
    kIOOperationStatus = 4,
    kIOOperationKill = 5
} IOOperationType;

typedef void (*IOCompletionProc)(IOParamPtr pb);
typedef void (*AsyncIOCompletionProc)(void* request, OSErr error);

// Device Control Entry typedef already defined above
typedef DCtlEntry DCE;
typedef DCE* DCEPtr;
typedef DCEPtr* DCEHandle;

// Additional error codes
enum {
    dsIOCoreErr = -1,
    userCanceledErr = -128,
    queueOverflow = -129,
    ioTimeout = -130
};

// Additional types
typedef UInt8 SignedByte;
typedef UInt8 Byte;

// System 7 Extensions
typedef struct HighLevelEventMsg {
    UInt16   theMsgEvent;
    UInt32   when;
    Point    where;
    UInt16   modifiers;
    OSType   msgClass;
    UInt32   msgBuffer[6];
} HighLevelEventMsg;

// PPC Toolbox
typedef struct PPCPortRec {
    Str32    name;
    UInt16   portKindSelector;
    Str32    portTypeStr;
} PPCPortRec;

typedef PPCPortRec* PPCPortPtr;

// Communications Toolbox
typedef struct CMBufferSizes {
    SInt32   ctsSize;
    SInt32   crmSize;
    SInt32   ctbSize;
} CMBufferSizes;

// Help Manager
typedef struct HMMessageRecord {
    SInt16   hmmHelpType;
    union {
        Str255   hmmString;
        SInt16   hmmPict;
        Handle   hmmTEHandle;
    } u;
} HMMessageRecord;

// Color types
typedef struct ColorSpec {
    SInt16   value;
    RGBColor rgb;
} ColorSpec;

typedef ColorSpec* ColorSpecPtr;

typedef struct ColorTable {
    SInt32      ctSeed;
    SInt16      ctFlags;
    SInt16      ctSize;
    ColorSpec   ctTable[1];
} ColorTable;

typedef ColorTable* CTabPtr;
typedef CTabPtr* CTabHandle;
typedef CTabHandle ITabHandle;  // Inverse table handle
typedef struct CCrsr** CCrsrHandle;  // Color cursor handle
typedef struct CQDProcs CQDProcs;  // Color QD procs
typedef struct OpenCPicParams OpenCPicParams;  // Open CPic params

// Color cursor structure
struct CCrsr {
    UInt16      crsrType;       // Type of cursor
    PixMapPtr   crsrMap;        // Pixel map
    Handle      crsrData;       // Cursor data
    Handle      crsrXData;      // Extended data
    SInt16      crsrXValid;     // Extended data valid
    Handle      crsrXHandle;    // Extended handle
};

// QuickDraw Universal Procedure Pointers
typedef void* QDTextUPP;
typedef void* QDLineUPP;
typedef void* QDRectUPP;
typedef void* QDRRectUPP;
typedef void* QDOvalUPP;
typedef void* QDArcUPP;
typedef void* QDPolyUPP;
typedef void* QDRgnUPP;
typedef void* QDBitsUPP;
typedef void* QDCommentUPP;
typedef void* QDTxMeasUPP;
typedef void* QDGetPicUPP;
typedef void* QDPutPicUPP;

// Color QuickDraw procedures
struct CQDProcs {
    QDTextUPP       textProc;
    QDLineUPP       lineProc;
    QDRectUPP       rectProc;
    QDRRectUPP      rRectProc;
    QDOvalUPP       ovalProc;
    QDArcUPP        arcProc;
    QDPolyUPP       polyProc;
    QDRgnUPP        rgnProc;
    QDBitsUPP       bitsProc;
    QDCommentUPP    commentProc;
    QDTxMeasUPP     txMeasProc;
    QDGetPicUPP     getPicProc;
    QDPutPicUPP     putPicProc;
};

// Fixed-point type (move here from later in file)
typedef SInt32 Fixed;
typedef Fixed* FixedPtr;

// OpenCPicParams structure
struct OpenCPicParams {
    Rect        srcRect;
    Fixed       hRes;
    Fixed       vRes;
    SInt16      version;
    SInt16      reserved1;
    SInt32      reserved2;
};

typedef struct Palette {
    SInt16   pmEntries;
    Handle   pmDataFields[1];
} Palette;

typedef Palette* PalettePtr;
typedef PalettePtr* PaletteHandle;

// Speech Manager
typedef long SpeechChannel;
typedef long VoiceSpec;

typedef struct VoiceDescription {
    SInt32   length;
    VoiceSpec voice;
    SInt32   version;
    Str63    name;
    Str255   comment;
    SInt16   gender;
    SInt16   age;
    SInt16   script;
    SInt16   language;
    SInt16   region;
    SInt32   reserved[4];
} VoiceDescription;

// Additional types
typedef struct ThreadID* ThreadTaskRef;

typedef struct ThreadState {
    void*    regs[16];
    void*    sp;
    void*    pc;
    void*    toc;
} ThreadState;

typedef struct MCEntry {
    SInt16   mctID;
    SInt16   mctItem;
    RGBColor mctRGB1;
    RGBColor mctRGB2;
    RGBColor mctRGB3;
    RGBColor mctRGB4;
    SInt16   mctReserved;
} MCEntry;

typedef MCEntry* MCEntryPtr;

typedef struct MCTable {
    SInt16   mctCount;
    MCEntry  mctTable[1];
} MCTable;

typedef MCTable* MCTablePtr;
typedef MCTablePtr* MCTableHandle;

typedef struct Collection* CollectionTag;

typedef struct FMetricRec {
    SInt32   ascent;
    SInt32   descent;
    SInt32   leading;
    SInt32   widMax;
    Handle   wTabHandle;
} FMetricRec;

typedef FMetricRec* FMetricRecPtr;
typedef FMetricRecPtr* FMetricRecHandle;

typedef struct CIcon {
    PixMap   iconPMap;
    BitMap   iconMask;
    BitMap   iconBMap;
    Handle   iconData;
    SInt16   iconMaskData[1];
} CIcon;

typedef CIcon* CIconPtr;
typedef CIconPtr* CIconHandle;

// Additional missing types
typedef Handle TEStyleHandle;
typedef Handle STHandle;
typedef SInt16 StyleField;
typedef SInt16 TeActionKind;
typedef SInt16 *SInt16Ptr;
typedef SInt32 *SInt32Ptr;

typedef struct VBLTask* VBLTaskPtr;
typedef void (*VBLProcPtr)(VBLTaskPtr task);

typedef SInt16 ControlPartCode;
typedef SInt16 ControlProcID;
typedef SInt16 ControlVariant;

typedef SInt16 WindowPartCode;
typedef SInt16 WindowClass;
typedef SInt32 WindowAttributes;
typedef SInt16 WindowRegionCode;
typedef WindowPtr* WindowRef;

typedef SInt16 MenuID;
typedef SInt16 MenuItemIndex;
typedef UInt32 MenuCommand;
typedef SInt16 MenuAttributes;
typedef Handle MenuBarHandle;

typedef SInt16 DITLMethod;
typedef SInt16 StageList;
typedef DialogPtr* DialogRef;

typedef SInt16 DataHandleIndex;
typedef Boolean (*ListSearchProcPtr)(Ptr cellData, Ptr searchData, SInt16 cellDataLen, SInt16 searchDataLen);
typedef SInt16 (*ListClickLoopProcPtr)(void);

typedef Handle ResourceHandle;
typedef SInt16 ResourceCount;
typedef SInt16 ResourceIndex;
typedef SInt16 ResourceID;
typedef UInt32 ResourceType;  /* Same as ResType */
typedef SInt16 ResourceAttributes;

typedef struct WDPBRec {
    struct WDPBRec* qLink;
    SInt16   qType;
    SInt16   ioTrap;
    Ptr      ioCmdAddr;
    void*    ioCompletion;
    OSErr    ioResult;
    StringPtr ioNamePtr;
    SInt16   ioVRefNum;
    SInt16   filler1;
    SInt16   ioWDIndex;
    SInt32   ioWDProcID;
    SInt16   ioWDVRefNum;
    SInt16   filler2[5];
    SInt32   ioWDDirID;
} WDPBRec;

typedef WDPBRec* WDPBPtr;

// Forward declare these types if not already defined
#ifndef FILE_TYPES_DEFINED
#define FILE_TYPES_DEFINED
typedef SInt32 DirID;
typedef SInt16 VolumeRefNum;
typedef SInt16 FileRefNum;
typedef SInt16 WDRefNum;
typedef UInt32 FileID;

#endif // FILE_TYPES_DEFINED
// File Control Block parameter block
typedef struct FCBPBRec {
    QElemPtr        qLink;
    UInt16        qType;
    UInt16        ioTrap;
    void*           ioCmdAddr;
    void*           ioCompletion;
    OSErr           ioResult;
    StringPtr       ioNamePtr;
    VolumeRefNum    ioVRefNum;
    FileRefNum      ioRefNum;
    UInt16        filler;
    SInt16         ioFCBIndx;
    UInt32        ioFCBFlNm;
    UInt16        ioFCBFlags;
    UInt16        ioFCBStBlk;
    UInt32        ioFCBEOF;
    UInt32        ioFCBPLen;
    UInt32        ioFCBCrPs;
    WDRefNum        ioFCBVRefNum;
    UInt32        ioFCBClpSiz;
    DirID           ioFCBParID;
} FCBPBRec;

typedef FCBPBRec* FCBPBPtr;

typedef OSType FolderType;
typedef OSType FolderClass;

typedef struct FolderDesc {
    SInt32   descSize;
    FolderType foldType;
    UInt32   flags;
    FolderClass foldClass;
    FolderType foldLocation;
    UInt32   badgeSignature;
    UInt32   badgeType;
    UInt32   reserved;
    StrFileName name;
} FolderDesc;

typedef FolderDesc* FolderDescPtr;

typedef struct MenuRec MenuRec;
typedef MenuRec* MenuRecPtr;
typedef struct Region {
    SInt16 rgnSize;
    Rect rgnBBox;
    // Additional region data follows
} Region;
typedef struct Picture Picture;
typedef Picture* PicPtr;
typedef PicPtr* PicHandle;
typedef struct Polygon Polygon;
typedef Polygon* PolyPtr;
typedef PolyPtr* PolyHandle;

// HAL types
typedef void* System71HAL_Context;
typedef void* MemoryMgr_HAL;
typedef void* ProcessMgr_HAL;
typedef void* QuickDraw_HAL;
typedef void* WindowMgr_HAL;
typedef void* EventMgr_HAL;
typedef void* MenuMgr_HAL;
typedef void* ControlMgr_HAL;
typedef void* DialogMgr_HAL;
typedef void* TextEdit_HAL;
typedef void* ResourceMgr_HAL;
typedef void* FileMgr_HAL;
typedef void* SoundMgr_HAL;
typedef void* StandardFile_HAL;
typedef void* ListMgr_HAL;
typedef void* ScrapMgr_HAL;
typedef void* PrintMgr_HAL;
typedef void* HelpMgr_HAL;
typedef void* ColorMgr_HAL;
typedef void* ComponentMgr_HAL;
typedef void* TimeMgr_HAL;
typedef void* PackageMgr_HAL;
typedef void* AppleEventMgr_HAL;
typedef void* Calculator_HAL;
typedef void* AlarmClock_HAL;
typedef void* Notepad_HAL;
typedef void* Finder_HAL;

// Platform types
typedef void* IOContext;
typedef void* PlatformWindowRef;
typedef void* PlatformEventRef;
typedef void* PlatformMenuRef;
typedef void* PlatformControlRef;
// Removed - defined in QuickDrawPlatform.h as a struct
typedef void* PlatformFontRef;
typedef void* PlatformTimerRef;


// ADB Manager missing types
typedef struct ADBManager ADBManager;
typedef struct ADBHardwareInterface ADBHardwareInterface;
typedef struct ADBDeviceEntry ADBDeviceEntry;

struct ADBDeviceEntry {
    UInt8 address;
    UInt8 deviceType;
    UInt8 origAddress;
    UInt8 originalAddr; // Alias
    UInt8 currentAddr;  // Another alias
    void* handler;
    void* userData;
    Boolean active;
};
typedef void (*ADBCompletionProc)(Ptr refCon, SInt16 command, Ptr buffer, OSErr error);
typedef ADBCompletionProc ADBCompletionUPP;


struct ADBHardwareInterface {
    int (*startRequest)(ADBManager* mgr, UInt8 command, UInt8* data, int dataLen, Boolean isSynchronous);
    int (*resetBus)(ADBManager* mgr);
    int (*pollDevice)(ADBManager* mgr);
};

struct ADBManager {
    ADBHardwareInterface* hardware;
    ADBDeviceEntry deviceTable[16];
    UInt16 deviceMap;
    UInt16 hasDevice;
    UInt8 currentAddress;
    UInt8 currentCommand;
    UInt8 pollBuffer[8];
    UInt8 dataCount;
    UInt16 flags;
    void* eventCallback;
    void* timerCallback;
    void* eventUserData;
    void* timerUserData;

    /* Missing command queue members */
    void* commandQueue;
    void* queueBegin;
    void* queueEnd;
    void* queueHead;
    void* queueTail;

    /* Missing flags and state */
    Boolean interruptsEnabled;
    UInt16 auxFlags;
    UInt32 deviceTableOffset;

    /* More missing members */
    UInt8 initAddress;
    UInt8 moveCount;
    UInt8 keyboardType;
    UInt8 keyboardLast;
};



// Missing ADB types
typedef struct ADBOpBlock {
    UInt8 command;
    void* buffer;
    void* completion;
    void* userData;
    void* dataBuffer;
    void* serviceRoutine;
    void* dataArea;
} ADBOpBlock;

typedef struct ADBSetInfoBlock {
    void* siServiceRtPtr;
    void* siDataAreaAddr;
} ADBSetInfoBlock;

/* Missing ADB command queue entry type */
typedef struct ADBCmdQEntry {
    UInt8 command;
    void* buffer;
    void* completion;
    void* userData;
} ADBCmdQEntry;

/* Missing keyboard driver data type */
typedef struct KeyboardDriverData {
    UInt8 keyMap[16];
    UInt8 modifiers;
    void* reserved;
    UInt8 numBuffers;
    void* kchrPtr;
    void* kmapPtr;
} KeyboardDriverData;


typedef void (*ADBEventCallback)(UInt16 event, void* userData);
typedef void (*ADBTimerCallback)(void* userData);

// Missing Trap Dispatcher types
typedef void* TrapContext;
typedef void* FLineTrapContext;
typedef void (*TrapHandler)(void);

// Missing Apple Event types
typedef OSType AEEventClass;
typedef OSType AEEventID;
typedef SInt32 AESendMode;
typedef SInt16 AESendPriority;
typedef SInt16 AEInteractAllowed;
typedef void (*EventHandlerProcPtr)(const AppleEvent* event, AppleEvent* reply, SInt32 refCon);
typedef void (*CoercionHandlerProcPtr)(DescType fromType, const void* fromData, Size fromSize, 
                                       DescType toType, SInt32 refCon, void* toData, Size* toSize);
typedef Boolean (*IdleProcPtr)(EventRecord* event, SInt32* sleepTime, RgnHandle* mouseRgn);
typedef Boolean (*EventFilterProcPtr)(EventRecord* event);

// Missing OSA/AppleScript types
typedef struct OSAScript* OSAScript;
typedef ComponentInstance OSAComponentInstance;

// Missing Resource Manager types
typedef SInt16 RefNum;
typedef SInt16 ResAttributes;
typedef struct ResourceEntry ResourceEntry;

// Missing Event Manager types
enum {
    kCoreEventClass = 0x61657674  /* 'aevt' */
};

// Missing constants for ADB (removed duplicates below)

/* ADB service routine type */
typedef void (*ADBServiceRoutineProcPtr)(Ptr buffer, UInt16 command, void* userData);

/* ADB device handler type */
typedef void (*ADBDeviceHandler)(UInt8 command, UInt32 deviceInfo, void* buffer, void* userData);

enum {
    // ADB error codes
    ADB_ERROR_INVALID_PARAM = -1,
    ADB_ERROR_HARDWARE = -2,
    ADB_ERROR_TIMEOUT = -3,
    ADB_ERROR_COLLISION = -4,
    ADB_ERROR_NO_DEVICE = -5
};

// Missing QuickDraw types
typedef struct QDProcs {
    void* textProc;
    void* lineProc;
    void* rectProc;
    void* rRectProc;
    void* ovalProc;
    void* arcProc;
    void* polyProc;
    void* rgnProc;
    void* bitsProc;
    void* commentProc;
    void* txMeasProc;
    void* getPicProc;
    void* putPicProc;
    void* opcodeProc;
    void* newProc1;
    void* glyphProc;
    void* printerStatusProc;
    void* newProc4;
    void* newProc5;
    void* newProc6;
} QDProcs;

typedef QDProcs* QDProcsPtr;

// Missing Resource Manager types
typedef void (*ResErrProcPtr)(OSErr error);
typedef Handle (*DecompressHookProc)(Handle rsrc);

// Missing Window Manager types
typedef struct AuxWinRec {
    struct AuxWinRec* awNext;
    WindowPtr awOwner;
    CTabHandle awCTable;  /* Changed from GWorldPtr to CTabHandle */
    Handle dialogCItem;    /* Dialog control items (optional) */
    SInt32 awFlags;
    Handle awReserved;
    Handle awRefCon;
} AuxWinRec;

typedef AuxWinRec* AuxWinPtr;
typedef AuxWinPtr* AuxWinHandle;

/* Color Window types */
typedef struct CWindowRecord {
    CGrafPort port;          /* Color graphics port */
    WindowRecord winRec;     /* Regular window record */
} CWindowRecord;
typedef CWindowRecord* CWindowPtr;

/* Window Manager port type */
typedef GrafPort WMgrPort;

/* [WM-055] Window kind constants now in WindowManager/WindowKinds.h */

/* Update flags for windows - see also lines 487-490 for original definitions */

// Missing File Manager types
typedef SInt16 FSIORefNum;

// Missing Menu Manager types
typedef struct MCInfo {
    MenuHandle mctMenu;
    SInt16 mctItem;
    RGBColor mctRGB1;
    RGBColor mctRGB2;
    RGBColor mctRGB3;
    RGBColor mctRGB4;
    SInt16 mctReserved1;
    SInt16 mctReserved2;
} MCInfo;

typedef MCInfo* MCInfoPtr;

// Missing Memory Manager types
typedef struct MemoryBlock {
    struct MemoryBlock* next;
    SInt32 size;
    Boolean locked;
    Boolean purgeable;
    Boolean resource;
} MemoryBlock;

// Additional utility types
typedef char Str15[16];
typedef char Str27[28];

// Boot Loader types
typedef struct DeviceSpec {
    SInt16   devType;
    SInt16   devID;
    UInt32   devFlags;
    Handle   devConfig;
} DeviceSpec;

typedef struct DiskInfo {
    SInt16   diskType;
    SInt16   diskID;
    Str255   diskName;
    UInt32   diskSize;
    Boolean  isBootable;
} DiskInfo;

typedef struct BootDialog {
    SInt16   dialogType;
    Str255   message;
    Str255   buttonText;
} BootDialog;

typedef struct SystemInfo {
    UInt32   systemVersion;
    UInt32   memorySize;
    UInt32   processorType;
    UInt32   quickDrawVersion;
} SystemInfo;

// QuickDraw missing types
typedef struct PenState {
    Point    pnLoc;
    Point    pnSize;
    SInt16   pnMode;
    Pattern  pnPat;
} PenState;

// Missing Edition Manager types
typedef SInt32 EditionRefNum;
typedef struct Edition* EditionPtr;

// Missing Component Manager types
typedef SInt32 ComponentResult;

// Missing Dialog Manager types
typedef struct AlertTemplate {
    Rect boundsRect;
    SInt16 itemsID;
    SInt16 stages;
} AlertTemplate;

typedef AlertTemplate* AlertTPtr;
typedef AlertTPtr* AlertTHndl;

// Missing List Manager types

typedef Point LPoint;
typedef LPoint* LPointPtr;
typedef Rect LRect;
typedef LRect* LRectPtr;

// Missing TextEdit types
typedef struct TextStyle {
    SInt16 tsFont;
    Style tsFace;
    SInt16 tsSize;
    RGBColor tsColor;
} TextStyle;

typedef TextStyle* TextStylePtr;
typedef TextStylePtr* TextStyleHandle;

typedef struct STElement {
    SInt32 stCount;
    SInt32 stHeight;
    SInt32 stAscent;
    SInt32 stFont;
    Style stFace;
    SInt16 stSize;
    RGBColor stColor;
} STElement;

typedef STElement* STPtr;

typedef struct LongSTElement {
    SInt32 lCount;
    SInt32 lHeight;
    SInt32 lAscent;
    TextStyle lStyle;
} LongSTElement;

typedef LongSTElement* LongSTPtr;

typedef struct TextLineStart {
    SInt16 tlStart;
} TextLineStart;

typedef TextLineStart* LineStartPtr;
typedef LineStartPtr* LineStartHandle;

// Missing Print Manager types  
typedef struct TPrInfo {
    SInt16 iDev;
    SInt16 iVRes;
    SInt16 iHRes;
    Rect rPage;
} TPrInfo;

typedef struct TPrStl {
    SInt16 wDev;
    SInt16 iPageV;
    SInt16 iPageH;
    SInt8 bPort;
    SInt8 feed;
} TPrStl;

typedef struct TPrXInfo {
    SInt16 iRowBytes;
    SInt16 iBandV;
    SInt16 iBandH;
    SInt16 iDevBytes;
    SInt16 iBands;
    SInt8 bPatScale;
    SInt8 bUlThick;
    SInt8 bUlOffset;
    SInt8 bUlShadow;
    SInt8 scan;
    SInt8 bXInfoX;
} TPrXInfo;

typedef struct TPrJob {
    SInt16 iFstPage;
    SInt16 iLstPage;
    SInt16 iCopies;
    SInt8 bJDocLoop;
    Boolean fFromUsr;
    void* pIdleProc;
    void* pFileName;
    SInt16 iFileVol;
    SInt8 bFileVers;
    SInt8 bJobX;
} TPrJob;

// Missing Sound Manager types
typedef struct SoundHeader {
    Ptr samplePtr;
    UInt32 length;
    UInt32 sampleRate;
    UInt32 loopStart;
    UInt32 loopEnd;
    UInt8 encode;
    UInt8 baseFrequency;
} SoundHeader;

typedef SoundHeader* SoundHeaderPtr;

typedef struct ExtSoundHeader {
    Ptr samplePtr;
    UInt32 numChannels;
    UInt32 sampleRate;
    UInt32 loopStart;
    UInt32 loopEnd;
    UInt8 encode;
    UInt8 baseFrequency;
    UInt32 numFrames;
    void* AIFFSampleRate;
    Ptr markerChunk;
    void* instrumentChunks;
    void* AESRecording;
    UInt16 sampleSize;
    UInt16 futureUse1;
    UInt32 futureUse2;
    UInt32 futureUse3;
    UInt32 futureUse4;
} ExtSoundHeader;

typedef ExtSoundHeader* ExtSoundHeaderPtr;

// Missing Script Manager types
typedef struct ItlbRecord {
    SInt16 itlbNumber;
    SInt16 itlbDate;
    SInt16 itlbSort;
    SInt16 itlbFlags;
    SInt16 itlbToken;
    SInt16 itlbEncoding;
    SInt16 itlbLang;
    SInt16 itlbNumRep;
    SInt16 itlbDateRep;
    SInt16 itlbKeys;
    SInt16 itlbIcon;
} ItlbRecord;

typedef ItlbRecord* ItlbPtr;

// Process Manager additions
typedef struct LaunchParamBlockRec {
    UInt32 reserved1;
    SInt16 reserved2;
    SInt16 launchBlockID;
    UInt32 launchEPBLength;
    SInt16 launchFileFlags;
    OSType launchControlFlags;
    FSSpecPtr launchAppSpec;
    ProcessSerialNumber launchProcessSN;
    UInt32 launchPreferredSize;
    UInt32 launchMinimumSize;
    UInt32 launchAvailableSize;
    void* launchAppParameters;
} LaunchParamBlockRec;

typedef LaunchParamBlockRec* LaunchPBPtr;

// Help Manager additions
typedef SInt16 HMContentType;
typedef struct HMContentRec {
    HMContentType contentType;
    union {
        Handle hmmString;
        SInt16 hmmResID;
        Handle hmmPictHandle;
        Handle hmmTEHandle;
    } u;
} HMContentRec;

// Gestalt Manager additions
typedef OSErr (*GestaltProcPtr)(OSType selector, SInt32* response);

// Notification Manager additions
typedef struct NMProcRec {
    QElemPtr qLink;
    SInt16 qType;
    SInt16 nmFlags;
    SInt32 nmPrivate;
    SInt16 nmReserved;
    SInt16 nmMark;
    Handle nmIcon;
    Handle nmSound;
    StringPtr nmStr;
    void (*nmResp)(NMRecPtr);
    SInt32 nmRefCon;
} NMProcRec;

// Additional common types

typedef SInt16 CharParameter;
// Fixed and FixedPtr already defined earlier
typedef SInt32 Fract;
typedef Fract* FractPtr;
typedef SInt32 TimeValue;
typedef TimeValue CompTimeValue;
typedef struct TimeBaseRec* TimeBase;
typedef SInt32 TimeScale;
typedef SInt64 TimeValue64;
typedef struct TimeRecord {
    CompTimeValue value;
    TimeScale scale;
    TimeBase base;
} TimeRecord;



// Hardware abstraction additions
typedef void* PlatformSoundRef;
typedef void* PlatformDeviceRef;
typedef void* PlatformResourceRef;

// Final missing pieces
typedef struct EventQueueRec {
    QElemPtr qLink;
    SInt16 qType;
    EventRecord evt;
} EventQueueRec;

typedef EventQueueRec* EventQueuePtr;

typedef struct VBLTask {
    QElemPtr qLink;
    SInt16 qType;
    VBLProcPtr vblAddr;
    SInt16 vblCount;
    SInt16 vblPhase;
} VBLTask;



// ============================================================================
// System Init Types
// ============================================================================
typedef SInt32 SystemError;

typedef enum {
    kSystemInitStage_None = 0,
    kSystemInitStage_Bootstrap,
    kSystemInitStage_Memory,
    kSystemInitStage_Core,
    kSystemInitStage_Managers,
    kSystemInitStage_Complete,
    kSystemInitStage_Process,      /* Extended stage for process manager */
    kSystemInitStage_FullComplete  /* Final completion stage */
} SystemInitStage;

typedef struct SystemCapabilities {
    UInt32 processorType;
    UInt32 systemVersion;
    UInt32 totalMemory;
    UInt32 availableMemory;
    Boolean hasColorQD;
    Boolean hasFPU;
    Boolean hasMMU;
    Boolean has32BitMode;
    /* Additional fields */
    UInt32 cpu_type;
    UInt32 ram_size;
    Boolean has_color_quickdraw;
    Boolean has_fpu;
    Boolean has_mmu;
    Boolean has_adb;
    Boolean has_scsi;
    Boolean has_ethernet;
    Boolean has_sound_manager;
    Boolean has_power_manager;
    UInt32 rom_version;
} SystemCapabilities;

typedef struct BootConfiguration {
    UInt32 screenWidth;
    UInt32 screenHeight;
    UInt32 colorDepth;
    void* framebuffer;
    UInt32 totalMemory;
    void* heapStart;
    UInt32 heapSize;
} BootConfiguration;

typedef struct SystemInitCallbacks {
    void (*progressCallback)(const char* stage, UInt32 percent);
    void (*errorCallback)(SystemError error, const char* message);
    void (*debugCallback)(const char* message);
} SystemInitCallbacks;

typedef struct SystemGlobals {
    SystemCapabilities capabilities;
    BootConfiguration bootConfig;
    void* expandMem;
    void* systemHeap;
    void* applZone;
} SystemGlobals;

// ============================================================================
// ExpandMem Types
// ============================================================================
typedef struct ExpandMemRec {
    UInt32 signature;
    UInt32 size;
    void* emKeyboardGlobals;
    void* emAppleTalkInactive;
    void* emResourceDecompressor;
    void* reserved[64];
} ExpandMemRec;

// ============================================================================
// File Manager Types
// ============================================================================
// These types are already defined above with FILE_TYPES_DEFINED guard

// Master Directory Block structure
typedef struct MDB {
    UInt16  drSigWord;      // Volume signature
    UInt32  drCrDate;       // Date and time of volume creation
    UInt32  drLsMod;        // Date and time of last modification
    UInt16  drAtrb;         // Volume attributes
    UInt16  drNmFls;        // Number of files in root directory
    UInt16  drVBMSt;        // First block of volume bitmap
    UInt16  drAllocPtr;     // Start of next allocation search
    UInt16  drNmAlBlks;     // Number of allocation blocks in volume
    UInt32  drAlBlkSiz;     // Size (in bytes) of allocation blocks
    UInt32  drClpSiz;       // Default clump size
    UInt16  drAlBlSt;       // First allocation block in volume
    UInt32  drNxtCNID;      // Next unused catalog node ID
    UInt16  drFreeBks;      // Number of unused allocation blocks
    Str27   drVN;           // Volume name
    UInt32  drVolBkUp;      // Date and time of last backup
    UInt16  drVSeqNum;      // Volume backup sequence number
    UInt32  drWrCnt;        // Volume write count
    UInt32  drXTClpSiz;     // Clump size for extents overflow file
    UInt32  drCTClpSiz;     // Clump size for catalog file
    UInt16  drNmRtDirs;     // Number of directories in root directory
    UInt32  drFilCnt;       // Number of files in volume
    UInt32  drDirCnt;       // Number of directories in volume
} MDB;

typedef struct VCB {
    QElemPtr qLink;
    SInt16 qType;
    SInt16 vcbFlags;
    UInt16 vcbSigWord;
    UInt32 vcbCrDate;
    UInt32 vcbLsMod;
    UInt16 vcbWrCnt;        // Write count
    SInt16 vcbAtrb;
    UInt16 vcbNmFls;
    SInt16 vcbVBMSt;
    SInt16 vcbAllocPtr;
    UInt16 vcbNmAlBlks;
    SInt32 vcbAlBlkSiz;
    SInt32 vcbClpSiz;
    SInt16 vcbAlBlSt;
    UInt32 vcbNxtCNID;
    UInt16 vcbFreeBks;
    Str27 vcbVN;
    SInt16 vcbDrvNum;
    SInt16 vcbDRefNum;
    SInt16 vcbFSID;
    SInt16 vcbVRefNum;
    Ptr vcbMAdr;
    Ptr vcbBufAdr;
    SInt16 vcbMLen;
    SInt16 vcbDirIndex;
    SInt16 vcbDirBlk;
    SInt16 vcbXTRef;        // Extents overflow file reference
    SInt16 vcbCTRef;        // Catalog file reference
    UInt16 vcbXTAlBlks;     // Size of extents overflow file
    UInt16 vcbCTAlBlks;     // Size of catalog file
} VCB;

// Define ExtentDescriptor first
typedef struct ExtentDescriptor {
    UInt16 startBlock;
    UInt16 blockCount;
} ExtentDescriptor;

// Now define ExtentRecord using ExtentDescriptor
typedef struct ExtentRecord {
    ExtentDescriptor extent[3];  // Array of extent descriptors
} ExtentRecord;

typedef struct FCB {
    UInt32 fcbFlNm;
    SInt16 fcbFlags;
    SInt16 fcbTypByt;
    SInt16 fcbSBlk;
    UInt32 fcbEOF;
    UInt32 fcbPLen;
    UInt32 fcbCrPs;
    SInt16 fcbVRefNum;
    SInt16 fcbClpSiz;
    UInt32 fcbBfAdr;
    struct VCB* fcbVPtr;         // Volume control block pointer
    ExtentRecord fcbExtRec;      // Extent record
    ExtentDescriptor extent[3];  // Extents array
} FCB;

// ============================================================================
// Resource Decompression Types
// ============================================================================
typedef struct DonnBitsHeader {
    UInt32 signature;
    UInt32 uncompressedSize;
    UInt32 compressedSize;
    UInt16 flags;
    UInt16 reserved;
} DonnBitsHeader;

typedef struct GreggyBitsHeader {
    UInt32 signature;
    UInt32 uncompressedSize;
    UInt32 compressedSize;
    UInt16 algorithm;
    UInt16 flags;
} GreggyBitsHeader;

typedef struct ExtendedResourceHeader {
    UInt32 signature;
    UInt32 headerSize;
    UInt32 totalSize;
    UInt32 uncompressedSize;
    UInt32 compressedSize;
    UInt16 compressionType;
    UInt16 flags;
    UInt32 checksum;
} ExtendedResourceHeader;

typedef struct DecompressContext {
    void* inputBuffer;
    void* outputBuffer;
    UInt32 inputSize;
    UInt32 outputSize;
    UInt32 inputPos;
    UInt32 outputPos;
    void* workBuffer;
    UInt32 workBufferSize;
    void* privateData;
} DecompressContext;

typedef struct VarTable {
    UInt32 numVars;
    UInt32* values;
    UInt32 maxVars;
} VarTable;

typedef struct DecompressStats {
    UInt32 bytesProcessed;
    UInt32 bytesOutput;
    UInt32 compressionRatio;
    UInt32 elapsedTime;
} DecompressStats;

typedef OSErr (*DecompressProc)(DecompressContext* ctx);

// Str27 already defined above, removing duplicate

// Zone types already defined above, just add missing ZonePtr
typedef Zone* ZonePtr;

// PurgeProc and GrowZoneProc might be missing
#ifndef PURGE_PROC_DEFINED
#define PURGE_PROC_DEFINED
typedef void (*PurgeProc)(Handle h);
typedef void (*GrowZoneProc)(Size cbNeeded);
#endif // PURGE_PROC_DEFINED

typedef enum {
    CPU_68000 = 0,
    CPU_68020 = 1,
    CPU_68030 = 2,
    CPU_68040 = 3,
    CPU_68060 = 4,
    CPU_PPC601 = 5,
    CPU_PPC603 = 6,
    CPU_PPC604 = 7,
    CPU_X86 = 8,
    CPU_X86_64 = 9,
    CPU_ARM = 10,
    CPU_ARM64 = 11
} ProcessorType;

// Control Manager additional types
typedef struct CCTab **CCTabHandle;

typedef struct AuxCtlRec {
    Handle         acNext;       /* Next auxiliary record */
    ControlHandle  acOwner;      /* Control that owns this record */
    CCTabHandle    acCTable;     /* Color table for control */
    SInt16         acFlags;      /* Reserved flags */
    SInt32         acReserved;   /* Reserved for future use */
} AuxCtlRec;

typedef struct AuxCtlRec **AuxCtlHandle;
typedef SInt32 (*ControlDefProcPtr)(SInt16 varCode, ControlHandle theControl, SInt16 message, SInt32 param);
typedef Boolean (*TextValidationProcPtr)(ControlHandle control, Str255 text, SInt32 refCon);

// Event Manager types
typedef struct EventMgrGlobals EventMgrGlobals;

// Menu Manager types
typedef struct MenuManagerState MenuManagerState;

// Dialog Manager types
typedef void (*ResumeProcPtr)(void);
typedef void (*SoundProcPtr)(SInt16 soundNumber);
typedef struct DialogTheme DialogTheme;

// QuickDraw additional types
typedef struct GDevice **GDHandle;
typedef struct QDGlobals QDGlobals;
typedef void (*ColorSearchProcPtr)(RGBColor *rgb, long *position);
typedef void (*ColorComplementProcPtr)(RGBColor *rgb);
typedef struct CSpecArray CSpecArray;
typedef struct ReqListRec ReqListRec;
typedef void (*DeviceLoopDrawingProcPtr)(short depth, short deviceFlags, GDHandle targetDevice, long userData);
typedef struct RegionIterator RegionIterator;
typedef struct RegionScanLine RegionScanLine;
typedef enum { HIT_OUTSIDE = 0, HIT_INSIDE = 1, HIT_ON_BOUNDARY = 2 } HitTestResult;
typedef enum { OP_UNION = 0, OP_INTERSECT = 1, OP_DIFF = 2, OP_XOR = 3 } RegionOperation;

// QuickDraw constants
#define kMinRegionSize 10
#define qdNoError 0

// Additional types found in errors
typedef SInt16 QDErr;
typedef SInt16 RegionError;

// TextEdit types
typedef Handle CharsHandle;
typedef Handle StScrpHandle;
typedef UInt32 TextEncoding;
typedef void* TEIntHook;
typedef void (*ClikLoopProcPtr)(void);
typedef Boolean (*WordBreakProcPtr)(Ptr text, SInt16 charPos);

// Font Manager types
typedef struct FMOutput* FMOutPtr;
typedef struct FMInput FMInput;
typedef struct KernPair KernPair;
typedef struct OpenTypeFont OpenTypeFont;
typedef struct WOFFFont WOFFFont;
typedef struct SystemFont SystemFont;
typedef struct FontCollection FontCollection;
typedef struct ModernFont ModernFont;
typedef struct WebFontMetadata WebFontMetadata;
typedef struct FontRec FontRec;
typedef struct FamRec FamRec;
typedef struct WidthTable WidthTable;
typedef struct FontManagerState FontManagerState;

// List Manager types
typedef Handle ListSearchUPP;

// Process Manager types
typedef UInt32 LaunchFlags;
typedef struct ProcessControlBlock ProcessControlBlock;
typedef UInt32 EventMask;
typedef UInt16 EventKind;
typedef enum { PM_FOREGROUND = 0, PM_BACKGROUND = 1 } ProcessMode;
typedef struct ProcessQueue ProcessQueue;

// Scrap Manager types
typedef OSType ScrapFlavorType;
typedef Handle ScrapRef;
typedef OSErr (*ScrapConverterProc)(Handle srcData, ScrapFlavorType srcType,
                                   Handle *dstData, ScrapFlavorType dstType);
typedef void (*ScrapChangeCallback)(ScrapRef scrap, ScrapFlavorType type);

// Sound Manager types
typedef struct Synthesizer* SynthesizerPtr;
typedef struct SquareWaveSynth SquareWaveSynth;
typedef struct SampledSynth SampledSynth;
typedef struct WaveTableSynth WaveTableSynth;
typedef struct WaveTable WaveTable;
typedef struct MIDISynth MIDISynth;
typedef struct MIDIVoice MIDIVoice;
typedef struct Mixer* MixerPtr;
typedef struct SoundHardware* SoundHardwarePtr;
typedef UInt32 AudioAPIType;
typedef struct AudioDeviceInfo AudioDeviceInfo;
typedef UInt32 AudioEncodingType;

// Additional missing types
typedef struct FontInfo {
    SInt16 ascent;
    SInt16 descent;
    SInt16 widMax;
    SInt16 leading;
} FontInfo;

typedef OSType SFTypeList[4];

// Additional QuickDraw types
typedef const Pattern* ConstPatternParam;
typedef struct Cursor {
    UInt16 data[16];
    UInt16 mask[16];
    Point hotSpot;
} Cursor;
typedef struct PixPat **PixPatHandle;
// KeyMap - use the 16-byte version for compatibility
typedef unsigned char KeyMap[16];

// Mouse tracking types
typedef void (*MouseTrackingCallback)(Point where, void* userData);
typedef struct MouseRegion MouseRegion;

// Resource Manager types
typedef struct OpenResourceFile OpenResourceFile;
typedef struct ResourceDataHeader ResourceDataHeader;
typedef struct ResourceForkHeader ResourceForkHeader;
typedef struct FileControlBlock FileControlBlock;


// OpenResourceFile struct
struct OpenResourceFile {
    int16_t     fileRef;            // File reference number
    Handle      resourceMapHandle;  // Handle to resource map
    uint16_t    fileAttributes;     // File attributes
    int16_t     refNum;             // Reference number
    Handle      mapHandle;          // Map handle
    Handle      dataHandle;         // Handle to resource data
    int16_t     fileAttrs;          // File attributes
    int16_t     vRefNum;            // Volume reference number
    int16_t     version;            // Version
    int8_t      permissionByte;     // Permission byte
    int8_t      reserved;           // Reserved
    Str63       fileName;           // File name
};

// ResourceTypeEntry struct

// Other missing structs

typedef void* DriverPtr;
typedef void* DeviceDriver;
typedef void* EventQueueData;
typedef struct QDGlobals* QDGlobalsPtr;
typedef void* CursPtr;
typedef void* CursHandle;
typedef void* GWorldFlags;
typedef void* QDErr_t;

// Missing function types
typedef void (*ExceptionHandler)(void);
typedef void (*TrapVector)(void);

// Missing constants
#define kSystemBootInProgress 1
#define kSystemInitComplete 2

// Missing Error Manager types
typedef OSErr ErrorCode;
typedef void (*ErrorHandler)(ErrorCode err);

// Multiboot types
typedef struct multiboot_info multiboot_info_t;
typedef struct multiboot_memory_map multiboot_memory_map_t;

// System71 types
typedef struct System71Globals System71Globals;
typedef struct System71Config System71Config;
typedef struct System71ManagerState System71ManagerState;

// Quickdraw missing types
typedef struct QDPicture QDPicture;
typedef QDPicture* QDPicturePtr;
typedef QDPicturePtr* QDPictureHandle;




// Missing struct definitions




// FileMgr BTree types
typedef struct BTHeader {
    UInt16 treeDepth;
    UInt32 rootNode;
    UInt32 leafRecords;
    UInt32 firstLeafNode;
    UInt32 lastLeafNode;
    UInt16 nodeSize;
    UInt16 maxKeyLength;
    UInt32 totalNodes;
    UInt32 freeNodes;
} BTHeader;

typedef struct BTNode {
    UInt16 kind;
    UInt16 numRecords;
    UInt32 fLink;
    UInt32 bLink;
    UInt8 data[1];
} BTNode;

typedef struct BTNodeDescriptor {
    UInt32 fLink;
    UInt32 bLink;
    UInt8 kind;
    UInt8 height;
    UInt16 numRecords;
    UInt16 reserved;
} BTNodeDescriptor;

// HFS+ types
typedef struct HFSPlusForkData {
    UInt64 logicalSize;
    UInt32 clumpSize;
    UInt32 totalBlocks;
} HFSPlusForkData;

typedef struct HFSPlusVolumeHeader {
    UInt16 signature;
    UInt16 version;
    UInt32 attributes;
    UInt32 lastMountedVersion;
    UInt32 journalInfoBlock;
    UInt32 createDate;
    UInt32 modifyDate;
    UInt32 backupDate;
    UInt32 checkedDate;
    UInt32 fileCount;
    UInt32 folderCount;
    UInt32 blockSize;
    UInt32 totalBlocks;
    UInt32 freeBlocks;
} HFSPlusVolumeHeader;

typedef struct HFSPlusExtentRecord {
    UInt32 startBlock;
    UInt32 blockCount;
} HFSPlusExtentRecord;

typedef struct HFSPlusExtentDescriptor {
    HFSPlusExtentRecord extents[8];
} HFSPlusExtentDescriptor;

typedef struct HFSPlusCatalogKey {
    UInt16 keyLength;
    UInt32 parentID;
    UInt16 nodeName[255];
} HFSPlusCatalogKey;

// Volume types
typedef struct VolumeHeader {
    UInt16 signature;
    UInt32 createDate;
    UInt32 modifyDate;
    UInt16 attributes;
    UInt16 nmFls;
    UInt16 vBMSt;
    UInt16 allocPtr;
    UInt16 nmAlBlks;
    UInt32 alBlkSiz;
    UInt32 clpSiz;
    UInt16 alBlSt;
    UInt32 nxtCNID;
    UInt16 freeBks;
    Str27 vN;
} VolumeHeader;

// QuickDraw missing types
typedef struct QDState {
    GrafPtr thePort;
    Pattern grayPattern;
    Pattern blackPattern;
    Pattern whitePattern;
    Cursor arrow;
    BitMap screenBits;
    SInt32 randSeed;
} QDState;

typedef struct QDGlobals {
    void* privates;
    SInt32 randSeed;
    BitMap screenBits;
    Cursor arrow;
    Pattern dkGray;
    Pattern ltGray;
    Pattern gray;
    Pattern black;
    Pattern white;
    GrafPtr thePort;
    SInt32 hiliteRGB;
    void* reserved;
} QDGlobals;

typedef struct GDevice {
    SInt16 gdRefNum;
    SInt16 gdID;
    SInt16 gdType;
    Handle gdITable;
    SInt16 gdResPref;
    Handle gdSearchProc;
    Handle gdCompProc;
    SInt16 gdFlags;
    PixMapHandle gdPMap;
    SInt32 gdRefCon;
    Handle gdNextGD;
    Rect gdRect;
    SInt32 gdMode;
    SInt16 gdCCBytes;
    SInt16 gdCCDepth;
    Handle gdCCXData;
    Handle gdCCXMask;
    SInt32 gdReserved;
} GDevice;

typedef GDevice* GDPtr;
typedef GDPtr* GDHandle;

// Picture opcodes
typedef enum {
    OP_NOP = 0x00,
    OP_CLIP = 0x01,
    OP_BKPAT = 0x02,
    OP_TXFONT = 0x03,
    OP_TXFACE = 0x04,
    OP_TXMODE = 0x05,
    OP_SPEXTRA = 0x06,
    OP_PNSIZE = 0x07,
    OP_PNMODE = 0x08,
    OP_PNPAT = 0x09,
    OP_FILLPAT = 0x0A,
    OP_OVSIZE = 0x0B,
    OP_ORIGIN = 0x0C,
    OP_TXSIZE = 0x0D,
    OP_FGCOLOR = 0x0E,
    OP_BKCOLOR = 0x0F,
    OP_LINE = 0x20,
    OP_LINE_FROM = 0x21,
    OP_SHORT_LINE = 0x22,
    OP_SHORT_LINE_FROM = 0x23,
    OP_LONG_TEXT = 0x28,
    OP_DH_TEXT = 0x29,
    OP_DV_TEXT = 0x2A,
    OP_DHDV_TEXT = 0x2B,
    OP_RECT = 0x30,
    OP_FRAMERECT = 0x30,
    OP_PAINTRECT = 0x31,
    OP_ERASERRECT = 0x32,
    OP_INVERTRECT = 0x33,
    OP_FILLRECT = 0x34,
    OP_FRAMESAMRECT = 0x38,
    OP_PAINTSAMRECT = 0x39,
    OP_ERASESAMERECT = 0x3A,
    OP_INVERTSAMERECT = 0x3B,
    OP_FILLSAMERECT = 0x3C,
    OP_RRECT = 0x40,
    OP_FRAMRRECT = 0x40,
    OP_PAINTRRECT = 0x41,
    OP_ERASERECT = 0x42,
    OP_INVERTRRECT = 0x43,
    OP_FILLRRECT = 0x44,
    OP_FRAMESAMRRECT = 0x48,
    OP_PAINTSAMRRECT = 0x49,
    OP_ERASESAMRRECT = 0x4A,
    OP_INVERTSAMRRECT = 0x4B,
    OP_FILLSAMRRECT = 0x4C,
    OP_OVAL = 0x50,
    OP_FRAMEOVAL = 0x50,
    OP_PAINTOVAL = 0x51,
    OP_ERASEOVAL = 0x52,
    OP_INVERTOVAL = 0x53,
    OP_FILLOVAL = 0x54,
    OP_FRAMESAMEOVAL = 0x58,
    OP_PAINTSAMEOVAL = 0x59,
    OP_ERASESAMEOVAL = 0x5A,
    OP_INVERTSAMEOVAL = 0x5B,
    OP_FILLSAMEOVAL = 0x5C,
    OP_FRAMESAMEARC = 0x68,
    OP_PAINTSAMEAVARC = 0x69,
    OP_ERASESAMEAVARC = 0x6A,
    OP_INVERTSAMEAVARC = 0x6B,
    OP_FILLSAMEAVARC = 0x6C,
    OP_FRAMEPOLY = 0x70,
    OP_PAINTPOLY = 0x71,
    OP_ERASEPOLY = 0x72,
    OP_INVERTPOLY = 0x73,
    OP_FILLPOLY = 0x74,
    OP_FRAMESAMEPOLY = 0x78,
    OP_PAINTSAMEPOLY = 0x79,
    OP_ERASESAMEPOLY = 0x7A,
    OP_INVERTSAMEPOLY = 0x7B,
    OP_FILLSAMEPOLY = 0x7C,
    OP_FRAMERGN = 0x80,
    OP_PAINTRGN = 0x81,
    OP_ERASERGN = 0x82,
    OP_INVERTRGN = 0x83,
    OP_FILLRGN = 0x84,
    OP_FRAMESAMERGN = 0x88,
    OP_PAINTSAMERGN = 0x89,
    OP_ERASESAMERGN = 0x8A,
    OP_INVERTSAMERGN = 0x8B,
    OP_FILLSAMERGN = 0x8C,
    OP_BITSRECT = 0x90,
    OP_BITSRGN = 0x91,
    OP_PACKBITSRECT = 0x98,
    OP_PACKBITSRGN = 0x99,
    OP_OPCOLOR = 0x9A,
    OP_SHORTCOMMENT = 0xA0,
    OP_LONGCOMMENT = 0xA1,
    OP_PICVERSION = 0x11,
    OP_ENDPIC = 0xFF
} PictureOpcode;

// Pattern types
typedef struct PixPat {
    SInt16 patType;
    PixMapHandle patMap;
    Handle patData;
    Handle patXData;
    SInt16 patXValid;
    Handle patXMap;
    Pattern pat1Data;
} PixPat;

typedef PixPat* PixPatPtr;
typedef PixPatPtr* PixPatHandle;

// Polygon types
typedef struct Polygon {
    SInt16 polySize;
    Rect polyBBox;
    Point polyPoints[1];
} Polygon;

// Picture types
typedef struct Picture {
    SInt16 picSize;
    Rect picFrame;
} Picture;

// Additional missing stubs
typedef void* BTNodePtr;
typedef void* BTHeaderPtr;
typedef void* HFSPlusVolumeHeaderPtr;
typedef void* ExtentKeyPtr;
typedef void* CatalogKeyPtr;
typedef void* AttributeKeyPtr;

/* Cursor resource IDs */
#define kArrowCursorID 0
#define kIBeamCursorID 1
#define kCrosshairCursorID 2
#define kWatchCursorID 4

/* Pattern resource IDs */
#define kDesktopPatternID 16
#define kGray25PatternID 17
#define kGray50PatternID 18
#define kGray75PatternID 19
#define kScrollPatternID 20

/* Pattern types */
typedef Pattern* PatHandle;

/* Hit test results */
#define kHitTestHit 1
#define kHitTestMiss 0
#define kHitTestPartial 2

/* Region error codes */
#define kRegionNoError 0
#define kRegionMemoryError -1
#define kRegionInvalidError -2
#define kRegionOverflowError -3
#define kMaxRegionSize 32767

/* Additional missing types */
typedef RgnHandle RgnPtr;

typedef struct PatternList {
    SInt16 count;
    Pattern patterns[32];
} PatternList;

/* Resource data types */
typedef struct ResourceData {
    ResourceType type;
    UInt16 id;
    UInt32 size;
    Ptr data;
} ResourceData;

typedef UInt32 ResourceDataType;

// Fix BTHeader member names
#define bthDepth treeDepth
#define bthRoot rootNode
#define bthNRecs leafRecords
#define bthFNode firstLeafNode
#define bthLNode lastLeafNode
#define bthNodeSize nodeSize
#define bthKeyLen maxKeyLength
#define bthNNodes totalNodes
#define bthFree freeNodes

// Fix BTNode member names
#define ndType kind
#define ndNHeight numRecords
#define ndNRecs numRecords
#define ndData data

// Add missing constants
#define ndHdrNode 1
#define ndIndexNode 0
#define ndLeafNode 255
#define dupFNErr -48
#define rfNumErr -51
#define bdNamErr -37
#define BTREE_NODE_SIZE 512

#define MAX_OPEN_RES_FILES 128
// OpenResourceFile already defined above

/* File seek constants */
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

#endif // SYSTEM_TYPES_H


// Dialog extensions - ensure DialogPeek has all needed fields
#ifndef DIALOG_PEEK_EXTENDED
#define DIALOG_PEEK_EXTENDED

// Forward declare if needed
#ifndef MODAL_FILTER_UPP_DEFINED
#define MODAL_FILTER_UPP_DEFINED
typedef void (*ModalFilterUPP)(DialogPtr dialog, void* event, short* item);
typedef void (*ControlActionUPP)(ControlHandle control, short part);
#endif

typedef struct DialogRecordEx {
    WindowRecord window;
    Handle items;
    TEHandle textH;
    short editField;
    short editOpen;
    short aDefItem;
    // Additional fields for compatibility
    Handle itemList;
    short itemCount;
    ModalFilterUPP filterProc;
    void* refCon;
} DialogRecordEx;

typedef DialogRecordEx* DialogPeekEx;

// Add missing EditionManager types
typedef short SectionType;
typedef short FormatType;
typedef short UpdateMode;

// Add missing keyboard types
typedef struct KeyboardLayoutRec {
    short version;
    short keyMapID;
    Handle keyMapData;
    Str255 layoutName;
} KeyboardLayoutRec;

// Ensure thePort is declared
#ifndef THE_PORT_DECLARED
#define THE_PORT_DECLARED
extern GrafPtr thePort;
#endif

// Additional missing constants only if not defined
#ifndef MENU_BAR_CONSTANTS_DEFINED
#define MENU_BAR_CONSTANTS_DEFINED
enum {
    kMenuBarHeight = 20,
    kTitleBarHeight = 20,
    kScrollBarWidth = 16,
    kGrowBoxSize = 16
};
#endif

// Missing dialog constants
#ifndef DIALOG_ITEM_CONSTANTS_DEFINED
#define DIALOG_ITEM_CONSTANTS_DEFINED
enum {
    ctrlItem = 4,
    btnCtrl = 0,
    chkCtrl = 1,
    radCtrl = 2,
    resCtrl = 3,
    statText = 8,
    editText = 16,
    iconItem = 32,
    picItem = 64,
    userItem = 0,
    itemDisable = 128
};
#endif

// Missing window constants
#ifndef WINDOW_PROC_CONSTANTS_DEFINED
#define WINDOW_PROC_CONSTANTS_DEFINED
enum {
    documentProc = 0,
    dBoxProc = 1,
    plainDBox = 2,
    altDBoxProc = 3,
    noGrowDocProc = 4,
    movableDBoxProc = 5,
    zoomDocProc = 8,
    zoomNoGrow = 12,
    rDocProc = 16
};
#endif

// Missing event constants
#ifndef EVENT_TYPE_CONSTANTS_DEFINED
#define EVENT_TYPE_CONSTANTS_DEFINED
enum {
    nullEvent = 0,
    mouseDown = 1,
    mouseUp = 2,
    keyDown = 3,
    keyUp = 4,
    autoKey = 5,
    updateEvt = 6,
    diskEvt = 7,
    activateEvt = 8,
    osEvt = 15,
    everyEvent = 0xFFFF
};
#endif

// Only define CharsHandle if not already defined
#ifndef CHARS_HANDLE_DEFINED
#define CHARS_HANDLE_DEFINED
typedef Handle CharsHandle;
#endif

// Only define Style if not already defined
#ifndef STYLE_TYPE_DEFINED
#define STYLE_TYPE_DEFINED
typedef unsigned char Style;
#endif

#endif // DIALOG_PEEK_EXTENDED

// Text style constants
#ifndef TEXT_STYLE_CONSTANTS_DEFINED
#define TEXT_STYLE_CONSTANTS_DEFINED
enum {
    normal = 0,
    bold = 1,
    italic = 2,
    underline = 4,
    outline = 8,
    shadow = 16,
    condense = 32,
    extend = 64
};
#endif

// Add missing GrafPort members if needed
#ifndef GRAFPORT_EXTENSIONS_DEFINED
#define GRAFPORT_EXTENSIONS_DEFINED
// Extension structure for missing members
typedef struct {
    short h;
    short v;
} GrafPortExt;
#endif

/* ===== Classic Mac types needed by Time Manager ===== */
#ifndef SYSTEMTYPES_TIME_MANAGER_SHIM
#define SYSTEMTYPES_TIME_MANAGER_SHIM

/* Classic UnsignedWide (hi:lo) */
typedef struct UnsignedWide {
    UInt32 hi;
    UInt32 lo;
} UnsignedWide;

/* Time unit helpers */
#ifndef MICROSECONDS_PER_SECOND
#define MICROSECONDS_PER_SECOND 1000000ULL
#endif
#ifndef NANOSECONDS_PER_SECOND
#define NANOSECONDS_PER_SECOND  1000000000ULL
#endif
#ifndef NANOSECONDS_PER_MICROSECOND
#define NANOSECONDS_PER_MICROSECOND 1000ULL
#endif

#ifndef paramErr
#define paramErr -50
#endif

#endif /* SYSTEMTYPES_TIME_MANAGER_SHIM */

