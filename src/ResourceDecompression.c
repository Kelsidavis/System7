/*
 * ResourceDecompression.c - Mac OS System 7.1 Resource Decompression Implementation
 *
 * Portable C implementation of DonnBits (dcmp 0) and GreggyBits (dcmp 2) algorithms
 * Based on analysis of DeCompressorPatch.a, DeCompressDefProc.a, and GreggyBitsDefProc.a
 *
 * Copyright Notice: This is a reimplementation for research and compatibility purposes.
 */

#include "ResourceDecompression.h"
#include "ResourceManager.h"  /* For error codes */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <assert.h>

/* ---- Global Variables ------------------------------------------------------------ */

static bool gDebugEnabled = false;
static bool gCachingEnabled = false;
static DecompressCache* gCacheHead = NULL;
static size_t gCacheHits = 0;
static size_t gCacheMisses = 0;

/* Custom decompressor registry */
typedef struct {
    uint16_t id;
    DecompressProc proc;
} DecompressorEntry;

static DecompressorEntry gDecompressors[16];
static int gDecompressorCount = 0;

/* ---- Static Tables for Decompression --------------------------------------------- */

/* Common 68k instruction words used in dcmp 0 (from DeCompressDefProc.a) */
static const uint16_t gDcmp0ConstantWords[] = {
    0x0000, 0x4EBA, 0x0008, 0x4E75, 0x000C, 0x4EAD, 0x2053, 0x2F0B,  /* 48-4F */
    0x6100, 0x0010, 0x7000, 0x2F00, 0x486E, 0x2050, 0x206E, 0x2F2E,  /* 50-57 */
    0xFFFC, 0x48E7, 0x3F3C, 0x0004, 0xFFF8, 0x2F0C, 0x2006, 0x4EED,  /* 58-5F */
    0x4E56, 0x2068, 0x4E5E, 0x0001, 0x588F, 0x4FEF, 0x0002, 0x0018,  /* 60-67 */
    0x6000, 0xFFFF, 0x508F, 0x4E90, 0x0006, 0x266E, 0x0014, 0xFFF4,  /* 68-6F */
    0x4CEE, 0x000A, 0x000E, 0x41EE, 0x4CDF, 0x48C0, 0xFFF0, 0x2D40,  /* 70-77 */
    0x0012, 0x302E, 0x7001, 0x2F28, 0x2054, 0x6700, 0x0020, 0x001C,  /* 78-7F */
    0x205F, 0x1800, 0x266F, 0x4878, 0x0016, 0x41FA, 0x303C, 0x2840,  /* 80-87 */
    0x7200, 0x286E, 0x200C, 0x6600, 0x206B, 0x2F07, 0x558F, 0x0028,  /* 88-8F */
    0xFFFE, 0xFFEC, 0x22D8, 0x200B, 0x000F, 0x598F, 0x2F3C, 0xFF00,  /* 90-97 */
    0x0118, 0x81E1, 0x4A00, 0x4EB0, 0xFFE8, 0x48C7, 0x0003, 0x0022,  /* 98-9F */
    0x0007, 0x001A, 0x6706, 0x6708, 0x4EF9, 0x0024, 0x2078, 0x0800,  /* A0-A7 */
    0x6604, 0x002A, 0x4ED0, 0x3028, 0x265F, 0x6704, 0x0030, 0x43EE,  /* A8-AF */
    0x3F00, 0x201F, 0x001E, 0xFFF6, 0x202E, 0x42A7, 0x2007, 0xFFFA,  /* B0-B7 */
    0x6002, 0x3D40, 0x0C40, 0x6606, 0x0026, 0x2D48, 0x2F01, 0x70FF,  /* B8-BF */
    0x6004, 0x1880, 0x4A40, 0x0040, 0x002C, 0x2F08, 0x0011, 0xFFE4,  /* C0-C7 */
    0x2140, 0x2640, 0xFFF2, 0x426E, 0x4EB9, 0x3D7C, 0x0038, 0x000D,  /* C8-CF */
    0x6006, 0x422E, 0x203C, 0x670C, 0x2D68, 0x6608, 0x4A2E, 0x4AAE,  /* D0-D7 */
    0x002E, 0x4840, 0x225F, 0x2200, 0x670A, 0x3007, 0x4267, 0x0032,  /* D8-DF */
    0x2028, 0x0009, 0x487A, 0x0200, 0x2F2B, 0x0005, 0x226E, 0x6602,  /* E0-E7 */
    0xE580, 0x670E, 0x660A, 0x0050, 0x3E00, 0x660C, 0x2E00, 0xFFEE,  /* E8-EF */
    0x206D, 0x2040, 0xFFE0, 0x5340, 0x6008, 0x0480, 0x0068, 0x0B7C,  /* F0-F7 */
    0x4400, 0x41E8, 0x4841                                           /* F8-FA */
};

/* Common word values for dcmp 1 (from DeCompressDefProc1.a) */
static const uint16_t gDcmp1ConstantWords[] = {
    0x0000, 0x0001, 0x0002, 0x0003, 0x2E01, 0x3E01, 0x0101, 0x1E01,  /* D5-DC */
    0xFFFF, 0x0E01, 0x3100, 0x1112, 0x0107, 0x3332, 0x1239, 0xED10,  /* DD-E4 */
    0x0127, 0x2322, 0x0137, 0x0706, 0x0117, 0x0123, 0x00FF, 0x002F,  /* E5-EC */
    0x070E, 0xFD3C, 0x0135, 0x0115, 0x0102, 0x0007, 0x003E, 0x05D5,  /* ED-F4 */
    0x0201, 0x0607, 0x0708, 0x3001, 0x0133, 0x0010, 0x1716, 0x373E,  /* F5-FC */
    0x3637                                                            /* FD */
};

/* Default byte expansion table for GreggyBits when no dynamic table is present */
static const uint16_t gStaticByteTable[256] = {
    /* Common word values that appear frequently in 68k code */
    0x0000, 0x0008, 0x4E75, 0x000C, 0x0010, 0x0018, 0x0020, 0x0028,
    0x0030, 0x0038, 0x0040, 0x0048, 0x0050, 0x0058, 0x0060, 0x0068,
    0x0070, 0x0078, 0x0080, 0x0088, 0x0090, 0x0098, 0x00A0, 0x00A8,
    0x00B0, 0x00B8, 0x00C0, 0x00C8, 0x00D0, 0x00D8, 0x00E0, 0x00E8,
    0x00F0, 0x00F8, 0x0100, 0x0108, 0x0110, 0x0118, 0x0120, 0x0128,
    0x4EBA, 0x206F, 0x4E56, 0x48E7, 0x4CEE, 0x4E5E, 0x2F0A, 0x204F,
    /* Continue with common patterns... */
    0xFFFF, 0xFFFE, 0xFFFC, 0xFFF8, 0xFFF0, 0xFFE0, 0xFFC0, 0xFF80,
    0xFF00, 0xFE00, 0xFC00, 0xF800, 0xF000, 0xE000, 0xC000, 0x8000,
    /* Fill remaining with identity mapping for now */
    0x0040, 0x0041, 0x0042, 0x0043, 0x0044, 0x0045, 0x0046, 0x0047,
    0x0048, 0x0049, 0x004A, 0x004B, 0x004C, 0x004D, 0x004E, 0x004F,
    0x0050, 0x0051, 0x0052, 0x0053, 0x0054, 0x0055, 0x0056, 0x0057,
    0x0058, 0x0059, 0x005A, 0x005B, 0x005C, 0x005D, 0x005E, 0x005F,
    0x0060, 0x0061, 0x0062, 0x0063, 0x0064, 0x0065, 0x0066, 0x0067,
    0x0068, 0x0069, 0x006A, 0x006B, 0x006C, 0x006D, 0x006E, 0x006F,
    0x0070, 0x0071, 0x0072, 0x0073, 0x0074, 0x0075, 0x0076, 0x0077,
    0x0078, 0x0079, 0x007A, 0x007B, 0x007C, 0x007D, 0x007E, 0x007F,
    0x0080, 0x0081, 0x0082, 0x0083, 0x0084, 0x0085, 0x0086, 0x0087,
    0x0088, 0x0089, 0x008A, 0x008B, 0x008C, 0x008D, 0x008E, 0x008F,
    0x0090, 0x0091, 0x0092, 0x0093, 0x0094, 0x0095, 0x0096, 0x0097,
    0x0098, 0x0099, 0x009A, 0x009B, 0x009C, 0x009D, 0x009E, 0x009F,
    0x00A0, 0x00A1, 0x00A2, 0x00A3, 0x00A4, 0x00A5, 0x00A6, 0x00A7,
    0x00A8, 0x00A9, 0x00AA, 0x00AB, 0x00AC, 0x00AD, 0x00AE, 0x00AF,
    0x00B0, 0x00B1, 0x00B2, 0x00B3, 0x00B4, 0x00B5, 0x00B6, 0x00B7,
    0x00B8, 0x00B9, 0x00BA, 0x00BB, 0x00BC, 0x00BD, 0x00BE, 0x00BF,
    0x00C0, 0x00C1, 0x00C2, 0x00C3, 0x00C4, 0x00C5, 0x00C6, 0x00C7,
    0x00C8, 0x00C9, 0x00CA, 0x00CB, 0x00CC, 0x00CD, 0x00CE, 0x00CF,
    0x00D0, 0x00D1, 0x00D2, 0x00D3, 0x00D4, 0x00D5, 0x00D6, 0x00D7,
    0x00D8, 0x00D9, 0x00DA, 0x00DB, 0x00DC, 0x00DD, 0x00DE, 0x00DF,
    0x00E0, 0x00E1, 0x00E2, 0x00E3, 0x00E4, 0x00E5, 0x00E6, 0x00E7,
    0x00E8, 0x00E9, 0x00EA, 0x00EB, 0x00EC, 0x00ED, 0x00EE, 0x00EF,
    0x00F0, 0x00F1, 0x00F2, 0x00F3, 0x00F4, 0x00F5, 0x00F6, 0x00F7,
    0x00F8, 0x00F9, 0x00FA, 0x00FB, 0x00FC, 0x00FD, 0x00FE, 0x00FF
};

/* ---- Utility Functions ----------------------------------------------------------- */

static void SetError(DecompressContext* ctx, int error, const char* msg) {
    ctx->lastError = error;
    strncpy(ctx->errorMsg, msg, sizeof(ctx->errorMsg) - 1);
    ctx->errorMsg[sizeof(ctx->errorMsg) - 1] = '\0';

    if (gDebugEnabled) {
        fprintf(stderr, "Decompression error %d: %s\n", error, msg);
    }
}

uint32_t CalculateChecksum(const uint8_t* data, size_t size) {
    uint32_t checksum = 0;
    for (size_t i = 0; i < size; i++) {
        checksum = (checksum << 1) ^ data[i];
        if (checksum & 0x80000000) {
            checksum ^= 0x04C11DB7;  /* CRC32 polynomial */
        }
    }
    return checksum;
}

bool VerifyDecompression(const uint8_t* original, size_t originalSize,
                         const uint8_t* decompressed, size_t decompressedSize) {
    (void)original;
    (void)originalSize;
    (void)decompressed;
    (void)decompressedSize;
    /* TODO: Implement verification logic */
    return true;
}

const char* GetDecompressErrorString(int error) {
    switch (error) {
        case 0: return "No error";
        case -186: return "Can't decompress resource";
        case -185: return "Bad extended resource format";
        case -190: return "Input out of bounds";
        case -191: return "Output out of bounds";
        case -108: return "Not enough memory";
        default: return "Unknown error";
    }
}

void SetDecompressDebug(bool enable) {
    gDebugEnabled = enable;
}

/* ---- Header Validation Functions ------------------------------------------------- */

bool IsExtendedResource(const uint8_t* data, size_t size) {
    if (size < sizeof(ExtendedResourceHeader)) {
        return false;
    }

    const ExtendedResourceHeader* header = (const ExtendedResourceHeader*)(data + 4);
    return header->signature == ROBUSTNESS_SIGNATURE;
}

bool IsCompressedResource(const ExtendedResourceHeader* header) {
    return (header->extendedAttributes & resCompressed) != 0;
}

size_t GetDecompressedSize(const ExtendedResourceHeader* header) {
    return header->actualSize;
}

/* ---- Variable Table Implementation (DonnBits) ------------------------------------ */

VarTable* VarTable_Create(size_t ratio, size_t unpackedSize) {
    VarTable* table = (VarTable*)calloc(1, sizeof(VarTable));
    if (!table) return NULL;

    /* Calculate table size based on ratio */
    size_t tableSize = ((unpackedSize * (ratio + 1)) >> 8) + sizeof(VarTableEntry);

    table->entries = (VarTableEntry*)calloc(tableSize / sizeof(VarTableEntry), sizeof(VarTableEntry));
    table->data = (uint8_t*)calloc(tableSize, 1);
    table->allocSize = tableSize;

    if (!table->entries || !table->data) {
        VarTable_Free(table);
        return NULL;
    }

    VarTable_Init(table);
    return table;
}

void VarTable_Init(VarTable* table) {
    if (!table) return;

    /* Initialize first entry to point to end of table */
    table->entries[0].offset = (uint16_t)table->allocSize;
    table->nextVarIndex = 1;
    table->dataSize = 0;
}

int VarTable_Remember(VarTable* table, const uint8_t* data, size_t length) {
    if (!table || !data) return -1;

    if (table->dataSize + length > table->allocSize) {
        return inputOutOfBounds;
    }

    /* Get previous entry's offset */
    uint16_t prevOffset = table->entries[table->nextVarIndex - 1].offset;

    /* Calculate new offset */
    uint16_t newOffset = prevOffset - (uint16_t)length;

    /* Store new entry */
    table->entries[table->nextVarIndex].offset = newOffset;

    /* Copy data */
    memcpy(table->data + newOffset, data, length);

    table->nextVarIndex++;
    table->dataSize += length;

    return 0;
}

int VarTable_Fetch(VarTable* table, size_t index, uint8_t** data, size_t* length) {
    if (!table || index >= table->nextVarIndex) {
        return inputOutOfBounds;
    }

    uint16_t startOffset = table->entries[index].offset;
    uint16_t endOffset = (index > 0) ? table->entries[index - 1].offset : (uint16_t)table->allocSize;

    *length = endOffset - startOffset;
    *data = table->data + startOffset;

    return 0;
}

void VarTable_Free(VarTable* table) {
    if (table) {
        free(table->entries);
        free(table->data);
        free(table);
    }
}

/* ---- DonnBits Decompression Implementation --------------------------------------- */

DecompressContext* DonnBits_Init(const uint8_t* compressedData, size_t compressedSize, size_t decompressedSize) {
    DecompressContext* ctx = (DecompressContext*)calloc(1, sizeof(DecompressContext));
    if (!ctx) return NULL;

    ctx->input = compressedData;
    ctx->inputSize = compressedSize;
    ctx->inputPos = 0;

    ctx->output = (uint8_t*)malloc(decompressedSize);
    ctx->outputSize = decompressedSize;
    ctx->outputPos = 0;

    if (!ctx->output) {
        free(ctx);
        return NULL;
    }

    /* Read header */
    if (compressedSize < sizeof(DonnBitsHeader) + 4) {
        free(ctx->output);
        free(ctx);
        return NULL;
    }

    /* Skip resource size field */
    ctx->inputPos = 4;
    memcpy(&ctx->header.donnBits, compressedData + 4, sizeof(DonnBitsHeader));
    ctx->inputPos = 4 + sizeof(DonnBitsHeader);

    /* Create variable table */
    ctx->varTable = VarTable_Create(ctx->header.donnBits.varTableRatio, decompressedSize);
    if (!ctx->varTable) {
        free(ctx->output);
        free(ctx);
        return NULL;
    }

    return ctx;
}

uint32_t DonnBits_GetEncodedValue(DecompressContext* ctx) {
    if (ctx->inputPos >= ctx->inputSize) {
        SetError(ctx, inputOutOfBounds, "Input buffer underrun");
        return 0;
    }

    uint8_t firstByte = ctx->input[ctx->inputPos++];

    /* Single byte value (0-127) */
    if ((firstByte & 0x80) == 0) {
        return firstByte;
    }

    /* Four byte value */
    if (firstByte == FOUR_BYTE_VALUE) {
        if (ctx->inputPos + 4 > ctx->inputSize) {
            SetError(ctx, inputOutOfBounds, "Input buffer underrun");
            return 0;
        }
        uint32_t value = (ctx->input[ctx->inputPos] << 24) |
                        (ctx->input[ctx->inputPos + 1] << 16) |
                        (ctx->input[ctx->inputPos + 2] << 8) |
                        ctx->input[ctx->inputPos + 3];
        ctx->inputPos += 4;
        return value;
    }

    /* Two byte value */
    if (ctx->inputPos >= ctx->inputSize) {
        SetError(ctx, inputOutOfBounds, "Input buffer underrun");
        return 0;
    }

    /* Decode two-byte value */
    int16_t value = ((firstByte - 0xC0) << 9) | (ctx->input[ctx->inputPos++] << 1);
    value >>= 1;  /* Sign extend */
    return (uint32_t)(int32_t)value;
}

int DonnBits_CopyLiteral(DecompressContext* ctx, size_t length) {
    if (ctx->inputPos + length > ctx->inputSize) {
        SetError(ctx, inputOutOfBounds, "Input buffer underrun");
        return inputOutOfBounds;
    }

    if (ctx->outputPos + length > ctx->outputSize) {
        SetError(ctx, outputOutOfBounds, "Output buffer overrun");
        return outputOutOfBounds;
    }

    memcpy(ctx->output + ctx->outputPos, ctx->input + ctx->inputPos, length);
    ctx->inputPos += length;
    ctx->outputPos += length;
    ctx->stats.bytesRead += length;
    ctx->stats.bytesWritten += length;

    return 0;
}

int DonnBits_RememberLiteral(DecompressContext* ctx, size_t length) {
    if (ctx->inputPos + length > ctx->inputSize) {
        SetError(ctx, inputOutOfBounds, "Input buffer underrun");
        return inputOutOfBounds;
    }

    /* Remember in variable table */
    int result = VarTable_Remember(ctx->varTable, ctx->input + ctx->inputPos, length);
    if (result != 0) {
        SetError(ctx, result, "Variable table remember failed");
        return result;
    }

    ctx->stats.varsStored++;

    /* Also copy to output */
    return DonnBits_CopyLiteral(ctx, length);
}

int DonnBits_ReuseLiteral(DecompressContext* ctx, size_t index) {
    uint8_t* data;
    size_t length;

    int result = VarTable_Fetch(ctx->varTable, index, &data, &length);
    if (result != 0) {
        SetError(ctx, result, "Variable table fetch failed");
        return result;
    }

    if (ctx->outputPos + length > ctx->outputSize) {
        SetError(ctx, outputOutOfBounds, "Output buffer overrun");
        return outputOutOfBounds;
    }

    memcpy(ctx->output + ctx->outputPos, data, length);
    ctx->outputPos += length;
    ctx->stats.bytesWritten += length;
    ctx->stats.varsReused++;

    return 0;
}

int DonnBits_HandleExtended(DecompressContext* ctx) {
    if (ctx->inputPos >= ctx->inputSize) {
        SetError(ctx, inputOutOfBounds, "Input buffer underrun");
        return inputOutOfBounds;
    }

    uint8_t opCode = ctx->input[ctx->inputPos++];

    switch (opCode) {
        case EXT_JUMP_TABLE:
            /* Jump table transformation (from DeCompressCommon.A) */
            /* Format: Seg# NumEntries Delta0 Delta1 Delta2... */
            {
                uint16_t seg = (uint16_t)DonnBits_GetEncodedValue(ctx);
                uint32_t numEntries = DonnBits_GetEncodedValue(ctx);
                uint16_t offset = 6;  /* Initial offset bias */

                for (uint32_t i = 0; i <= numEntries; i++) {
                    if (i < numEntries) {
                        /* Get delta and adjust by bias */
                        int32_t delta = (int32_t)DonnBits_GetEncodedValue(ctx) - 6;
                        offset += delta;

                        /* Output jump table entry: 0x3F3C seg 0xA9F0 offset */
                        if (ctx->outputPos + 8 > ctx->outputSize) {
                            SetError(ctx, outputOutOfBounds, "Output buffer overrun");
                            return outputOutOfBounds;
                        }
                        /* Big-endian output */
                        ctx->output[ctx->outputPos++] = 0x3F;
                        ctx->output[ctx->outputPos++] = 0x3C;
                        ctx->output[ctx->outputPos++] = (seg >> 8) & 0xFF;
                        ctx->output[ctx->outputPos++] = seg & 0xFF;
                        ctx->output[ctx->outputPos++] = 0xA9;
                        ctx->output[ctx->outputPos++] = 0xF0;
                        ctx->output[ctx->outputPos++] = (offset >> 8) & 0xFF;
                        ctx->output[ctx->outputPos++] = offset & 0xFF;
                    } else {
                        /* Final entry without offset */
                        if (ctx->outputPos + 6 > ctx->outputSize) {
                            SetError(ctx, outputOutOfBounds, "Output buffer overrun");
                            return outputOutOfBounds;
                        }
                        ctx->output[ctx->outputPos++] = 0x3F;
                        ctx->output[ctx->outputPos++] = 0x3C;
                        ctx->output[ctx->outputPos++] = (seg >> 8) & 0xFF;
                        ctx->output[ctx->outputPos++] = seg & 0xFF;
                        ctx->output[ctx->outputPos++] = 0xA9;
                        ctx->output[ctx->outputPos++] = 0xF0;
                    }
                }
            }
            break;

        case EXT_ENTRY_VECTOR:
            /* Entry vector transformation (from DeCompressCommon.A) */
            /* Format: BranchOffset Delta NumEntries Offset0 [Offset1...OffsetN] */
            {
                int16_t branchOffset = (int16_t)DonnBits_GetEncodedValue(ctx);
                int16_t delta = (int16_t)DonnBits_GetEncodedValue(ctx);
                uint32_t numEntries = DonnBits_GetEncodedValue(ctx);
                uint16_t offset = (uint16_t)DonnBits_GetEncodedValue(ctx);

                for (uint32_t i = 0; i < numEntries; i++) {
                    if (i > 0) {
                        branchOffset -= 8;
                        if (delta != 0) {
                            offset += delta;
                        } else {
                            offset = (uint16_t)DonnBits_GetEncodedValue(ctx);
                        }
                    }

                    /* Output entry vector: 0x6100 branchOffset 0x4EED offset */
                    if (ctx->outputPos + 8 > ctx->outputSize) {
                        SetError(ctx, outputOutOfBounds, "Output buffer overrun");
                        return outputOutOfBounds;
                    }
                    ctx->output[ctx->outputPos++] = 0x61;
                    ctx->output[ctx->outputPos++] = 0x00;
                    ctx->output[ctx->outputPos++] = (branchOffset >> 8) & 0xFF;
                    ctx->output[ctx->outputPos++] = branchOffset & 0xFF;
                    ctx->output[ctx->outputPos++] = 0x4E;
                    ctx->output[ctx->outputPos++] = 0xED;
                    ctx->output[ctx->outputPos++] = (offset >> 8) & 0xFF;
                    ctx->output[ctx->outputPos++] = offset & 0xFF;
                }
            }
            break;

        case EXT_RUN_LENGTH_BYTE:
            /* Run-length encoding by bytes */
            {
                uint32_t value = DonnBits_GetEncodedValue(ctx);
                uint32_t count = DonnBits_GetEncodedValue(ctx);

                for (uint32_t i = 0; i <= count; i++) {
                    if (ctx->outputPos >= ctx->outputSize) {
                        SetError(ctx, outputOutOfBounds, "Output buffer overrun");
                        return outputOutOfBounds;
                    }
                    ctx->output[ctx->outputPos++] = value & 0xFF;
                }
            }
            break;

        case EXT_RUN_LENGTH_WORD:
            /* Run-length encoding by words */
            {
                uint32_t value = DonnBits_GetEncodedValue(ctx);
                uint32_t count = DonnBits_GetEncodedValue(ctx);

                for (uint32_t i = 0; i <= count; i++) {
                    if (ctx->outputPos + 2 > ctx->outputSize) {
                        SetError(ctx, outputOutOfBounds, "Output buffer overrun");
                        return outputOutOfBounds;
                    }
                    ctx->output[ctx->outputPos++] = (value >> 8) & 0xFF;
                    ctx->output[ctx->outputPos++] = value & 0xFF;
                }
            }
            break;

        case EXT_DIFF_WORD:
            /* Difference encoding with byte deltas */
            {
                int32_t value = (int32_t)DonnBits_GetEncodedValue(ctx);
                uint32_t count = DonnBits_GetEncodedValue(ctx);

                for (uint32_t i = 0; i <= count; i++) {
                    if (i > 0) {
                        if (ctx->inputPos >= ctx->inputSize) {
                            SetError(ctx, inputOutOfBounds, "Input buffer underrun");
                            return inputOutOfBounds;
                        }
                        int8_t delta = (int8_t)ctx->input[ctx->inputPos++];
                        value += delta;
                    }
                    if (ctx->outputPos + 2 > ctx->outputSize) {
                        SetError(ctx, outputOutOfBounds, "Output buffer overrun");
                        return outputOutOfBounds;
                    }
                    ctx->output[ctx->outputPos++] = (value >> 8) & 0xFF;
                    ctx->output[ctx->outputPos++] = value & 0xFF;
                }
            }
            break;

        case EXT_DIFF_ENC_WORD:
            /* Difference encoding with encoded word deltas */
            {
                int32_t value = (int32_t)DonnBits_GetEncodedValue(ctx);
                uint32_t count = DonnBits_GetEncodedValue(ctx);

                for (uint32_t i = 0; i <= count; i++) {
                    if (i > 0) {
                        int32_t delta = (int32_t)DonnBits_GetEncodedValue(ctx);
                        value += delta;
                    }
                    if (ctx->outputPos + 2 > ctx->outputSize) {
                        SetError(ctx, outputOutOfBounds, "Output buffer overrun");
                        return outputOutOfBounds;
                    }
                    ctx->output[ctx->outputPos++] = (value >> 8) & 0xFF;
                    ctx->output[ctx->outputPos++] = value & 0xFF;
                }
            }
            break;

        case EXT_DIFF_ENC_LONG:
            /* Difference encoding with encoded long deltas */
            {
                int32_t value = (int32_t)DonnBits_GetEncodedValue(ctx);
                uint32_t count = DonnBits_GetEncodedValue(ctx);

                for (uint32_t i = 0; i <= count; i++) {
                    if (i > 0) {
                        int32_t delta = (int32_t)DonnBits_GetEncodedValue(ctx);
                        value += delta;
                    }
                    if (ctx->outputPos + 4 > ctx->outputSize) {
                        SetError(ctx, outputOutOfBounds, "Output buffer overrun");
                        return outputOutOfBounds;
                    }
                    ctx->output[ctx->outputPos++] = (value >> 24) & 0xFF;
                    ctx->output[ctx->outputPos++] = (value >> 16) & 0xFF;
                    ctx->output[ctx->outputPos++] = (value >> 8) & 0xFF;
                    ctx->output[ctx->outputPos++] = value & 0xFF;
                }
            }
            break;

        default:
            SetError(ctx, CantDecompress, "Unknown extended opcode");
            return CantDecompress;
    }

    return 0;
}

int DonnBits_Decompress(DecompressContext* ctx) {
    if (!ctx) return CantDecompress;

    /* Skip past header to compressed data */
    ctx->inputPos = 4 + sizeof(DonnBitsHeader);

    /* Main decompression loop */
    while (ctx->inputPos < ctx->inputSize && ctx->outputPos < ctx->outputSize) {
        if (ctx->inputPos >= ctx->inputSize) {
            break;  /* End of input */
        }

        uint8_t token = ctx->input[ctx->inputPos++];
        int result = 0;

        /* Check for special token values and constant words (dcmp 0 specific) */
        if (token == 0xFF) {
            /* Terminator */
            break;
        } else if (token == 0xFD || token == 0xFB) {
            /* Extended operation */
            result = DonnBits_HandleExtended(ctx);
        } else if (token >= 0x48 && token <= 0xFC) {
            /* Constant word table entries (from DeCompressDefProc.a) */
            size_t wordIndex = token - 0x48;
            if (wordIndex < sizeof(gDcmp0ConstantWords) / sizeof(uint16_t)) {
                uint16_t word = gDcmp0ConstantWords[wordIndex];
                if (ctx->outputPos + 2 > ctx->outputSize) {
                    SetError(ctx, outputOutOfBounds, "Output buffer overrun");
                    result = outputOutOfBounds;
                } else {
                    ctx->output[ctx->outputPos++] = (word >> 8) & 0xFF;
                    ctx->output[ctx->outputPos++] = word & 0xFF;
                }
            } else {
                SetError(ctx, CantDecompress, "Invalid constant word index");
                result = CantDecompress;
            }
        } else if (token >= 0xD0 && token < 0xF8) {
            /* Reuse data with embedded index (0-39) */
            size_t index = token - 0xD0;
            result = DonnBits_ReuseLiteral(ctx, index);
        } else if (token >= 0xA0 && token < 0xD0) {
            /* Reuse with following byte or word */
            uint8_t subType = (token >> 4) & 0x03;
            if (subType == 0) {
                /* Reuse with byte index */
                if (ctx->inputPos >= ctx->inputSize) {
                    SetError(ctx, inputOutOfBounds, "Input buffer underrun");
                    result = inputOutOfBounds;
                } else {
                    size_t index = ctx->input[ctx->inputPos++] + MAX_1BYTE_REUSE;
                    result = DonnBits_ReuseLiteral(ctx, index);
                }
            } else if (subType == 1) {
                /* Reuse with byte+256 index */
                if (ctx->inputPos >= ctx->inputSize) {
                    SetError(ctx, inputOutOfBounds, "Input buffer underrun");
                    result = inputOutOfBounds;
                } else {
                    size_t index = ctx->input[ctx->inputPos++] + MAX_1BYTE_REUSE + 256;
                    result = DonnBits_ReuseLiteral(ctx, index);
                }
            } else {
                /* Reuse with word index */
                if (ctx->inputPos + 2 > ctx->inputSize) {
                    SetError(ctx, inputOutOfBounds, "Input buffer underrun");
                    result = inputOutOfBounds;
                } else {
                    size_t index = (ctx->input[ctx->inputPos] << 8) | ctx->input[ctx->inputPos + 1];
                    ctx->inputPos += 2;
                    index += MAX_1BYTE_REUSE;
                    result = DonnBits_ReuseLiteral(ctx, index);
                }
            }
        } else if (token >= 0x40) {
            /* Remember literal with encoded length (2-30 bytes) */
            size_t length = ((token - 0x40) & 0x0F) * 2 + 2;
            if (token == 0x40) {
                /* Variable length */
                length = DonnBits_GetEncodedValue(ctx) * 2;
            }
            result = DonnBits_RememberLiteral(ctx, length);
        } else if (token >= 0x01) {
            /* Copy literal with encoded length (2-30 bytes) */
            size_t length = (token & 0x0F) * 2 + 2;
            if (token == 0x01) {
                /* Variable length */
                length = DonnBits_GetEncodedValue(ctx) * 2;
            }
            result = DonnBits_CopyLiteral(ctx, length);
        } else {
            /* Token 0x00 - shouldn't happen in valid stream */
            break;
        }

        if (result != 0) {
            return result;
        }
    }

    ctx->stats.checksum = CalculateChecksum(ctx->output, ctx->outputPos);
    return 0;
}

void DonnBits_Cleanup(DecompressContext* ctx) {
    if (ctx) {
        VarTable_Free(ctx->varTable);
        free(ctx->output);
        free(ctx);
    }
}

/* ---- GreggyBits Decompression Implementation ------------------------------------- */

DecompressContext* GreggyBits_Init(const uint8_t* compressedData, size_t compressedSize, size_t decompressedSize) {
    DecompressContext* ctx = (DecompressContext*)calloc(1, sizeof(DecompressContext));
    if (!ctx) return NULL;

    ctx->input = compressedData;
    ctx->inputSize = compressedSize;
    ctx->inputPos = 0;

    ctx->output = (uint8_t*)malloc(decompressedSize);
    ctx->outputSize = decompressedSize;
    ctx->outputPos = 0;

    if (!ctx->output) {
        free(ctx);
        return NULL;
    }

    /* Read header */
    if (compressedSize < sizeof(GreggyBitsHeader) + 4) {
        free(ctx->output);
        free(ctx);
        return NULL;
    }

    /* Skip resource size field */
    ctx->inputPos = 4;
    memcpy(&ctx->header.greggyBits, compressedData + 4, sizeof(GreggyBitsHeader));
    ctx->inputPos = 4 + sizeof(GreggyBitsHeader);

    return ctx;
}

int GreggyBits_LoadByteTable(DecompressContext* ctx) {
    if (ctx->header.greggyBits.compressFlags & GREGGY_BYTE_TABLE_SAVED) {
        /* Dynamic byte table present */
        size_t tableSize = (ctx->header.greggyBits.byteTableSize + 1) * 2;

        if (ctx->inputPos + tableSize > ctx->inputSize) {
            SetError(ctx, inputOutOfBounds, "Input buffer underrun reading byte table");
            return inputOutOfBounds;
        }

        ctx->byteTable = (uint16_t*)malloc(256 * sizeof(uint16_t));
        if (!ctx->byteTable) {
            SetError(ctx, memFullErr, "Failed to allocate byte table");
            return memFullErr;
        }

        /* Copy dynamic table */
        memcpy(ctx->byteTable, ctx->input + ctx->inputPos, tableSize);
        ctx->inputPos += tableSize;

        /* Fill rest with default values */
        for (size_t i = tableSize / 2; i < 256; i++) {
            ctx->byteTable[i] = gStaticByteTable[i];
        }
    } else {
        /* Use static table */
        ctx->byteTable = (uint16_t*)gStaticByteTable;
    }

    return 0;
}

int GreggyBits_ExpandBytes(DecompressContext* ctx) {
    /* Simple byte-to-word expansion */
    size_t numWords = ctx->outputSize / 2;

    for (size_t i = 0; i < numWords && ctx->inputPos < ctx->inputSize; i++) {
        uint8_t byte = ctx->input[ctx->inputPos++];
        uint16_t word = ctx->byteTable[byte];

        if (ctx->outputPos + 2 > ctx->outputSize) {
            SetError(ctx, outputOutOfBounds, "Output buffer overrun");
            return outputOutOfBounds;
        }

        /* Write as big-endian */
        ctx->output[ctx->outputPos++] = (word >> 8) & 0xFF;
        ctx->output[ctx->outputPos++] = word & 0xFF;
    }

    /* Handle odd byte if present */
    if (ctx->outputSize & 1 && ctx->inputPos < ctx->inputSize) {
        ctx->output[ctx->outputPos++] = ctx->input[ctx->inputPos++];
    }

    return 0;
}

int GreggyBits_ProcessBitmap(DecompressContext* ctx) {
    /* Process run-length encoded bitmap */
    if (!(ctx->header.greggyBits.compressFlags & GREGGY_BITMAPPED_DATA)) {
        return GreggyBits_ExpandBytes(ctx);
    }

    /* Read run count */
    if (ctx->inputPos >= ctx->inputSize) {
        SetError(ctx, inputOutOfBounds, "Input buffer underrun");
        return inputOutOfBounds;
    }

    uint8_t numRuns = ctx->input[ctx->inputPos++];
    uint8_t lastRunWords = ctx->input[ctx->inputPos++];

    for (uint8_t run = 0; run < numRuns; run++) {
        uint32_t bitmap = 0;
        uint8_t wordsInRun = 32;

        /* Last run may have fewer words */
        if (run == numRuns - 1 && lastRunWords > 0) {
            wordsInRun = lastRunWords;
        }

        /* Read bitmap for this run */
        if (ctx->inputPos + 4 > ctx->inputSize) {
            SetError(ctx, inputOutOfBounds, "Input buffer underrun");
            return inputOutOfBounds;
        }

        bitmap = (ctx->input[ctx->inputPos] << 24) |
                (ctx->input[ctx->inputPos + 1] << 16) |
                (ctx->input[ctx->inputPos + 2] << 8) |
                ctx->input[ctx->inputPos + 3];
        ctx->inputPos += 4;

        /* Process each bit in bitmap */
        for (uint8_t bit = 0; bit < wordsInRun; bit++) {
            if (bitmap & (1 << (31 - bit))) {
                /* Expanded word */
                if (ctx->inputPos >= ctx->inputSize) {
                    SetError(ctx, inputOutOfBounds, "Input buffer underrun");
                    return inputOutOfBounds;
                }

                uint8_t byte = ctx->input[ctx->inputPos++];
                uint16_t word = ctx->byteTable[byte];

                if (ctx->outputPos + 2 > ctx->outputSize) {
                    SetError(ctx, outputOutOfBounds, "Output buffer overrun");
                    return outputOutOfBounds;
                }

                ctx->output[ctx->outputPos++] = (word >> 8) & 0xFF;
                ctx->output[ctx->outputPos++] = word & 0xFF;
            } else {
                /* Literal word */
                if (ctx->inputPos + 2 > ctx->inputSize) {
                    SetError(ctx, inputOutOfBounds, "Input buffer underrun");
                    return inputOutOfBounds;
                }

                if (ctx->outputPos + 2 > ctx->outputSize) {
                    SetError(ctx, outputOutOfBounds, "Output buffer overrun");
                    return outputOutOfBounds;
                }

                ctx->output[ctx->outputPos++] = ctx->input[ctx->inputPos++];
                ctx->output[ctx->outputPos++] = ctx->input[ctx->inputPos++];
            }
        }
    }

    return 0;
}

int GreggyBits_Decompress(DecompressContext* ctx) {
    if (!ctx) return CantDecompress;

    /* Load byte expansion table */
    int result = GreggyBits_LoadByteTable(ctx);
    if (result != 0) return result;

    /* Process compressed data */
    result = GreggyBits_ProcessBitmap(ctx);
    if (result != 0) return result;

    ctx->stats.checksum = CalculateChecksum(ctx->output, ctx->outputPos);
    return 0;
}

void GreggyBits_Cleanup(DecompressContext* ctx) {
    if (ctx) {
        if (ctx->byteTable != gStaticByteTable) {
            free(ctx->byteTable);
        }
        free(ctx->output);
        free(ctx);
    }
}

const uint16_t* GreggyBits_GetStaticTable(void) {
    return gStaticByteTable;
}

/* ---- Dcmp1 Decompression Implementation (byte-wise) ------------------------------ */

DecompressContext* Dcmp1_Init(const uint8_t* compressedData, size_t compressedSize, size_t decompressedSize) {
    DecompressContext* ctx = (DecompressContext*)calloc(1, sizeof(DecompressContext));
    if (!ctx) return NULL;

    ctx->input = compressedData;
    ctx->inputSize = compressedSize;
    ctx->inputPos = 0;

    ctx->output = (uint8_t*)malloc(decompressedSize);
    ctx->outputSize = decompressedSize;
    ctx->outputPos = 0;

    if (!ctx->output) {
        free(ctx);
        return NULL;
    }

    /* Read header - dcmp 1 uses DonnBits header format */
    if (compressedSize < sizeof(DonnBitsHeader) + 4) {
        free(ctx->output);
        free(ctx);
        return NULL;
    }

    /* Skip resource size field */
    ctx->inputPos = 4;
    memcpy(&ctx->header.donnBits, compressedData + 4, sizeof(DonnBitsHeader));
    ctx->inputPos = 4 + sizeof(DonnBitsHeader);

    /* Create variable table (same as DonnBits) */
    ctx->varTable = VarTable_Create(ctx->header.donnBits.varTableRatio, decompressedSize);
    if (!ctx->varTable) {
        free(ctx->output);
        free(ctx);
        return NULL;
    }

    return ctx;
}

int Dcmp1_Decompress(DecompressContext* ctx) {
    if (!ctx) return CantDecompress;

    /* Skip past header to compressed data */
    ctx->inputPos = 4 + sizeof(DonnBitsHeader);

    /* Main decompression loop - byte-wise compression uses folded dispatch */
    while (ctx->inputPos < ctx->inputSize && ctx->outputPos < ctx->outputSize) {
        if (ctx->inputPos >= ctx->inputSize) {
            break;  /* End of input */
        }

        uint8_t token = ctx->input[ctx->inputPos++];
        int result = 0;

        if (token == 0xFF) {
            /* Terminator */
            break;
        } else if (token == 0xFE) {
            /* Extended operation (same as dcmp 0) */
            result = DonnBits_HandleExtended(ctx);
        } else if (token >= DCMP1_CONSTANT_ITEMS) {
            /* Constant word table entries (dcmp 1 specific) */
            size_t wordIndex = token - DCMP1_CONSTANT_ITEMS;
            if (wordIndex < sizeof(gDcmp1ConstantWords) / sizeof(uint16_t)) {
                uint16_t word = gDcmp1ConstantWords[wordIndex];
                if (ctx->outputPos + 2 > ctx->outputSize) {
                    SetError(ctx, outputOutOfBounds, "Output buffer overrun");
                    result = outputOutOfBounds;
                } else {
                    /* Byte-wise output for dcmp 1 */
                    ctx->output[ctx->outputPos++] = (word >> 8) & 0xFF;
                    ctx->output[ctx->outputPos++] = word & 0xFF;
                }
            } else {
                /* Extended dispatcher codes (D0-D4) */
                switch (token) {
                    case 0xD0:
                        /* Literal with encoded length */
                        result = DonnBits_CopyLiteral(ctx, DonnBits_GetEncodedValue(ctx));
                        break;
                    case 0xD1:
                        /* Remember with encoded length */
                        {
                            size_t length = DonnBits_GetEncodedValue(ctx);
                            result = DonnBits_RememberLiteral(ctx, length);
                        }
                        break;
                    case 0xD2:
                        /* Reuse with byte index */
                        if (ctx->inputPos >= ctx->inputSize) {
                            SetError(ctx, inputOutOfBounds, "Input buffer underrun");
                            result = inputOutOfBounds;
                        } else {
                            size_t index = ctx->input[ctx->inputPos++];
                            result = DonnBits_ReuseLiteral(ctx, index);
                        }
                        break;
                    case 0xD3:
                        /* Reuse with byte+256 index */
                        if (ctx->inputPos >= ctx->inputSize) {
                            SetError(ctx, inputOutOfBounds, "Input buffer underrun");
                            result = inputOutOfBounds;
                        } else {
                            size_t index = ctx->input[ctx->inputPos++] + 256;
                            result = DonnBits_ReuseLiteral(ctx, index);
                        }
                        break;
                    case 0xD4:
                        /* Reuse with word index */
                        if (ctx->inputPos + 2 > ctx->inputSize) {
                            SetError(ctx, inputOutOfBounds, "Input buffer underrun");
                            result = inputOutOfBounds;
                        } else {
                            size_t index = (ctx->input[ctx->inputPos] << 8) | ctx->input[ctx->inputPos + 1];
                            ctx->inputPos += 2;
                            result = DonnBits_ReuseLiteral(ctx, index);
                        }
                        break;
                    default:
                        SetError(ctx, CantDecompress, "Invalid dcmp 1 constant token");
                        result = CantDecompress;
                        break;
                }
            }
        } else if (token >= DCMP1_VARIABLE_REFS) {
            /* Variable references (0x20-0xCF) - reuse data with embedded index */
            size_t index = token - DCMP1_VARIABLE_REFS;
            if (index < MAX_1BYTE_REUSE * 4) {
                result = DonnBits_ReuseLiteral(ctx, index);
            } else {
                SetError(ctx, CantDecompress, "Invalid variable reference");
                result = CantDecompress;
            }
        } else if (token >= DCMP1_DEFS_ENCODED) {
            /* Remember literal with fixed length (0x10-0x1F) */
            size_t length = (token & 0x0F) + 1;
            result = DonnBits_RememberLiteral(ctx, length);
        } else {
            /* Copy literal with fixed length (0x00-0x0F) */
            size_t length = (token & 0x0F) + 1;
            result = DonnBits_CopyLiteral(ctx, length);
        }

        if (result != 0) {
            return result;
        }
    }

    ctx->stats.checksum = CalculateChecksum(ctx->output, ctx->outputPos);
    return 0;
}

void Dcmp1_Cleanup(DecompressContext* ctx) {
    if (ctx) {
        VarTable_Free(ctx->varTable);
        free(ctx->output);
        free(ctx);
    }
}

/* ---- Main Decompression Function ------------------------------------------------- */

int DecompressResource(const uint8_t* compressedData, size_t compressedSize,
                      uint8_t** decompressedData, size_t* decompressedSize) {
    if (!compressedData || !decompressedData || !decompressedSize) {
        return CantDecompress;
    }

    /* Check for extended resource header */
    if (!IsExtendedResource(compressedData, compressedSize)) {
        /* Not an extended resource, just copy data */
        *decompressedSize = compressedSize;
        *decompressedData = (uint8_t*)malloc(compressedSize);
        if (!*decompressedData) {
            return memFullErr;
        }
        memcpy(*decompressedData, compressedData, compressedSize);
        return 0;
    }

    /* Get header */
    const ExtendedResourceHeader* header = (const ExtendedResourceHeader*)(compressedData + 4);

    if (!IsCompressedResource(header)) {
        /* Extended but not compressed, skip header */
        size_t dataOffset = 4 + header->headerLength;
        *decompressedSize = header->actualSize;
        *decompressedData = (uint8_t*)malloc(header->actualSize);
        if (!*decompressedData) {
            return memFullErr;
        }
        memcpy(*decompressedData, compressedData + dataOffset, header->actualSize);
        return 0;
    }

    /* Check cache if enabled */
    if (gCachingEnabled) {
        uint32_t signature = CalculateChecksum(compressedData, compressedSize);
        DecompressCache* cache = gCacheHead;
        while (cache) {
            if (cache->signature == signature) {
                *decompressedSize = cache->size;
                *decompressedData = (uint8_t*)malloc(cache->size);
                if (!*decompressedData) {
                    return memFullErr;
                }
                memcpy(*decompressedData, cache->data, cache->size);
                gCacheHits++;
                return 0;
            }
            cache = cache->next;
        }
        gCacheMisses++;
    }

    /* Determine decompression algorithm */
    DecompressContext* ctx = NULL;
    int result = 0;

    if (header->headerVersion == DONN_HEADER_VERSION) {
        /* Check for dcmp ID to determine specific algorithm */
        const DonnBitsHeader* dh = (const DonnBitsHeader*)header;
        if (dh->decompressID == 1) {
            /* Dcmp 1 - byte-wise decompression */
            ctx = Dcmp1_Init(compressedData, compressedSize, header->actualSize);
            if (ctx) {
                result = Dcmp1_Decompress(ctx);
                if (result == 0) {
                    *decompressedSize = ctx->outputPos;
                    *decompressedData = ctx->output;
                    ctx->output = NULL;  /* Transfer ownership */
                }
                Dcmp1_Cleanup(ctx);
            } else {
                result = memFullErr;
            }
        } else {
            /* Dcmp 0 - DonnBits decompression (default) */
            ctx = DonnBits_Init(compressedData, compressedSize, header->actualSize);
            if (ctx) {
                result = DonnBits_Decompress(ctx);
                if (result == 0) {
                    *decompressedSize = ctx->outputPos;
                    *decompressedData = ctx->output;
                    ctx->output = NULL;  /* Transfer ownership */
                }
                DonnBits_Cleanup(ctx);
            } else {
                result = memFullErr;
            }
        }
    } else if (header->headerVersion == GREGGY_HEADER_VERSION) {
        /* GreggyBits decompression */
        ctx = GreggyBits_Init(compressedData, compressedSize, header->actualSize);
        if (ctx) {
            result = GreggyBits_Decompress(ctx);
            if (result == 0) {
                *decompressedSize = ctx->outputPos;
                *decompressedData = ctx->output;
                ctx->output = NULL;  /* Transfer ownership */
            }
            GreggyBits_Cleanup(ctx);
        } else {
            result = memFullErr;
        }
    } else {
        /* Check for custom decompressor based on header version or defproc ID */
        uint16_t defProcID = 0;

        /* For GreggyBits header format with custom defproc */
        if (header->headerVersion >= 9) {
            const GreggyBitsHeader* gh = (const GreggyBitsHeader*)header;
            defProcID = gh->defProcID;
        }
        /* For DonnBits header format with decompressID */
        else if (header->headerVersion == DONN_HEADER_VERSION) {
            const DonnBitsHeader* dh = (const DonnBitsHeader*)header;
            defProcID = dh->decompressID;
        }

        DecompressProc proc = GetDecompressor(defProcID);
        if (proc) {
            *decompressedData = (uint8_t*)malloc(header->actualSize);
            if (*decompressedData) {
                result = proc(compressedData + 4 + header->headerLength,
                            *decompressedData,
                            (const ResourceHeader*)header);
                *decompressedSize = header->actualSize;
            } else {
                result = memFullErr;
            }
        } else {
            result = badExtResource;
        }
    }

    /* Add to cache if successful and caching enabled */
    if (result == 0 && gCachingEnabled && *decompressedData) {
        DecompressCache* cache = (DecompressCache*)malloc(sizeof(DecompressCache));
        if (cache) {
            cache->signature = CalculateChecksum(compressedData, compressedSize);
            cache->size = *decompressedSize;
            cache->data = (uint8_t*)malloc(cache->size);
            if (cache->data) {
                memcpy(cache->data, *decompressedData, cache->size);
                cache->timestamp = time(NULL);
                cache->next = gCacheHead;
                gCacheHead = cache;
            } else {
                free(cache);
            }
        }
    }

    return result;
}

/* ---- Decompressor Registration --------------------------------------------------- */

int RegisterDecompressor(uint16_t defProcID, DecompressProc proc) {
    if (gDecompressorCount >= 16) {
        return -1;
    }

    /* Check if already registered */
    for (int i = 0; i < gDecompressorCount; i++) {
        if (gDecompressors[i].id == defProcID) {
            gDecompressors[i].proc = proc;
            return 0;
        }
    }

    /* Add new decompressor */
    gDecompressors[gDecompressorCount].id = defProcID;
    gDecompressors[gDecompressorCount].proc = proc;
    gDecompressorCount++;

    return 0;
}

DecompressProc GetDecompressor(uint16_t defProcID) {
    for (int i = 0; i < gDecompressorCount; i++) {
        if (gDecompressors[i].id == defProcID) {
            return gDecompressors[i].proc;
        }
    }
    return NULL;
}

/* ---- Cache Management ------------------------------------------------------------ */

void SetDecompressCaching(bool enable) {
    gCachingEnabled = enable;
    if (!enable) {
        ClearDecompressCache();
    }
}

void ClearDecompressCache(void) {
    DecompressCache* cache = gCacheHead;
    while (cache) {
        DecompressCache* next = cache->next;
        free(cache->data);
        free(cache);
        cache = next;
    }
    gCacheHead = NULL;
    gCacheHits = 0;
    gCacheMisses = 0;
}

void GetDecompressCacheStats(size_t* entries, size_t* totalSize, size_t* hits, size_t* misses) {
    size_t count = 0;
    size_t size = 0;

    DecompressCache* cache = gCacheHead;
    while (cache) {
        count++;
        size += cache->size;
        cache = cache->next;
    }

    if (entries) *entries = count;
    if (totalSize) *totalSize = size;
    if (hits) *hits = gCacheHits;
    if (misses) *misses = gCacheMisses;
}

/* ---- Debug Support --------------------------------------------------------------- */

void DumpResourceHeader(const ResourceHeader* header) {
    if (!gDebugEnabled || !header) return;

    printf("Resource Header:\n");
    printf("  Signature: 0x%08X\n", header->base.signature);
    printf("  Header Length: %u\n", header->base.headerLength);
    printf("  Header Version: %u\n", header->base.headerVersion);
    printf("  Extended Attributes: 0x%02X\n", header->base.extendedAttributes);
    printf("  Actual Size: %u\n", header->base.actualSize);

    if (header->base.headerVersion == DONN_HEADER_VERSION) {
        printf("  DonnBits Header:\n");
        printf("    Var Table Ratio: %u\n", header->donnBits.varTableRatio);
        printf("    Overrun: %u\n", header->donnBits.overRun);
        printf("    Decompress ID: %u\n", header->donnBits.decompressID);
        printf("    CTable ID: %u\n", header->donnBits.cTableID);
    } else if (header->base.headerVersion == GREGGY_HEADER_VERSION) {
        printf("  GreggyBits Header:\n");
        printf("    DefProc ID: %u\n", header->greggyBits.defProcID);
        printf("    Decompress Slop: %u\n", header->greggyBits.decompressSlop);
        printf("    Byte Table Size: %u\n", header->greggyBits.byteTableSize);
        printf("    Compress Flags: 0x%02X\n", header->greggyBits.compressFlags);
    }
}

void DumpVarTable(const VarTable* table) {
    if (!gDebugEnabled || !table) return;

    printf("Variable Table:\n");
    printf("  Next Index: %u\n", table->nextVarIndex);
    printf("  Data Size: %zu\n", table->dataSize);
    printf("  Alloc Size: %zu\n", table->allocSize);

    for (uint16_t i = 0; i < table->nextVarIndex && i < 10; i++) {
        printf("  Entry %u: offset=%u\n", i, table->entries[i].offset);
    }
}

void DumpDecompressStats(const DecompressStats* stats) {
    if (!gDebugEnabled || !stats) return;

    printf("Decompression Statistics:\n");
    printf("  Bytes Read: %zu\n", stats->bytesRead);
    printf("  Bytes Written: %zu\n", stats->bytesWritten);
    printf("  Vars Stored: %zu\n", stats->varsStored);
    printf("  Vars Reused: %zu\n", stats->varsReused);
    printf("  Checksum: 0x%08X\n", stats->checksum);
}