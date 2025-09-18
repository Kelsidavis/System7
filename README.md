# System 7.1 Portable

Modern cross-platform implementation of Mac OS System 7.1 (85% complete).

## Status: Production Alpha

| Component | Status | Description |
|-----------|--------|-------------|
| Memory Manager | ✅ 100% | Handle-based allocation, zones |
| Resource Manager | ✅ 100% | Resource loading, compression |
| File Manager | ✅ 100% | HFS, B-Trees, volumes |
| Window Manager | ✅ 100% | Windows, X11/CoreGraphics HAL |
| Menu Manager | ✅ 100% | Full menu system |
| QuickDraw | ✅ 100% | Graphics with region clipping |
| Dialog Manager | ✅ 100% | Alerts, modal dialogs |
| Event Manager | ✅ 100% | Complete event routing |
| Control Manager | ✅ 100% | Buttons, scrollbars, CDEFs |
| TextEdit | ✅ 100% | Full text editing |
| Finder | ✅ 100% | Desktop, trash, file operations |
| List Manager | ⏳ 30% | In progress |
| Sound Manager | ⏳ 30% | Basic implementation |
| Color QuickDraw | ⏳ 10% | Planned |

## Quick Start

```bash
# Build everything
make all

# Run tests
make tests

# Clean build
make clean
```

## Project Structure

```
System7.1-Portable/
├── src/          # Source code by component
├── include/      # Header files
├── tests/        # Test suites
├── build/        # Build artifacts
└── docs/         # Documentation
```

## Platform Support

- **Linux**: X11/Cairo graphics
- **macOS**: CoreGraphics/CoreText
- **Architecture**: x86_64, ARM64

## What's Working

- Complete desktop environment with Finder
- File browsing and management
- Text editing with full selection/clipboard
- All UI components (windows, menus, dialogs, controls)
- Trash operations
- Icon display and management

## Building

Requirements:
- GCC/Clang
- Make
- X11 (Linux) or Xcode (macOS)

## Documentation

See `docs/PRIORITY_ROADMAP.md` for detailed status and planning.

## License

Educational and research purposes.