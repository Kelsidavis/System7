# System 7 Documentation Index

## Getting Started

- [README](../README.md) — what this is, what works, how to build and run it
- [Getting Started](GETTING_STARTED.md) — first build and first boot
- [Quick Reference](QUICKREF.md) — build, run and debug commands on one page
- [Project Evolution Retrospective](PROJECT_EVOLUTION.md) — a historical account of the project's development
- [Featured In](FEATURED_IN.md) — the Action Retro video and its findings on the tested revision

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
| Patterns | `src/PatternMgr/` and `patterns.json` — 17 colour `ppat` and 32 black-and-white `PAT` resources; `Patterns.rsrc` is generated during the build |

[Components index](components/README.md)

## Contributing and Planning

- [Contributing](CONTRIBUTING.md)
- [Porting plan](future/PORTING_PLAN.md) — other architectures
- [Refactoring plan](future/REFACTORING_PLAN.md) — the platform layer

## Project Layout

```
System7/
├── README.md
├── config/                # build configurations
├── docs/                  # project and subsystem documentation
│   ├── components/        # subsystem guides
│   └── future/            # planning documents
├── include/               # headers, by subsystem
├── resources/             # localized strings, patterns, and device trees
├── scripts/               # repository checks and generation helpers
├── src/                   # implementation, by subsystem
├── tests/                 # test harnesses, guest programs, and test notes
├── tools/                 # asset and image utilities
└── patterns.json          # pattern sources for the generated Patterns.rsrc
```

The integration tests live in `src/Integration/IntegrationTests.c` and run when
the kernel is built with `INTEGRATION_TESTS=1`.
