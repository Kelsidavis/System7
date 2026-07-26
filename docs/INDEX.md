# System 7 Documentation Index

Welcome to the System 7 documentation. This index will help you find what you're looking for.

## 🚀 Getting Started

**First time here?**
- [Getting Started](GETTING_STARTED.md) — 5-minute quick start guide
- [Project Evolution](PROJECT_EVOLUTION.md) — Understanding this project's journey
- [Featured In](FEATURED_IN.md) — YouTube feature and real hardware testing results
- [README (main)](../README.md) — Project overview

## 📚 Understanding the System

### Architecture & Design
- [IMPLEMENTATION_STATUS_AUDIT.md](IMPLEMENTATION_STATUS_AUDIT.md) — What's implemented, what's not
- [MEMORY_MANAGEMENT.md](MEMORY_MANAGEMENT.md) — Zone-based allocation, how memory works
- [MALLOC_PREVENTION.md](MALLOC_PREVENTION.md) — Why we don't use malloc/free in kernel

### Bare Metal & Hardware
- [BARE_METAL_IMPROVEMENTS.md](BARE_METAL_IMPROVEMENTS.md) — Plan for real hardware support
- Details: Current QEMU-only limitations and roadmap for bare metal improvements

### Known Issues & Limitations
- [KNOWN_ISSUES.md](KNOWN_ISSUES.md) — Current bugs and workarounds
- [System7_Compatibility_Gaps.md](components/Compatibility/System7_Compatibility_Gaps.md) — Differences from real System 7

## 🔧 Component Deep Dives

Each component has its own documentation:

| Component | Documentation |
|-----------|----------------|
| **Window Manager** | [WindowManager.md](components/WindowManager.md) |
| **Menu Manager** | [MenuManager.md](components/MenuManager.md) |
| **Event Manager** | [EventManager.md](components/EventManager.md) |
| **Dialog Manager** | [DialogManager/](components/DialogManager/) |
| **Control Manager** | [ControlManager/](components/ControlManager/) |
| **Font Manager** | [FontManager/](components/FontManager/) |
| **Resource Manager** | [ResourceManager.md](components/ResourceManager.md) |
| **Serial Logging** | [System/Logging.md](components/System/Logging.md) |
| **Component Status** | [STATUS.md](components/STATUS.md) |

**[Full Components Index](components/README.md)** — Organized by subsystem

## 🌍 Internationalization

- [TRANSLATIONS.md](TRANSLATIONS.md) — Available in 37 languages
- `translations/` — All translated README files organized by language group

## 👥 Contributing

- [CONTRIBUTING.md](CONTRIBUTING.md) — How to contribute to the project
- [CONTRIBUTING.md](../docs/CONTRIBUTING.md) — Types of contributions welcome

## 🛣️ Planning & Future Work

- [IMPLEMENTATION_PRIORITIES.md](../IMPLEMENTATION_PRIORITIES.md) — Planned work and roadmap
- [future/REFACTORING_PLAN.md](future/REFACTORING_PLAN.md) — Potential refactoring
- [future/PORTING_PLAN.md](future/PORTING_PLAN.md) — Cross-platform porting notes

## 🏗️ Project Structure

### Main directories
```
System7/
├── README.md              # Start here
├── docs/                  # All documentation
│   ├── components/        # Component guides
│   ├── future/            # Planning docs
│   ├── translations/      # 37 language READMEs
│   └── [this index]
├── include/               # Public headers
├── src/                   # Implementation (organized by subsystem)
├── resources/
│   ├── strings/           # Localization files
│   └── device-tree/       # QEMU device trees
├── scripts/               # Development utilities
└── [other files]
```

## 📋 Quick Reference

### What You're Looking For
- **"How do I get started?"** → [Getting Started](GETTING_STARTED.md)
- **"What actually works?"** → [IMPLEMENTATION_STATUS_AUDIT.md](IMPLEMENTATION_STATUS_AUDIT.md)
- **"Why is this so rough?"** → [Project Evolution](PROJECT_EVOLUTION.md)
- **"How do I help?"** → [CONTRIBUTING.md](CONTRIBUTING.md)
- **"How does X work?"** → [Components](components/) or search below
- **"What's broken?"** → [KNOWN_ISSUES.md](KNOWN_ISSUES.md)
- **"What are you planning?"** → [IMPLEMENTATION_PRIORITIES.md](../IMPLEMENTATION_PRIORITIES.md)

### By Experience Level
- **Complete beginner** → Start with [Getting Started](GETTING_STARTED.md)
- **Want to understand design** → Read [components/README.md](components/README.md)
- **Want to understand limitations** → Read [KNOWN_ISSUES.md](KNOWN_ISSUES.md) and [Project Evolution](PROJECT_EVOLUTION.md)
- **Ready to dive into code** → Pick a component in [components/](components/) and read the source

## 🔍 Searching

If you can't find what you're looking for:
1. Check [IMPLEMENTATION_STATUS_AUDIT.md](IMPLEMENTATION_STATUS_AUDIT.md) for system overview
2. Look through [components/README.md](components/README.md) for specific subsystems
3. Search code comments with `grep TODO` or `grep FIXME`
4. Open a GitHub issue asking for guidance

---

**Last updated**: July 2026  
**Total documentation files**: 60+  
**Organized by**: Component, topic, and experience level
