# Contributing to System7

Thank you for your interest in contributing to the System7 project! This guide will help you get started.

## Code of Conduct

- Be respectful and professional
- Focus on technical merit
- Help others learn and grow
- Credit sources and prior work

## How to Contribute

### 1. Find an Area to Work On

Check our [TODO.md](TODO.md) for tasks organized by priority:
- **Priority 1**: Core system components (Print Manager, Sound Manager)
- **Priority 2**: Enhanced features (Help Manager, Notification Manager)
- **Priority 3**: Networking & communications
- **Priority 4**: Stub completions (~295 remaining)
- **Priority 5**: Hardware abstraction
- **Priority 6**: Testing & documentation

### 2. Set Up Development Environment

```bash
# Fork and clone the repository
git clone https://github.com/[your-username]/System7.git
cd System7

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
1. **Print Manager** - Basic printing support
2. **Sound Manager** - Audio playback
3. **Standard File Package** - File dialogs
4. **Color Manager** - Color management

### Medium Priority
1. **Stub Completions** - Replace TODO/FIXME markers
2. **Test Coverage** - Increase from 40% to 80%
3. **Performance** - Optimize critical paths
4. **Documentation** - API references

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

## License

All contributions must be compatible with the MIT License. By contributing, you agree to license your work under the same terms.

## Resources

- [Original System 7.1 Documentation](http://developer.apple.com/legacy)
- [Inside Macintosh](https://vintageapple.org/inside/)
- [Mac OS Architecture](docs/ARCHITECTURE.md)
- [Implementation Status](TODO.md)

Thank you for helping preserve computing history!