# Contributing to System 7

Thank you for your interest in the System 7 reimplementation project! This is an educational and preservation project, and we appreciate all forms of contribution.

## Ways to Contribute

### 🐛 Bug Reports
- Check [existing issues](https://github.com/Mikecraft1224/System7/issues) first to avoid duplicates
- Include detailed reproduction steps
- Specify your hardware/emulator and OS
- Attach screenshots or logs if relevant

### 📚 Documentation
- Improve existing guides in `docs/components/`
- Add architectural explanations
- Document undiscovered System 7 behaviors
- Fix typos or clarify complex sections

### 🌍 Translations
- Help translate the README into additional languages
- Add language-specific resource files in `resources/strings/`
- Create locale-specific documentation

### 💻 Code Improvements
- Fix bugs identified in [known issues](docs/KNOWN_ISSUES.md)
- Improve code quality (test coverage, documentation)
- Optimize performance
- For major features, open an issue first to discuss approach

### 🧪 Testing
- Report compatibility issues on different hardware/emulators
- Test on various QEMU configurations
- Verify language-specific features
- Test real System 7 applications

## Development Setup

### Prerequisites
```bash
# Ubuntu/Debian
sudo apt-get install build-essential gcc-multilib grub-pc-bin xorriso qemu-system-x86 python3 vim-common

# Other distros - install equivalent packages
```

### Building
```bash
# Build kernel
make

# Build with additional language
make LOCALE_FR=1

# Run in QEMU
make run

# Run with GDB debugging
make debug
```

## Project Structure

```
System7/
├── include/              # Header files for all subsystems
├── src/                  # Source files (not shown, would be same structure)
├── docs/                 # Project documentation
│   ├── components/       # Detailed component guides
│   ├── future/           # Planning documents
│   └── TRANSLATIONS.md   # Multi-language README index
├── resources/            # Resource files, fonts, patterns
│   ├── strings/          # Localization files (one per language)
│   └── device-tree/      # QEMU device tree files
├── scripts/              # Development utility scripts
├── Makefile              # Build system
└── README.md             # Main project README
```

## Code Style

- Follow existing conventions in the codebase
- Use `serial_printf()` for debugging, not `printf`
- Include Finding IDs in comments for non-obvious design decisions
- Document public APIs with clear comments
- No malloc/free in kernel (use zone-based allocation)

## Commit Messages

Write clear commit messages:
```
<type>: <short description>

<longer explanation if needed>

Fixes #123
```

Types: `feat`, `fix`, `docs`, `test`, `refactor`, `perf`, `style`

## Getting Help

- Review [IMPLEMENTATION_PRIORITIES.md](../IMPLEMENTATION_PRIORITIES.md) for planned work
- Check [IMPLEMENTATION_STATUS_AUDIT.md](../docs/IMPLEMENTATION_STATUS_AUDIT.md) for subsystem details
- Read component documentation in `docs/components/`
- Ask questions in GitHub issues

## License

By contributing, you agree that your contributions are licensed under the same license as the project.
