# System 7.1 Portable Makefile
# Builds complete System 7.1 kernel

# Build error handling: delete targets on failure, stop on first error
.DELETE_ON_ERROR:
.SUFFIXES:

# Default goal (build kernel)
.DEFAULT_GOAL := all

# Platform configuration
PLATFORM ?= x86

# Load build configuration (default or user-specified)
CONFIG ?= default
-include config/$(CONFIG).mk

# Directories
BUILD_DIR = build
OBJ_DIR = $(BUILD_DIR)/obj
BIN_DIR = $(BUILD_DIR)/bin
ISO_DIR = iso
HAL_DIR = src/Platform/$(PLATFORM)

# Output files
KERNEL = kernel.elf
ISO = system71.iso

# Tools
CC = gcc
AS = as
LD = ld
GRUB = grub-mkrescue

# Flags
# [WM-050] SYS71_PROVIDE_FINDER_TOOLBOX=1 means: DO NOT provide Toolbox stubs; real implementations win.
#          When defined, stubs in sys71_stubs.c are excluded via #ifndef guards.
#          This ensures single source of truth per symbol (no duplicate definitions).
# [WM-052] Warnings are errors - no papering over issues

# Base CFLAGS (optimization level from config)
OPT_FLAGS = -O$(OPT_LEVEL)
ifeq ($(DEBUG_SYMBOLS),1)
  OPT_FLAGS += -g
endif

CFLAGS = -DSYS71_PROVIDE_FINDER_TOOLBOX=1 \
         -ffreestanding -fno-builtin -fno-stack-protector -nostdlib \
         -fno-pic -fno-pie \
         -Wall -Wextra -Wformat=2 -Wmissing-prototypes -Wmissing-declarations -Wshadow -Wcast-qual \
         -Wpointer-arith -Wstrict-prototypes -Wno-unused-parameter \
         -Wundef -Wvla -Wcast-align -Wlogical-op -Wduplicated-cond -Wduplicated-branches \
         -Wnull-dereference -Wjump-misses-init -Warray-bounds=2 -Wshift-overflow=2 \
         $(OPT_FLAGS) -fno-inline -fno-optimize-sibling-calls -I./include -I./src -std=c2x -m32 \
         -Wuninitialized -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 \
         -Wno-multichar -Wno-pointer-sign -Wno-sign-compare \
         -fno-common -fno-delete-null-pointer-checks \
         -MMD -MP

# Add feature flags based on configuration
ifeq ($(ENABLE_RESOURCES),1)
  CFLAGS += -DENABLE_RESOURCES=1
endif
ifeq ($(ENABLE_FILEMGR_EXTRA),1)
  CFLAGS += -DENABLE_FILEMGR_EXTRA=1
endif
ifeq ($(ENABLE_PROCESS_COOP),1)
  CFLAGS += -DENABLE_PROCESS_COOP=1
endif
ifeq ($(MODERN_INPUT_ONLY),1)
  CFLAGS += -DMODERN_INPUT_ONLY=1
endif
ASFLAGS = --32
LDFLAGS = -melf_i386 -nostdlib -no-pie

# Resource files
RSRC_JSON = patterns.json
RSRC_BIN = Patterns.rsrc
PATTERN_RESOURCE ?= resources/patterns_authentic_color.json

# Source files
C_SOURCES = src/main.c \
            src/boot.c \
            src/SystemInit.c \
            src/sys71_stubs.c \
            src/System71StdLib.c \
            src/System/SystemTheme.c \
            src/ToolboxCompat.c \
            src/Finder/finder_main.c \
            src/Finder/desktop_manager.c \
            src/Finder/folder_window.c \
            src/Finder/alias_manager.c \
            src/Finder/trash_folder.c \
            src/Finder/AboutThisMac.c \
            src/QuickDraw/QuickDrawCore.c \
            src/QuickDraw/Bitmaps.c \
            src/QuickDraw/QuickDrawPlatform.c \
            src/QuickDraw/Coordinates.c \
            src/QuickDraw/Regions.c \
            src/QuickDraw/Pictures.c \
            src/QuickDraw/ColorQuickDraw.c \
            src/QuickDraw/GWorld.c \
            src/QuickDraw/Patterns.c \
            src/OSUtils/OSUtilsTraps.c \
            src/Platform/WindowPlatform.c \
            src/Platform/x86/io.c \
            src/Platform/x86/ata.c \
            src/Platform/x86/ps2.c \
            src/Platform/x86/hal_boot.c \
            src/Platform/x86/hal_input.c \
            src/SoundManager/SoundManagerBareMetal.c \
            src/SoundManager/SoundHardwarePC.c \
            src/MenuManager/MenuManagerCore.c \
            src/MenuManager/MenuSelection.c \
            src/MenuManager/MenuDisplay.c \
            src/MenuManager/menu_savebits.c \
            src/MenuManager/MenuTitleTracking.c \
            src/MenuManager/MenuTrack.c \
            src/MenuManager/MenuAppleIcon.c \
            src/MenuManager/MenuItems.c \
            src/MenuManager/platform_stubs.c \
            src/MenuCommands.c \
            src/Finder/Icon/icon_system.c \
            src/Finder/Icon/icon_resources.c \
            src/Finder/Icon/icon_resolver.c \
            src/Finder/Icon/icon_draw.c \
            src/Finder/Icon/icon_label.c \
            src/trash_icons.c \
            src/chicago_font_data.c \
            src/FontManager/FontManagerCore.c \
            src/FontManager/FontResourceLoader.c \
            src/FontManager/FontStyleSynthesis.c \
            src/FontManager/FontScaling.c \
            src/test_fontmgr.c \
            src/PatternMgr/pattern_manager.c \
            src/PatternMgr/pattern_resources.c \
            src/PatternMgr/pram_prefs.c \
            src/Resources/pattern_data.c \
            src/Resources/happy_mac_icon.c \
            src/ControlPanels/cdev_desktop.c \
            src/simple_resource_manager.c \
            src/ControlManager/ControlManagerCore.c \
            src/ControlManager/ControlTracking.c \
            src/ControlManager/ScrollbarControls.c \
            src/ControlManager/StandardControls.c \
            src/ControlManager/ControlSmoke.c \
            src/control_stubs.c \
            src/patterns_rsrc.c \
            src/FS/hfs_diskio.c \
            src/FS/hfs_volume.c \
            src/FS/hfs_btree.c \
            src/FS/hfs_catalog.c \
            src/FS/hfs_file.c \
            src/FS/vfs.c \
            src/FS/trash.c \
            src/FS/vfs_ops.c \
            src/MemoryMgr/MemoryManager.c \
            src/MemoryMgr/memory_manager_core.c \
            src/MemoryMgr/heap_compaction.c \
            src/MemoryMgr/blockmove_optimization.c \
            src/Nanokernel/nk_memory.c \
            src/Nanokernel/nk_memory_test.c \
            src/Nanokernel/nk_task.c \
            src/Nanokernel/nk_thread.c \
            src/Nanokernel/nk_sched.c \
            src/Nanokernel/nk_timer.c \
            src/Nanokernel/nk_test_threads.c \
            src/Nanokernel/nk_debug.c \
            src/Nanokernel/nk_stats.c \
            src/Nanokernel/nk_pic.c \
            src/Nanokernel/nk_idt.c \
            src/Nanokernel/nk_irq_handlers.c \
            src/Nanokernel/nk_resched.c \
            src/Nanokernel/nk_fault_sentinel.c \
            src/Nanokernel/nk_pre_sti_safety.c \
            src/Nanokernel/platform/x86/nk_platform_init.c \
            src/Resources/Icons/hd_icon.c \
            src/color_icons.c \
            src/DeskManager/DeskManagerCore.c \
            src/DeskManager/DeskManagerStubs.c \
            src/DialogManager/DialogManagerCore.c \
            src/DialogManager/DialogManagerStubs.c \
            src/DialogManager/ModalDialogs.c \
            src/DialogManager/AlertDialogs.c \
            src/DialogManager/AlertSmoke.c \
            src/DialogManager/DialogEvents.c \
            src/DialogManager/DialogItems.c \
            src/DialogManager/DialogResourceParser.c \
            src/DialogManager/DialogDrawing.c \
            src/DialogManager/DialogEditText.c \
            src/DialogManager/dialog_manager_private.c \
            src/DialogManager/DialogHelpers.c \
            src/DialogManager/DialogKeyboard.c \
            src/StandardFile/StandardFile.c \
            src/StandardFile/StandardFileHAL_Shims.c \
            src/FileManager.c \
            src/FileManagerStubs.c \
            src/EventManager/event_manager.c \
            src/EventManager/EventGlobals.c \
            src/EventManager/ModernInput.c \
            src/EventManager/EventDispatcher.c \
            src/EventManager/MouseEvents.c \
            src/EventManager/KeyboardEvents.c \
            src/EventManager/SystemEvents.c \
            src/ProcessMgr/ProcessManager.c \
            src/CPU/CPUBackend.c \
            src/CPU/m68k_interp/M68KBackend.c \
            src/CPU/m68k_interp/M68KDecode.c \
            src/CPU/m68k_interp/M68KOpcodes.c \
            src/CPU/m68k_interp/LowMemGlobals.c \
            src/SegmentLoader/SegmentLoader.c \
            src/SegmentLoader/CodeParser.c \
            src/SegmentLoader/A5World.c \
            src/SegmentLoader/SegmentLoaderTest.c \
            src/TextEdit/TextEdit.c \
            src/TextEdit/TextEditDraw.c \
            src/TextEdit/TextEditInput.c \
            src/TextEdit/TextEditScroll.c \
            src/TextEdit/TextEditClipboard.c \
            src/TextEdit/TextBreak.c \
            src/TextEdit/TextEditTest.c \
            src/WindowManager/WindowDisplay.c \
            src/WindowManager/WindowEvents.c \
            src/WindowManager/WindowManagerCore.c \
            src/WindowManager/WindowManagerHelpers.c \
            src/WindowManager/WindowDragging.c \
            src/WindowManager/WindowResizing.c \
            src/WindowManager/WindowLayering.c \
            src/WindowManager/WindowParts.c \
            src/TimeManager/PlatformTime.c \
            src/TimeManager/TimeBase.c \
            src/TimeManager/MicrosecondTimer.c \
            src/TimeManager/TimeManager.c \
            src/TimeManager/TimeManagerCore.c \
            src/TimeManager/TimerInterrupts.c \
            src/TimeManager/TimerTasks.c \
            src/Apps/SimpleText/SimpleText.c \
            src/Apps/SimpleText/STDocument.c \
            src/Apps/SimpleText/STView.c \
            src/Apps/SimpleText/STMenus.c \
            src/Apps/SimpleText/STFileIO.c \
            src/Apps/SimpleText/STClipboard.c \
            src/StartupScreen/StartupScreen.c

# Add ResourceMgr sources if enabled
ifeq ($(ENABLE_RESOURCES),1)
C_SOURCES += src/ResourceMgr/ResourceMgr.c
endif

# Add FileMgr extra sources if enabled
ifeq ($(ENABLE_FILEMGR_EXTRA),1)
C_SOURCES += src/FileMgr/btree_services.c \
             src/FileMgr/extent_manager.c \
             src/FileMgr/tfs_dispatch.c \
             src/FileMgr/volume_manager.c
endif

# Add Gestalt Manager if enabled
ifeq ($(ENABLE_GESTALT),1)
C_SOURCES += src/Gestalt/Gestalt.c \
             src/Gestalt/GestaltBuiltins.c
CFLAGS += -DENABLE_GESTALT=1
endif

# Conditionally add Process Manager cooperative scheduling
ifeq ($(ENABLE_PROCESS_COOP),1)
C_SOURCES += src/ProcessMgr/CooperativeScheduler.c \
             src/ProcessMgr/EventIntegration.c
endif

# Add ScrapManager if enabled
ifeq ($(ENABLE_SCRAP),1)
C_SOURCES += src/ScrapManager/ScrapManager.c
CFLAGS += -DENABLE_SCRAP=1
# Enable self-test for debugging
CFLAGS += -DSCRAP_SELFTEST=1 -DDEBUG_DOUBLECLICK=1
endif

# Add ListManager if enabled
ifeq ($(ENABLE_LIST),1)
C_SOURCES += src/ListManager/ListManager.c \
             src/ListManager/list_manager.c \
             src/ListManager/ListSmoke.c
CFLAGS += -DENABLE_LIST=1
ifeq ($(LIST_SMOKE_TEST),1)
CFLAGS += -DLIST_SMOKE_TEST=1
endif
endif

# Add Control smoke test if enabled
ifeq ($(CTRL_SMOKE_TEST),1)
CFLAGS += -DCTRL_SMOKE_TEST=1
endif

# Alert smoke test (alert dialogs)
ifeq ($(ALERT_SMOKE_TEST),1)
CFLAGS += -DALERT_SMOKE_TEST=1
endif

ASM_SOURCES = $(HAL_DIR)/platform_boot.S \
              src/Nanokernel/platform/x86/nk_context.S \
              src/Nanokernel/platform/x86/nk_isr.S \
              src/Nanokernel/platform/x86/nk_thread_entry.S

# Object files
C_OBJECTS = $(patsubst %.c,$(OBJ_DIR)/%.o,$(notdir $(C_SOURCES)))
ASM_OBJECTS = $(patsubst %.S,$(OBJ_DIR)/%.o,$(notdir $(ASM_SOURCES)))
OBJECTS = $(ASM_OBJECTS) $(C_OBJECTS)

# Dependency files (auto-generated by -MMD -MP)
DEPS = $(C_OBJECTS:.o=.d)

# Build directories (created as order-only prerequisites for parallel builds)
BUILD_DIRS = $(BUILD_DIR) $(OBJ_DIR) $(BIN_DIR) $(ISO_DIR)/boot/grub

$(BUILD_DIRS):
	@mkdir -p $@

# Tool version requirements
GCC_MIN_VERSION = 7.0
PYTHON_MIN_VERSION = 3.6

# Check build tools (run once per make invocation)
.PHONY: check-tools
check-tools:
	@scripts/check_tool_versions.sh $(GCC_MIN_VERSION) $(PYTHON_MIN_VERSION)

# Default target
all: check-tools $(RSRC_BIN) $(KERNEL)

# Build resource file
$(RSRC_BIN): $(RSRC_JSON) $(PATTERN_RESOURCE) gen_rsrc.py
	@echo "GEN $(RSRC_BIN)"
	@python3 gen_rsrc.py $(RSRC_JSON) $(PATTERN_RESOURCE) $(RSRC_BIN) || \
		{ echo "ERROR: Resource generation failed"; exit 1; }
	@test -f $(RSRC_BIN) || { echo "ERROR: $(RSRC_BIN) not created"; exit 1; }
	@echo "✓ Resource file generated successfully"

# Convert resource file to C source
src/patterns_rsrc.c: $(RSRC_BIN)
	@echo "XXDC $<"
	@echo '/* Auto-generated from Patterns.rsrc */' > $@
	@echo 'const unsigned char patterns_rsrc_data[] = {' >> $@
	@xxd -i < $< >> $@
	@echo '};' >> $@
	@echo 'const unsigned int patterns_rsrc_size = sizeof(patterns_rsrc_data);' >> $@

# Link kernel
$(KERNEL): $(OBJECTS) | $(BUILD_DIR)
	@echo "LD $(KERNEL)"
	@$(LD) $(LDFLAGS) -T $(HAL_DIR)/linker.ld -o $(KERNEL) $(OBJECTS)
	@readelf -h $(KERNEL) >/dev/null 2>&1 || { echo "ERROR: Invalid ELF file"; exit 1; }
	@echo "✓ Kernel linked successfully ($(shell stat -c%s $(KERNEL) 2>/dev/null || stat -f%z $(KERNEL) 2>/dev/null) bytes)"

# Define source directories for vpath search
vpath %.c src:src/System:src/QuickDraw:src/WindowManager:src/MenuManager:src/ControlManager \
          src/EventManager:src/DialogManager:src/StandardFile:src/TextEdit:src/ListManager \
          src/ScrapManager:src/ProcessMgr:src/TimeManager:src/SoundManager:src/FontManager \
          src/Gestalt:src/MemoryMgr:src/Nanokernel:src/Nanokernel/platform/x86:src/ResourceMgr \
          src/FileMgr:src/FS:src/Finder:src/Finder/Icon:src/DeskManager:src/ControlPanels \
          src/PatternMgr:src/Resources:src/Resources/Icons:src/Apps/SimpleText:src/Platform \
          src/Platform/x86:src/PrintManager:src/HelpManager:src/ComponentManager:src/EditionManager \
          src/NotificationManager:src/PackageManager:src/NetworkExtension:src/ColorManager \
          src/CommunicationToolbox:src/GestaltManager:src/SpeechManager:src/BootLoader \
          src/SegmentLoader:src/CPU:src/CPU/m68k_interp:src/DeviceManager:src/Keyboard \
          src/Datetime:src/Calculator:src/Chooser:src/StartupScreen:src/OSUtils
vpath %.S $(HAL_DIR):src/Nanokernel/platform/x86

# Compile assembly files
$(OBJ_DIR)/%.o: %.S | $(OBJ_DIR)
	@echo "AS $<"
	@$(AS) $(ASFLAGS) $< -o $@

# Compile C files (single rule for all directories via vpath)
$(OBJ_DIR)/%.o: %.c | $(OBJ_DIR)
	@echo "CC $<"
	@$(CC) $(CFLAGS) -c $< -o $@

# Special targets for stubs
build/obj/universal_stubs.o: src/universal_stubs.c
	@echo "CC src/universal_stubs.c"
	@$(CC) $(CFLAGS) -c src/universal_stubs.c -o build/obj/universal_stubs.o

build/obj/ultimate_stubs.o: src/ultimate_stubs.c
	@echo "CC src/ultimate_stubs.c"
	@$(CC) $(CFLAGS) -c src/ultimate_stubs.c -o build/obj/ultimate_stubs.o

# ISO target
iso: $(ISO)

$(ISO): $(KERNEL) | $(ISO_DIR)/boot/grub
	@echo "Creating bootable ISO..."
	@cp $(KERNEL) $(ISO_DIR)/boot/
	@echo 'set timeout=0' > $(ISO_DIR)/boot/grub/grub.cfg
	@echo '' >> $(ISO_DIR)/boot/grub/grub.cfg
	@echo 'menuentry "System 7.1 Portable" {' >> $(ISO_DIR)/boot/grub/grub.cfg
	@echo '    multiboot2 /boot/$(KERNEL)' >> $(ISO_DIR)/boot/grub/grub.cfg
	@echo '    boot' >> $(ISO_DIR)/boot/grub/grub.cfg
	@echo '}' >> $(ISO_DIR)/boot/grub/grub.cfg
	@$(GRUB) -d /usr/lib/grub/i386-pc -o $(ISO) $(ISO_DIR)

# Run with QEMU (PC speaker with PulseAudio backend, SMP enabled for threading)
run: $(ISO)
	qemu-system-i386 -cdrom $(ISO) -drive file=test_disk.img,format=raw,if=ide -m 1024 -vga std -serial file:/tmp/serial.log \
		-smp 1 -audiodev pa,id=snd0,server=/run/user/$(shell id -u)/pulse/native -machine pcspk-audiodev=snd0

# Debug with QEMU
debug: $(ISO)
	qemu-system-i386 -cdrom $(ISO) -drive file=test_disk.img,format=raw,if=ide -m 1024 -vga std -s -S

# Clean
clean:
	rm -rf $(BUILD_DIR) obj isodir iso system71.iso kernel.elf src/patterns_rsrc.c

# Show compilation info
info:
	@echo "C Sources: $(words $(C_SOURCES)) files"
	@echo "Source Files: $(words $(ASM_SOURCES)) files"
	@echo "Total Objects: $(words $(OBJECTS)) files"

# Check exported symbols against allowlist
check-exports: kernel.elf
	@bash tools/check_exports.sh

# Help target - show available commands
.PHONY: help
help: ## Show this help message
	@echo "════════════════════════════════════════════════════════════════"
	@echo "  System 7.1 Portable Build System"
	@echo "════════════════════════════════════════════════════════════════"
	@echo ""
	@echo "USAGE:"
	@echo "  make [target] [CONFIG=<config>] [options]"
	@echo ""
	@echo "MAIN TARGETS:"
	@echo "  all              Build kernel (default target)"
	@echo "  iso              Create bootable ISO image"
	@echo "  run              Run kernel in QEMU with serial logging"
	@echo "  debug            Launch QEMU in debug mode (waits for GDB)"
	@echo "  clean            Remove all build artifacts"
	@echo "  check-tools      Verify build tool versions"
	@echo "  check-exports    Validate exported symbol surface"
	@echo "  info             Show build statistics"
	@echo "  help             Show this help message"
	@echo ""
	@echo "BUILD CONFIGURATIONS:"
	@echo "  CONFIG=default   Standard development build (default)"
	@echo "  CONFIG=debug     Debug build with smoke tests enabled"
	@echo "  CONFIG=release   Optimized release build"
	@echo ""
	@echo "CONFIGURATION OPTIONS (override in config/*.mk or command line):"
	@echo "  PLATFORM=<name>          Target platform (default: x86)"
	@echo "  ENABLE_RESOURCES=1       Enable Resource Manager"
	@echo "  ENABLE_FILEMGR_EXTRA=1   Enable File Manager extras"
	@echo "  ENABLE_PROCESS_COOP=1    Enable cooperative scheduling"
	@echo "  ENABLE_GESTALT=1         Enable Gestalt Manager"
	@echo "  ENABLE_SCRAP=1           Enable Scrap Manager"
	@echo "  ENABLE_LIST=1            Enable List Manager"
	@echo "  CTRL_SMOKE_TEST=1        Enable Control Manager tests"
	@echo "  LIST_SMOKE_TEST=1        Enable List Manager tests"
	@echo "  ALERT_SMOKE_TEST=1       Enable Alert Dialog tests"
	@echo "  OPT_LEVEL=0-3            Optimization level (0=none, 1=default, 2-3=release)"
	@echo "  DEBUG_SYMBOLS=0/1        Include debug symbols"
	@echo ""
	@echo "EXAMPLES:"
	@echo "  make                     Build with default configuration"
	@echo "  make -j8                 Parallel build with 8 jobs"
	@echo "  make CONFIG=debug        Build with debug configuration"
	@echo "  make CONFIG=release iso  Build optimized ISO"
	@echo "  make clean all           Clean rebuild"
	@echo "  make CTRL_SMOKE_TEST=1   Build with control tests enabled"
	@echo ""
	@echo "PARALLEL BUILDS:"
	@echo "  Use 'make -j<N>' for parallel compilation (recommended)"
	@echo "  Example: make -j\$(nproc)  # Use all CPU cores"
	@echo ""
	@echo "For more information, see docs/BUILD_SYSTEM_IMPROVEMENTS.md"
	@echo "════════════════════════════════════════════════════════════════"

# Include dependency files (auto-generated by -MMD -MP)
-include $(DEPS)

.PHONY: all clean iso run debug info check-exports check-tools help
