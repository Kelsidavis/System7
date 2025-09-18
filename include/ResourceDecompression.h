/*
 * ResourceDecompression.h - Mac OS System 7.1 Resource Decompression Engine
 *
 * Portable C implementation of DonnBits (dcmp 0) and GreggyBits (dcmp 2) algorithms
 * Based on analysis of DeCompressorPatch.a, DeCompressDefProc.a, and GreggyBitsDefProc.a
 *
 * Copyright Notice: This is a reimplementation for research and compatibility purposes.
 */

#ifndef RESOURCE_DECOMPRESSION_H
#define RESOURCE_DECOMPRESSION_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Decompression Constants ----------------------------------------------------- */

/* Decompressor resource type identifier */
#define DECOMPRESS_DEF_TYPE     0x64636D70  /* 'dcmp' */

/* Robustness signature for extended resources */
#define ROBUSTNESS_SIGNATURE    0xA89F6572  /* Unimplemented instruction + 'er' */

/* Header version numbers */
#define DONN_HEADER_VERSION     8           /* DonnBits header version */
#define GREGGY_HEADER_VERSION   9           /* GreggyBits header version */

/* Encoding constants for DonnBits */
#define TWO_BYTE_VALUE          128         /* Values < 128 are single byte */
#define FOUR_BYTE_VALUE         255         /* Marker for 32-bit values */
#define MAX_1BYTE_REUSE         40          /* Max variables encodable in 1 byte */

/* Token types for DonnBits compression */
typedef enum {
    TOKEN_LITERAL           = 0x00,     /* Literal data copy */
    TOKEN_REMEMBER          = 0x10,     /* Remember literal in var table */
    TOKEN_REUSE             = 0x20,     /* Reuse data from var table */
    TOKEN_EXTENDED          = 0xFF      /* Extended operation */
} DonnBitsToken;

/* Extended operation codes */
typedef enum {
    EXT_JUMP_TABLE          = 0,        /* Jump table transformation */
    EXT_ENTRY_VECTOR        = 1,        /* Entry vector transformation */
    EXT_RUN_LENGTH_BYTE     = 2,        /* Run-length encoding by bytes */
    EXT_RUN_LENGTH_WORD     = 3,        /* Run-length encoding by words */
    EXT_DIFF_WORD           = 4,        /* Difference encoding by words */
    EXT_DIFF_ENC_WORD       = 5,        /* Difference with encoded words */
    EXT_DIFF_ENC_LONG       = 6         /* Difference with encoded longs */
} ExtendedOpCode;

/* dcmp 1 specific constants */
#define DCMP1_LITERAL_ENCODED   0x00    /* Literals with encoded values */
#define DCMP1_DEFS_ENCODED      0x10    /* Remember literals with encoded values */
#define DCMP1_VARIABLE_REFS     0x20    /* Variable references start here */
#define DCMP1_CONSTANT_ITEMS    0xD0    /* Constant values start here */

/* GreggyBits flags */
#define GREGGY_BYTE_TABLE_SAVED 0x01    /* Dynamic byte table present */
#define GREGGY_BITMAPPED_DATA   0x02    /* Data includes run-length bitmap */

/* ---- Extended Resource Header Structures ---------------------------------------- */

/* Base extended resource header (all extended resources have this) */
typedef struct {
    uint32_t signature;                 /* Must be ROBUSTNESS_SIGNATURE */
    uint16_t headerLength;              /* Length of header in bytes */
    uint8_t  headerVersion;             /* Version number (8 or 9) */
    uint8_t  extendedAttributes;        /* Extended attributes byte */
    uint32_t actualSize;                /* Uncompressed size */
} ExtendedResourceHeader;

/* DonnBits compressed resource header (version 8) */
typedef struct {
    ExtendedResourceHeader base;        /* Base header */
    uint8_t  varTableRatio;             /* Var table size as 256ths of unpacked size */
    uint8_t  overRun;                   /* Extra bytes needed during decompression */
    uint16_t decompressID;              /* ID of decompression algorithm (0 for default) */
    uint16_t cTableID;                  /* ID of compression table (0 for none) */
} DonnBitsHeader;

/* GreggyBits compressed resource header (version 9) */
typedef struct {
    ExtendedResourceHeader base;        /* Base header */
    uint16_t defProcID;                 /* Decompressor defproc ID */
    uint16_t decompressSlop;            /* Extra space needed for decompression */
    uint8_t  byteTableSize;             /* Size of byte table (in words) */
    uint8_t  compressFlags;             /* Compression flags */
} GreggyBitsHeader;

/* Combined header for reading */
typedef union {
    ExtendedResourceHeader base;
    DonnBitsHeader donnBits;
    GreggyBitsHeader greggyBits;
    uint8_t bytes[64];                  /* Raw bytes for I/O */
} ResourceHeader;

/* ---- Variable Table Management (DonnBits) ---------------------------------------- */

/* Variable table entry */
typedef struct {
    uint16_t offset;                    /* Self-relative offset to string data */
} VarTableEntry;

/* Variable table record */
typedef struct {
    uint16_t nextVarIndex;              /* Index past last variable */
    VarTableEntry* entries;             /* Array of variable entries */
    uint8_t* data;                      /* Variable data storage */
    size_t dataSize;                    /* Size of data area */
    size_t allocSize;                   /* Allocated size */
} VarTable;

/* ---- Decompression Context ------------------------------------------------------- */

/* Decompression statistics */
typedef struct {
    size_t bytesRead;                   /* Bytes read from input */
    size_t bytesWritten;                /* Bytes written to output */
    size_t varsStored;                  /* Variables stored in table */
    size_t varsReused;                  /* Variables reused from table */
    uint32_t checksum;                  /* Running checksum */
} DecompressStats;

/* Decompression context */
typedef struct {
    const uint8_t* input;               /* Input buffer */
    size_t inputSize;                   /* Input buffer size */
    size_t inputPos;                    /* Current input position */

    uint8_t* output;                    /* Output buffer */
    size_t outputSize;                  /* Output buffer size */
    size_t outputPos;                   /* Current output position */

    VarTable* varTable;                 /* Variable table (DonnBits) */
    uint16_t* byteTable;                /* Byte expansion table (GreggyBits) */

    ResourceHeader header;              /* Resource header */
    DecompressStats stats;              /* Decompression statistics */

    int lastError;                      /* Last error code */
    char errorMsg[256];                 /* Error message buffer */
} DecompressContext;

/* ---- Main Decompression Functions ------------------------------------------------ */

/* Decompress a resource using automatic algorithm detection */
int DecompressResource(
    const uint8_t* compressedData,     /* Compressed data with header */
    size_t compressedSize,              /* Size of compressed data */
    uint8_t** decompressedData,        /* Output: decompressed data (allocated) */
    size_t* decompressedSize           /* Output: size of decompressed data */
);

/* Check if data has a valid extended resource header */
bool IsExtendedResource(
    const uint8_t* data,
    size_t size
);

/* Check if extended resource is compressed */
bool IsCompressedResource(
    const ExtendedResourceHeader* header
);

/* Get the decompressed size from header */
size_t GetDecompressedSize(
    const ExtendedResourceHeader* header
);

/* ---- DonnBits Decompression (dcmp 0) --------------------------------------------- */

/* Initialize DonnBits decompressor */
DecompressContext* DonnBits_Init(
    const uint8_t* compressedData,
    size_t compressedSize,
    size_t decompressedSize
);

/* Perform DonnBits decompression */
int DonnBits_Decompress(
    DecompressContext* ctx
);

/* Clean up DonnBits decompressor */
void DonnBits_Cleanup(
    DecompressContext* ctx
);

/* DonnBits helper functions */
uint32_t DonnBits_GetEncodedValue(DecompressContext* ctx);
int DonnBits_CopyLiteral(DecompressContext* ctx, size_t length);
int DonnBits_RememberLiteral(DecompressContext* ctx, size_t length);
int DonnBits_ReuseLiteral(DecompressContext* ctx, size_t index);
int DonnBits_HandleExtended(DecompressContext* ctx);

/* ---- GreggyBits Decompression (dcmp 2) ------------------------------------------- */

/* Initialize GreggyBits decompressor */
DecompressContext* GreggyBits_Init(
    const uint8_t* compressedData,
    size_t compressedSize,
    size_t decompressedSize
);

/* Perform GreggyBits decompression */
int GreggyBits_Decompress(
    DecompressContext* ctx
);

/* Clean up GreggyBits decompressor */
void GreggyBits_Cleanup(
    DecompressContext* ctx
);

/* GreggyBits helper functions */
int GreggyBits_LoadByteTable(DecompressContext* ctx);
int GreggyBits_ExpandBytes(DecompressContext* ctx);
int GreggyBits_ProcessBitmap(DecompressContext* ctx);

/* ---- Dcmp1 Decompression (dcmp 1 - byte-wise) ------------------------------------ */

/* Initialize Dcmp1 decompressor */
DecompressContext* Dcmp1_Init(
    const uint8_t* compressedData,
    size_t compressedSize,
    size_t decompressedSize
);

/* Perform Dcmp1 decompression */
int Dcmp1_Decompress(
    DecompressContext* ctx
);

/* Clean up Dcmp1 decompressor */
void Dcmp1_Cleanup(
    DecompressContext* ctx
);

/* ---- Variable Table Functions (DonnBits) ----------------------------------------- */

/* Create a variable table */
VarTable* VarTable_Create(size_t ratio, size_t unpackedSize);

/* Initialize variable table */
void VarTable_Init(VarTable* table);

/* Store data in variable table */
int VarTable_Remember(VarTable* table, const uint8_t* data, size_t length);

/* Retrieve data from variable table */
int VarTable_Fetch(VarTable* table, size_t index, uint8_t** data, size_t* length);

/* Free variable table */
void VarTable_Free(VarTable* table);

/* ---- Static Byte Tables (GreggyBits) --------------------------------------------- */

/* Get static byte expansion table */
const uint16_t* GreggyBits_GetStaticTable(void);

/* ---- Decompressor DefProc Support ------------------------------------------------ */

/* Decompressor procedure type */
typedef int (*DecompressProc)(
    const uint8_t* src,
    uint8_t* dest,
    const ResourceHeader* header
);

/* Register a custom decompressor */
int RegisterDecompressor(
    uint16_t defProcID,
    DecompressProc proc
);

/* Get decompressor for ID */
DecompressProc GetDecompressor(uint16_t defProcID);

/* ---- Utility Functions ----------------------------------------------------------- */

/* Calculate checksum of data */
uint32_t CalculateChecksum(const uint8_t* data, size_t size);

/* Verify decompressed data */
bool VerifyDecompression(
    const uint8_t* original,
    size_t originalSize,
    const uint8_t* decompressed,
    size_t decompressedSize
);

/* Get error message for error code */
const char* GetDecompressErrorString(int error);

/* ---- Debugging Support ----------------------------------------------------------- */

/* Enable/disable debug output */
void SetDecompressDebug(bool enable);

/* Dump resource header */
void DumpResourceHeader(const ResourceHeader* header);

/* Dump variable table */
void DumpVarTable(const VarTable* table);

/* Dump decompression statistics */
void DumpDecompressStats(const DecompressStats* stats);

/* ---- Cache Support --------------------------------------------------------------- */

#include <time.h>

/* Decompression cache entry */
typedef struct DecompressCache {
    uint32_t signature;                 /* Resource signature/checksum */
    uint8_t* data;                      /* Decompressed data */
    size_t size;                        /* Data size */
    time_t timestamp;                   /* Cache timestamp */
    struct DecompressCache* next;       /* Next cache entry */
} DecompressCache;

/* Enable/disable decompression caching */
void SetDecompressCaching(bool enable);

/* Clear decompression cache */
void ClearDecompressCache(void);

/* Get cache statistics */
void GetDecompressCacheStats(size_t* entries, size_t* totalSize, size_t* hits, size_t* misses);

#ifdef __cplusplus
}
#endif

#endif /* RESOURCE_DECOMPRESSION_H */