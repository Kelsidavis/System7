# System 7.1 Package Manager

This directory contains the complete implementation of the Mac OS Package Manager system for System 7.1 Portable. The Package Manager provides essential functionality that many Mac applications depend on for basic operations.

## Critical Importance

The Package Manager is **ESSENTIAL** for Mac application compatibility. Without it, applications will:
- Crash on floating point operations (SANE package)
- Fail to display file dialogs (Standard File package)
- Have broken string comparisons (International Utilities)
- Cannot display scrollable lists (List Manager)

## Implemented Packages

### SANE Package (Pack 4/5)
- Complete IEEE 754 floating point arithmetic
- Transcendental functions (sin, cos, tan, log, exp, sqrt, etc.)
- Exception handling and environment control
- Binary/decimal conversion
- Financial functions

### Standard File Package (Pack 3)
- File open/save dialogs
- Platform-independent file selection
- Directory navigation
- File type filtering
- Volume management

### International Utilities Package (Pack 6)
- String comparison and sorting
- Date/time formatting
- Number formatting
- Script and language support
- Text encoding conversion

### List Manager Package (Pack 0)
- Scrollable list display
- Cell selection and navigation
- Data management
- Keyboard navigation
- Platform drawing abstraction

## File Structure

```
PackageManager/
├── PackageManagerCore.c      # Core package loading and dispatch
├── SANEPackage.c            # SANE floating point implementation
├── StringPackage.c          # International Utilities
├── StandardFilePackage.c    # File dialog implementation
├── ListManagerPackage.c     # List management
├── PackageDispatch.c        # Central dispatch system
└── CMakeLists.txt          # Build configuration
```

## Key Features

- **Mac OS Compatibility**: Exact API compatibility with original packages
- **Thread Safety**: Mutex protection for multi-threaded applications
- **Platform Integration**: Abstraction layer for native platform features
- **Modern Math**: Uses IEEE 754 standard with Mac OS compatibility layer
- **Memory Management**: Efficient memory usage with cleanup
- **Error Handling**: Comprehensive error reporting and recovery

## Usage

### Initialization
```c
#include "PackageManager/PackageManager.h"

// Initialize Package Manager
InitPackageManager();

// Initialize all packages
InitAllPacks();

// Or initialize specific packages
InitPack(flPoint);  // SANE
InitPack(stdFile);  // Standard File
InitPack(intUtil);  // International Utilities
InitPack(listMgr);  // List Manager
```

### SANE Example
```c
#include "PackageManager/SANEPackage.h"

// Basic math operations
double result = sane_add(2.5, 3.7);
double sqrtResult = sane_sqrt(16.0);
double sinResult = sane_sin(M_PI / 2);

// Transcendental functions
double logResult = sane_log(10.0);
double expResult = sane_exp(1.0);
double powResult = sane_pow(2.0, 8.0);
```

### Standard File Example
```c
#include "PackageManager/StandardFilePackage.h"

// Open file dialog
StandardFileReply reply;
StandardGetFile(NULL, 0, NULL, &reply);
if (reply.sfGood) {
    // User selected a file
    char filename[256];
    SF_FSSpecToPath(&reply.sfFile, filename, sizeof(filename));
    printf("Selected: %s\n", filename);
}

// Save file dialog
Str255 prompt = "\pSave document as:";
Str255 defaultName = "\pUntitled";
StandardPutFile(prompt, defaultName, &reply);
```

### String Utilities Example
```c
#include "PackageManager/StringPackage.h"

// String comparison
int result = IUCompString("Hello", "World");

// Date formatting
char dateStr[64];
IUDateString(time(NULL), shortDate, dateStr);

// Number formatting
char numStr[32];
NumToString(12345, numStr);
```

### List Manager Example
```c
#include "PackageManager/ListManagerPackage.h"

// Create list
Rect bounds = {0, 0, 200, 100};
Rect dataBounds = {0, 0, 50, 1};
Point cellSize = {100, 16};
ListHandle list = LNew(&bounds, &dataBounds, cellSize, 0, NULL,
                      true, false, false, true);

// Add data to cells
Cell cell = {0, 0};
LSetCell("Item 1", 6, cell, list);
cell.v = 1;
LSetCell("Item 2", 6, cell, list);

// Draw list
LDoDraw(true, list);

// Cleanup
LDispose(list);
```

## Platform Integration

The Package Manager provides hooks for platform-specific functionality:

```c
// Set platform math library
SetPlatformMathLibrary(platformMathFunctions);

// Set platform file dialogs
PlatformFileDialogs dialogs = {
    .ShowOpenDialog = MyOpenDialog,
    .ShowSaveDialog = MySaveDialog,
    .SetDialogParent = MySetParent
};
SF_SetPlatformFileDialogs(&dialogs);

// Set platform drawing callbacks
ListPlatformCallbacks callbacks = {
    .InvalidateRect = MyInvalidateRect,
    .ScrollRect = MyScrollRect
};
LSetPlatformCallbacks(&callbacks);
```

## Thread Safety

The Package Manager supports thread-safe operation:

```c
// Enable thread safety
SetPackageThreadSafe(true);

// Manual locking (if needed)
LockPackageManager();
// ... perform package operations ...
UnlockPackageManager();
```

## Building

The Package Manager builds as part of the System 7.1 Portable project:

```bash
mkdir build && cd build
cmake ..
make PackageManager
```

## Debugging

Enable debug output:

```c
SetPackageDebug(true);
```

Run self-tests:

```c
Boolean success = TestPackageSystem();
```

List available packages:

```c
ListAvailablePackages();
```

## Notes

- All package APIs maintain exact compatibility with Mac OS System 7.1
- SANE package provides IEEE 754 compliance with Mac OS compatibility layer
- Standard File package provides console fallbacks for platforms without GUI
- List Manager includes platform abstraction for custom drawing
- Memory management follows Mac OS patterns with modern safety improvements

This implementation ensures that Mac applications can run with full functionality on modern platforms while maintaining the exact behavior they expect from the original Mac OS Package Manager system.