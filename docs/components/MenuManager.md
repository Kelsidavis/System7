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
- `src/MenuManager/platform_stubs.c` – platform compatibility routines
- `src/MenuCommands.c` – dispatch selected menu commands through `DoMenuCommand`

## Responsibilities
- Maintain the menu list and menu bar data structures populated during `InitMenus`
- Draw the menu bar chrome (lozenge, titles) and each dropdown into off-screen buffers to reduce flicker
- Track mouse movement during a menu session, highlighting items and switching between menus when the pointer crosses titles
- Deliver final selections through the application-provided `MenuSelect`/`MenuChoice` loop and clean up saved bits
- Manage highlighting state, checkmarks, and enabling/disabling of items in response to application state

## Integration Points
- **Window Manager** coordinates activate/deactivate events; menus relinquish highlight when the application loses focus
- **Event Manager** feeds mouse-down events to kick off `MenuSelect` and supplies repeated mouse moves during tracking
- **Control Manager** supplies controls drawn in menus; the declared `PopUpMenuSelect` API is not implemented yet
- **Resource Manager** provides 'MENU'/'MBAR' resources parsed by `MenuResources.c` and loaded by `GetMenu`/`GetNewMBar`
- **Event Manager** routes Command-key shortcuts through `MenuKey` and selected commands through `DoMenuCommand`

## Testing & Debugging
- Use `make run` and interact with the Finder or SimpleText to exercise menu tracking; verify highlights and Command-key shortcuts
- Menu logs use the `[MENU]` tag; include `System71StdLib.h` and call `SysLogSetModuleLevel(kLogModuleMenu, kLogLevelDebug)` to enable debug-level output
- `make check-exports` confirms exported menu traps remain aligned with `docs/symbols_allowlist.txt`
- Edge cases: nested hierarchical menus, disabled items mid-track, SaveBits/RestoreBits correctness when overlapping windows

## Future Work
- Implement `PopUpMenuSelect` and connect it to controls that need pop-up menus
- Add auto-scroll for menus taller than the screen once Scroll Manager infrastructure is ready
