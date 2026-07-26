---
name: Bug Report
about: Report something that doesn't work as expected
title: "[BUG] "
labels: bug
assignees: ''

---

## Description
A clear and concise description of what the bug is.

## Environment
- **Testing Environment**: QEMU / Bare Metal / Other
- **QEMU Version** (if applicable): 
- **Host OS**: Ubuntu/Debian/macOS/Other
- **Build Configuration**: English only / Multi-language / Custom

## Steps to Reproduce
1. Step one
2. Step two
3. ...

## Expected Behavior
What you expected to happen

## Actual Behavior
What actually happened (include crash logs, error messages, screenshots)

## Relevant Code or Configuration
Any relevant Makefile flags, build commands, or code snippets

## Additional Context
- **Is this QEMU-only or affects bare metal?** (knowing this is important since most testing is QEMU-based)
- Any other context or information that might help diagnose the issue

## Notes
- Be aware: this is a sloppy experimental OS, crashes and edge cases are expected
- Most subsystems are untested on bare metal
- Many features work in QEMU but may behave differently on real hardware
