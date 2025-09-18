# System 7.1 Portable Resource Manager

## Overview

This is a complete, portable C implementation of the Apple Macintosh System 7.1 Resource Manager with integrated resource decompression support. The implementation is based on careful analysis of the original System 7.1 source code, particularly the resource decompression patches.

## Features

### Core Resource Manager
- **Complete API Implementation**: All major Resource Manager functions including GetResource, LoadResource, AddResource, etc.
- **Multi-file Resource Chain**: Support for multiple resource files in a search chain
- **Handle-based Memory Management**: Mac OS-style relocatable memory handles
- **Resource Attributes**: Full support for resource attributes (locked, purgeable, protected, etc.)
- **Thread Safety**: Thread-local storage for globals with mutex-protected handle operations

### Resource Decompression
- **DonnBits Algorithm (dcmp 0)**: Token-based compression with variable table for pattern reuse
- **GreggyBits Algorithm (dcmp 2)**: Byte-to-word expansion with optional dynamic lookup tables
- **Automatic Decompression**: Transparent decompression of compressed resources during loading
- **Custom Decompressors**: Support for registering additional decompression algorithms
- **Caching**: Optional decompression result caching for improved performance

## Architecture

### Key Components

1. **ResourceManager.h/c**
   - Public API matching classic Mac OS Resource Manager
   - Resource file management and chain handling
   - Integration with decompression engine

2. **ResourceDecompression.h/c**
   - DonnBits decompression implementation
   - GreggyBits decompression implementation
   - Variable table management for pattern storage
   - Decompression context and statistics

3. **Handle Management**
   - Simple but functional handle implementation
   - Lock/unlock and purge support
   - Thread-safe operations using pthread mutexes

## Building

```bash
make          # Build library
make test     # Build and run tests
make clean    # Clean build artifacts
```

## Usage Example

```c
#include "ResourceManager.h"

// Initialize Resource Manager
InitResourceManager();

// Open a resource file
RefNum refNum = OpenResFile("MyApp.rsrc");

// Get a resource
Handle codeResource = GetResource('CODE', 1);
if (codeResource && *codeResource) {
    // Resource loaded and decompressed automatically
    void* data = *codeResource;
    // Use resource data...
}

// Clean up
CloseResFile(refNum);
CleanupResourceManager();
```

## Decompression Details

### DonnBits (dcmp 0)
- Token-based compression scheme by Donn Denman
- Uses dispatch table with 8-byte entries for fast decoding
- Variable table stores frequently used patterns
- Supports literal copy, pattern remember/reuse, and extended operations

### GreggyBits (dcmp 2)
- Byte-to-word expansion by Greg Marriott
- Optional dynamic byte tables for custom expansions
- Bitmap-controlled expansion for mixed literal/expanded data
- Optimized for 68k code patterns

## Compatibility

### Platform Support
- **ARM64**: Native support for Apple Silicon and ARM servers
- **x86_64**: Full support for Intel processors
- **Operating Systems**: Linux, macOS, and other POSIX systems

### Original System 7.1 Compatibility
- Binary-compatible resource headers and structures
- Exact reproduction of decompression algorithms
- Support for BeforePatches.a hook integration

## Implementation Notes

1. **Endianness**: The implementation handles endianness conversion for resource headers
2. **Memory Safety**: All decompression operations include bounds checking
3. **Error Handling**: Complete error code compatibility with classic Mac OS
4. **Testing**: Comprehensive test suite validates all major functionality

## Status

This is a production-ready implementation suitable for:
- Reading and decompressing System 7.1 resources on modern platforms
- Integration with System 7.1 emulation projects
- Research and analysis of classic Mac OS software
- Educational purposes to understand resource compression techniques

## Files

- `ResourceManager.h` - Public Resource Manager API
- `ResourceManager.c` - Core Resource Manager implementation
- `ResourceDecompression.h` - Decompression engine API
- `ResourceDecompression.c` - DonnBits and GreggyBits implementations
- `test_resource_manager.c` - Comprehensive test suite
- `Makefile` - Build configuration

## License

This is a reimplementation for research and compatibility purposes based on publicly available System 7.1 source code.