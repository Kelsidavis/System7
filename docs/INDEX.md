# System 7 Documentation Index

## Getting Started

- [README](../README.md) — what this is, what works, how to build and run it
- [Getting Started](GETTING_STARTED.md) — first build and first boot
- [Quick Reference](QUICKREF.md) — build, run and debug commands on one page
- [Project Evolution](PROJECT_EVOLUTION.md) — how the project got here
- [Featured In](FEATURED_IN.md) — the Action Retro video and what it found

## Understanding the System

- [Memory Management](MEMORY_MANAGEMENT.md) — zones, handles, and how the heap is sized
- [malloc Prevention](MALLOC_PREVENTION.md) — why the kernel does not use malloc/free
- [Known Issues](KNOWN_ISSUES.md) — what is broken or missing, and why
- [Compatibility Gaps](components/Compatibility/System7_Compatibility_Gaps.md) — where the Toolbox differs from Inside Macintosh
- [Bare Metal](BARE_METAL_IMPROVEMENTS.md) — real-hardware support and what remains

## Components

| Component | Documentation |
|-----------|---------------|
| Window Manager | [WindowManager.md](components/WindowManager.md) |
| Menu Manager | [MenuManager.md](components/MenuManager.md) |
| Event Manager | [EventManager.md](components/EventManager.md) |
| Dialog Manager | [DialogManager/](components/DialogManager/) |
| Control Manager | [ControlManager/](components/ControlManager/) |
| Font Manager | [FontManager/](components/FontManager/) |
| Resource Manager | [ResourceManager.md](components/ResourceManager.md) |
| Serial Logging | [System/Logging.md](components/System/Logging.md) |
| Desk Accessories | `src/DeskManager/` — Calculator, Alarm Clock, Key Caps, Note Pad, Chooser |
| Patterns | `src/PatternMgr/` and `Patterns.rsrc` — 17 colour and 32 black-and-white patterns |

[Components index](components/README.md)

## Contributing and Planning

- [Contributing](CONTRIBUTING.md)
- [Porting plan](future/PORTING_PLAN.md) — other architectures
- [Refactoring plan](future/REFACTORING_PLAN.md) — the platform layer

## Project Layout

```
System7/
├── README.md
├── docs/                  # this documentation
│   ├── components/        # subsystem guides
│   └── future/            # plans
├── include/               # headers, by subsystem
├── src/                   # implementation, by subsystem
├── resources/strings/     # STR# tables, one per language (LOCALE_ALL=1 builds them all)
├── patterns.json          # pattern sources for Patterns.rsrc
└── tests/                 # test notes and harnesses
```

The integration tests live in `src/Integration/IntegrationTests.c` and run when
the kernel is built with `INTEGRATION_TESTS=1`.
