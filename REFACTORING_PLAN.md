# Universal Desktop Item System Refactoring Plan

## Goal
Replace special-case icon handling with a universal system that works for volumes, trash, files, folders, and aliases.

## Current Problems
- Volume icons use `IconPosition` array with indices 0..N
- Trash uses special index 999, not in array
- `gTrashDragPos` global needed for trash-specific positioning
- Hard to add new item types (files, folders, shortcuts)

## New Design
- Single `DesktopItem` array for all desktop items
- Each item has type enum: Volume, Trash, File, Folder, Alias
- Trash becomes regular item at index determined at initialization
- Universal positioning, dragging, hit-testing for all types

## Implementation Steps

### 1. Type Definitions (✓ DONE)
- Added `DesktopItemType` enum to finder_types.h
- Added `DesktopItem` struct with type, name, position, movable flag, type-specific data union

### 2. Global State Changes
- Change `gDesktopIcons` from `IconPosition*` to `DesktopItem*`
- Remove `gTrashDragPos` (no longer needed)
- Keep `gSelectedIcon` as index into unified array

### 3. Initialization
- Update `AllocateDesktopIcons()` to allocate DesktopItem array
- Add trash as regular item during initialization
- Set trash.movable = false, trash.type = kDesktopItemTrash

### 4. Core Functions to Update
- `IconAtPoint()` - remove special case for trash (999), check all items
- `UpdateIconRect()` - remove special case for trash (999), use item.position
- `TrackIconDragSync()` - remove 999 check, use item.movable flag
- `HandleDesktopClick()` - remove 999 check, use normal indexing
- `DrawVolumeIcon()` - remove trash position override check, iterate all items by type

### 5. Helper Functions
- Add `Desktop_AddItem()` for adding new items dynamically
- Add `Desktop_RemoveItem()` for removing items
- Add `Desktop_GetItemIcon()` to get icon based on item type

## Benefits
- Easy to add files, folders, aliases
- No special-case code
- Clean, maintainable codebase
- Extensible for future item types