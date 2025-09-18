# Apple Event Manager for System 7.1 Portable

A complete implementation of the Apple Event Manager providing inter-application communication, AppleScript support, and automation capabilities for modern platforms.

## Overview

The Apple Event Manager is a powerful system for enabling applications to communicate with each other through structured messages called Apple Events. This implementation provides:

- **Complete Mac OS 7.1 API compatibility** - All original Apple Event Manager functions
- **Modern platform support** - Works on Linux, macOS, Windows, and other POSIX systems
- **AppleScript integration** - Full support for AppleScript compilation and execution
- **Event recording and playback** - Automation capabilities with macro support
- **Object model support** - Hierarchical object addressing and manipulation
- **Process targeting** - Sophisticated inter-application communication
- **Thread-safe operation** - Modern concurrency support

## Architecture

### Core Components

1. **AppleEventManagerCore.c** - Fundamental Apple Event operations
2. **EventDescriptors.c** - Data structure manipulation and coercion
3. **EventHandlers.c** - Event routing and handler management
4. **AppleScript.c** - AppleScript engine integration
5. **ObjectModel.c** - Object specifier resolution
6. **ProcessTargeting.c** - Process discovery and addressing
7. **EventRecording.c** - Automation and scripting support

### Key Features

#### Apple Event Fundamentals
- Apple Event creation, sending, and receiving
- Descriptor creation and manipulation (lists, records, basic types)
- Built-in type coercion (text, numbers, booleans, etc.)
- Memory management with handle-based storage

#### Handler Management
- Event handler registration and dispatch
- Coercion handler installation
- Pre/post-dispatch hooks
- Error handling and recovery
- Performance monitoring

#### AppleScript Support
- Script compilation and execution
- Variable and property access
- Handler invocation
- Debugging and error reporting
- Recording and playback integration

#### Object Model
- Object specifier creation and resolution
- Hierarchical object addressing
- Property and element access
- Accessibility support
- Application object registration

#### Process Communication
- Process discovery and enumeration
- Multiple addressing modes (PSN, signature, name, path, PID)
- Application launching and control
- Network and remote process support
- Security and permissions

#### Automation Framework
- Event recording and playback
- Macro creation and execution
- Script generation (AppleScript, JavaScript, Python)
- Workflow integration
- Task scheduling

## API Reference

### Basic Apple Event Operations

```c
// Initialize the Apple Event Manager
OSErr AEManagerInit(void);

// Create an Apple Event descriptor
OSErr AECreateDesc(DescType typeCode, const void* dataPtr,
                   Size dataSize, AEDesc* result);

// Create an Apple Event
OSErr AECreateAppleEvent(AEEventClass theAEEventClass,
                         AEEventID theAEEventID,
                         const AEAddressDesc* target,
                         int16_t returnID, int32_t transactionID,
                         AppleEvent* result);

// Send an Apple Event
OSErr AESend(const AppleEvent* theAppleEvent, AppleEvent* reply,
             AESendMode sendMode, AESendPriority sendPriority,
             int32_t timeOutInTicks, IdleProcPtr idleProc,
             EventFilterProcPtr filterProc);
```

### Event Handler Registration

```c
// Install an event handler
OSErr AEInstallEventHandler(AEEventClass theAEEventClass,
                           AEEventID theAEEventID,
                           EventHandlerProcPtr handler,
                           int32_t handlerRefcon,
                           bool isSysHandler);

// Handler function prototype
typedef OSErr (*EventHandlerProcPtr)(const AppleEvent* theAppleEvent,
                                    AppleEvent* reply,
                                    int32_t handlerRefcon);
```

### AppleScript Integration

```c
// Initialize AppleScript
OSErr OSAInit(void);

// Compile a script
OSErr OSACompile(OSAComponentInstance scriptingComponent,
                 const char* sourceText,
                 OSAExecutionMode modeFlags,
                 OSAScript* resultScript);

// Execute a script
OSErr OSAExecute(OSAComponentInstance scriptingComponent,
                 OSAScript script,
                 OSAScript contextScript,
                 OSAExecutionMode modeFlags,
                 AEDesc* result);
```

### Process Targeting

```c
// Get list of running processes
OSErr AEGetProcessList(ProcessInfo** processList, int32_t* processCount);

// Find process by name
OSErr AEFindProcessByName(const char* processName,
                          ProcessSerialNumber* psn);

// Create address descriptor for process
OSErr AECreateProcessAddressDesc(const ProcessSerialNumber* psn,
                                AEAddressDesc* result);

// Launch an application
OSErr AELaunchApplication(const LaunchParams* launchParams,
                         ProcessSerialNumber* launchedPSN);
```

### Event Recording

```c
// Create recording session
OSErr AECreateRecordingSession(AERecordingSession* session);

// Start recording
OSErr AEStartRecording(AERecordingSession session,
                       AERecordingMode mode);

// Generate script from recording
OSErr AEGenerateScriptFromRecording(AERecordingSession session,
                                   const AEScriptGenerationOptions* options,
                                   char** scriptText, Size* scriptSize);
```

## Usage Examples

### Basic Application Communication

```c
#include "AppleEventManager/AppleEventManager.h"

int main() {
    // Initialize
    AEManagerInit();

    // Find target application
    ProcessSerialNumber finderPSN;
    AEFindProcessByName("Finder", &finderPSN);

    // Create address for Finder
    AEAddressDesc finderAddr;
    AECreateProcessAddressDesc(&finderPSN, &finderAddr);

    // Create "Get Info" event
    AppleEvent getInfoEvent;
    AECreateAppleEvent('FNDR', 'info', &finderAddr,
                       kAutoGenerateReturnID, kAnyTransactionID,
                       &getInfoEvent);

    // Send event and get reply
    AppleEvent reply;
    AECreateList(NULL, 0, true, &reply);

    OSErr err = AESend(&getInfoEvent, &reply, kAEWaitReply,
                       kAENormalPriority, kAEDefaultTimeout,
                       NULL, NULL);

    if (err == noErr) {
        // Process reply...
    }

    // Cleanup
    AEDisposeDesc(&getInfoEvent);
    AEDisposeDesc(&reply);
    AEDisposeDesc(&finderAddr);
    AEManagerCleanup();
}
```

### Installing Event Handlers

```c
// Handler for "Open Documents" events
OSErr HandleOpenDocuments(const AppleEvent* event, AppleEvent* reply,
                          int32_t refcon) {
    // Extract document list
    AEDescList docList;
    AEGetParamDesc(event, keyDirectObject, typeAEList, &docList);

    int32_t docCount;
    AECountItems(&docList, &docCount);

    // Process each document
    for (int32_t i = 1; i <= docCount; i++) {
        AEDesc docDesc;
        AEKeyword keyword;
        AEGetNthDesc(&docList, i, typeAlias, &keyword, &docDesc);

        // Open document...

        AEDisposeDesc(&docDesc);
    }

    AEDisposeDesc(&docList);
    return noErr;
}

// Install the handler
AEInstallEventHandler(kCoreEventClass, kAEOpenDocuments,
                     HandleOpenDocuments, 0, false);
```

### AppleScript Execution

```c
#include "AppleEventManager/AppleScript.h"

void ExecuteAppleScript() {
    OSAInit();

    OSAComponentInstance component;
    OSAOpenDefaultComponent(&component);

    // Compile script
    const char* source = "tell application \"Finder\" to get name";
    OSAScript script;
    OSACompile(component, source, kOSAModeNull, &script);

    // Execute script
    AEDesc result;
    OSAExecute(component, script, NULL, kOSAModeNull, &result);

    // Convert result to text
    char* resultText;
    Size resultSize;
    AECoerceToText(&result, &resultText, &resultSize);
    printf("Result: %.*s\n", (int)resultSize, resultText);

    // Cleanup
    free(resultText);
    AEDisposeDesc(&result);
    OSADisposeScript(script);
    OSACloseComponent(component);
    OSACleanup();
}
```

### Event Recording and Automation

```c
#include "AppleEventManager/EventRecording.h"

void RecordUserActions() {
    // Create recording session
    AERecordingSession session;
    AECreateRecordingSession(&session);

    // Configure recording
    AERecordingOptions options = {
        .recordingMode = kAERecordUserActions | kAERecordApplicationEvents,
        .includeTimestamps = true,
        .maxRecordingTime = 60 // seconds
    };
    AESetRecordingOptions(session, &options);

    // Start recording
    printf("Recording user actions...\n");
    AEStartRecording(session, options.recordingMode);

    // ... user performs actions ...
    sleep(10); // Record for 10 seconds

    // Stop recording
    AEStopRecording(session);

    // Generate AppleScript
    char* script;
    Size scriptSize;
    AEScriptGenerationOptions scriptOptions = {
        .scriptLanguage = "AppleScript",
        .includeComments = true,
        .optimizeForReadability = true
    };

    AEGenerateScriptFromRecording(session, &scriptOptions,
                                 &script, &scriptSize);

    printf("Generated script:\n%.*s\n", (int)scriptSize, script);

    // Save recording
    AESaveRecording(session, "automation.aer", kAERecordingFormatBinary);

    // Cleanup
    free(script);
    AEDisposeRecordingSession(session);
}
```

## Building and Integration

### Prerequisites

- C99-compatible compiler (GCC, Clang, MSVC)
- POSIX-compliant system for full functionality
- pthread library for thread safety

### Compilation

```bash
# Compile individual components
gcc -c -std=c99 -pthread AppleEventManagerCore.c
gcc -c -std=c99 -pthread EventDescriptors.c
gcc -c -std=c99 -pthread EventHandlers.c
# ... other components

# Link example application
gcc -o AppleEventExample AppleEventExample.c \
    *.o -pthread -lm
```

### CMake Integration

```cmake
# Add Apple Event Manager to your project
add_library(AppleEventManager
    AppleEventManagerCore.c
    EventDescriptors.c
    EventHandlers.c
    AppleScript.c
    ObjectModel.c
    ProcessTargeting.c
    EventRecording.c
)

target_include_directories(AppleEventManager PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
)

target_link_libraries(AppleEventManager pthread m)

# Link to your application
target_link_libraries(YourApp AppleEventManager)
```

## Platform-Specific Features

### Linux
- D-Bus integration for system-wide messaging
- Desktop environment integration
- Process discovery via /proc filesystem

### macOS
- Native Apple Event support with compatibility layer
- XPC service integration
- Accessibility framework support

### Windows
- COM object integration
- Windows message queue integration
- Process discovery via Windows API

## Error Handling

The Apple Event Manager uses standard Mac OS error codes:

```c
#define noErr                    0
#define errAECoercionFail       -1700
#define errAEDescNotFound       -1701
#define errAECorruptData        -1702
#define errAEWrongDataType      -1703
#define errAEEventNotHandled    -1708
#define errAEHandlerNotFound    -1717
```

Always check return values and handle errors appropriately:

```c
OSErr err = AECreateDesc(typeChar, data, size, &desc);
if (err != noErr) {
    fprintf(stderr, "Error creating descriptor: %d\n", err);
    return err;
}
```

## Performance Considerations

- **Memory Management**: Always dispose of descriptors and handles
- **Handler Registration**: Install handlers once during initialization
- **Event Batching**: Group related operations for efficiency
- **Recording Size**: Limit recording duration and size for large automations

## Thread Safety

The Apple Event Manager is thread-safe with these considerations:

- Event handlers may be called from any thread
- Use appropriate locking for shared application state
- Recording sessions are not thread-safe per session
- Multiple recording sessions can run concurrently

## Debugging

Enable debug output by defining `DEBUG`:

```c
#define DEBUG 1
#include "AppleEventManager/AppleEventManager.h"

// Debug functions become available
AEPrintDesc(&descriptor, "My Descriptor");
AEPrintDescList(&list, "My List");
```

## Migration from Classic Mac OS

This implementation maintains API compatibility with classic Mac OS:

1. **Include paths**: Update to use new header structure
2. **Memory management**: Same handle-based system
3. **Error codes**: Identical to original implementation
4. **Handler signatures**: Compatible with existing code

## License

This implementation is based on the leaked Mac OS 7.1 source code and is provided for research and educational purposes only.

## Contributing

When contributing to this implementation:

1. Maintain API compatibility with Mac OS 7.1
2. Follow the existing code style and patterns
3. Add comprehensive error checking
4. Include debug support where appropriate
5. Update documentation for new features

## See Also

- Mac OS 7.1 Apple Event Manager documentation
- Inside Macintosh: Interapplication Communication
- AppleScript Language Guide
- System 7.1 Portable documentation