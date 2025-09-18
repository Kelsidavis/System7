# Resource Decompression Enhancements

## Overview

The ResourceDecompression module has been enhanced to support all decompression algorithms found in System 7.1's DeCompressDefProc.a and DeCompressDefProc1.a files. This provides complete compatibility with all compressed resource formats used in System 7.1.

## Supported Decompression Algorithms

### 1. DonnBits (dcmp 0) - Enhanced
**Source:** DeCompressDefProc.a
**Description:** Token-based compression with variable table support

**Enhancements:**
- Full constant word table support (180+ common 68k instruction words)
- Extended operation support (see below)
- Improved token dispatch with accurate parsing
- Support for all literal and variable reference encoding modes

### 2. Dcmp1 (dcmp 1) - New
**Source:** DeCompressDefProc1.a
**Description:** Byte-wise compression optimized for byte-aligned data

**Features:**
- Folded dispatch table for compact token encoding
- Separate constant word table optimized for byte operations
- Variable-length literals and references
- Shared variable table infrastructure with DonnBits

### 3. GreggyBits (dcmp 2) - Existing
**Source:** GreggyBitsDefProc.a
**Description:** Byte-to-word expansion compression

**Features:**
- Static and dynamic byte expansion tables
- Bitmap-based run-length encoding
- Optimized for 68k code compression

## Extended Operations Support

All extended operations from DeCompressCommon.A are now fully implemented:

### Jump Table Transformation (EXT_JUMP_TABLE = 0)
- Reconstructs 68k jump tables from compressed format
- Format: `Seg# NumEntries Delta0 Delta1...`
- Outputs: `0x3F3C seg 0xA9F0 offset` sequences

### Entry Vector Transformation (EXT_ENTRY_VECTOR = 1)
- Reconstructs entry vectors for code segments
- Format: `BranchOffset Delta NumEntries Offset0 [Offset1...OffsetN]`
- Outputs: `0x6100 branchOffset 0x4EED offset` sequences

### Run-Length Encoding (EXT_RUN_LENGTH_BYTE = 2, EXT_RUN_LENGTH_WORD = 3)
- Byte and word-level run-length decompression
- Format: `Value Count`
- Efficiently handles repeated values

### Difference Encoding (EXT_DIFF_WORD = 4, EXT_DIFF_ENC_WORD = 5, EXT_DIFF_ENC_LONG = 6)
- Delta compression for sequences with small differences
- Three variants: byte deltas, encoded word deltas, encoded long deltas
- Format: `InitialValue Count Delta0 Delta1...`

## Constant Word Tables

### DonnBits (dcmp 0) Constants
- 180+ common 68k instruction words
- Tokens 0x48-0xFC map to constant words
- Optimized for CODE resource compression

### Dcmp1 Constants
- 41 specialized constant words
- Tokens 0xD5-0xFD map to constant words
- Optimized for byte-wise operations

## Implementation Details

### Header Structure
All algorithms use the extended resource header format:
- Signature: `0xA89F6572` (Robustness signature)
- Version 8: DonnBits header with decompressID field
- Version 9: GreggyBits header with defProcID field

### Algorithm Detection
```c
if (header->headerVersion == DONN_HEADER_VERSION) {
    if (dh->decompressID == 1) {
        // Use Dcmp1 (byte-wise)
    } else {
        // Use DonnBits (default)
    }
} else if (header->headerVersion == GREGGY_HEADER_VERSION) {
    // Use GreggyBits or custom defproc
}
```

### Variable Table
Shared implementation between DonnBits and Dcmp1:
- Self-relative offset storage
- Dynamic sizing based on compression ratio
- Efficient fetch and store operations

## API Functions

### Core Functions
```c
// Main decompression entry point
int DecompressResource(
    const uint8_t* compressedData,
    size_t compressedSize,
    uint8_t** decompressedData,
    size_t* decompressedSize
);

// Algorithm-specific functions
DecompressContext* DonnBits_Init(...);
int DonnBits_Decompress(DecompressContext* ctx);
void DonnBits_Cleanup(DecompressContext* ctx);

DecompressContext* Dcmp1_Init(...);
int Dcmp1_Decompress(DecompressContext* ctx);
void Dcmp1_Cleanup(DecompressContext* ctx);

DecompressContext* GreggyBits_Init(...);
int GreggyBits_Decompress(DecompressContext* ctx);
void GreggyBits_Cleanup(DecompressContext* ctx);
```

### Extended Operations
```c
int DonnBits_HandleExtended(DecompressContext* ctx);
```

### Custom Decompressor Support
```c
int RegisterDecompressor(uint16_t defProcID, DecompressProc proc);
DecompressProc GetDecompressor(uint16_t defProcID);
```

## Performance Features

### Caching
- Automatic caching of decompressed resources
- Checksum-based cache validation
- Configurable cache size limits

### Statistics
- Bytes read/written tracking
- Variable table usage statistics
- Cache hit/miss rates

## Testing

The `test_enhanced_decompression.c` file provides comprehensive testing:
- Individual algorithm tests
- Extended operation verification
- Custom decompressor registration
- Cache functionality
- Performance measurements

## Compatibility

### Platform Support
- ARM64 and x86_64 architectures
- Thread-safe implementation with pthread
- Endian-aware data handling

### System 7.1 Compatibility
- Full compatibility with original System 7.1 compressed resources
- Supports all dcmp resource types (0, 1, 2)
- Handles all extended resource attributes

## Error Handling

Comprehensive error codes:
- `noErr` (0): Success
- `CantDecompress` (-186): Decompression failed
- `badExtResource` (-185): Invalid extended resource
- `inputOutOfBounds` (-190): Input buffer underrun
- `outputOutOfBounds` (-191): Output buffer overrun
- `memFullErr` (-108): Memory allocation failed

## Future Enhancements

Potential additions:
- dcmp 3+ algorithm support (if found)
- Compression implementation (reverse operations)
- Additional optimization for modern CPUs
- Resource fork integration

## References

Source files analyzed:
- `Patches/DeCompressDefProc.a` - DonnBits (dcmp 0) implementation
- `Patches/DeCompressDefProc1.a` - Dcmp1 (byte-wise) implementation
- `Patches/DeCompressCommon.A` - Common decompression routines
- `Patches/GreggyBitsDefProc.a` - GreggyBits (dcmp 2) implementation

## License

This is a reimplementation for research and compatibility purposes based on analysis of System 7.1 source code.