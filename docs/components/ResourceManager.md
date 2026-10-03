# Resource Manager

## Overview
Implements the classic Mac Resource Manager APIs used to load and update resources in the system and application resource forks.

## Source Layout
- `src/ResourceMgr/ResourceMgr.c` – resource map parsing, handle management,
  lookup, serialization, and error handling (`GetResource`,
  `GetIndResource`, `GetNamedResource`, `OpenResFile`, `FSpOpenResFile`,
  `ReleaseResource`, `AddResource`, `RemoveResource`, …)
- `src/ResourceMgr/StringResources.c` – `STRS`/`STR#` accessors
- `src/CPU/m68k_interp/M68KToolbox.c` – 68K Resource Manager trap dispatch
- `src/Resources/` – generated data blobs (patterns and icons)
- `src/PatternMgr/` & `src/color_icons.c` – consumers of PAT/ppat/icon resources

## Responsibilities
- Load resource map tables, build type and reference lists, and resolve resources by type (FourCC) and ID
- Hand out handles whose master pointers integrate with the Memory Manager zone allocator
- Cache loaded resources as handles and preserve their resource-map identity
- Support name-based lookup, resource attributes, and dirty tracking
- Serve as the central authority for high-level systems (Font Manager, Menu Manager, Dialog Manager) expecting ROM resource behaviour

## Tooling & Data Flow
- JSON manifests feed the repository-root `gen_rsrc.py`, which emits resource files for inclusion at build time
- Icon conversion and generation tools live under `tools/` and `scripts/`
- `docs/symbols_allowlist.txt` tracks required exported routines checked by `tools/check_exports.sh`

## Integration Points
- **Memory Manager** supplies zone-based handles used for resource storage
- **Font Manager** parses NFNT/FOND resources and resolves them through `GetResource`; validate the loader with representative application resource forks
- **Menu/Dialog/Control Managers** load MENU/MBAR/DLOG/DITL/CNTL resources for UI construction
- **Pattern/Icon systems** use PAT/ppat/icon resources for desktop rendering
  (`patterns.json` generates `Patterns.rsrc`, embedded in the build; `src/PatternMgr/`
  serves its pattern resources, and the Set Desktop Pattern control panel
  previews the selection live)

## Testing & Debugging
- Run `make check-exports` to ensure expected Resource Manager traps remain exported
- Resource loading currently relies on generated assets; inspect the generated
  `Patterns.rsrc` when validating output from `patterns.json`
- Serial logging tagged `[RSRC]` can be enabled to trace cache hits/misses (ensure whitelist in `System71StdLib.c` includes the tag)

## Future Work
- Extend integration tests across resource lookup, mutation, serialization, and reopen paths
- Add coverage tests that load every resource type produced by tooling to catch manifest regressions
