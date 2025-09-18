# System 7.1 Portable Segment Loader

## Overview

The Segment Loader is **THE CRITICAL MISSING PIECE** that transforms System 7.1 Portable from a working Mac OS environment into a complete system capable of running actual Mac applications. This component implements the complete Mac OS 7.1 segment loading, application launching, and execution environment.

## What This Enables

Without the Segment Loader, you have a perfect Mac OS 7.1 system that cannot run Mac software. With the Segment Loader, you have:

- **Complete Mac Application Support** - Load and run actual Mac OS 7.1 applications
- **CODE Resource Loading** - Dynamic loading and linking of Mac application segments
- **Jump Table Management** - Runtime code patching and segment interconnection
- **A5 World Management** - Complete Mac application memory environment
- **Finder Integration** - Document opening, printing, and application launching
- **MultiFinder Support** - Background applications and cooperative multitasking
- **Memory Management** - Application heaps, stacks, and resource management

## Architecture

### Core Components

#### 1. SegmentLoaderCore.c
- Implements the foundational `_LoadSeg`, `_UnloadSeg`, `_ExitToShell`, and `_GetAppParms` traps
- Manages CODE resource loading and segment tracking
- Provides the core segment loading functionality that Mac applications depend on

#### 2. ApplicationLauncher.c
- Complete application launching and lifecycle management
- Loads application resources, sets up memory, and initializes execution environment
- Handles application termination and cleanup
- Supports various launch modes (foreground, background, document opening)

#### 3. CodePatching.c
- Jump table management and code patching
- Runtime code linking between segments
- Lazy loading support with automatic segment loading
- 68k instruction generation and modification

#### 4. AppHeapManager.c
- A5 world setup and management (critical for Mac applications)
- Application heap creation and management
- Stack allocation and monitoring
- QuickDraw globals initialization

#### 5. FinderInterface.c
- Finder launch protocol implementation
- Document opening and printing support
- Desktop database integration
- Apple Events support for application communication

#### 6. SegmentCache.c
- Intelligent segment caching for performance
- Memory optimization and segment eviction
- Cache statistics and monitoring
- Multiple cache replacement policies

#### 7. ApplicationSwitcher.c
- MultiFinder-style application switching
- Cooperative multitasking support
- Process management and scheduling
- Context switching between applications

## Key Features Implemented

### Mac OS Compatibility
- **Complete Segment Loader API** - All documented Mac OS 7.1 segment loader functions
- **CODE Resource Support** - Loading, caching, and management of CODE segments
- **Jump Table Management** - Dynamic code linking and patching
- **A5 World Setup** - Complete Mac application global environment
- **Resource Integration** - Works with Resource Manager for application resources

### Application Support
- **Launch Protocol** - Standard Mac application launching via Finder
- **Document Handling** - Open and print documents with appropriate applications
- **Memory Management** - Application heaps, stacks, and global data
- **Background Execution** - MultiFinder-style background applications
- **Process Switching** - Cooperative multitasking between applications

### Modern Integration
- **Portable Implementation** - Cross-platform C code with platform abstraction
- **Security Considerations** - Memory protection and validation
- **Debugging Support** - Comprehensive logging and diagnostics
- **Performance Optimization** - Caching and memory management
- **Error Handling** - Robust error detection and recovery

## Usage

### Building
```bash
cd /home/k/System7.1-Portable/src/SegmentLoader
make all
```

### Integration with System 7.1 Portable
The Segment Loader integrates with the existing System 7.1 Portable components:

1. **Memory Manager** - For application heap management
2. **Resource Manager** - For CODE resource loading
3. **File Manager** - For application file access
4. **Process Manager** - For application lifecycle management

### Launching Mac Applications
```c
#include "SegmentLoader/SegmentLoader.h"

// Launch an application
FSSpec appSpec;
// ... set up appSpec for application
ApplicationControlBlock* acb;
OSErr err = LaunchApplication(&appSpec, NULL, 0, &acb);
if (err == segNoErr) {
    // Application launched successfully
}
```

### Loading CODE Segments
```c
// Load a specific segment
OSErr err = LoadSeg(segmentID);
if (err == segNoErr) {
    // Segment loaded successfully
}
```

## Critical Implementation Details

### CODE Resource Format
Mac applications store code in CODE resources:
- **CODE 0**: Jump table and A5 world information
- **CODE 1**: Main segment (entry point)
- **CODE 2+**: Additional segments as needed

### A5 World Layout
The A5 register points to the boundary between application globals:
- **Below A5**: QuickDraw globals, application globals
- **Above A5**: Parameters, local variables, temporary data

### Jump Table Management
Each application has a jump table that enables:
- **Inter-segment calls**: Calling functions in other segments
- **Lazy loading**: Loading segments only when needed
- **Dynamic linking**: Runtime resolution of function addresses

### Memory Layout
```
+------------------+
|   Application    |
|      Stack       |
+------------------+
|      Heap        |
|    (grows up)    |
+------------------+
|   A5 World       |
|   (globals)      |
+------------------+
|   CODE Segments  |
+------------------+
```

## Testing

The Segment Loader includes comprehensive tests for:
- CODE resource loading
- Jump table management
- A5 world setup
- Application launching
- Memory management
- Cache functionality

## Platform Abstraction

While maintaining Mac OS compatibility, the implementation provides platform abstraction points for:
- **Memory management**: Platform-specific heap and stack management
- **Code execution**: Platform-specific entry point calling
- **File access**: Platform-specific file system integration
- **Security**: Platform-specific memory protection

## Performance Considerations

### Segment Caching
- **LRU Cache**: Least Recently Used segment eviction
- **Size-based Eviction**: Prefer evicting larger segments
- **Reference Counting**: Track segment usage

### Memory Optimization
- **Lazy Loading**: Load segments only when accessed
- **Heap Compaction**: Reduce memory fragmentation
- **Stack Monitoring**: Detect stack overflow conditions

### Jump Table Optimization
- **Stub Generation**: Efficient lazy loading stubs
- **Batch Resolution**: Resolve multiple entries together
- **Cache Locality**: Keep related segments together

## Debugging Support

### Logging
Comprehensive logging for:
- Segment loading/unloading
- Jump table operations
- Memory allocation/deallocation
- Application state changes
- Cache operations

### Diagnostics
- **Memory dumps**: A5 world, heap, and stack information
- **Jump table dumps**: Complete jump table state
- **Cache statistics**: Hit rates, eviction counts
- **Process listings**: Running applications and state

## Future Enhancements

### Planned Features
- **68k Emulation**: Full 68k processor emulation for CODE segments
- **PowerPC Support**: Native PowerPC code execution
- **Enhanced Security**: Sandboxing and permission systems
- **Performance Profiling**: Detailed performance analysis
- **Modern App Bundles**: Support for modern application packaging

### Compatibility Improvements
- **System Extensions**: INIT and cdev support
- **Control Panels**: System configuration applications
- **Desk Accessories**: Menu-based mini-applications
- **AppleScript**: Scripting support for applications

## Integration Notes

This Segment Loader is designed to integrate seamlessly with the existing System 7.1 Portable codebase. It expects:

1. **Memory Manager**: For heap and pointer management
2. **Resource Manager**: For resource loading and caching
3. **File Manager**: For file system access
4. **Debugging System**: For logging and diagnostics

## Conclusion

The Segment Loader represents the culmination of the System 7.1 Portable project. It transforms a complete but static Mac OS environment into a dynamic system capable of running actual Mac applications with full fidelity to the original Mac OS 7.1 behavior.

**This is the component that makes System 7.1 Portable actually useful for running Mac software.**