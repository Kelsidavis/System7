# Contributing to System7.1-Portable

Thank you for your interest in contributing to the System7.1-Portable project! This guide will help you get started.

## Code of Conduct

- Be respectful and professional
- Focus on technical merit
- Help others learn and grow
- Credit sources and prior work

## How to Contribute

### 1. Find an Area to Work On

Check our [TODO.md](TODO.md) for tasks organized by priority:
- **Priority 1**: Core system components (Sound Manager, Apple Events, Component Manager)
- **Priority 2**: Networking & communications (AppleTalk, TCP/IP)
- **Priority 3**: Enhanced features (Notification Manager, Speech Manager)
- **Priority 4**: Stub completions (~150 remaining)
- **Priority 5**: Testing & documentation (60% → 80% coverage)
- **Priority 6**: Platform-specific enhancements

### 2. Set Up Development Environment

```bash
# Fork and clone the repository
git clone https://github.com/[your-username]/System7.1-Portable.git
cd System7.1-Portable

# Create a feature branch
git checkout -b feature/component-name

# Build the project
make clean && make all

# Run tests
make tests
```

### 3. Development Guidelines

#### Code Style
- Use consistent indentation (4 spaces)
- Follow existing naming conventions
- Comment complex logic
- Keep functions focused and small

#### Architecture
- Maintain HAL separation for platform code
- Use handle-based memory for Mac compatibility
- Follow Manager pattern for components
- Preserve original System 7.1 APIs

#### Example Structure
```c
/*
 * Copyright (c) 2024 System7 Project
 * MIT License - See LICENSE file
 */

#include "ComponentName.h"

/* Private types */
typedef struct {
    /* Implementation details */
} ComponentState;

/* HAL functions for platform abstraction */
void Component_HAL_PlatformSpecific(void) {
    #ifdef __APPLE__
        /* macOS implementation */
    #elif defined(HAS_X11)
        /* Linux implementation */
    #endif
}

/* Public API matching System 7.1 */
OSErr ComponentFunction(Handle param) {
    /* Implementation */
    return noErr;
}
```

### 4. Testing Requirements

- Write unit tests for new functions
- Ensure existing tests still pass
- Test on multiple platforms if possible
- Document any platform-specific behavior

### 5. Documentation

- Update relevant documentation
- Add inline comments for complex code
- Update TODO.md when completing tasks
- Document API changes

### 6. Submitting Changes

```bash
# Ensure code builds
make clean && make all

# Run all tests
make tests

# Commit with descriptive message
git commit -m "Component: Brief description

- Detailed change 1
- Detailed change 2
- Fixes #issue-number"

# Push to your fork
git push origin feature/component-name
```

### 7. Pull Request Process

1. Create PR with clear title and description
2. Reference related issues
3. List testing performed
4. Wait for code review
5. Address feedback
6. Maintain clean commit history

## Priority Areas

### High Priority (Most Needed)
1. **Sound Manager** - Complete audio synthesis and playback (15% done)
2. **Apple Events** - Inter-application communication
3. **Component Manager** - Plugin architecture
4. **Networking** - AppleTalk and TCP/IP stack

### Medium Priority
1. **Stub Completions** - Replace ~130 TODO/FIXME markers
2. **Test Coverage** - Increase from 65% to 80%
3. **Performance** - Optimize critical paths
4. **Documentation** - Complete API references

### Platform-Specific
1. **Linux** - Wayland support
2. **macOS** - Metal rendering
3. **Windows** - Initial HAL implementation

## Getting Help

- Open an issue for questions
- Check existing issues first
- Join discussions in pull requests
- Review the architecture documentation

## Recognition

Contributors will be recognized in:
- Git history
- CONTRIBUTORS.md file
- Release notes
- Project documentation

## Recently Completed

The following components have been successfully integrated and serve as good examples:
- **Startup Screen** - Classic boot screen with extension loading display
- **Resource Data** - Embedded System 7 resources (icons, cursors, patterns, sounds)
- **Color Manager** - Full RGB color management with HAL
- **Help Manager** - Balloon help with native tooltips
- **Print Manager** - PostScript generation and platform dialogs
- **Package Manager** - PACK resource loading and dispatch
- **Time Manager** - Microsecond precision timing
- **Standard File** - Native file dialogs on all platforms

## License

All contributions must be compatible with the MIT License. By contributing, you agree to license your work under the same terms.

## Resources

- [Original System 7.1 Documentation](http://developer.apple.com/legacy)
- [Inside Macintosh](https://vintageapple.org/inside/)
- [Mac OS Architecture](docs/ARCHITECTURE.md)
- [Implementation Status](TODO.md)
- [Change History](CHANGELOG.md)

Thank you for helping preserve computing history!