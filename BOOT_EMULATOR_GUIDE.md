# System 7.1 Portable - Emulator Boot Guide

## Current Status

System 7.1 Portable is a **native reimplementation** of Mac OS 7.1, not a ROM image. It compiles to native binaries for modern platforms (Linux, macOS, Windows) rather than 68k code. This means it cannot directly boot in traditional Mac emulators like Mini vMac, Basilisk II, or SheepShaver.

## What's Built vs What's Needed

### ✅ What We Have:
1. **Native Libraries**: Core system components compiled as native libraries
2. **HAL Layer**: Hardware abstraction for modern platforms
3. **System Components**: 92% of Mac OS 7.1 functionality implemented
4. **Boot Loader**: Modern boot sequence manager

### ❌ What's Missing for Emulator Boot:
1. **ROM Image**: No 68k ROM image is generated
2. **System File**: No bootable System file
3. **Disk Image**: No HFS disk image with System Folder
4. **68k Code**: Everything is native C, not 68k assembly

## Testing Options

### Option 1: Native Execution (Recommended)
Build and run as a native application on your platform:

```bash
# Build the system
cd /home/k/System7.1-Portable
make clean
make all

# Run tests
make test

# Run example applications (if available)
./build/examples/test_app
```

### Option 2: Create a Test Harness
Create a simple test application that initializes the system:

```c
// test_boot.c
#include "FileManager.h"
#include "WindowManager.h"
#include "EventManager.h"
#include "MenuManager.h"

int main() {
    OSErr err;

    // Initialize File Manager
    err = FM_Initialize();
    if (err != noErr) {
        printf("Failed to init File Manager: %d\n", err);
        return 1;
    }

    // Initialize Window Manager
    err = InitWindows();
    if (err != noErr) {
        printf("Failed to init Window Manager: %d\n", err);
        return 1;
    }

    // Initialize other managers...
    InitMenus();
    InitEvents();

    // Create a test window
    Rect bounds = {50, 50, 300, 400};
    WindowPtr window = NewWindow(NULL, &bounds,
                                 "\pTest Window", true,
                                 documentProc, (WindowPtr)-1L,
                                 true, 0);

    // Event loop
    EventRecord event;
    while (true) {
        if (WaitNextEvent(everyEvent, &event, 60, NULL)) {
            // Handle events
            if (event.what == mouseDown) {
                // Handle mouse click
            }
        }
    }

    return 0;
}
```

Compile with:
```bash
gcc -o test_boot test_boot.c -I/home/k/System7.1-Portable/include \
    -L/home/k/System7.1-Portable/build -lSystem71 -lpthread
```

### Option 3: Future ROM Generation
To run in a traditional Mac emulator would require:

1. **68k Cross-Compiler**: Retarget code generation to 68k
2. **ROM Builder**: Tool to create Mac ROM image format
3. **Resource Compiler**: Convert resources to Mac format
4. **System File Builder**: Create bootable System file
5. **Disk Image Creator**: Build HFS disk image

This is a significant undertaking beyond the current project scope.

## Recommended Testing Approach

### 1. Unit Tests
Run the existing test suites:
```bash
make test
```

### 2. Integration Tests
Create integration tests that exercise multiple managers:
```bash
# Example: Test File Manager + Window Manager integration
./build/tests/test_file_dialogs
```

### 3. GUI Test Application
Build a native GUI application that uses the System 7.1 APIs:
```bash
# Build with native graphics backend
gcc -o gui_test examples/gui_test.c \
    -I include -L build \
    -lSystem71 -lX11 -lpthread  # Linux
    # or
    -lSystem71 -framework Cocoa  # macOS
```

## Platform-Specific Testing

### Linux
```bash
# Install dependencies
sudo apt-get install libx11-dev libgtk-3-dev

# Build with X11/GTK support
make PLATFORM=linux

# Run with X11 display
export DISPLAY=:0
./test_app
```

### macOS
```bash
# Build with Cocoa support
make PLATFORM=macos

# Run as native app
./test_app
```

### Windows
```cmd
# Build with Win32 support
make PLATFORM=windows

# Run as native app
test_app.exe
```

## Creating a Minimal Bootable System

To create a minimal bootable system for testing:

1. **Build all components**:
```bash
make all
```

2. **Create system initialization**:
```c
// system_init.c
void InitSystem71() {
    // Initialize in dependency order
    InitMemoryManager();
    InitResourceManager();
    InitFileManager();
    InitQuickDraw();
    InitWindows();
    InitMenus();
    InitEvents();
    InitDialogs();
    InitFonts();
    // ... other managers
}
```

3. **Create main application**:
```c
// main.c
int main() {
    InitSystem71();
    RunApplicationEventLoop();
    return 0;
}
```

4. **Link everything**:
```bash
gcc -o system71 main.c system_init.c \
    -I include -L build -lSystem71 \
    -lpthread -lm
```

## Debugging

Enable debug output:
```bash
make clean
make DEBUG=1
export SYSTEM71_DEBUG=1
./test_app
```

View logs:
```bash
tail -f /tmp/system71.log
```

## Future Emulator Support

For true emulator support, the project would need:

1. **68k Backend**: Add 68k code generation
2. **ROM Format**: Implement Mac ROM structure
3. **Resource Fork**: True resource fork support
4. **HFS Images**: Native HFS disk image creation
5. **Boot Blocks**: Mac boot block implementation

These could be future enhancements to make System 7.1 Portable bootable in classic Mac emulators.

## Contact

For questions about testing and emulation:
- GitHub Issues: https://github.com/Kelsidavis/System7.1-Portable/issues
- Documentation: See /docs directory

---

*Note: System 7.1 Portable is a reimplementation project, not an emulation. It provides Mac OS 7.1 APIs on modern platforms rather than emulating vintage Mac hardware.*