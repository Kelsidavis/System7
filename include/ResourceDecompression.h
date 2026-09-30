/*
 * ResourceDecompression.h - Mac OS System 7.1 Resource Decompression Engine
 *
 * Portable C implementation of DonnBits (dcmp 0) and GreggyBits (dcmp 2) algorithms
 *
 * Copyright Notice: This is a reimplementation for research and compatibility purposes.
 */

#ifndef RESOURCE_DECOMPRESSION_H
#define RESOURCE_DECOMPRESSION_H

#include "SystemTypes.h"

#include "SystemTypes.h"

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

#include "SystemTypes.h"
#define GREGGY_HEADER_VERSION   9           /* GreggyBits header version */

#include "SystemTypes.h"

/* Encoding constants for DonnBits */
#define TWO_BYTE_VALUE          128         /* Values < 128 are single byte */
#define FOUR_BYTE_VALUE         255         /* Marker for 32-bit values */
#define MAX_1BYTE_REUSE         40          /* Max variables encodable in 1 byte */

/* Token types for DonnBits compression */

/* Extended operation codes */

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

/* DonnBits compressed resource header (version 8) */

/* GreggyBits compressed resource header (version 9) */

/* Combined header for reading */
typedef union {
    DonnBitsHeader donnBits;
    GreggyBitsHeader greggyBits;
    UInt8 bytes[64];                  /* Raw bytes for I/O */
} ResourceHeader;

/* ---- Variable Table Management (DonnBits) ---------------------------------------- */

/* Variable table entry */

/* Variable table record */

/* ---- Decompression Context ------------------------------------------------------- */

/* Decompression statistics */

/* Decompression context */

/* ---- Main Decompression Functions ------------------------------------------------ */

/* Decompress a resource using automatic algorithm detection */
int DecompressResource(
    const UInt8* compressedData,     /* Compressed data with header */
    size_t compressedSize,              /* Size of compressed data */
    UInt8** decompressedData,        /* Output: decompressed data (allocated) */
    size_t* decompressedSize           /* Output: size of decompressed data */
);

/* Check if data has a valid extended resource header */
Boolean IsExtendedResource(
    const UInt8* data,
    size_t size
);

/* Check if extended resource is compressed */
Boolean IsCompressedResource(
    const ExtendedResourceHeader* header
);

/* Get the decompressed size from header */
size_t GetDecompressedSize(
    const ExtendedResourceHeader* header
);

/* ---- DonnBits Decompression (dcmp 0) --------------------------------------------- */

/* Initialize DonnBits decompressor */
DecompressContext* DonnBits_Init(
    const UInt8* compressedData,
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


/* ---- GreggyBits Decompression (dcmp 2) ------------------------------------------- */

/* Initialize GreggyBits decompressor */
DecompressContext* GreggyBits_Init(
    const UInt8* compressedData,
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


/* ---- Dcmp1 Decompression (dcmp 1 - byte-wise) ------------------------------------ */

/* Initialize Dcmp1 decompressor */
DecompressContext* Dcmp1_Init(
    const UInt8* compressedData,
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


/* ---- Static Byte Tables (GreggyBits) --------------------------------------------- */


/* ---- Decompressor DefProc Support ------------------------------------------------ */

/* Decompressor procedure type */

/* Register a custom decompressor */
int RegisterDecompressor(
    UInt16 defProcID,
    DecompressProc proc
);


/* ---- Utility Functions ----------------------------------------------------------- */


/* Verify decompressed data */
Boolean VerifyDecompression(
    const UInt8* original,
    size_t originalSize,
    const UInt8* decompressed,
    size_t decompressedSize
);


/* ---- Debugging Support ----------------------------------------------------------- */


/* ---- Cache Support --------------------------------------------------------------- */

/* Decompression cache entry */


#ifdef __cplusplus
}
#endif

#endif /* RESOURCE_DECOMPRESSION_H */