# System 7.1 interface reference

The user-supplied Finder screenshots are the visual reference for system-owned
chrome and Finder. Application contents, installed files, machine identity,
memory figures, and the clock must reflect the running system; they are not
fixed values to copy from the examples.

## Visual requirements

- A white, 20-pixel menu bar with Chicago text, the color Apple icon, and
  distinct Help and application-menu icons. Disabled items must be visibly
  dimmed without changing menu spacing.
- Document windows have a white title bar, six thin horizontal rules while
  active, centered Chicago titles, and small outlined close/zoom boxes. An
  inactive title bar has no rules or active controls and a dimmed title.
- Control rectangles must be shared by drawing, hit testing, and tracking.
  Chrome and text must respect overlapping windows and screen boundaries.
- Finder's item/disk summary belongs immediately below the title bar, above
  icon or list contents. List headers, row clicks, and scrolling must account
  for that summary strip.
- Finder icon labels use the small Geneva face, not bold Chicago. Names,
  including Mac Roman characters and italic aliases, must be measured with
  the same glyph metrics used to draw them.
- Folder, document, disk, application, and Trash icons must be recognizable
  classic 32-pixel icons, with working masks and selection rendering.
- Finder windows need classic scrollbars and grow boxes with working arrows,
  thumb tracking, paging, and resize behavior; painted controls alone are not
  sufficient.
- The default desktop is the black/white gray stipple. A user's selected
  pattern or solid color must survive subsequent desktop initialization.
- About This Macintosh uses the classic compact memory-summary layout while
  reporting the actual hardware and memory accounting.

## Verification

Compare unscaled framebuffer captures at small classic resolutions and larger
displays. Exercise active/inactive windows, overlapping dialogs, long titles,
menus, selected icons, scrolled lists, and resizing. Pixel assertions should
check controls and rules, while repaint tests should compare against the
actual desktop pattern rather than assume that a desktop pixel cannot be
black.

## Implementation gaps

Only the Chicago font strike is shipped; Geneva currently falls back to it.
Finder labels use a vertically reduced Chicago bitmap to approach the
reference size. The generic folder has shaded color artwork, while document
artwork remains placeholder line art and some icon resource mappings point to
unavailable IDs. Icon view draws classic scrollbar controls. Its
vertical bar supports arrow, page-track, mouse-wheel, and keyboard-selection
scrolling. The horizontal bar now reflects content width and supports arrow
and page-track scrolling; thumb dragging remains unimplemented. The Help icon,
grow-box artwork, and About window also require visual comparison.
