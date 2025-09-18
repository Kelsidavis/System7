/*
 * Copyright (c) 2024 System7 Project
 * MIT License - See LICENSE file
 */

/*
 * ColorTypes.h - Color Manager Types and Structures
 * System 7.1 color management types
 */

#ifndef COLORTYPES_H
#define COLORTYPES_H

#include <stdint.h>
#include <stdbool.h>
#include "DeskManager/Types.h"

/* RGB Color - 6 bytes, 16-bit components */
typedef struct RGBColor {
    uint16_t red;     /* Red component (0-65535) */
    uint16_t green;   /* Green component (0-65535) */
    uint16_t blue;    /* Blue component (0-65535) */
} RGBColor;

/* Color Specification - 8 bytes */
typedef struct ColorSpec {
    int16_t value;    /* Color table index or special value */
    RGBColor rgb;     /* RGB color value */
} ColorSpec;

/* Color Table - variable size */
typedef struct ColorTable {
    int32_t ctSeed;       /* Color table seed value */
    int16_t ctFlags;      /* Color table flags */
    int16_t ctSize;       /* Number of ColorSpec entries - 1 */
    ColorSpec ctTable[];  /* Array of ColorSpec entries */
} ColorTable;

/* Palette Entry - 8 bytes */
typedef struct PaletteEntry {
    RGBColor rgb;         /* RGB color value */
    uint8_t usage;        /* Usage flags */
    uint8_t tolerance;    /* Tolerance value */
} PaletteEntry;

/* Palette - variable size */
typedef struct Palette {
    int16_t pmEntries;        /* Number of palette entries */
    int16_t pmDataFields;     /* Private data fields */
    PaletteEntry pmInfo[];    /* Array of palette entries */
} Palette;

/* Inverse Color Table - variable size */
typedef struct ITab {
    int32_t iTabSeed;     /* Inverse table seed */
    int16_t iTabRes;      /* Inverse table resolution */
    uint8_t iTTable[];    /* Inverse table data */
} ITab;

/* Graphics Device - variable size */
typedef struct GDevice {
    int16_t gdRefNum;         /* Driver reference number */
    int16_t gdID;             /* Device ID */
    int16_t gdType;           /* Device type */
    ITab** gdITable;          /* Inverse table handle */
    int16_t gdResPref;        /* Resolution preference */
    void* gdSearchProc;       /* Search procedure */
    void* gdCompProc;         /* Complement procedure */
    int16_t gdFlags;          /* Device flags */
    void* gdPMap;             /* Device pixel map */
    int32_t gdRefCon;         /* Reference constant */
    struct GDevice** gdNextGD; /* Next graphics device */
    struct {                  /* Device rectangle */
        int16_t top, left, bottom, right;
    } gdRect;
    int32_t gdMode;           /* Device mode */
    int16_t gdCCBytes;        /* Color correction bytes */
    int16_t gdCCDepth;        /* Color correction depth */
    void** gdCCXData;         /* Color correction data */
    void** gdCCXMask;         /* Color correction mask */
    void** gdExt;             /* Device extension */
} GDevice;

/* Request List Record */
typedef struct ReqListRec {
    int16_t reqLSize;         /* Request list size */
    int16_t reqLData[];       /* Request list data */
} ReqListRec;

/* Handle Types */
typedef ColorTable** CTabHandle;
typedef Palette** PaletteHandle;
typedef ITab** ITabHandle;
typedef GDevice** GDHandle;
typedef struct PixPat** PixPatHandle;

/* Color Manager Constants */
#define pixPurge         0x8000   /* Color table can be purged */
#define noUpdates        0x4000   /* Don't update color table */
#define pixNotPurgeable  0x0000   /* Color table not purgeable */

/* Palette Usage Flags */
#define pmCourteous      0x0000   /* Courteous palette usage */
#define pmDithered       0x0001   /* Use dithering */
#define pmTolerant       0x0002   /* Use tolerance matching */
#define pmAnimated       0x0004   /* Animated palette entries */
#define pmExplicit       0x0008   /* Explicit palette matching */

/* Graphics Device Flags */
#define gdDevType        0x0000   /* Standard device type */
#define burstDevice      0x0001   /* Burst device */
#define ext32Device      0x0002   /* 32-bit clean device */
#define ramInit          0x0004   /* RAM initialization */
#define mainScreen       0x0008   /* Main screen device */
#define allInit          0x0010   /* All initialization complete */
#define screenDevice     0x0020   /* Screen device */
#define noDriver         0x0040   /* No driver present */
#define screenActive     0x0080   /* Screen is active */
#define hiliteBit        0x0080   /* Highlight bit */
#define roundedDevice    0x0020   /* Rounded corners device */
#define hasAuxMenuBar    0x0008   /* Has auxiliary menu bar */

/* Core Color Drawing Functions */
void RGBForeColor(const RGBColor* rgb);
void RGBBackColor(const RGBColor* rgb);
void GetForeColor(RGBColor* rgb);
void GetBackColor(RGBColor* rgb);
void GetCPixel(int16_t h, int16_t v, RGBColor* cPix);
void SetCPixel(int16_t h, int16_t v, const RGBColor* cPix);

/* Color Table Management */
CTabHandle GetCTable(int16_t ctID);
void DisposeCTable(CTabHandle cTabH);
CTabHandle CopyColorTable(CTabHandle srcTab);
void MakeRGBPat(PixPatHandle pp, const RGBColor* myColor);

/* Color Conversion Functions */
void Index2Color(int32_t index, RGBColor* aColor);
int32_t Color2Index(const RGBColor* myColor);
void InvertColor(RGBColor* myColor);
Boolean RealColor(const RGBColor* rgb);
void GetSubTable(CTabHandle myColors, int16_t iTabRes, CTabHandle targetTbl);

/* Inverse Table Functions */
OSErr MakeITable(CTabHandle cTabH, ITabHandle iTabH, int16_t res);

/* Palette Management Functions */
PaletteHandle NewPalette(int16_t entries, CTabHandle srcColors,
                        int16_t srcUsage, int16_t srcTolerance);
PaletteHandle GetNewPalette(int16_t PaletteID);
void DisposePalette(PaletteHandle srcPalette);
void ActivatePalette(WindowPtr srcWindow);
void SetPalette(WindowPtr dstWindow, PaletteHandle srcPalette, Boolean cUpdates);
PaletteHandle GetPalette(WindowPtr srcWindow);
void CopyPalette(PaletteHandle srcPalette, PaletteHandle dstPalette,
                int16_t srcEntry, int16_t dstEntry, int16_t dstLength);

/* Palette Color Functions */
void PmForeColor(int16_t dstEntry);
void PmBackColor(int16_t dstEntry);
void AnimateEntry(WindowPtr dstWindow, int16_t dstEntry, const RGBColor* srcRGB);
void AnimatePalette(WindowPtr dstWindow, CTabHandle srcCTab, int16_t srcIndex,
                   int16_t dstEntry, int16_t dstLength);
void GetEntryColor(PaletteHandle srcPalette, int16_t srcEntry, RGBColor* dstRGB);
void SetEntryColor(PaletteHandle dstPalette, int16_t dstEntry, const RGBColor* srcRGB);
void GetEntryUsage(PaletteHandle srcPalette, int16_t srcEntry,
                  int16_t* dstUsage, int16_t* dstTolerance);
void SetEntryUsage(PaletteHandle dstPalette, int16_t dstEntry,
                  int16_t srcUsage, int16_t srcTolerance);
int32_t Palette2CTab(PaletteHandle srcPalette, CTabHandle dstCTab);
int32_t CTab2Palette(CTabHandle srcCTab, PaletteHandle dstPalette,
                    int16_t srcUsage, int16_t srcTolerance);

/* Extended Color Table Functions */
void ProtectEntry(int16_t index, Boolean protect);
void ReserveEntry(int16_t index, Boolean reserve);
void SetEntries(int16_t start, int16_t count, CTabHandle srcTable);
void RestoreEntries(CTabHandle srcTable, CTabHandle dstTable, ReqListRec* selection);
void SaveEntries(CTabHandle srcTable, CTabHandle resultTable, ReqListRec* selection);

/* Error Handling */
int16_t QDError(void);

/* Color Manager Initialization */
OSErr ColorManager_Init(void);
void ColorManager_Cleanup(void);

/* Global State Access */
RGBColor* ColorManager_GetForeColorPtr(void);
RGBColor* ColorManager_GetBackColorPtr(void);
CTabHandle ColorManager_GetDeviceColors(void);

#endif /* COLORTYPES_H */