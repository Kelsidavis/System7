# X11 Environment for System 7.1

This directory contains the X11 implementation for System 7.1, providing a modern desktop environment while maintaining Mac Classic aesthetics.

## Architecture

```
Boot Selector
├─ Legacy Mode (Original System 7 Desktop)
└─ X11 Mode (Modern X11 + Mac Classic DE)
    ├─ Xfbdev (X Server on framebuffer)
    ├─ MacWM (Custom Mac Classic window manager)
    └─ Desktop Environment
        ├─ Menu Bar (Mac Classic style)
        ├─ File Manager
        └─ Application Launcher
```

## Components

- `xserver/` - X11 server (Xfbdev)
- `wm/` - Window manager with Mac Classic theming
- `de/` - Desktop environment (menu bar, file browser)
- `apps/` - X11 applications (terminal, file manager)
- `config/` - Theme and configuration files

## Building

```bash
make x11-build      # Build X11 components
make x11-install    # Install to rootfs
make x11-test       # Test X11 setup
```

## Booting

At bootloader menu:
- Press 'L' for Legacy Mode (System 7)
- Press 'X' for X11 Mode (default)
