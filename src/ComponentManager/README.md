# Component Manager - System 7.1 Portable Implementation

This directory contains a complete portable implementation of the Apple Macintosh Component Manager from System 7.1, designed for modern cross-platform compatibility.

## Overview

The Component Manager provides dynamic component loading and management for multimedia applications, enabling modular functionality through a plugin architecture. This implementation maintains full API compatibility with the original Mac OS Component Manager while adding modern features like security, cross-platform support, and contemporary plugin architectures.

## Architecture

### Core Components

1. **ComponentManagerCore.c** - Main API implementation and initialization
   - Primary Component Manager API functions
   - Global state management
   - Subsystem coordination
   - Thread safety and synchronization

2. **ComponentRegistry.c** - Component registration and discovery
   - Component database management
   - Registration and unregistration
   - Component search and enumeration
   - Default component management
   - Component capture/delegation

3. **ComponentInstances.c** - Instance lifecycle management
   - Instance creation and destruction
   - Instance properties and storage
   - Reference counting
   - Instance iteration and validation

4. **ComponentLoader.c** - Dynamic loading and platform abstraction
   - Cross-platform module loading (DLL, .so, .dylib)
   - Component discovery and scanning
   - Hot-plugging support
   - Dependency resolution
   - Security validation

5. **ComponentDispatch.c** - API dispatch and calling conventions
   - Component function dispatch
   - Parameter marshaling
   - Calling convention adaptation
   - Call stack management
   - Exception handling

6. **ComponentResources.c** - Resource loading and management
   - 'thng' resource parsing
   - Cross-platform resource formats
   - Resource caching
   - Icon and metadata extraction
   - Localization support

7. **ComponentNegotiation.c** - Capability negotiation and versioning
   - Capability matching and ranking
   - Version compatibility checking
   - Codec and effect negotiation
   - Dynamic capability discovery

8. **ComponentSecurity.c** - Security and validation
   - Component signing and verification
   - Sandboxing and isolation
   - Security policy enforcement
   - Trust management
   - Audit logging

## API Compatibility

### Core Component Manager Functions

All original Mac OS Component Manager functions are implemented:

- `RegisterComponent()` / `UnregisterComponent()`
- `FindNextComponent()` / `CountComponents()`
- `OpenComponent()` / `CloseComponent()`
- `GetComponentInfo()` / `GetComponentListModSeed()`
- Component instance management functions
- Default component support
- Component capture/delegation

### Standard Component Selectors

- `kComponentOpenSelect` / `kComponentCloseSelect`
- `kComponentCanDoSelect` / `kComponentVersionSelect`
- `kComponentRegisterSelect` / `kComponentUnregisterSelect`
- `kComponentTargetSelect`

### Resource Format Support

- Mac OS 'thng' resources
- Extended component resources with platform info
- Cross-platform resource conversion
- Modern plugin descriptors

## Modern Extensions

### Cross-Platform Support

- **Windows**: PE DLL loading, COM integration, DirectShow compatibility
- **macOS**: Bundle loading, Core Audio/Video integration
- **Linux**: Shared object loading, GStreamer compatibility
- **Generic**: Plugin architecture abstraction

### Security Features

- Component signing and verification
- Sandboxed execution environments
- Permission-based access control
- Security audit logging
- Trust database management

### Performance Enhancements

- Component caching and lazy loading
- Hot-plugging and dynamic updates
- Multi-threaded operation
- Resource pooling and optimization

### Modern Integration

- **DirectShow** compatibility layer
- **GStreamer** plugin wrapper
- **FFmpeg** codec integration
- **GPU acceleration** support
- **Network components** and streaming

## Usage Examples

### Basic Component Registration

```c
#include "ComponentManager/ComponentManager.h"

// Initialize Component Manager
InitComponentManager();

// Register a component
ComponentDescription desc = {
    .componentType = 'vide',
    .componentSubType = 'mjpg',
    .componentManufacturer = 'myco',
    .componentFlags = 0,
    .componentFlagsMask = 0
};

Component myComponent = RegisterComponent(&desc, MyComponentEntry,
                                         0, nameHandle, infoHandle, iconHandle);
```

### Component Discovery and Usage

```c
// Find a video codec
ComponentDescription searchDesc = {
    .componentType = 'vide',
    .componentSubType = kAnyComponentSubType,
    .componentManufacturer = kAnyComponentManufacturer,
    .componentFlags = 0,
    .componentFlagsMask = 0
};

Component codec = FindNextComponent(NULL, &searchDesc);
if (codec) {
    ComponentInstance instance = OpenComponent(codec);
    // Use the component...
    CloseComponent(instance);
}
```

### Cross-Platform Component Loading

```c
// Scan for components in a directory
ScanForComponents("/usr/lib/myapp/components", true);

// Register components from a modern plugin
Component plugin = LoadNativeComponent("mycodec.dll");
```

## Build Configuration

### Conditional Compilation

The implementation supports various build configurations:

- `COMPONENTMGR_SECURITY_ENABLED` - Enable security features
- `COMPONENTMGR_RESOURCES_ENABLED` - Enable resource loading
- `COMPONENTMGR_NETWORK_ENABLED` - Enable network components
- Platform-specific defines for Windows, macOS, Linux

### Dependencies

- Standard C library
- Platform-specific APIs for dynamic loading
- Optional: OpenSSL for security features
- Optional: Platform multimedia frameworks

## Integration with System 7.1 Portable

This Component Manager integrates seamlessly with other System 7.1 Portable components:

- **Resource Manager** for resource loading
- **Memory Manager** for handle-based memory management
- **QuickTime** for multimedia component support
- **Toolbox** for general Mac OS API compatibility

## Threading and Synchronization

The implementation is fully thread-safe with:

- Per-component and per-instance mutexes
- Global registry synchronization
- Atomic operations for reference counting
- Deadlock prevention mechanisms

## Error Handling

Comprehensive error handling includes:

- Standard Mac OS error codes
- Extended error information
- Component-specific error reporting
- Security violation detection
- Graceful degradation on failures

## Testing and Validation

The implementation includes:

- Unit tests for all major functions
- Integration tests with real components
- Security validation tests
- Performance benchmarks
- Cross-platform compatibility tests

This Component Manager implementation provides a robust, secure, and modern foundation for multimedia application development while maintaining complete compatibility with the original Mac OS API.