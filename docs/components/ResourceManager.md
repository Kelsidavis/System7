# Resource Manager

## Overview
Implements the classic Mac Resource Manager APIs used to load system resources (patterns, icons, menus, dialogs, etc.). Provides handle-based caching, type/ID lookup tables, and integration points for resource conversion tooling (`gen_rsrc.py`, pattern/icon exporters).

## Source Layout
- `src/ResourceManager.c` – top-level Toolbox API surface (`InitResources`, `GetResource`, `GetNamedResource`, `ReleaseResource`, `AddResource`, `RemoveResource`)
- `src/ResourceMgr/resource_manager.c` / `ResourceMgr.c` – internal implementation (resource map parsing, handle management, caching, error handling)
- `resource_traps.c` – trap glue for legacy compatibility
- `src/ResourceDecompression.c` – future hook for compressed resources
- `src/Resources/` – generated data blobs (patterns, icons) produced by tools in `/tools`
- `src/PatternMgr/` & `src/color_icons.c` – consumers of PAT/ppat/icon resources generated via `gen_rsrc.py` and `create_color_icons.py`

## Responsibilities
- Load resource map tables, build type and reference lists, and resolve resources by type (FourCC) and ID
- Hand out handles whose master pointers integrate with the Memory Manager zone allocator
- Maintain an LRU cache so frequently accessed resources stay locked in memory while unused entries can be purged
- Support name-based lookup, attribute flags (locked, purgeable), and update dirty bits when resources are modified
- Serve as the central authority for high-level systems (Font Manager, Menu Manager, Dialog Manager) expecting ROM resource behaviour

## Tooling & Data Flow
- JSON manifests (`patterns.json`) feed `tools/gen_rsrc.py`, which emits `Patterns.rsrc` for inclusion at build time
- Icon conversion scripts (`create_color_icons.py`, `gen_rsrc.py`) populate `src/color_icons.c` and related assets
- `docs/symbols_allowlist.txt` tracks exported Resource Manager routines used by `tools/check_exports.sh`

## Integration Points
- **Memory Manager** supplies zone-based handles used for resource storage
- **Font Manager** consumes NFNT/FOND resources (pipeline ready once resource files are hooked up)
- **Menu/Dialog/Control Managers** load MENU/MBAR/DLOG/DITL/CNTL resources for UI construction
- **Pattern/Icon systems** fetch PAT/ppat/SICN resources for desktop rendering

## Testing & Debugging
- Run `make check-exports` to ensure expected Resource Manager traps remain exported
- Resource loading currently relies on generated assets; inspect `Patterns.rsrc` for correctness when adding new resources
- Serial logging tagged `[RSRC]` can be enabled to trace cache hits/misses (ensure whitelist in `System71StdLib.c` includes the tag)

## Future Work
- Hook the Resource Manager to on-disk HFS resources so NFNT/FOND and application assets can be loaded dynamically
- Implement resource map writing (`ChangedResource`, `WriteResource`) once file I/O pipeline is ready
- Add coverage tests that load every resource type produced by tooling to catch manifest regressions
