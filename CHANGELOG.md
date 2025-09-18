# Changelog

All notable changes to System7.1-Portable will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.87.0] - 2024-01-18

### Added

#### Startup Screen
- **Classic "Welcome to Macintosh" Boot Screen**
  - Happy Mac icon display with authentic drawing
  - Configurable welcome duration and colors
  - Extension loading visualization with grid layout
  - Multi-phase boot sequence (Init, Welcome, Extensions, Drivers, Finder)
  - Progress bar with percentage display
  - Error display capability with error codes
  - Full integration with boot sequence manager

#### Resource Data System
- **Authentic System 7 Resources Embedded**
  - Calculator icon (32x32) with mask
  - Standard cursors: arrow, I-beam, crosshair, watch with proper hotspots
  - Window control icons: close, zoom, collapse boxes
  - UI elements: checkbox and radio button patterns
  - Desktop and gray patterns (25%, 50%, 75%)
  - Menu icons: Apple logo, command key (⌘), checkmark
  - Dialog alert icons: stop, note, caution (32x32)
  - Small icons: folder, document, application, trash (16x16)
  - System beep sound (8-bit PCM square wave)
  - Complete ResourceData management module

#### QuickDraw Enhancements
- **CursorManager** - Standard cursor management with resource data
- **PatternManager** - System pattern management with caching

## [0.85.0] - 2024-01-18

### Added

#### Major Component Integrations
- **Color Manager** - Complete RGB color management system with palettes and CLUTs
  - Platform-specific color space support (NSColorSpace, X11 Colormap, GDI+)
  - RGB565/RGB888 conversion utilities
  - Palette animation and color matching
  - Full HAL implementation for cross-platform support

- **Help Manager** - Context-sensitive balloon help system
  - Native tooltip integration on all platforms
  - Balloon help resources (hmnu, hdlg, hwin)
  - Hot rectangle tracking
  - Platform HAL for macOS (NSHelpManager), Linux (GTK tooltips), Windows (tooltips)

- **Print Manager** - Complete printing subsystem
  - PostScript Level 2 generation
  - Native print dialogs on all platforms
  - Print spooling and job management
  - Platform HAL using CUPS (Linux), NSPrintOperation (macOS), GDI+ (Windows)

- **Package Manager** - PACK resource loading and dispatch
  - Support for all 16 standard packages (PACK 0-15)
  - Dynamic package loading from resources
  - Built-in implementations for List, SANE, International, Binary-Decimal
  - Full HAL layer for platform-specific operations

- **Time Manager** - High-precision timing system
  - Microsecond and nanosecond resolution timers
  - Timer task scheduling with callbacks
  - VBL (Vertical Blanking) task support
  - Platform HAL using mach_absolute_time (macOS), POSIX timers (Linux), multimedia timers (Windows)

- **Standard File Package** - Native file dialogs
  - Open/Save dialog implementations
  - File type filtering
  - Directory navigation
  - Platform HAL for native file dialogs on all systems

### Changed
- Calculator now displays authentic System 7 icon
- SysBeep now uses embedded System 7 beep sound
- Boot sequence now shows visual progress
- Updated build system to include StartupScreen and Resources modules
- Improved Makefile organization with better module separation
- Enhanced CMake configuration for all new components
- Increased overall project completion from 85% to 87%

### Fixed
- Resolved header file conflicts in ColorManager
- Fixed build configuration duplicates
- Corrected test suite compilation for new components

### Testing
- Added comprehensive test suites for all new components:
  - `test_colormanager.c` - RGB operations, palette management
  - `test_helpmanager.c` - Balloon help display and tracking
  - `test_printmanager.c` - PostScript generation, print dialogs
  - `test_packagemanager.c` - PACK resource loading
  - `test_timemanager.c` - Timer precision and scheduling
  - `test_standardfile.c` - File dialog operations

## [0.75.0] - 2024-01-10

### Added
- Calculator desk accessory (100% complete)
- List Manager with LDEF support (85% complete)
- Scrap Manager clipboard operations (90% complete)
- TextEdit multi-style text editing (85% complete)
- Control Manager standard controls (85% complete)

### Changed
- Improved QuickDraw graphics primitives (90% complete)
- Enhanced Dialog Manager (90% complete)
- Updated Event Manager (95% complete)
- Better Menu Manager keyboard shortcuts (95% complete)
- Optimized Window Manager (95% complete)

### Fixed
- Window refresh issues with overlapping regions
- Menu tracking stability improvements
- Dialog focus handling
- Memory handle allocation bugs

## [0.70.0] - 2024-01-01

### Added
- Initial Finder implementation (75% complete)
- File Manager HFS+ operations (70% complete)
- Memory Manager handle-based system (75% complete)
- Resource Manager fork management (80% complete)

### Infrastructure
- Hardware Abstraction Layer (HAL) architecture
- Cross-platform support for macOS, Linux, Windows
- ARM64 and x86_64 native support
- Comprehensive build system with Make and CMake

## [0.60.0] - 2023-12-15

### Added
- Core Window Manager implementation
- Basic Menu Manager
- Event Manager foundation
- QuickDraw graphics primitives
- Dialog Manager basics

### Development
- Project structure established
- Build system created
- Test framework initialized
- Documentation structure defined

## Future Releases

### [0.90.0] - Planned
- Sound Manager implementation
- Apple Events support
- Component Manager architecture
- Network extension basics

### [0.95.0] - Planned
- Communication Toolbox
- Notification Manager
- Speech Manager
- Complete test coverage (80%+)

### [1.0.0] - Planned
- Full System 7.1 compatibility
- Complete documentation
- Performance optimizations
- Production-ready release

---

For detailed implementation status, see [TODO.md](TODO.md)
For contribution guidelines, see [CONTRIBUTING.md](CONTRIBUTING.md)