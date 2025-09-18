# Desk Manager - System 7.1 Portable Implementation

This directory contains a complete portable C implementation of the Apple Macintosh System 7.1 Desk Manager, providing essential daily utility desk accessories that Mac users depend on.

## Overview

The Desk Manager provides a framework for loading, managing, and interacting with desk accessories (DAs) - small utility programs that integrate with the Mac OS system menu and provide specific functionality like calculation, character mapping, time management, and device selection.

## Architecture

### Core Components

- **DeskManagerCore.c** - Main DA management and system integration
- **DALoader.c** - DA resource loading and registry management
- **DAWindows.c** - Window management for DAs
- **SystemMenu.c** - Apple menu integration
- **BuiltinDAs.c** - Built-in DA registration and interface

### Built-in Desk Accessories

1. **Calculator.c** - Complete calculator with:
   - Basic arithmetic operations (+, -, ×, ÷)
   - Scientific functions (sin, cos, tan, log, sqrt, etc.)
   - Programmer mode with hex/binary/octal support
   - Memory operations (store, recall, clear)
   - Calculation history
   - Keyboard and mouse input

2. **KeyCaps.c** - Keyboard layout display with:
   - Visual keyboard representation
   - Character mapping for different modifier keys
   - Multiple keyboard layouts support
   - Dead key handling for accented characters
   - Character insertion into target applications
   - Unicode support for extended characters

3. **AlarmClock.c** - Time and alarm management with:
   - Real-time clock display
   - Multiple alarm support with repeat patterns
   - System notification integration
   - 12/24 hour format support
   - Date display in multiple formats
   - Sound and visual alarm notifications

4. **Chooser.c** - Device and printer selection with:
   - Network device discovery
   - AppleTalk zone browsing
   - Printer configuration and status
   - Device driver management
   - Background device scanning
   - Connection testing and validation

## Key Features

### Mac OS Compatibility
- Exact API compatibility with System 7.1 Desk Manager
- Original OpenDeskAcc/CloseDeskAcc function signatures
- System event routing (SystemEvent, SystemClick, SystemTask)
- Apple menu integration with SystemMenu
- Edit operation support (cut, copy, paste, undo)

### Modern Enhancements
- Platform abstraction for cross-platform compatibility
- Unicode character support in Key Caps
- Scientific calculator functions beyond original
- Enhanced alarm notification system
- Modern network device discovery in Chooser
- High-DPI display support
- Accessibility features

### Resource Management
- DA registry system for extensibility
- Resource loading abstraction
- Memory management with proper cleanup
- Window creation and management
- Event handling and routing

## API Usage

### Basic DA Operations

```c
#include "DeskManager/DeskManager.h"

// Initialize the Desk Manager
DeskManager_Initialize();

// Open a desk accessory
int16_t refNum = OpenDeskAcc("Calculator");
if (refNum > 0) {
    printf("Calculator opened with ref num: %d\n", refNum);
}

// Handle system events
EventRecord event;
if (SystemEvent(&event)) {
    printf("Event handled by DA\n");
}

// Periodic processing
SystemTask();

// Close desk accessory
CloseDeskAcc(refNum);

// Shutdown
DeskManager_Shutdown();
```

### Calculator Usage

```c
#include "DeskManager/Calculator.h"

Calculator calc;
Calculator_Initialize(&calc);

// Perform calculation: 2 + 3 = 5
Calculator_EnterDigit(&calc, 2);
Calculator_PerformOperation(&calc, CALC_OP_ADD);
Calculator_EnterDigit(&calc, 3);
Calculator_PerformOperation(&calc, CALC_OP_EQUALS);

printf("Result: %s\n", Calculator_GetDisplay(&calc));
```

### Key Caps Usage

```c
#include "DeskManager/KeyCaps.h"

KeyCaps keyCaps;
KeyCaps_Initialize(&keyCaps);

// Get character for key with modifiers
uint16_t ch = KeyCaps_GetCharForKey(&keyCaps, 30, MOD_SHIFT | MOD_OPTION);
printf("Character: %c\n", (char)ch);

// Handle keyboard layout changes
KeyCaps_SetModifiers(&keyCaps, MOD_OPTION);
```

### Alarm Clock Usage

```c
#include "DeskManager/AlarmClock.h"

AlarmClock clock;
AlarmClock_Initialize(&clock);

// Create alarm for 9:00 AM daily
DateTime triggerTime;
triggerTime.time.hour = 9;
triggerTime.time.minute = 0;
triggerTime.time.second = 0;

Alarm *alarm = AlarmClock_CreateAlarm(&clock, "Morning Alarm",
                                      &triggerTime, ALARM_TYPE_DAILY);

// Check for triggered alarms
AlarmClock_UpdateTime(&clock);
int triggered = AlarmClock_CheckAlarms(&clock);
```

### Chooser Usage

```c
#include "DeskManager/Chooser.h"

Chooser chooser;
Chooser_Initialize(&chooser);

// Scan for printers
int found = Chooser_ScanDevices(&chooser, DEVICE_TYPE_PRINTER);
printf("Found %d printers\n", found);

// Select default printer
Chooser_SetDefaultPrinter(&chooser, "LaserWriter");

DeviceInfo *printer = Chooser_GetDefaultPrinter(&chooser);
if (printer) {
    printf("Default printer: %s\n", printer->name);
}
```

## File Structure

```
DeskManager/
├── DeskManagerCore.c       # Core DA management
├── DALoader.c              # Resource loading and registry
├── DAWindows.c             # Window management
├── SystemMenu.c            # Apple menu integration
├── BuiltinDAs.c           # Built-in DA registration
├── Calculator.c            # Calculator implementation
├── KeyCaps.c              # Key Caps implementation
├── AlarmClock.c           # Alarm Clock implementation
├── Chooser.c              # Chooser implementation
└── README.md              # This file

include/DeskManager/
├── DeskManager.h          # Main API header
├── DeskAccessory.h        # DA structures and interfaces
├── Calculator.h           # Calculator API
├── KeyCaps.h             # Key Caps API
├── AlarmClock.h          # Alarm Clock API
├── Chooser.h             # Chooser API
└── Types.h               # Common type definitions
```

## Building

The Desk Manager is designed to be integrated into a larger System 7.1 portable implementation. To build:

1. Include the header directory in your include path
2. Compile all .c files in the DeskManager directory
3. Link with math library (-lm) for Calculator scientific functions
4. Link with time library for AlarmClock functionality

Example GCC build:
```bash
gcc -I../include -c *.c
ar rcs libdeskmanager.a *.o
```

## Platform Integration

### Event System Integration
The Desk Manager expects to be integrated with a platform-specific event system that can:
- Deliver mouse and keyboard events
- Handle window management
- Provide timer services for periodic updates
- Support sound playback for alarms

### Window System Integration
DA windows are abstracted through the DAWindows system, which needs platform-specific implementation for:
- Window creation and destruction
- Drawing and update regions
- Mouse and keyboard event routing
- Window activation and focus management

### Menu System Integration
The SystemMenu component provides Apple menu integration but requires platform-specific implementation for:
- Menu bar creation and updates
- Menu item management
- Menu selection handling

## Error Handling

All functions return appropriate error codes:
- `DESK_ERR_NONE` (0) - Success
- `DESK_ERR_NO_MEMORY` (-1) - Out of memory
- `DESK_ERR_NOT_FOUND` (-2) - DA not found
- `DESK_ERR_ALREADY_OPEN` (-3) - DA already open
- `DESK_ERR_INVALID_PARAM` (-4) - Invalid parameter
- `DESK_ERR_SYSTEM_ERROR` (-5) - System error
- `DESK_ERR_DA_ERROR` (-6) - DA-specific error

## Thread Safety

The current implementation is not thread-safe and assumes single-threaded access. For multi-threaded environments, add appropriate synchronization around:
- DA registry operations
- Window list management
- System menu updates
- Alarm checking and notification

## Memory Management

The implementation uses standard C memory management:
- All allocations use malloc/calloc
- All deallocations use free
- Proper cleanup in shutdown functions
- No memory leaks in normal operation

## Testing

Each DA can be tested independently:

```c
// Test Calculator
Calculator calc;
assert(Calculator_Initialize(&calc) == CALC_ERR_NONE);
Calculator_EnterDigit(&calc, 5);
Calculator_PerformOperation(&calc, CALC_OP_SQUARE);
assert(strcmp(Calculator_GetDisplay(&calc), "25") == 0);
Calculator_Shutdown(&calc);
```

## Future Enhancements

Potential areas for future development:
1. **Plugin System** - Dynamic DA loading from external libraries
2. **Network Integration** - Enhanced network device discovery
3. **Scripting Support** - AppleScript automation for DAs
4. **Accessibility** - Screen reader and keyboard navigation support
5. **Theming** - Customizable appearance and layouts
6. **Localization** - Multi-language support for all DAs

## Historical Context

This implementation preserves the essential functionality and user experience of the original Mac OS System 7.1 desk accessories while providing a foundation for modern portable computing environments. The desk accessories were a crucial part of the Mac user experience, providing always-available utilities that enhanced daily productivity.

The original implementations were written in 68K assembly language and Pascal, integrated tightly with the Mac OS Toolbox. This portable C implementation maintains API compatibility while providing the flexibility to run on modern platforms and architectures.

## Contributing

When extending or modifying the Desk Manager:

1. Maintain API compatibility with original System 7.1 functions
2. Follow the established error handling patterns
3. Ensure proper memory management and cleanup
4. Add appropriate documentation for new features
5. Test thoroughly with all built-in DAs
6. Consider backwards compatibility and platform portability

The Desk Manager serves as both a functional utility system and a demonstration of how classic Mac OS functionality can be preserved and enhanced in modern portable implementations.