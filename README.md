# System7 - Portable System 7.1 Implementation

Modern cross-platform implementation of Mac OS System 7.1 (75% complete).

## Status: Beta

| Component | Status | Description |
|-----------|--------|-------------|
| Window Manager | ✅ 95% | Complete window management, HAL |
| Menu Manager | ✅ 95% | Hierarchical menus, shortcuts |
| QuickDraw | ✅ 90% | Graphics, regions, patterns |
| Dialog Manager | ✅ 90% | Modal/modeless, alerts |
| Event Manager | ✅ 95% | Event queue and dispatch |
| Control Manager | ✅ 85% | All standard controls |
| TextEdit | ✅ 85% | Multi-style text editing |
| List Manager | ✅ 85% | Scrollable lists, LDEFs |
| Scrap Manager | ✅ 90% | System clipboard |
| Calculator | ✅ 100% | Complete desk accessory |
| Resource Manager | ✅ 80% | Resource loading/management |
| Memory Manager | ✅ 75% | Handle-based memory |
| File Manager | ✅ 70% | HFS+ operations |
| Finder | ✅ 75% | Desktop management |
| Print Manager | ⏳ 5% | In development |
| Sound Manager | ⏳ 10% | Basic framework |
| Color Manager | ⏳ 5% | Planned |
| Help Manager | ⏳ 5% | Planned |

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
- **macOS**: CoreGraphics/CoreServices
- **Architecture**: x86_64, ARM64
- **Windows**: Planned (Win32/DirectX)

## What's Working

- Complete desktop environment with Finder
- Window management with drag, resize, close
- Hierarchical menu system with keyboard shortcuts
- Modal and modeless dialogs
- All standard controls (buttons, scrollbars, lists)
- Text editing with selection and clipboard
- File operations and trash management
- Calculator desk accessory
- Resource loading and management
- Handle-based memory management

## Building

### Prerequisites

**Linux:**
```bash
sudo apt-get install build-essential cmake libx11-dev libcairo2-dev
```

**macOS:**
```bash
brew install cmake
```

### Build Instructions

```bash
# Clone repository
git clone https://github.com/Kelsidavis/System7.git
cd System7

# Using Make
make all

# Run tests
make tests

# Clean build
make clean
```

## Documentation

- [Implementation Status](TODO.md) - Detailed component status
- [Architecture Guide](docs/ARCHITECTURE.md)
- [Priority Roadmap](docs/PRIORITY_ROADMAP.md)

## License

MIT License

Copyright (c) 2024 System7 Project

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

## Acknowledgments

- Apple Inc. for the original System 7.1
- Open source community for preservation efforts
- Contributors to this reimplementation project

---

*System7 is an educational project. Mac OS and System 7 are trademarks of Apple Inc.*