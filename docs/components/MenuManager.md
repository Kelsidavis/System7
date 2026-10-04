# Menu Manager

## Overview
Recreates the System 7 menu bar and pull-down menu experience, from resource loading to live tracking. Handles menu creation, insertion/sorting, drawing, highlighting, tracking loops, and command dispatch.

## Source Layout
- `src/MenuManager/MenuManagerCore.c` – menu creation and bar APIs, including `GetMenu` and `GetNewMBar`
- `src/MenuManager/MenuDisplay.c` / `MenuTrack.c` – draw menus and track pointer interaction
- `src/MenuManager/MenuSelection.c` – `MenuSelect`, `MenuKey`, and `MenuChoice`
- `src/MenuManager/MenuItems.c` – item manipulation, command keys, and hierarchical menu support
- `src/MenuManager/MenuTitleTracking.c` – top-level title interaction
- `src/MenuManager/MenuResources.c` – parse 'MENU' and 'MBAR' resources
- `src/MenuManager/menu_savebits.c` – save and restore screen bits during menu tracking
- `src/MenuManager/MenuResourceNames.c` – resource-name menu population (`AddResMenu`, `InsertResMenu`)
- `src/MenuManager/MenuBitsPool.c`, `MenuAppleIcon.c`, and `MenuAppIcon.c` – menu drawing support
- `src/MenuManager/MenuPlatform.c` – framebuffer save/restore, input tracking, and platform compatibility fallbacks
- `src/MenuCommands.c` – dispatch selected menu commands through `DoMenuCommand`

## Responsibilities
- Maintain the menu list and menu bar data structures populated during `InitMenus`
- Draw menu bar chrome and dropdowns directly to the framebuffer; save and restore the affected screen regions through the shared pooled SaveBits path
- Track mouse movement during a menu session, highlighting items and switching between menus when the pointer crosses titles
- Deliver final selections through the application-provided `MenuSelect`/`MenuChoice` loop and clean up saved bits
- Manage highlighting state, checkmarks, and enabling/disabling of items in response to application state

## Integration Points
- **Window Manager** coordinates activate/deactivate events; menus relinquish highlight when the application loses focus
- **Event Manager** feeds mouse-down events to kick off `MenuSelect` and supplies repeated mouse moves during tracking
- **Resource Manager** provides 'MENU'/'MBAR' resources parsed by `MenuResources.c` and loaded by `GetMenu`/`GetNewMBar`
- **Event Manager** routes Command-key shortcuts through `MenuKey` and selected commands through `DoMenuCommand`

## Testing & Debugging
- Use `make run` and interact with the Finder or SimpleText to exercise menu tracking; verify highlights and Command-key shortcuts
- Menu logs use the `[MENU]` tag; include `System71StdLib.h` and call `SysLogSetModuleLevel(kLogModuleMenu, kLogLevelDebug)` to enable debug-level output
- `make check-exports` confirms exported menu traps remain aligned with `docs/symbols_allowlist.txt`
- `make test-menu-names` checks literal names, sorted insertion blocks, prefix filtering, full-menu handling, and native DA lists beyond 32 entries. Finder and application resource menus share this insertion path; the guest suite checks names from two simultaneously open resource files. Ordering is bytewise Mac Roman, not international script collation. Both routines search all open resource files, as specified by Apple's [AppendResMenu](https://dev.os9.ca/techpubs/mac/Toolbox/Toolbox-147.html) and [InsertResMenu](https://dev.os9.ca/techpubs/mac/Toolbox/Toolbox-148.html) references.
- Edge cases: nested hierarchical menus, disabled items mid-track, SaveBits/RestoreBits correctness when overlapping windows

## Future Work
- Implement the remaining [Menu Manager compatibility gaps](Compatibility/System7_Compatibility_Gaps.md#window-dialog-control-and-menu-managers), including popup tracking, international resource insertion, and procedural menu setup
- Add automatic scrolling during tracking for menus taller than the screen
