# Contributing to System 7

Thank you for your interest in the System 7 reimplementation project! This is an educational and preservation project, and we appreciate all forms of contribution.

## Ways to Contribute

### Bug Reports
- Check [existing issues](https://github.com/Kelsidavis/System7/issues) first to avoid duplicates
- Include detailed reproduction steps
- Specify your hardware/emulator and OS
- Attach screenshots or logs if relevant

### Documentation
- Improve existing guides in `docs/components/`
- Add architectural explanations
- Document undiscovered System 7 behaviors
- Fix typos or clarify complex sections

### Translations
- Help translate the README into additional languages
- Add language-specific resource files in `resources/strings/`
- Create locale-specific documentation

### Code Improvements
- Fix bugs identified in [known issues](KNOWN_ISSUES.md)
- Improve code quality (test coverage, documentation)
- Optimize performance
- For major features, open an issue first to discuss approach

### Testing
- Report compatibility issues on different hardware/emulators
- Test on various QEMU configurations
- Verify language-specific features
- Test real System 7 applications

## Development Setup

### Prerequisites
```bash
# Ubuntu/Debian
sudo apt-get install build-essential gcc-multilib grub-pc-bin grub-efi-amd64-bin mtools xorriso qemu-system-x86 python3 python3-venv vim-common

# Other distros - install equivalent packages
```

Install the pinned Python lint and formatting tool before running the quality
gate:

```bash
python3 -m venv .venv
.venv/bin/pip install -r requirements-dev.txt
source .venv/bin/activate
```

### Building
```bash
# Build kernel
make

# Run the local quality gate (strict build, lint, documentation, and tests)
make check

# Build with every language, or one more than English
make LOCALE_ALL=1
make LOCALE_FR=1

# Build a bootable image, run it in QEMU, and collect integration-test results
# Requires grub-mkrescue and QEMU in addition to the build dependencies
make test-integration

# Run in QEMU
make run

# Run with GDB debugging
make debug
```

## Project Structure

```
System7/
├── include/              # Header files for all subsystems
├── src/                  # Code by subsystem, plus Platform/ and Integration/
├── docs/                 # Project documentation
│   ├── components/       # Detailed component guides
│   ├── future/           # Planning documents
├── resources/            # Resource files, fonts, patterns
│   ├── strings/          # STR# tables, one per language
│   └── device-tree/      # QEMU device tree files
├── scripts/              # Development utility scripts
├── Makefile              # Build system
└── README.md             # Main project README
```

## Code Style

- Follow existing conventions in the codebase
- Prefer a subsystem logging helper or `serial_logf()` with an explicit module
  and level. Use `serial_printf()` only where module-aware logging is unsuitable;
  do not use hosted `printf()` in the kernel.
- Use comments to explain behavior, invariants, constraints, or provenance.
  Track work status in issues or planning documents, not code comments.
- Document public APIs with clear comments
- No malloc/free in kernel (use zone-based allocation)

## Commit Messages

Use a concise imperative summary, matching the repository's recent history.
Add a body only when the summary needs context; reference an issue when relevant.
```
Refresh component guide with current commands

Explain any non-obvious motivation or trade-off here.
```

## Getting Help

- Check [KNOWN_ISSUES.md](KNOWN_ISSUES.md) for what is broken or missing
- Read component documentation in `docs/components/`
- Ask questions in GitHub issues

## License

By contributing, you agree that your contributions are licensed under the same license as the project.
