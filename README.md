# System 7

<img width="793" height="657" alt="System 7 running on modern hardware" src="https://github.com/user-attachments/assets/be84b83e-191c-4f9d-a786-11d0bd04203b" />

> ⚠️ **PROOF OF CONCEPT** - This is an experimental, educational reimplementation of Apple's Macintosh System 7. This is NOT a finished product and should not be considered production-ready software.

An open-source reimplementation of Apple Macintosh System 7 for modern x86 hardware, bootable via GRUB2/Multiboot2. This project aims to recreate the classic Mac OS experience while documenting the System 7 architecture through reverse engineering analysis.

---

# ⚠️ **FUTURE BRANCH - EXPERIMENTAL NANOKERNEL BUILD**

**You are viewing the `future` branch** - This branch contains **experimental nanokernel threading infrastructure** and is **INCOMPATIBLE** with the stable `main` branch.

## Key Differences from Main Branch

### What Makes This Branch "Future"

This branch contains a complete **System 7.1 Nanokernel** implementation with preemptive multitasking - a significant architectural departure from the stable `main` branch:

**Threading & Scheduling** (7,700+ new lines):
- ✅ Full preemptive threading with hardware timer interrupts (IRQ0)
- ✅ Context switching with register save/restore (assembly ISRs)
- ✅ Cooperative scheduler with priority-based task management
- ✅ PIT (8254) timer integration for 100 Hz tick generation
- ✅ Physical memory manager (PMM) providing heap backend
- ✅ Complete Memory Manager integration with nanokernel allocations

**Interrupt Infrastructure**:
- ✅ IDT (Interrupt Descriptor Table) setup and management
- ✅ PIC (8259) programmable interrupt controller support
- ✅ IRQ0 handler with deferred scheduling for interrupt safety
- ✅ NMI (Non-Maskable Interrupt) handling and diagnostics

**Extensive Diagnostic Logging**:
- ✅ Thread context switch tracing
- ✅ IRQ handling instrumentation
- ✅ PS/2 stuck button detection
- ✅ Heap allocation tracking and validation
- ✅ Window Manager/Menu Manager detailed event logs

### Incompatibilities with Main

**Memory Architecture**:
- Main: Direct malloc/calloc for Memory Manager
- Future: Nanokernel provides physical memory, Memory Manager sits on top

**Boot Sequence**:
- Main: Simple initialization → Finder
- Future: Nanokernel init → Timer setup → IRQ0 enabled → Thread scheduler → Finder

**Interrupt Handling**:
- Main: Polled input (PS/2 status register checks)
- Future: IRQ-driven with deferred scheduling and interrupt safety

**Code Complexity**:
- Main: ~57,500 lines
- Future: ~65,200 lines (+7,700 lines of threading/scheduler/interrupt code)

### Why "Future" Branch Exists

This branch serves multiple purposes:

1. **Research Platform**: Tests advanced Mac OS 7.1 features (Blue Box, nanokernel threading)
2. **Architectural Experiments**: Explores preemptive vs cooperative multitasking trade-offs
3. **Performance Testing**: Hardware interrupts vs polling for input handling
4. **Educational Value**: Documents full System 7.1 nanokernel architecture

### Should You Use This Branch?

**Use `main` branch if you want**:
- ✅ Stable, production-ready code
- ✅ Clean, readable implementation
- ✅ Easier to understand and modify
- ✅ All critical bug fixes included

**Use `future` branch if you want**:
- 🔬 Experimental threading/scheduler implementation
- 🔬 Research-grade nanokernel architecture
- 🔬 IRQ-driven interrupt handling
- ⚠️ More complex, harder to debug
- ⚠️ Additional failure modes from interrupts

### Documentation

Extensive documentation for future branch features:
- `docs/kernel/interrupts.md` - 856 lines on interrupt subsystem
- `NANOKERNEL_THREADING.md` - Threading implementation details
- `INTERRUPT_SAFE_CONTEXT_SWITCHING.md` - Context switching design
- `IRQ_CONTEXT_SWITCH_FINAL.md` - IRQ handling architecture
- `NANOKERNEL_MIGRATION.md` - Migration guide from classic to nanokernel
- `src/Nanokernel/README.md` - Nanokernel API reference

---

## 🎯 Project Status

**Current State**: Active development, core functionality partially working

### Recent Updates

- ✅ **Memory Manager & OSUtils Integration**: Shared host/68K heap mapping with classic low-memory sync
  - `MemoryManager_MapToM68K()` maps the System/App zones directly into the interpreter page table
  - Low-memory globals (`MemTop`, `SysZone`, `ApplZone`) now track real heap limits
  - `_TickCount` trap wired through `OSUtils_InstallTraps()` with a 60 Hz Time Manager task
  - Segment loader tests install and tear down OSUtils services for clean runs
- ✅ **Menu Bar QuickDraw Polish**: Pixel-perfect Apple icon and title metrics in the menu bar
  - `MenuAppleIcon_Draw()` composites the 16×16 RGBA asset into the active `GrafPort`
  - Hardcoded titles now pass through `StringWidth` to pick QuickDraw-accurate hit regions
  - Apple menu uses the icon width for subsequent menu placement instead of constants
- ✅ **Hardware Abstraction Layer (HAL)**: Complete platform abstraction for multi-architecture support
  - Clean separation of platform-specific code from core System 7 implementation
  - Pluggable architecture: `src/Platform/{platform}/` directory structure
  - Four HAL interfaces: boot, I/O, storage, and input
  - x86 implementation complete and verified in QEMU
  - Ready for ARM (Raspberry Pi, Apple Silicon) and PowerPC ports
  - Build system with `PLATFORM` variable (default: x86)
  - Core code is now 100% platform-agnostic
  - Full documentation in `docs/future/PORTING_PLAN.md`
- ✅ **Portable 68K Segment Loader**: Complete ISA-agnostic segment loading system
  - Classic CODE resource parsing (CODE 0 metadata + CODE 1..N segments)
  - A5 world construction (below/above A5, jump table, QuickDraw globals)
  - Lazy segment loading with self-patching jump table stubs
  - Pluggable CPU backend interface (M68K interpreter, future PPC/native backends)
  - Big-endian safe parsing, abstract relocations, complete portability
  - Integrated with Process Manager for application launch flow
  - Full documentation in `docs/SegmentLoader_DESIGN.md`
- ✅ **Serial Logging System**: Comprehensive module-based logging framework
  - Hierarchical log levels (Error, Warn, Info, Debug, Trace)
  - Module-specific filtering (Window Manager, Control Manager, Dialog Manager, Event Manager, etc.)
  - Runtime control via `SysLogSetGlobalLevel()` and `SysLogSetModuleLevel()`
  - Module-specific macros: `WM_LOG_DEBUG()`, `CTRL_LOG_DEBUG()`, `DM_LOG_DEBUG()`, etc.
  - `serial_logf()` API for explicit module and level specification
  - Legacy `serial_printf()` support via bracket tag parsing `[WM]`, `[CTRL]`, `[DM]`
  - 100% migration complete: All 624+ calls now use module-specific logging
  - Full documentation in `docs/components/System/Logging.md`
- ✅ **Keyboard Navigation Integration**: Complete System 7-style keyboard focus and navigation
  - Dialog Manager focus tracking with XOR toggle pattern for focus rings
  - Tab/Shift+Tab traversal with intelligent focus filtering (skips invisible/disabled/zero-sized controls)
  - Space key activation for checkboxes and radio buttons
  - Return key activates default button, Escape activates cancel button
  - Debounce mechanism (100ms window) prevents mouse+keyboard double-fire
  - Window activation/deactivation suspends/restores keyboard focus
  - Focus ring restoration after Draw1Control redraws
  - Automatic cleanup on window/control disposal (no ghost XOR rings)
  - StandardFile dialog integration with initial focus priming
  - Event dispatcher activation hooks for cross-window focus management
  - Alert & Modal Dialog keyboard integration fully implemented
  - Full documentation in `docs/components/DialogManager/KeyboardIntegration.md`
- ✅ **Scrollbar Controls Implementation**: Complete System 7-style scrollbar controls with classic Mac semantics
  - NewVScrollBar/NewHScrollBar creation functions with auto-orientation detection
  - Full CDEF implementation: drawing, hit-testing, tracking, and highlighting
  - Proportional thumb sizing based on visible content span
  - Repeat-on-hold behavior for arrows (8/3 tick timing) and page areas (8/4 tick timing)
  - Delta-based tracking with TrackScrollbar() returning net value change
  - List Manager integration via UpdateScrollThumb() for atomic updates
  - Disabled state handling (max <= min or inactiveHilite)
  - Smart routing from generic TrackControl() to delta-based scrollbar tracking
- ✅ **List Manager Implementation**: Complete System 7.1-compatible List Manager with full API support
  - 20-function API: LNew, LDispose, LAddRow, LDelRow, LSetCell, LGetCell, LUpdate, LDraw, LClick, LKey, etc.
  - Single and multi-selection modes with Shift/Cmd modifier support
  - Keyboard navigation (arrow keys, PageUp/Down, Home/End)
  - Proper QuickDraw state management with port/pen/clip save/restore
  - Efficient redraw with narrow erase and band invalidation
  - Dialog Manager integration support for list dialog items
  - Cell-based storage with handle-based memory management
  - Production hardening: parameter validation, edge case handling, smoke tests
- ✅ **Menu Manager Dropdown Fix**: Fixed critical bug where menus wouldn't display dropdowns
  - Root cause: SaveBits/RestoreBits were stubs that didn't actually save/restore framebuffer pixels
  - TrackMenu() had no tracking loop - returned immediately after drawing menu
  - Fixed SaveBits to copy actual framebuffer pixels (32-bit RGBA) to save buffer
  - Rewrote TrackMenu() with complete mouse tracking loop using Button()/GetMouse()
  - Added live item highlighting during tracking with UpdateMenuTrackingNew()
  - Background properly saved before menu display and restored on exit
  - Menus now fully functional: dropdowns appear, items highlight on hover, selection works
  - Fixes Known Issue #1 - SimpleText menus now fully operational
- ✅ **Scrap Manager & SimpleText Integration**: Complete clipboard system with classic Mac OS API
  - Classic System 7.1 API: ZeroScrap, GetScrap, PutScrap, LoadScrap, UnloadScrap, InfoScrap
  - Multiple flavor support (TEXT, PICT, etc.) with handle-based memory management
  - Full TextEdit integration via TEFromScrap/TEToScrap
  - SimpleText application with functional Cut/Copy/Paste operations
  - Change counting for clipboard update detection
  - End-to-end clipboard stack: SimpleText → TextEdit API → Scrap Manager → Global Clipboard
- ✅ **SimpleText Application**: Complete text editor implementation
  - Full MDI support with multiple document windows
  - Complete menu system (Apple, File, Edit, Font, Size, Style)
  - TextEdit API integration for editing, selection, and clipboard operations
  - File I/O with type/creator codes (TEXT/ttxt)
  - Document state tracking (dirty flag, undo support)
  - Authentic System 7.1 UI with menu bar and document windows
- ✅ **Font Manager Implementation**: Complete System 7.1-compatible Font Manager
  - Core font management with InitFonts(), GetFontName(), TextFont(), TextFace(), TextSize()
  - Style synthesis: Bold (+1 pixel), Italic (1:4 shear), Underline, Shadow, Outline, Condense/Extend
  - Multiple font sizes: 9, 10, 12, 14, 18, 24 points with nearest-neighbor scaling
  - FOND/NFNT resource parsing for bitmap fonts
  - LRU cache with 256KB limit per strike for performance
  - Full integration with Window Manager (window titles) and Menu Manager (menu items)
  - Replaced legacy font paths with unified Font Manager API
- ⚠️ **XOR Ghost Dragging for Folder Windows**: Initial implementation (in progress)
  - XOR ghost outline appears and follows mouse during drag
  - Proper port context management (window → screen → window)
  - Known issue: Ghost coordinates need adjustment for proper initial positioning
  - Drop-to-desktop alias creation pending coordinate fix
- ✅ **Drag-Drop System Implementation**: Complete drag-drop with modifier key support
  - Option key creates aliases, Cmd key forces copy, cross-volume auto-copy
  - Folder window drops, desktop drops, and trash integration
  - VFS operations for move, copy, delete with unique name generation
- ✅ **Modal Drag Loop Freeze Fix**: Fixed desktop and folder window drag freezes
  - Root cause: `gCurrentButtons` not updated during modal drag loops
  - Added `ProcessModernInput()` calls to update button state during drag
  - Desktop icons and folder items now drag/release correctly
- ✅ **HFS B-tree Catalog Fixed**: Folder windows now display file system contents correctly
  - Root cause: B-tree node size (512 bytes) too small for 7 initial catalog entries (~644 bytes)
  - Buffer overflow was corrupting offset table, making entries unreadable
  - Fixed by increasing catalog B-tree node size from 512 to 1024 bytes
  - Folder windows now show 5 root folder icons (System Folder, Documents, Applications, etc.)
- ✅ **VFS Integration for Folder Windows**: Folder windows now display actual file system contents via VFS_Enumerate()
- ✅ **Icon Label Spacing**: Adjusted spacing for full-height icons (label offset 40px for folders, 34px for desktop)
- ✅ **Icon Rendering Fixed**: Full-height 32x32 folder/document icons, restored "Trash is empty" message
- ✅ **Folder Window Icons Fixed**: Correct vertical positioning in folder windows, proper coordinate transformation
- ✅ **Window Close Cleanup**: Fixed crash when closing folder windows by adding proper state cleanup
- ✅ **Window Dragging Restored**: XOR mode implementation for drag outlines, smooth window repositioning
- ✅ **QuickDraw XOR Mode**: PenMode support with proper XOR rendering for interactive feedback
- ✅ **Production-Quality Time Manager**: Accurate TSC frequency calibration, <100 ppm drift, generation checking
- ✅ **Enhanced Resource Manager**: O(log n) binary search, LRU cache, comprehensive bounds checking
- ✅ **Gestalt Manager**: Clean-room implementation with multi-architecture support (x86/ARM/RISC-V/PowerPC)
- ✅ **TextEdit Application**: Complete text editor with window creation, event handling, and full compilation
- ✅ **Performance Optimizations**: Resource cold miss <15 µs, cache hit <2 µs, timer precision ±10 µs

This is a proof-of-concept implementation focused on understanding and recreating System 7's architecture. Many features are incomplete, stubbed, or in various stages of development.

### What Works ✅

- **Hardware Abstraction Layer (HAL)**: Platform-independent architecture
  - Boot interface (`hal_boot_init()`) for platform initialization
  - I/O interface (`hal_inb()`, `hal_outb()`, etc.) for port operations
  - Storage interface (`hal_storage_*()`) for block device access
  - Input interface (`hal_input_poll()`, `hal_input_get_mouse()`, etc.)
  - x86 implementation with ATA, PS/2, and VGA support
  - Build system supports `make PLATFORM=x86` (default)
  - Ready for ARM and PowerPC platform implementations
- **Boot System**: Successfully boots via GRUB2/Multiboot2 on x86 hardware
- **Serial Logging System**: Module-based logging with runtime filtering and hierarchical levels
  - Log levels: Error, Warn, Info, Debug, Trace
  - Per-module control (Window Manager, Control Manager, Dialog Manager, etc.)
  - Module-specific macros: `WM_LOG_DEBUG()`, `CTRL_LOG_DEBUG()`, `DM_LOG_DEBUG()`, etc.
  - `serial_logf()` API for structured logging with explicit module/level
  - Runtime configuration via `SysLogSetGlobalLevel()` and `SysLogSetModuleLevel()`
  - Legacy `serial_printf()` supported via bracket tag parsing only
- **Graphics Foundation**: VESA framebuffer (800x600x32) with QuickDraw primitives
  - PenMode support including XOR mode (patXor) for interactive drag feedback
  - Rect, Line, and Frame operations with mode-aware rendering
  - Menu bar icon drawing leverages direct RGBA blits via `MenuAppleIcon_Draw()`
- **Desktop Rendering**:
  - System 7 menu bar with rainbow Apple logo
  - Desktop pattern rendering with authentic System 7 patterns
  - Hard drive icon with proper rendering and compositing
  - Trash icon support
- **Typography**: Chicago bitmap font with pixel-perfect rendering and proper kerning
- **Font Manager**: Full System 7.1-compatible implementation with:
  - Multiple font sizes (9-24pt) with nearest-neighbor scaling
  - Style synthesis (bold, italic, underline, shadow, outline, condense/extend)
  - FOND/NFNT resource parsing for bitmap fonts
  - Character width calculation and metrics
  - LRU caching for performance (cold miss <15µs, cache hit <2µs)
- **Input System**: PS/2 keyboard and mouse support with full event forwarding pipeline
- **Event Manager**:
  - Cooperative multitasking via WaitNextEvent
  - Unified event queue with mouse/keyboard event dispatching
  - Proper event mask filtering and sleep/idle support
- **Resource System**: Pattern (PAT) and pixel pattern (ppat) resources from JSON
- **Icon System**: Small icons (SICN) and 32-bit color icons (ARGB) with mask compositing
- **Memory Manager**: Zone-based memory management (8MB total: System and App zones)
  - 68K interpreter shares the same heaps via `MemoryManager_MapToM68K()` and low-memory sync helpers
- **Menu Manager**: Complete menu system with full dropdown functionality
  - Menu bar rendering with File, Edit, View, and Label menus
  - Pull-down menu display with proper background save/restore
  - Mouse tracking with live item highlighting
  - MenuSelect() tracking loop with Button()/GetMouse()
  - SaveBits/RestoreBits for flicker-free menu display
  - Fully functional with SimpleText application
  - Apple menu icon drawn from RGBA asset for accurate hit tracking
- **File System**: HFS virtual file system with B-tree implementation and trash folder integration
  - VFS layer with directory enumeration (VFS_Enumerate)
  - Folder windows display actual file system contents
  - File metadata tracking (FileID, DirID, type, creator)
- **Window Manager**:
  - Window structure, creation, and display with chrome (title bar, close box)
  - Interactive window dragging with XOR outline feedback
  - Window layering and activation
  - Mouse hit testing (title bar, content, close box)
  - Font Manager integration for window titles (Chicago 12pt)
- **Time Manager**: Production-quality implementation with:
  - Accurate TSC frequency calibration via CPUID (±100 ppm drift)
  - Binary min-heap scheduler with O(log n) operations
  - Microsecond precision with generation checking
  - Periodic timer drift correction with catch-up limits
- **OSUtils Traps**: `_TickCount` trap serviced by Time Manager-backed periodic task for 68K code
- **Resource Manager**: High-performance implementation with:
  - Binary search index for O(log n) lookups
  - LRU cache with open-addressing hash table
  - Comprehensive bounds checking and validation
  - Named resource support (GetNamedResource)
- **Gestalt Manager**: Multi-architecture system information with:
  - Clean-room implementation (no dynamic allocation)
  - Architecture detection (x86/ARM/RISC-V/PowerPC)
  - Built-in selectors (sysv, mach, proc, fpu, init bits)
  - GetSysEnv compatibility layer
- **TextEdit Manager**: Complete System 7.1-compatible text editing engine with:
  - Core editing operations (TENew, TEDispose, TEKey, TEClick, TECut, TECopy, TEPaste)
  - Selection management with click/drag and keyboard navigation
  - Clipboard integration via Scrap Manager (TEFromScrap/TEToScrap)
  - Multi-line text support with word wrap and scrolling
  - Handle-based text storage with dynamic resizing
- **Scrap Manager**: Classic Mac OS clipboard system with:
  - System 7.1 API compatibility (ZeroScrap, GetScrap, PutScrap, LoadScrap, UnloadScrap, InfoScrap)
  - Multiple clipboard flavors (TEXT, PICT, style runs)
  - Handle-based memory management for clipboard data
  - Change counting for detecting clipboard updates
  - Process ownership tracking
  - Helper functions (ScrapHasFlavor, ScrapGetFlavorSize)
- **SimpleText Application**: Full-featured text editor with:
  - Multiple document interface (MDI) support
  - Complete menu system (Apple, File, Edit, Font, Size, Style menus)
  - Cut/Copy/Paste/Clear/Select All operations
  - File open/save with TEXT/ttxt type/creator codes
  - Document state management (dirty tracking, undo support)
  - TextEdit API integration for all editing operations
- **List Manager**: System 7.1-compatible list control implementation with:
  - Complete public API (LNew, LDispose, LAddRow, LDelRow, LSetCell, LGetCell, LUpdate, LDraw, LClick, LKey, LScroll, LSize)
  - Single-selection and multi-selection modes with modifier keys
  - Keyboard navigation (arrows, PageUp/PageDown, Home/End)
  - Cell-based storage with dynamic row/column management
  - Handle-based memory management with proper locking
  - QuickDraw integration with state save/restore
  - Dialog Manager integration for list dialog items
  - Efficient redraw with narrow erase and band invalidation
- **Control Manager**: Standard controls and scrollbar controls with classic Mac OS behavior:
  - Standard Controls (buttons, checkboxes, radio buttons):
    - NewControl creation with procID-based type dispatch (pushButProc, checkBoxProc, radioButProc)
    - Button variants: normal, default (thick border + Return key), cancel (Escape key)
    - Radio button group management with automatic mutual exclusion
    - Full CDEF implementation for drawing, hit-testing, and interaction
    - Mouse tracking with TrackControl and action procedure callbacks
    - Debounce mechanism prevents double-fire from rapid mouse+keyboard actions
  - Scrollbar Controls:
    - Vertical and horizontal scrollbar creation (NewVScrollBar, NewHScrollBar)
    - Control Definition Function (CDEF) for scrollbars with full message handling
    - Proportional thumb sizing: thumbLen = (visibleSpan × trackLen) / (range + visibleSpan)
    - Interactive tracking with repeat-on-hold (arrows: 8/3 ticks, pages: 8/4 ticks)
    - Delta-based tracking API (TrackScrollbar) for streamlined integration
    - Hit-testing with disabled state support (inactiveHilite, max <= min)
    - List Manager integration via UpdateScrollThumb for atomic value/range/span updates
    - Part-specific timing for authentic classic Mac feel
- **Dialog Manager**: Complete keyboard navigation and focus management:
  - Focus ring rendering with XOR toggle pattern (idempotent draw/erase)
  - Tab/Shift+Tab traversal with intelligent control filtering
  - Focus state tracking per window with MAX_DIALOGS=16 focus table
  - Space key activation for checkboxes and radio buttons
  - Return/Escape key handling for default/cancel buttons
  - DM_HandleDialogKey unified keyboard dispatcher
  - Activation lifecycle: focus suspend on deactivate, restore on activate
  - Automatic cleanup on window/control disposal
  - Graphics state save/restore for isolated drawing operations
  - StandardFile dialog integration with initial focus priming
  - Alert & Modal Dialog full keyboard integration
  - Debounce protection prevents double-activation from concurrent mouse/keyboard events
- **Segment Loader**: Portable, ISA-agnostic 68K segment loading system:
  - CODE resource parsing with big-endian safe readers (BE_Read16/BE_Read32)
  - CODE 0 metadata extraction (A5 world sizes, jump table offset/count)
  - A5 world construction: allocate below/above A5 areas, build jump table
  - Lazy segment loading: self-patching jump table stubs trigger _LoadSeg on first call
  - Abstract relocation system (kRelocAbsSegBase, kRelocA5Relative, kRelocJTImport)
  - CPU backend interface (ICPUBackend) for pluggable execution engines
  - M68K interpreter backend (reference implementation for x86 host)
  - Process Manager integration: LaunchApplication loads CODE 0/1 and transfers control
  - Complete portability: no host ISA assumptions, works on x86/ARM/any architecture

### Partially Working ⚠️

- **Window Manager**:
  - Window content rendering (coordinate fixes completed)
  - Update event pipeline (basic implementation in place)
  - WDEF (Window Definition Procedure) dispatch (partial)
  - Visible region (visRgn) calculation (basic implementation)
  - Window resizing and grow box handling (stubbed)
- **Desktop Icons**:
  - Icons render and display correctly
  - Double-click to open functional
- **Control Manager**: Framework in place, standard controls (buttons, checkboxes, radio buttons) and scrollbar controls complete
- **Dialog Manager**: Core structure complete with full keyboard navigation support
- **File Manager**: Core implemented, in progress

### Not Yet Implemented ❌

- **Application Execution**: Segment loader implemented, but M68K interpreter execution loop is stubbed
- **Sound Manager**: No audio support
- **Printing**: No print system
- **Networking**: No AppleTalk or network functionality
- **Desk Accessories**: Framework only, no functional DAs
- **Color Manager**: Minimal color support

### Subsystems Not Yet Compiled 🔧

The following subsystems have source code but are not yet integrated into the build:

- **AppleEventManager** (8 files): Inter-application messaging and scripting support
  - Core event dispatch, descriptors, handlers, coercion
  - Required Events (Open/Print/Quit), event recording
  - Located in `src/AppleEventManager/`
- **DeviceManager** (8 files): Device driver infrastructure
  - Driver loading, dispatch, and I/O operations
  - Async I/O support, unit table management
  - Device interrupt handling
  - Located in `src/DeviceManager/`
- **FontResources** (8 files): Font resource management and conversion
  - Embedded font support, font downloading
  - Resource converter and loader
  - Modern font compatibility layer
  - Located in `src/FontResources/`
- **GestaltManager** (2 files): Extended system capability detection
  - System capabilities and feature testing
  - Located in `src/GestaltManager/` (distinct from `src/Gestalt/` which IS compiled)
- **SpeechManager** (8 files): Text-to-speech synthesis
  - Voice management, speech output, synthesis engine
  - Speech channels, pronunciation, voice resources
  - Located in `src/SpeechManager/`
- **StartupScreen** (1 file): Boot splash screen
  - Located in `src/StartupScreen/`

## 🏗️ Architecture

### Technical Specifications

- **Architecture**: Multi-architecture support via Hardware Abstraction Layer (HAL)
  - Platform-agnostic core implementation
  - x86 platform complete (ATA, PS/2, VGA)
  - Ready for ARM (Raspberry Pi, Apple Silicon) and PowerPC ports
  - Build system: `make PLATFORM=<platform>` (default: x86)
- **Boot Protocol**: Multiboot2 (x86), platform-specific bootloaders for other architectures
- **Graphics**: VESA framebuffer, 800x600 @ 32-bit color
- **Memory Layout**: Kernel loads at 1MB physical address (x86)
- **Font Rendering**: Custom bitmap renderer with authentic Chicago font data
- **Input**: HAL-abstracted input system (x86: PS/2 keyboard and mouse)
- **Timing**: Architecture-agnostic counters with microsecond precision:
  - x86: RDTSC with CPUID frequency calibration
  - ARM/AArch64: Generic timer
  - RISC-V: Cycle CSR
  - PowerPC: Time Base Register
- **Resource Performance**:
  - Cold miss: <15 µs (target met)
  - Cache hit: <2 µs (target met)
  - Index lookup: O(log n) binary search
- **Toolbox Managers**: Modular implementation matching Inside Macintosh specifications

### Codebase Statistics

- **145+ header files** across 28+ subsystems
- **225+ source files** (~57,500+ lines of code)
- **69 resource types** extracted from original System 7.1
- **Performance targets achieved**:
  - Timer drift: <100 ppm (target met)
  - Timer precision: ±10 µs (target met)
  - Resource cold miss: <15 µs (target met)
  - Resource cache hit: <2 µs (target met)

### Key Subsystems

```
iteration2/
├── src/
│   ├── main.c                  # Kernel entry point & initialization
│   ├── boot.c                  # Platform-independent boot dispatcher
│   ├── SystemInit.c            # System startup sequence
│   ├── System71StdLib.c        # Serial logging framework with bracket tag parsing
│   ├── Platform/               # Hardware Abstraction Layer (HAL)
│   │   ├── include/            # Platform-independent HAL interfaces
│   │   │   ├── boot.h          # Boot initialization interface
│   │   │   ├── io.h            # I/O port operations (hal_inb, hal_outb, etc.)
│   │   │   ├── storage.h       # Block storage interface
│   │   │   └── input.h         # Input device interface
│   │   └── x86/                # x86 platform implementation
│   │       ├── platform_boot.S # Multiboot2 bootloader entry
│   │       ├── hal_boot.c      # x86 boot initialization
│   │       ├── io.c            # x86 port I/O operations
│   │       ├── ata.c           # ATA/IDE storage driver
│   │       ├── ps2.c           # PS/2 keyboard/mouse driver
│   │       ├── hal_input.c     # Input HAL implementation
│   │       └── linker.ld       # x86 linker script
│   ├── QuickDraw/              # 2D graphics primitives
│   ├── WindowManager/          # Window management (8 modules)
│   ├── MenuManager/            # Menu system (7 modules)
│   ├── FontManager/            # Font management (5 modules)
│   ├── EventManager/           # Event handling & cooperative multitasking
│   ├── MemoryMgr/              # Zone-based memory management
│   ├── Finder/                 # Desktop & Finder implementation
│   │   └── Icon/               # Icon system (5 modules)
│   ├── FS/                     # Virtual file system & HFS
│   ├── ControlManager/         # UI controls (4 modules: Core, Tracking, StandardControls, Scrollbar)
│   ├── DialogManager/          # Dialog boxes (3 modules: Core, Keyboard, StandardFile integration)
│   ├── DeskManager/            # Desk accessories
│   ├── PatternMgr/             # Pattern resources
│   ├── TimeManager/            # Production timer scheduler (7 modules)
│   ├── ResourceMgr/            # High-performance resource manager
│   ├── Gestalt/                # System information manager
│   ├── TextEdit/               # TextEdit Manager (7 modules)
│   ├── ScrapManager/           # Clipboard/Scrap Manager (Classic Mac OS API)
│   ├── ListManager/            # List Manager (3 modules)
│   ├── Apps/SimpleText/        # SimpleText application (7 modules)
│   ├── SegmentLoader/          # Portable 68K segment loader (3 modules)
│   ├── CPU/                    # CPU backend interface & implementations
│   │   └── m68k_interp/        # M68K interpreter backend
│   ├── ProcessMgr/             # Process Manager (integrated with segment loader)
│   └── FileMgr/                # File Manager subsystems
├── include/                    # Public headers (Inside Mac API)
├── docs/                       # Component documentation
│   ├── components/             # Subsystem guides
│   │   ├── ControlManager/     # Control Manager docs & QA checklist
│   │   ├── DialogManager/      # Dialog Manager & keyboard integration
│   │   ├── FontManager/        # Font Manager documentation
│   │   ├── System/             # Logging system documentation
│   │   ├── SegmentLoader_DESIGN.md   # Segment Loader architecture
│   │   ├── SegmentLoader_README.md   # Segment Loader quick reference
│   │   ├── SegmentLoader_SUMMARY.md  # Implementation summary
│   │   ├── EventManager.md     # Event Manager guide
│   │   ├── MenuManager.md      # Menu Manager guide
│   │   ├── WindowManager.md    # Window Manager guide
│   │   └── ResourceManager.md  # Resource Manager guide
│   ├── future/                 # Future development plans
│   │   ├── PORTING_PLAN.md     # Multi-platform porting strategy
│   │   └── REFACTORING_PLAN.md # HAL implementation plan
│   └── layouts/                # System structure layouts
└── System_Resources_Extracted/ # Original System 7.1 resources
    └── [69 resource type folders]
```

## 🔨 Building

### Requirements

- **GCC** with 32-bit support (`gcc-multilib` on 64-bit systems)
- **GNU Make**
- **GRUB tools**: `grub-mkrescue` (from `grub2-common` or `grub-pc-bin`)
- **QEMU** for testing (`qemu-system-i386`)
- **Python 3** for resource processing
- **xxd** for binary conversion

### Ubuntu/Debian Installation

```bash
sudo apt-get install build-essential gcc-multilib grub-pc-bin xorriso qemu-system-x86 python3 vim-common
```

### Build Commands

```bash
# Build kernel only (x86 by default)
make

# Build for specific platform
make PLATFORM=x86

# Create bootable ISO
make iso

# Build and run in QEMU
make run

# Clean build artifacts
make clean

# Check exported symbols
make check-exports

# Display build statistics
make info
```

## 🚀 Running

### Quick Start (QEMU)

```bash
# Standard run with serial logging
make run

# Or manually:
qemu-system-i386 -cdrom system71.iso -serial file:/tmp/serial.log -display sdl -vga std -m 256M
```

### Run Options

```bash
# Debug with GDB
make debug
# In another terminal: gdb kernel.elf -ex "target remote :1234"

# Custom QEMU options
qemu-system-i386 -cdrom system71.iso \
  -serial stdio \           # Serial to console
  -display sdl \            # SDL window
  -vga std \                # Standard VGA
  -m 256M                   # 256MB RAM
```

### Real Hardware

**⚠️ WARNING**: Running on real hardware is experimental and may not work. Use at your own risk.

1. Write ISO to USB drive: `dd if=system71.iso of=/dev/sdX bs=4M`
2. Boot from USB with legacy BIOS (not UEFI)
3. Serial output available on COM1 (115200 baud) if available

### Implementation Philosophy

This project follows an **archaeological approach** to reimplementation:

1. **Evidence-Based**: Every non-trivial implementation decision is backed by citations from:
   - Inside Macintosh documentation (official Apple specs)
   - MPW Universal Interfaces (canonical struct layouts)
   - System 7 ROM disassembly analysis
   - Emulator trace comparison

2. **Documented Findings**: All changes are tagged with Finding IDs (e.g., `[WM-001]`) that reference archaeological evidence in `docs/ARCHAEOLOGY.md`

3. **Behavioral Parity**: Goal is to match original System 7 behavior, not to improve or modernize it

4. **Clean Room**: Implementation is based on public documentation and black-box analysis, not original Apple source code

## 🎓 Educational Value

This project serves as:

- **Operating System Education**: Demonstrates classic OS architecture (cooperative multitasking, event-driven UI, resource management)
- **Reverse Engineering Study**: Documents the process of analyzing and recreating complex software systems
- **Historical Preservation**: Keeps System 7's architecture and design philosophy accessible
- **Low-Level Programming**: Real bare-metal x86 programming with no OS layer

## 🐛 Known Issues

1. **Icon Drag Artifacts**: Dragging desktop icons may cause visual artifacts
2. **Application Execution Stubbed**: Segment loader complete, but M68K interpreter execution loop not yet implemented
3. **No TrueType Support**: Font Manager supports bitmap fonts only (Chicago)
4. **HFS Read-Only**: File system is virtual/simulated, no real disk I/O
5. **No Stability Guarantees**: Crashes, hangs, and unexpected behavior are common

## 🤝 Contributing

This is primarily a learning/research project, but contributions are welcome:

1. **Bug Reports**: File issues with detailed reproduction steps
3. **Testing**: Test on different hardware/emulators and report results

## 📖 Resources

### Project Documentation

- **Component Guides** (`docs/components/`): Detailed subsystem documentation
  - Control Manager: `docs/components/ControlManager/README.md` & QA checklist
  - Dialog Manager: `docs/components/DialogManager/README.md` & keyboard integration guide
  - Font Manager: `docs/components/FontManager/README.md`
  - Serial Logging: `docs/components/System/Logging.md`
  - Event Manager: `docs/components/EventManager.md`
  - Menu Manager: `docs/components/MenuManager.md`
  - Window Manager: `docs/components/WindowManager.md`
  - Resource Manager: `docs/components/ResourceManager.md`

### Essential References

- **Inside Macintosh** (1992-1994 editions): Official Apple Toolbox documentation
- **MPW Universal Interfaces 3.2**: Canonical header files and struct definitions
- **Guide to Macintosh Family Hardware**: Hardware architecture reference

### Helpful Tools

- **Mini vMac**: System 7 emulator for behavioral reference
- **Basilisk II**: Another classic Mac emulator
- **ResEdit**: Resource editor (for studying original System 7 resources)
- **Ghidra/IDA**: For ROM disassembly analysis

## ⚖️ Legal

This is a **clean-room reimplementation** for **educational and preservation purposes**.

- **No Apple source code** was used in this implementation
- Implementation is based on public documentation and black-box analysis
- "System 7", "Macintosh", "QuickDraw", and Apple logo are trademarks of Apple Inc.
- This project is not affiliated with, endorsed by, or sponsored by Apple Inc.

### Fair Use Rationale

This project exists for:
- Educational purposes (teaching OS design and implementation)
- Historical preservation (documenting 1980s-90s OS architecture)
- Interoperability research (understanding file formats and protocols)

**Original System 7 ROM and software remain property of Apple Inc.** This reimplementation does not include or require original Apple software to function.

## 🙏 Acknowledgments

- **Apple Computer, Inc.** for creating the original System 7
- **Inside Macintosh authors** for comprehensive documentation
- **Classic Mac preservation community** for keeping the platform alive
- **68k.news and Macintosh Garden** for resource archives and documentation

## 📊 Project Statistics

- **Lines of Code**: ~57,500+ (including 2,500+ for segment loader)
- **Development Time**: Several months (ongoing)
- **Compilation Time**: ~3-5 seconds on modern hardware
- **Kernel Size**: ~1.8MB (kernel.elf)
- **ISO Size**: ~12.5MB (system71.iso)
- **Error Reduction**: 94% (30 of 32 Window Manager errors resolved)
- **New Managers Added**: 5 (Font Manager, Gestalt, enhanced Resource, production Time Manager, Segment Loader)

## 🔮 Future Direction

This project is in **active development** with no guaranteed timeline. Planned work includes:

**Short Term** (Proof of Concept Completion):
- Fix desktop artifacts
- Debug window close crash issue
- Complete window resizing functionality

**Medium Term** (Core Toolbox):
- Complete Window Definition Procedure (WDEF) dispatch
- Implement additional controls (text fields, pop-up menus, sliders)
- Add TrueType font support to Font Manager

**Long Term** (Application Support):
- ✅ Segment Loader for loading CODE resources (complete)
- ✅ Process Manager integration with segment loader (complete)
- Complete M68K interpreter execution loop
- Add PPC backend for fat binary support
- Basic desk accessories (Calculator, Note Pad)

**Stretch Goals** (Full System):
- Sound Manager with sample playback
- AppleTalk networking stack
- Color QuickDraw with 8-bit palette support

---

**Status**: Experimental - Educational - In Development

**Last Updated**: October 2025 (Segment Loader implemented)

For questions, issues, or discussion, please use GitHub Issues.
