/*
 * TrueTypeFonts.h - TrueType Font Support APIs
 *
 * Support for TrueType outline fonts including parsing, scaling,
 * rasterization, and hinting.
 */

#ifndef TRUETYPE_FONTS_H
#define TRUETYPE_FONTS_H

#include "FontTypes.h"
#include <Types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* TrueType Constants */
enum {
    kSFNTResourceType = 'sfnt',
    kTrueTypeTag = 0x00010000,
    kOpenTypeTag = 'OTTO',
    kTTCFTag = 'ttcf'
};

/* TrueType Table Tags */
enum {
    kCmapTableTag = 'cmap',
    kGlyfTableTag = 'glyf',
    kHeadTableTag = 'head',
    kHheaTableTag = 'hhea',
    kHmtxTableTag = 'hmtx',
    kLocaTableTag = 'loca',
    kMaxpTableTag = 'maxp',
    kNameTableTag = 'name',
    kPostTableTag = 'post',
    kOs2TableTag = 'OS/2',
    kCvtTableTag = 'cvt ',
    kFpgmTableTag = 'fpgm',
    kPrepTableTag = 'prep',
    kGaspTableTag = 'gasp'
};

/* TrueType Structures */
typedef struct TTHeader {
    Fixed version;              /* Font version */
    Fixed fontRevision;         /* Font revision */
    unsigned long checksumAdjustment; /* Checksum adjustment */
    unsigned long magicNumber;  /* Magic number (0x5F0F3CF5) */
    unsigned short flags;       /* Flags */
    unsigned short unitsPerEm;  /* Units per em */
    long created[2];           /* Creation time */
    long modified[2];          /* Modification time */
    short xMin, yMin;          /* Bounding box */
    short xMax, yMax;
    unsigned short macStyle;    /* Mac style */
    unsigned short lowestRecPPEM; /* Smallest readable size */
    short fontDirectionHint;   /* Font direction hint */
    short indexToLocFormat;    /* Index to location format */
    short glyphDataFormat;     /* Glyph data format */
} TTHeader;

typedef struct TTHorizontalHeader {
    Fixed version;             /* Version */
    short ascender;            /* Ascender */
    short descender;           /* Descender */
    short lineGap;             /* Line gap */
    unsigned short advanceWidthMax; /* Maximum advance width */
    short minLeftSideBearing;  /* Minimum left side bearing */
    short minRightSideBearing; /* Minimum right side bearing */
    short xMaxExtent;          /* Maximum X extent */
    short caretSlopeRise;      /* Caret slope rise */
    short caretSlopeRun;       /* Caret slope run */
    short caretOffset;         /* Caret offset */
    short reserved[4];         /* Reserved */
    short metricDataFormat;    /* Metric data format */
    unsigned short numLongHorMetrics; /* Number of horizontal metrics */
} TTHorizontalHeader;

typedef struct TTMaxProfile {
    Fixed version;             /* Version */
    unsigned short numGlyphs;  /* Number of glyphs */
    unsigned short maxPoints;  /* Maximum points in simple glyph */
    unsigned short maxContours; /* Maximum contours in simple glyph */
    unsigned short maxComponentPoints; /* Maximum points in composite */
    unsigned short maxComponentContours; /* Maximum contours in composite */
    unsigned short maxZones;   /* Maximum zones */
    unsigned short maxTwilightPoints; /* Maximum twilight points */
    unsigned short maxStorage; /* Maximum storage locations */
    unsigned short maxFunctionDefs; /* Maximum function definitions */
    unsigned short maxInstructionDefs; /* Maximum instruction definitions */
    unsigned short maxStackElements; /* Maximum stack elements */
    unsigned short maxSizeOfInstructions; /* Maximum instruction size */
    unsigned short maxComponentElements; /* Maximum component elements */
    unsigned short maxComponentDepth; /* Maximum component depth */
} TTMaxProfile;

typedef struct TTLongHorMetric {
    unsigned short advanceWidth; /* Advance width */
    short leftSideBearing;      /* Left side bearing */
} TTLongHorMetric;

typedef struct TTGlyphHeader {
    short numberOfContours;     /* Number of contours */
    short xMin, yMin;          /* Bounding box */
    short xMax, yMax;
} TTGlyphHeader;

typedef struct TTFont {
    Handle sfntData;           /* SFNT resource data */
    long sfntSize;             /* Size of SFNT data */
    TTHeader *head;            /* Head table */
    TTHorizontalHeader *hhea;  /* Horizontal header */
    TTMaxProfile *maxp;        /* Maximum profile */
    void *cmap;                /* Character map */
    void *glyf;                /* Glyph data */
    void *loca;                /* Location table */
    void *hmtx;                /* Horizontal metrics */
    void *name;                /* Name table */
    void *post;                /* PostScript table */
    short numTables;           /* Number of tables */
    void **tables;             /* Table pointers */
    unsigned long *tableTags;  /* Table tags */
    unsigned long *tableChecksums; /* Table checksums */
    Boolean valid;             /* Font is valid */
} TTFont;

typedef struct TTGlyph {
    unsigned short glyphIndex;  /* Glyph index */
    TTGlyphHeader header;      /* Glyph header */
    void *data;                /* Glyph data */
    long dataSize;             /* Data size */
    Point *points;             /* Glyph points */
    unsigned char *flags;      /* Point flags */
    unsigned short *contourEnds; /* Contour end points */
    short numPoints;           /* Number of points */
    short numContours;         /* Number of contours */
    Boolean isComposite;       /* Is composite glyph */
} TTGlyph;

/* TrueType Font Loading */
OSErr LoadTrueTypeFont(short fontID, TTFont **font);
OSErr LoadTrueTypeFontFromResource(Handle sfntResource, TTFont **font);
OSErr LoadTrueTypeFontFromFile(ConstStr255Param filePath, TTFont **font);
OSErr UnloadTrueTypeFont(TTFont *font);

/* SFNT Resource Parsing */
OSErr ParseSFNTResource(Handle resource, TTFont **font);
OSErr ValidateSFNTResource(Handle resource);
OSErr GetSFNTTableDirectory(TTFont *font, unsigned long **tags, short *count);
OSErr GetSFNTTable(TTFont *font, unsigned long tag, void **table, long *size);

/* TrueType Table Access */
OSErr GetHeadTable(TTFont *font, TTHeader **head);
OSErr GetHorizontalHeaderTable(TTFont *font, TTHorizontalHeader **hhea);
OSErr GetMaxProfileTable(TTFont *font, TTMaxProfile **maxp);
OSErr GetCharacterMapTable(TTFont *font, void **cmap, long *size);
OSErr GetGlyphDataTable(TTFont *font, void **glyf, long *size);
OSErr GetLocationTable(TTFont *font, void **loca, long *size);
OSErr GetHorizontalMetricsTable(TTFont *font, void **hmtx, long *size);

/* Character Mapping */
OSErr MapCharacterToGlyph(TTFont *font, unsigned short character, unsigned short *glyphIndex);
OSErr MapGlyphToCharacter(TTFont *font, unsigned short glyphIndex, unsigned short *character);
OSErr GetCharacterEncoding(TTFont *font, short *platformID, short *encodingID);
OSErr GetSupportedCharacters(TTFont *font, unsigned short **characters, short *count);

/* Glyph Access */
OSErr GetGlyph(TTFont *font, unsigned short glyphIndex, TTGlyph **glyph);
OSErr GetGlyphMetrics(TTFont *font, unsigned short glyphIndex,
                      short *advanceWidth, short *leftSideBearing);
OSErr GetGlyphBounds(TTFont *font, unsigned short glyphIndex, Rect *bounds);
OSErr UnloadGlyph(TTGlyph *glyph);

/* Glyph Outline Processing */
OSErr GetGlyphOutline(TTFont *font, unsigned short glyphIndex,
                      Point **points, unsigned char **flags, unsigned short **contours,
                      short *numPoints, short *numContours);
OSErr ScaleGlyphOutline(Point *points, short numPoints, Fixed scaleX, Fixed scaleY);
OSErr TransformGlyphOutline(Point *points, short numPoints, Fixed matrix[4]);

/* Font Scaling and Rasterization */
OSErr ScaleTrueTypeFont(TTFont *font, short pointSize, short dpi, TTFont **scaledFont);
OSErr RasterizeGlyph(TTFont *font, unsigned short glyphIndex, short pointSize, short dpi,
                     void **bitmap, short *width, short *height, short *rowBytes);
OSErr RasterizeCharacter(TTFont *font, unsigned short character, short pointSize, short dpi,
                        void **bitmap, short *width, short *height, short *rowBytes);

/* Font Hinting */
OSErr ExecuteGlyphInstructions(TTFont *font, TTGlyph *glyph, short pointSize, short dpi);
OSErr GetInstructionTables(TTFont *font, void **cvt, void **fpgm, void **prep,
                          long *cvtSize, long *fpgmSize, long *prepSize);
OSErr InitializeTrueTypeInterpreter(TTFont *font);
OSErr ExecuteFontProgram(TTFont *font);
OSErr ExecuteControlValueProgram(TTFont *font);

/* Font Metrics */
OSErr GetTrueTypeFontMetrics(TTFont *font, FMetricRec *metrics);
OSErr GetTrueTypeFontBounds(TTFont *font, Rect *bounds);
OSErr GetStringMetrics(TTFont *font, const unsigned short *text, short length,
                       short pointSize, short *width, short *height, Rect *bounds);

/* Font Information */
OSErr GetTrueTypeFontName(TTFont *font, short nameID, Str255 name);
OSErr GetTrueTypeFontFamily(TTFont *font, Str255 familyName);
OSErr GetTrueTypeFontStyle(TTFont *font, Str255 styleName);
OSErr GetTrueTypeFontVersion(TTFont *font, Fixed *version);
OSErr GetTrueTypeFontUnitsPerEm(TTFont *font, unsigned short *unitsPerEm);

/* Composite Glyph Support */
OSErr IsCompositeGlyph(TTFont *font, unsigned short glyphIndex, Boolean *isComposite);
OSErr GetCompositeComponents(TTFont *font, unsigned short glyphIndex,
                            unsigned short **components, short *count);
OSErr DecomposeGlyph(TTFont *font, unsigned short glyphIndex, TTGlyph **decomposedGlyph);

/* Font Validation */
OSErr ValidateTrueTypeFont(TTFont *font);
OSErr CheckTrueTypeFontIntegrity(TTFont *font, Boolean *isValid, char **errorMessage);
OSErr ValidateTrueTypeTable(void *table, unsigned long tag, long size);
OSErr CalculateTableChecksum(void *table, long size, unsigned long *checksum);

/* Font Conversion */
OSErr ConvertTrueTypeToBitmap(TTFont *font, short pointSize, short dpi,
                             BitmapFontData **bitmapFont);
OSErr ExtractBitmapFromTrueType(TTFont *font, short pointSize, short dpi,
                               short firstChar, short lastChar, BitmapFontData **bitmapFont);

/* Advanced Features */
OSErr GetKerningData(TTFont *font, KernPair **pairs, short *count);
OSErr GetGlyphClass(TTFont *font, unsigned short glyphIndex, short *glyphClass);
OSErr GetLanguageSupport(TTFont *font, short **languageCodes, short *count);

/* Memory Management */
OSErr AllocateTrueTypeFont(TTFont **font);
OSErr DisposeTrueTypeFont(TTFont *font);
OSErr CompactTrueTypeFont(TTFont *font);

/* Platform Integration */
OSErr LoadSystemTrueTypeFont(ConstStr255Param fontName, TTFont **font);
OSErr GetSystemTrueTypeFonts(Str255 **fontNames, short *count);
OSErr RegisterTrueTypeFontFile(ConstStr255Param filePath, short *fontID);

#ifdef __cplusplus
}
#endif

#endif /* TRUETYPE_FONTS_H */