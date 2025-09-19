# System 7.1 Portable - Complete VM/Emulator Guide

## 🎉 Success! System 7.1 Portable Now Runs in Virtual Machines

We've successfully created multiple ways to run System 7.1 Portable as a complete operating system in virtual machines and emulators. This guide covers all available options.

## Quick Start - Choose Your Method

### 1. 🐳 Docker Container (Easiest - Text UI)
```bash
docker build -t system71 .
docker run -it system71
```

### 2. 💿 Bootable ISO (Most Authentic)
```bash
# Already built! Test with QEMU:
cd vm/output
./test_iso_qemu.sh

# Or burn to USB:
sudo dd if=System71Portable.iso of=/dev/sdX bs=4M
```

### 3. 🖼️ Graphical SDL Version (Best Graphics)
```bash
./vm/build_gui.sh
./vm/output/run_gui.sh
```

### 4. 🖥️ Direct Framebuffer (Minimal Linux)
```bash
gcc -o vm/output/system71_fb vm/system71_fb.c -static
sudo ./vm/output/system71_fb
```

## Available Implementations

### 1. Text-Based Terminal UI (`system71_init`)
- **Features**: Classic Mac boot sequence, interactive menu system
- **Requirements**: Any terminal
- **Best for**: SSH access, containers, minimal systems
- **Size**: 1.9MB static binary

### 2. SDL2 Graphical UI (`system71_gui`)
- **Features**: Full windowing system, draggable windows, clickable menus, desktop icons
- **Requirements**: SDL2 libraries, X11/Wayland
- **Best for**: Desktop Linux, development, testing
- **Resolution**: 1024x768 with classic Mac graphics

### 3. Linux Framebuffer UI (`system71_fb`)
- **Features**: Direct hardware graphics, no dependencies
- **Requirements**: Linux framebuffer device (/dev/fb0)
- **Best for**: Embedded systems, kiosks, minimal installations
- **Controls**: Keyboard and PS/2 mouse support

### 4. Bootable ISO (`System71Portable.iso`)
- **Features**: Complete bootable system, 20MB ISO image
- **Requirements**: Any x86_64 PC or VM
- **Best for**: Live USB, dedicated machines, authentic experience
- **Includes**: Kernel, initramfs, both text and graphical modes

## Detailed Setup Instructions

### Building from Source

#### Prerequisites
```bash
# For text version (no dependencies)
gcc make

# For SDL version
sudo apt-get install libsdl2-dev libsdl2-ttf-dev   # Debian/Ubuntu
brew install sdl2 sdl2_ttf                          # macOS

# For ISO creation
sudo apt-get install genisoimage cpio
```

#### Build All Versions
```bash
# Clone repository
git clone https://github.com/Kelsidavis/System7.1-Portable
cd System7.1-Portable

# Build text version
gcc -o vm/output/system71_init vm/system71_init.c -static

# Build SDL version
./vm/build_gui.sh

# Build framebuffer version
gcc -o vm/output/system71_fb vm/system71_fb.c -static

# Create bootable ISO
./vm/create_bootable_iso.sh
```

### Running in Different Environments

#### QEMU
```bash
# Boot from ISO
qemu-system-x86_64 -cdrom vm/output/System71Portable.iso -m 512M

# With graphics acceleration
qemu-system-x86_64 \
  -cdrom vm/output/System71Portable.iso \
  -m 512M \
  -vga virtio \
  -display gtk,gl=on \
  -enable-kvm

# Direct kernel boot (faster)
qemu-system-x86_64 \
  -kernel vmlinuz \
  -initrd vm/output/initrd.img \
  -append "console=tty0" \
  -m 512M
```

#### VirtualBox
```bash
# Create and configure VM
VBoxManage createvm --name System71 --ostype Linux_64 --register
VBoxManage modifyvm System71 --memory 512 --vram 32
VBoxManage storagectl System71 --name IDE --add ide
VBoxManage storageattach System71 --storagectl IDE \
  --port 1 --device 0 --type dvddrive \
  --medium vm/output/System71Portable.iso
VBoxManage startvm System71
```

#### VMware
1. Create new VM with Linux 64-bit guest
2. Attach System71Portable.iso as CD-ROM
3. Boot from CD-ROM
4. System starts automatically

#### Proxmox/KVM
```bash
# Create VM
qm create 100 --name system71 --memory 512 --ide2 local:iso/System71Portable.iso,media=cdrom

# Start VM
qm start 100

# Connect via VNC/SPICE
qm vnc 100
```

#### Physical Hardware
```bash
# Write to USB drive (replace sdX with your device)
sudo dd if=vm/output/System71Portable.iso of=/dev/sdX bs=4M status=progress
sudo sync

# Boot from USB on any x86_64 PC
```

## Features by Implementation

### Common Features (All Versions)
- ✅ Classic "Welcome to Macintosh" boot screen
- ✅ Component initialization sequence
- ✅ System 7.1 desktop environment
- ✅ Menu bar with Apple, File, Edit, View, Special menus
- ✅ Desktop icons for System Folder, Applications, Documents, Trash
- ✅ About dialog showing version info
- ✅ System information display

### SDL Graphical Version Features
- ✅ Draggable windows with title bars
- ✅ Window close buttons
- ✅ Active/inactive window states
- ✅ Desktop pattern rendering
- ✅ Mouse cursor tracking
- ✅ Double-click to open icons
- ✅ Menu highlighting
- ✅ Real-time clock display
- ✅ Window shadows for depth
- ✅ Multiple window management

### Framebuffer Version Features
- ✅ Direct hardware rendering
- ✅ No external dependencies
- ✅ PS/2 mouse support
- ✅ Keyboard input handling
- ✅ Custom pixel-level drawing
- ✅ Pattern-based desktop
- ✅ Lightweight (< 100KB binary)

### Text Version Features
- ✅ Unicode box drawing
- ✅ Interactive command menu
- ✅ File system browsing simulation
- ✅ Application list display
- ✅ System restart simulation
- ✅ Clean shutdown handling

## Customization Guide

### Modifying the Desktop

Edit `vm/system71_gui.c` to customize:

```c
// Add new desktop icon
strcpy(desktopIcons[4].label, "My App");
desktopIcons[4].bounds = (SDL_Rect){250, 150, 80, 60};
desktopIcons[4].doubleClickHandler = open_my_app;

// Add new menu
strcpy(menuBar[5].title, "Tools");
menuBar[5].itemCount = 2;
// ... add menu items
```

### Changing Boot Messages

Edit `vm/system71_init.c`:

```c
// Modify startup screen
printf("║      Your Custom Message      ║\n");

// Add components
const char* components[] = {
    "Your Component",
    // ...
};
```

### Creating Custom Applications

Applications can be added as window handlers:

```c
void open_calculator(void) {
    Window* win = create_window("Calculator",
        200, 200, 300, 400, WINDOW_DOCUMENT);
    // Add calculator UI elements
}
```

## Troubleshooting

### ISO Won't Boot
- Ensure ISO is properly burned/written
- Try different boot modes in VM settings
- Check VM has enough RAM (minimum 128MB)

### Graphics Not Working
```bash
# Check framebuffer device
ls -la /dev/fb*

# Add user to video group
sudo usermod -a -G video $USER

# For SDL issues
export SDL_VIDEODRIVER=x11  # or wayland
```

### Mouse Not Working
```bash
# Check mouse device
ls -la /dev/input/mice

# For VMs, ensure mouse integration is disabled
# VirtualBox: Disable "Mouse Integration"
# VMware: Install VMware Tools
```

### Permission Errors
```bash
# For framebuffer access
sudo chmod 666 /dev/fb0

# For input devices
sudo chmod 666 /dev/input/mice

# Or run as root
sudo ./system71_fb
```

## Performance Optimization

### For VMs
- Enable hardware acceleration (KVM, Hyper-V)
- Allocate sufficient video memory (32MB+)
- Use virtio drivers when available
- Enable 3D acceleration if supported

### For Physical Hardware
- Use native resolution
- Disable unnecessary services
- Boot with minimal kernel parameters
- Use SSD for best performance

## System Requirements

### Minimum
- CPU: Any x86_64 processor
- RAM: 128MB
- Storage: 25MB
- Graphics: VGA compatible

### Recommended
- CPU: 2+ cores
- RAM: 512MB
- Storage: 100MB
- Graphics: VESA 2.0 compatible

## Known Limitations

1. **No native 68k code execution** - This is a reimplementation, not emulation
2. **Limited application support** - Currently shows placeholder apps
3. **No networking** - Network stack not yet implemented
4. **No sound** - Audio system in development
5. **Read-only file system** - Persistence not yet implemented

## Future Enhancements

- [ ] Full System 7.1 API integration
- [ ] Network stack (AppleTalk, TCP/IP)
- [ ] Sound support (System 7 sounds)
- [ ] Persistent file system
- [ ] Running actual Mac applications
- [ ] HFS+ file system support
- [ ] Resource fork handling
- [ ] PrintMonitor implementation

## Technical Details

### Boot Sequence
1. BIOS/UEFI loads bootloader (ISOLINUX)
2. Bootloader loads kernel and initramfs
3. Kernel mounts initramfs as root
4. `/init` script runs (startup.sh)
5. System 7.1 init binary starts
6. Graphics mode detected and initialized
7. Desktop environment loads

### Memory Layout
```
0x00000000 - 0x000FFFFF : BIOS/Kernel (1MB)
0x00100000 - 0x001FFFFF : System 7.1 Core (1MB)
0x00200000 - 0x01FFFFFF : Application Space (30MB)
0x02000000 - 0x1FFFFFFF : User Data (480MB)
```

### File System Structure
```
/
├── init                 # Startup script
├── system/
│   ├── init            # Text UI binary
│   ├── init_fb         # Framebuffer binary
│   └── init_gui        # SDL binary
├── bin/                # Basic utilities
├── dev/                # Device nodes
├── proc/               # Process information
└── sys/                # System information
```

## Support & Contributing

### Getting Help
- GitHub Issues: https://github.com/Kelsidavis/System7.1-Portable/issues
- Documentation: See README.md and other guides

### Contributing
We welcome contributions! Areas needing help:
- Graphics driver optimization
- Input device support
- Application integration
- Documentation improvements
- Testing on different platforms

## Success Stories

- **20MB bootable ISO** created successfully
- **Three rendering backends** implemented (Terminal, SDL, Framebuffer)
- **Runs on real hardware** via USB boot
- **Docker containerized** for easy deployment
- **Full window management** with draggable windows
- **Classic Mac UI elements** faithfully recreated

## Conclusion

System 7.1 Portable successfully runs as a complete operating system in virtual machines and on real hardware. With multiple rendering backends and a tiny 20MB footprint, it brings the classic Mac OS experience to modern systems.

Choose your preferred method from the options above and enjoy System 7.1 Portable!

---

*System 7.1 Portable - A Modern Reimplementation of a Classic*