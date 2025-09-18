# System 7.1 Portable

A modern, cross-platform implementation of classic Mac OS System 7.1 components designed for educational purposes, research, and cross-platform compatibility.

## Overview

This project provides a clean, organized, and portable implementation of key System 7.1 components, reimagined for modern development environments. The codebase maintains the architectural concepts and API designs of the original System 7.1 while providing cross-platform compatibility and contemporary development practices.

## Features

### Core System Managers
- **ADB Manager** - Apple Desktop Bus device communication
- **Device Manager** - Hardware device abstraction and management
- **Dialog Manager** - User interface dialog handling
- **Edition Manager** - Publish and subscribe data sharing
- **Event Manager** - User input and system event processing
- **Menu Manager** - Menu bar and context menu management
- **Window Manager** - Window creation, management, and drawing
- **Help Manager** - Context-sensitive help system
- **Trap Dispatcher** - System call routing and handling

### File and Resource Management
- **File Manager** - Hierarchical file system operations
- **Resource Manager** - Resource fork handling and decompression
- **HFS Implementation** - Hierarchical File System with catalog and allocation
- **SCSI Manager** - SCSI device communication and management

### Graphics and UI
- **Color Manager** - Color space management and device calibration
- **Control Manager** - User interface controls and widgets
- **Font Manager** - Font rendering and typography
- **List Manager** - List view controls and data display

### System Services
- **Memory Manager** - Heap management and virtual memory
- **Process Manager** - Task scheduling and process control
- **Component Manager** - Plugin architecture and component loading
- **Communication Toolbox** - Network and serial communication
- **Notification Manager** - System notifications and alerts

## Project Structure

```
System7.1-Portable/
├── src/                    # Source code organized by functional modules
│   ├── ADBManager/         # Apple Desktop Bus management
│   ├── DeviceManager/      # Hardware device abstraction
│   ├── DialogManager/      # Dialog and alert handling
│   ├── EventManager/       # Event processing and input
│   ├── FileManager/        # File system operations
│   ├── MenuManager/        # Menu management
│   ├── WindowManager/      # Window management
│   └── ...                 # Additional system components
├── include/                # Header files with clean API definitions
│   ├── ADBManager/         # ADB management headers
│   ├── DeviceManager/      # Device management headers
│   └── ...                 # Component-specific headers
├── docs/                   # Technical documentation
│   ├── api/                # API reference documentation
│   ├── implementation/     # Implementation guides and details
│   ├── examples/           # Usage examples and tutorials
│   └── build/              # Build system documentation
├── build/                  # Build artifacts and makefiles
├── examples/               # Example applications and demos
├── tests/                  # Comprehensive test suites
├── tools/                  # Build tools and utilities
├── Makefile               # Master build system
├── README.md              # This file
└── .gitignore             # Git ignore rules
```

## Building

### Prerequisites
- GCC or Clang compiler
- Make build system
- Standard C library with POSIX support

### Quick Start
```bash
# Build all components
make all

# Build only libraries
make libs

# Build and run tests
make tests

# Build example applications
make examples

# Clean build artifacts
make clean
```

### Build Targets
- `all` - Build libraries, examples, and tests
- `libs` - Build static and shared libraries
- `examples` - Build example applications
- `tests` - Build and run test suites
- `debug` - Build with debug symbols and assertions
- `clean` - Remove all build artifacts
- `install` - Install libraries and headers system-wide
- `docs` - Display documentation and usage information

### Cross-Platform Build
The build system automatically detects the target platform and adjusts compilation flags accordingly:

- **Linux**: Builds with `-D__linux__` and position-independent code
- **macOS**: Builds with `-D__APPLE__` and platform-specific optimizations
- **Architecture**: Automatically detects x86_64 vs ARM64 and defines appropriate macros

## Usage

### Library Integration
```c
#include <system71/ADBManager.h>
#include <system71/EventManager.h>
#include <system71/WindowManager.h>

int main() {
    // Initialize core managers
    InitADBManager();
    InitEventManager();
    InitWindowManager();

    // Your application code here

    return 0;
}
```

### Linking
```bash
# Static linking
gcc myapp.c -Lbuild -lsystem71 -o myapp

# Dynamic linking
gcc myapp.c -Lbuild -lsystem71 -o myapp
```

## Documentation

- **API Reference**: `docs/api/` - Complete API documentation for all managers
- **Implementation Guides**: `docs/implementation/` - Detailed implementation documentation
- **Examples**: `docs/examples/` - Usage examples and tutorials
- **Build Documentation**: `docs/build/` - Build system and platform-specific notes

## Testing

The project includes comprehensive test suites for all major components:

```bash
# Run all tests
make tests

# Build tests without running
make libs
cd build && ./test_adb_manager
```

## Examples

Example applications demonstrate practical usage of System 7.1 components:

```bash
# Build examples
make examples

# Run ADB device enumeration example
cd build && ./ADBExample
```

## Contributing

This project maintains historical accuracy while providing modern implementation practices. When contributing:

1. Follow the existing code style and organization
2. Add comprehensive tests for new functionality
3. Update documentation for API changes
4. Ensure cross-platform compatibility

## License

This project is provided for educational and research purposes. The implementation is inspired by the original System 7.1 architecture but is an independent reimplementation.

## Compatibility

### Supported Platforms
- Linux (x86_64, ARM64)
- macOS (Intel, Apple Silicon)
- Other POSIX-compatible systems

### Compiler Support
- GCC 4.8 or later
- Clang 3.5 or later
- C99 standard compliance required

## Architecture Notes

This implementation preserves the conceptual architecture of System 7.1 while adapting to modern systems:

- **Event-driven design** similar to original System 7.1
- **Manager-based architecture** with clear separation of concerns
- **Resource-based model** adapted for modern file systems
- **Trap-based API** reimplemented as function calls
- **Cross-platform abstractions** for hardware-specific operations

## Technical Highlights

- Clean separation between API and implementation
- Comprehensive error handling and validation
- Memory management with proper cleanup
- Thread-safe operations where applicable
- Extensive documentation and examples
- Professional build system with dependency tracking

## Status

This is an active development project. Core managers are implemented and functional, with ongoing work on advanced features and platform-specific optimizations.

For detailed component status and implementation progress, see the documentation in `docs/implementation/`.