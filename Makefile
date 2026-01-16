# System 7.1 Portable Makefile
# Builds complete System 7.1 kernel

# Build error handling: delete targets on failure, stop on first error
.DELETE_ON_ERROR:
.SUFFIXES:

# Default goal (build kernel)
.DEFAULT_GOAL := all

# Platform configuration
PLATFORM ?= x86

# If QEMU=1 is passed, automatically set PLATFORM to arm64
ifeq ($(QEMU),1)
  PLATFORM = arm64
endif

# Raspberry Pi model selection (for ARM platform builds)
# Valid values: pi3, pi4, pi5 (or leave empty for runtime detection)
PI_MODEL ?=

# Load build configuration (default or user-specified)
CONFIG ?= default
-include config/$(CONFIG).mk

# Directories
BUILD_DIR = build
OBJ_DIR = $(BUILD_DIR)/obj
BIN_DIR = $(BUILD_DIR)/bin
ISO_DIR = iso
HAL_DIR = src/Platform/$(PLATFORM)
LINKER_SCRIPT := $(HAL_DIR)/linker.ld

# Output files
KERNEL = kernel.elf
ISO = system71.iso

# Raspberry Pi model-specific configuration
ifeq ($(PLATFORM),arm)
    # Validate PI_MODEL if specified
    ifneq ($(PI_MODEL),)
        ifeq ($(filter $(PI_MODEL),pi3 pi4 pi5 virt),)
            $(error Invalid PI_MODEL '$(PI_MODEL)'. Valid values: pi3, pi4, pi5, virt)
        endif
    endif

    # Set ARM architecture based on Pi model
    ifeq ($(PI_MODEL),pi3)
        ARM_ARCH = armv6z
        PI_CPU_NAME = "ARM Cortex-A53"
        PI_CPU_FREQ = 1200
        CFLAGS_PI = -march=armv6z -mfpu=neon -mfloat-abi=hard -DRASPI_MODEL=3 -DRASPI_CPU_FREQ=1200
    else ifeq ($(PI_MODEL),pi4)
        ARM_ARCH = armv7-a
        PI_CPU_NAME = "ARM Cortex-A72"
        PI_CPU_FREQ = 1500
        CFLAGS_PI = -march=armv7-a -mfpu=neon -mfloat-abi=hard -DRASPI_MODEL=4 -DRASPI_CPU_FREQ=1500
    else ifeq ($(PI_MODEL),pi5)
        ARM_ARCH = armv7-a
        PI_CPU_NAME = "ARM Cortex-A76"
        PI_CPU_FREQ = 2400
        CFLAGS_PI = -march=armv7-a -mfpu=neon -mfloat-abi=hard -DRASPI_MODEL=5 -DRASPI_CPU_FREQ=2400
    else ifeq ($(PI_MODEL),virt)
        ARM_ARCH = armv7-a
        PI_CPU_NAME = "QEMU virt"
        PI_CPU_FREQ = 1000
        CFLAGS_PI = -march=armv7-a -mfpu=neon -mfloat-abi=hard -DQEMU_VIRT
    else
        # Default ARM architecture (runtime detection)
        ARM_ARCH = armv7-a
        PI_CPU_NAME = "ARM Cortex"
        PI_CPU_FREQ = 0
        CFLAGS_PI = -march=armv7-a -mfpu=neon -mfloat-abi=hard
    endif

    # ARM cross-compiler configuration
    ARM_TARGET ?= arm-linux-gnueabihf
    CROSS_COMPILE ?= arm-linux-gnueabihf-
    CC = $(CROSS_COMPILE)gcc
    AS = $(CROSS_COMPILE)as
    LD = $(CROSS_COMPILE)ld
    OBJCOPY = $(CROSS_COMPILE)objcopy
    ifeq ($(PI_MODEL),virt)
        LINKER_SCRIPT := $(HAL_DIR)/linker_virt.ld
    endif
else ifeq ($(PLATFORM),ppc)
    # PowerPC cross-compiler configuration (override CROSS_COMPILE to suit your setup)
    CROSS_COMPILE ?= powerpc-linux-gnu-
    CC = $(CROSS_COMPILE)gcc
    AS = $(CROSS_COMPILE)as
    LD = $(CROSS_COMPILE)ld
    OBJCOPY = $(CROSS_COMPILE)objcopy
else
    # x86 native compiler
    CC = gcc
    AS = as
    LD = ld
    OBJCOPY = objcopy
endif

# Common tools (non-platform specific)
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

# Platform-independent base flags
COMMON_CFLAGS = -DSYS71_PROVIDE_FINDER_TOOLBOX=1 \
         -ffreestanding -fno-builtin -fno-stack-protector -nostdlib \
         -fno-pic -fno-pie \
         -Wall -Wextra -Wformat=2 -Wmissing-prototypes -Wmissing-declarations -Wshadow -Wcast-qual \
         -Wpointer-arith -Wstrict-prototypes -Wno-unused-parameter \
         -Wundef -Wvla -Wcast-align -Wlogical-op -Wduplicated-cond -Wduplicated-branches \
         -Wnull-dereference -Wjump-misses-init -Warray-bounds=2 -Wshift-overflow=2 \
         $(OPT_FLAGS) -fno-inline -fno-optimize-sibling-calls -I./include -I./src -std=c2x \
         -Wuninitialized -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 \
         -Wno-multichar -Wno-pointer-sign -Wno-sign-compare \
         -fno-common -fno-delete-null-pointer-checks \
         -MMD -MP

# Platform-specific compiler flags
ifeq ($(PLATFORM),arm)
    # ARM 32-bit with PI_MODEL-specific architecture
    CFLAGS = $(COMMON_CFLAGS) $(CFLAGS_PI)
    ASFLAGS =
    LDFLAGS = -nostdlib -no-pie

    # Set Gestalt machine type based on PI_MODEL
    ifeq ($(PI_MODEL),pi3)
        ifeq ($(strip $(GESTALT_MACHINE_TYPE)),)
          GESTALT_MACHINE_TYPE := arm_pi3
        endif
    else ifeq ($(PI_MODEL),pi4)
        ifeq ($(strip $(GESTALT_MACHINE_TYPE)),)
          GESTALT_MACHINE_TYPE := arm_pi4
        endif
    else ifeq ($(PI_MODEL),pi5)
        ifeq ($(strip $(GESTALT_MACHINE_TYPE)),)
          GESTALT_MACHINE_TYPE := arm_pi5
        endif
    else
        # Default for runtime detection
        ifeq ($(strip $(GESTALT_MACHINE_TYPE)),)
          GESTALT_MACHINE_TYPE := arm_pi3
        endif
    endif
else ifeq ($(PLATFORM),arm64)
    # ARM 64-bit (AArch64) for QEMU virt machine
    # Use aarch64 cross-compiler (prefer elf, fallback to linux-gnu)
    AARCH64_PREFIX := $(shell command -v aarch64-elf-gcc >/dev/null 2>&1 && echo aarch64-elf || echo aarch64-linux-gnu)
    CC = $(AARCH64_PREFIX)-gcc
    AS = $(AARCH64_PREFIX)-as
    LD = $(AARCH64_PREFIX)-ld
    AR = $(AARCH64_PREFIX)-ar
    CFLAGS = $(COMMON_CFLAGS) -march=armv8-a -ffreestanding -DQEMU_BUILD
    ASFLAGS = -march=armv8-a
    LDFLAGS = -nostdlib -no-pie -Wl,--allow-multiple-definition -Wl,-z,execstack
    LINKER_SCRIPT := $(HAL_DIR)/link_qemu.ld
    ifeq ($(strip $(GESTALT_MACHINE_TYPE)),)
      GESTALT_MACHINE_TYPE := arm64_virt
    endif
else ifeq ($(PLATFORM),ppc)
    # PowerPC 32-bit big-endian
    CFLAGS = $(COMMON_CFLAGS) -mbig-endian -mno-toc -mno-sdata
    ASFLAGS := -mregnames -mbig-endian
    LDFLAGS = -nostdlib -no-pie -melf32ppc
    ifeq ($(strip $(GESTALT_MACHINE_TYPE)),)
      GESTALT_MACHINE_TYPE := 0x0050
    endif
else
    # x86 32-bit
    CFLAGS = $(COMMON_CFLAGS) -m32
    ASFLAGS = --32
    LDFLAGS = -melf_i386 -nostdlib -no-pie
    ifeq ($(strip $(GESTALT_MACHINE_TYPE)),)
      GESTALT_MACHINE_TYPE := 0
    endif
endif

CFLAGS += -DDEFAULT_GESTALT_MACHINE_TYPE=$(GESTALT_MACHINE_TYPE)
CFLAGS += -DEXPANDMEM_FULL_IMPL=1

BEZEL_STYLE ?= auto
ifeq ($(strip $(BEZEL_STYLE)),rounded)
  CFLAGS += -DDEFAULT_BEZEL_STYLE=1
else ifeq ($(strip $(BEZEL_STYLE)),flat)
  CFLAGS += -DDEFAULT_BEZEL_STYLE=2
else
  CFLAGS += -DDEFAULT_BEZEL_STYLE=0
endif

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

# Resource files
RSRC_JSON = patterns.json
RSRC_BIN = Patterns.rsrc
PATTERN_RESOURCE ?= resources/patterns_authentic_color.json

# 68K Interpreter for ARM Raspberry Pi Support
# The 68K interpreter is platform-independent and included unconditionally in all builds.
# It enables System 7.1 classic Mac applications to run on both x86 and ARM (Raspberry Pi 3/4/5).
# The interpreter uses explicit big-endian byte ordering for cross-platform compatibility.

# Source files
C_SOURCES = src/main.c \
            src/boot.c \
            src/SystemInit.c \
            src/sys71_stubs.c \
            src/System71StdLib.c \
            src/runtime_stubs.c \
            src/System/SystemTheme.c \
            src/ToolboxCompat.c \
            src/Toolbox/IconUtilities.c \
            src/Finder/finder_main.c \
            src/Finder/desktop_manager.c \
            src/Finder/folder_window.c \
            src/Finder/alias_manager.c \
            src/Finder/trash_folder.c \
            src/Finder/AboutThisMac.c \
            src/Finder/GetInfo.c \
            src/Finder/Find.c \
            src/ExpandMem.c \
            src/QuickDraw/QuickDrawCore.c \
            src/QuickDraw/Bitmaps.c \
            src/QuickDraw/QuickDrawPlatform.c \
            src/QuickDraw/quickdraw_pictures.c \
            src/QuickDraw/CursorManager.c \
            src/QuickDraw/Coordinates.c \
            src/QuickDraw/Regions.c \
            src/QuickDraw/Pictures.c \
            src/QuickDraw/ColorQuickDraw.c \
            src/QuickDraw/GWorld.c \
            src/QuickDraw/Patterns.c \
            src/QuickDraw/display_bezel.c \
            src/ColorManager/ColorManager.c \
            src/OSUtils/OSUtilsTraps.c \
            src/OSUtils/Munger.c \
            src/OSUtils/DateTime.c \
            src/OSUtils/QueueUtilities.c \
            src/OSUtils/BitManipulation.c \
            src/OSUtils/BitwiseOperations.c \
            src/OSUtils/FixedPointMath.c \
            src/OSUtils/MemoryUtilities.c \
            src/OSUtils/DebugUtils.c \
            src/Platform/WindowPlatform.c \
            $(if $(filter arm64,$(PLATFORM)), \
              src/Platform/arm64/uart_qemu.c \
              src/Platform/arm64/hal_boot.c \
              src/Platform/arm64/serial.c \
              src/Platform/arm64/timer.c \
              src/Platform/arm64/dtb.c \
              src/Platform/arm64/virtio_gpu.c \
              src/Platform/arm64/virtio_pci.c \
              src/Platform/arm64/virtio_input.c \
              src/Platform/arm64/virtio_blk.c \
              src/Platform/arm64/mmu.c \
              src/Platform/arm64/cache.c \
              src/Platform/arm64/exception_handlers.c \
              src/Platform/arm64/string.c \
              src/Platform/arm64/printf.c \
              src/Platform/arm64/io.c \
              src/Platform/arm64/hal_input.c \
              src/Platform/arm64/platform_info.c \
              src/Platform/arm64/storage.c \
              src/Platform/arm64/dwc2.c \
              src/Platform/arm64/usb_core.c \
              src/Platform/arm64/usb_hid.c, \
            $(if $(filter arm,$(PLATFORM)), \
              src/Platform/arm/hal_boot.c \
              src/Platform/arm/io.c \
              src/Platform/arm/device_tree.c \
              src/Platform/arm/hardware_detect.c \
              src/Platform/arm/platform_info.c \
              src/Platform/arm/mmio.c \
              src/Platform/arm/videocore.c \
              src/Platform/arm/framebuffer.c \
              src/Platform/arm/timer_arm.c \
              src/Platform/arm/sdhci.c \
              src/Platform/arm/sdhci_commands.c \
              src/Platform/arm/storage.c \
              src/Platform/arm/audio_hdmi.c \
              src/Platform/arm/xhci.c \
              src/Platform/arm/dwcotg.c \
              src/Platform/arm/usb_controller.c \
              src/Platform/arm/hid_input.c \
              src/Platform/arm/input_stubs.c, \
            $(if $(filter ppc,$(PLATFORM)), \
                src/Platform/ppc/hal_boot.c \
                src/Platform/ppc/io.c \
                src/Platform/ppc/platform_info.c \
                src/Platform/ppc/hal_input.c \
                src/Platform/ppc/storage.c \
                src/Platform/ppc/open_firmware.c \
                src/Platform/ppc/escc_uart.c \
                src/Platform/ppc/input_stubs.c, \
                src/Platform/x86/io.c \
                src/Platform/x86/ata.c \
                src/Platform/x86/ps2.c \
                src/Platform/x86/platform_info.c \
                src/Platform/x86/hal_boot.c \
                src/Platform/x86/hal_input.c))) \
            src/SoundManager/SoundManagerBareMetal.c \
            src/SoundManager/SoundHardwarePC.c \
            src/SoundManager/SoundBackend.c \
            src/SoundManager/SoundBackend_HDA.c \
            src/SoundManager/SoundBackend_SB16.c \
            src/SoundManager/SoundEffects.c \
            src/SoundManager/SoundBlaster16.c \
            src/SoundManager/DMA_Controller.c \
            src/MenuManager/MenuManagerCore.c \
            src/MenuManager/MenuSelection.c \
            src/MenuManager/MenuDisplay.c \
            src/MenuManager/menu_savebits.c \
            src/MenuManager/MenuBitsPool.c \
            src/MenuManager/MenuTitleTracking.c \
            src/MenuManager/MenuTrack.c \
            src/MenuManager/MenuAppleIcon.c \
            src/MenuManager/MenuAppIcon.c \
            src/MenuManager/MenuItems.c \
            src/MenuManager/MenuResources.c \
            src/MenuManager/platform_stubs.c \
            src/MenuManager/menu_stubs.c \
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
            src/Resources/generated/icons_generated.c \
            src/Resources/ResourceData.c \
            src/ControlPanels/cdev_desktop.c \
            src/simple_resource_manager.c \
            src/ControlManager/ControlManagerCore.c \
            src/ControlManager/ControlTracking.c \
            src/ControlManager/ScrollbarControls.c \
            src/ControlManager/StandardControls.c \
            src/ControlManager/ControlResources.c \
            src/ControlManager/ControlSmoke.c \
            src/control_stubs.c \
            src/ControlPanels/sound_cdev.c \
            src/ControlPanels/mouse_cdev.c \
            src/ControlPanels/keyboard_cdev.c \
            src/ControlPanels/control_strip.c \
            src/Datetime/datetime_cdev.c \
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
            src/MemoryMgr/HandleUtilities.c \
            src/MemoryMgr/MemoryInitialization.c \
            src/Resources/Icons/hd_icon.c \
            src/color_icons.c \
            src/DeskManager/DeskManagerCore.c \
            src/DeskManager/DeskManagerStubs.c \
            src/DeskManager/BuiltinDAs.c \
            src/DeskManager/DALoader.c \
            src/DeskManager/SystemMenu.c \
            src/DeskManager/DAPreferences.c \
            src/DeskManager/KeyCaps.c \
            src/DeskManager/Notepad.c \
            src/DeskManager/Calculator.c \
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
            src/StandardFile/Pack3_StandardFile.c \
            src/PackageManager/StringConversion.c \
            src/PackageManager/DateTimeFormatting.c \
            src/PackageManager/StringUtilities.c \
            src/PackageManager/StringManipulation.c \
            src/PackageManager/StringComparison.c \
            src/PackageManager/CharacterClassification.c \
            src/PackageManager/InternationalUtilities.c \
            src/PackageManager/PackageManagerCore.c \
            src/PackageManager/TextEncodingUtils.c \
            src/PackageManager/InternationalOrdering.c \
            src/PackageManager/Pack4_SANE.c \
            src/PackageManager/Pack6_InternationalUtils.c \
            src/PackageManager/Pack7_BinaryDecimal.c \
            src/PackageManager/Pack_Stubs.c \
            src/FileManager.c \
            src/FileManagerStubs.c \
            src/EventManager/event_manager.c \
            src/EventManager/EventGlobals.c \
            src/EventManager/ModernInput.c \
            src/EventManager/EventDispatcher.c \
            src/EventManager/MouseEvents.c \
            src/EventManager/KeyboardEvents.c \
            src/EventManager/AppSwitcher.c \
            src/EventManager/SystemEvents.c \
            src/ProcessMgr/ProcessManager.c \
            src/ProcessMgr/AppFileManager.c \
            src/ProcessMgr/ProcessAPI.c \
            src/CPU/CPUBackend.c \
            src/CPU/m68k_interp/M68KBackend.c \
            src/CPU/m68k_interp/M68KDecode.c \
            src/CPU/m68k_interp/M68KOpcodes.c \
            src/CPU/m68k_interp/LowMemGlobals.c \
            src/CPU/ppc_interp/PPCBackend.c \
            src/CPU/ppc_interp/PPCOpcodes.c \
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
            src/WindowManager/WindowGeometry.c \
            src/WindowManager/WindowRegions.c \
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
            src/Apps/MacPaint/MacPaint_Core.c \
            src/Apps/MacPaint/MacPaint_Tools.c \
            src/Apps/MacPaint/MacPaint_FileIO.c \
            src/Apps/MacPaint/MacPaint_Menus.c \
            src/Apps/MacPaint/MacPaint_Advanced.c \
            src/Apps/MacPaint/MacPaint_Integration.c \
            src/Apps/MacPaint/MacPaint_EventLoop.c \
            src/Apps/MacPaint/MacPaint_Rendering.c \
            src/Apps/MacPaint/MacPaint_Main.c \
            src/StartupScreen/StartupScreen.c

# Add IntegrationTests if enabled
ifeq ($(INTEGRATION_TESTS),1)
C_SOURCES += src/Integration/IntegrationTests.c
CFLAGS += -DINTEGRATION_TESTS=1
endif

# Add Phase 2 Integration Tests if enabled
ifeq ($(PHASE2_TESTS),1)
C_SOURCES += src/Integration/Phase2_EventDispatch.c \
             src/Integration/Phase2_FileIO.c \
             src/Integration/Phase2_DialogManager.c \
             src/Integration/Phase2_TextEdit.c \
             src/Integration/Phase2_QuickDraw.c \
             src/Integration/Phase2_WindowManager.c \
             src/Integration/Phase2_AppStartup.c \
             src/Integration/Phase2_SoundManager.c \
             src/Integration/Phase2_Rendering.c
CFLAGS += -DPHASE2_TESTS=1
endif

# Add ResourceMgr sources if enabled
ifeq ($(ENABLE_RESOURCES),1)
C_SOURCES += src/ResourceMgr/ResourceMgr.c \
             src/ResourceMgr/StringResources.c
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
             src/ListManager/ListSmoke.c \
             src/ListManager/Pack0_ListManager.c
CFLAGS += -DENABLE_LIST=1
ifeq ($(LIST_SMOKE_TEST),1)
CFLAGS += -DLIST_SMOKE_TEST=1
endif
endif

# Add ExtensionManager and related loaders
C_SOURCES += src/ExtensionManager/ExtensionManagerCore.c \
             src/ExtensionManager/CDEFLoader.c \
             src/ExtensionManager/ControlPanelManager.c \
             src/ExtensionManager/DRVRLoader.c \
             src/ExtensionManager/FKEYLoader.c \
             src/ExtensionManager/DefLoader.c
CFLAGS += -DENABLE_EXTENSIONS=1

# Speech Manager - Minimal implementation with SoundManager integration
C_SOURCES += src/SpeechManager/SpeechManagerCore.c \
             src/SpeechManager/SpeechOutput_Stub.c \
             src/SpeechManager/SpeechSmoke.c
CFLAGS += -DENABLE_SPEECH=1

# Speech Manager smoke test (optional testing)
ifeq ($(SPEECH_SMOKE_TEST),1)
CFLAGS += -DSPEECH_SMOKE_TEST=1
endif

# Notification Manager - Background notification system
C_SOURCES += src/NotificationManager/NotificationManagerCore.c
CFLAGS += -DENABLE_NOTIFICATION=1

# Add Control smoke test if enabled
ifeq ($(CTRL_SMOKE_TEST),1)
CFLAGS += -DCTRL_SMOKE_TEST=1
endif

# Alert smoke test (alert dialogs)
ifeq ($(ALERT_SMOKE_TEST),1)
CFLAGS += -DALERT_SMOKE_TEST=1
endif

ASM_SOURCES = $(HAL_DIR)/platform_boot.S
ifeq ($(PLATFORM),arm64)
ASM_SOURCES += $(HAL_DIR)/exceptions.S
endif

# Object files
C_OBJECTS = $(patsubst src/%.c,$(OBJ_DIR)/%.o,$(C_SOURCES))
ASM_OBJECTS = $(patsubst %.S,$(OBJ_DIR)/%.o,$(notdir $(ASM_SOURCES)))
OBJECTS = $(ASM_OBJECTS) $(C_OBJECTS)

# Dependency files (auto-generated by -MMD -MP)
DEPS = $(C_OBJECTS:.o=.d)

# Include dependency files if they exist
-include $(DEPS)

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
	@PLATFORM=$(PLATFORM) scripts/check_tool_versions.sh $(GCC_MIN_VERSION) $(PYTHON_MIN_VERSION)

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
	@if [ "$(PLATFORM)" = "arm64" ]; then \
        $(CC) $(LDFLAGS) -Wl,-T,$(LINKER_SCRIPT) -nostdlib -static -o $(KERNEL) $(OBJECTS); \
    elif [ "$(PLATFORM)" = "arm" ]; then \
        $(CC) $(LDFLAGS) -Wl,-T,$(LINKER_SCRIPT) -nostdlib -static -o $(KERNEL) $(OBJECTS) -lm -lgcc; \
    else \
        $(CC) -m32 -Wl,-T,$(LINKER_SCRIPT) -nostdlib -no-pie -static -o $(KERNEL) $(OBJECTS) -lm -lgcc; \
	fi
	@test -f $(KERNEL) || { echo "ERROR: Kernel not created"; exit 1; }
	@echo "✓ Kernel linked successfully ($(shell stat -c%s $(KERNEL) 2>/dev/null || stat -f%z $(KERNEL) 2>/dev/null) bytes)"

# Define source directories for vpath search
vpath %.c src:src/System:src/QuickDraw:src/WindowManager:src/MenuManager:src/ControlManager \
          src/EventManager:src/DialogManager:src/StandardFile:src/TextEdit:src/ListManager \
          src/ScrapManager:src/ProcessMgr:src/TimeManager:src/SoundManager:src/FontManager \
          src/Gestalt:src/MemoryMgr:src/ResourceMgr:src/FileMgr:src/FS:src/Finder \
          src/Finder/Icon:src/DeskManager:src/ControlPanels:src/PatternMgr:src/Resources \
          src/Resources/Icons:src/Apps/SimpleText:src/Apps/MacPaint:src/Platform:src/Platform/x86:src/Platform/arm:src/Platform/arm64:src/Platform/ppc:src/PrintManager \
          src/HelpManager:src/ComponentManager:src/EditionManager:src/NotificationManager \
          src/PackageManager:src/NetworkExtension:src/ColorManager:src/CommunicationToolbox \
          src/GestaltManager:src/SpeechManager:src/BootLoader \
          src/SegmentLoader:src/CPU:src/CPU/m68k_interp:src/DeviceManager:src/Keyboard \
          src/Datetime:src/Calculator:src/Chooser:src/StartupScreen:src/OSUtils
vpath %.S $(HAL_DIR)

# Compile assembly files
$(OBJ_DIR)/%.o: %.S | $(OBJ_DIR)
	@mkdir -p $(dir $@)
	@echo "AS $<"
ifeq ($(PLATFORM),arm64)
	@$(CC) -E -DQEMU_BUILD $< | $(AS) -march=armv8-a - -o $@
else
	@$(AS) $(ASFLAGS) $< -o $@
endif

# Compile C files (single rule for all directories via vpath)
$(OBJ_DIR)/%.o: %.c | $(OBJ_DIR)
	@mkdir -p $(dir $@)
	@echo "CC $<"
	@$(CC) $(CFLAGS) -c $< -o $@

# Special targets for stubs
build/obj/universal_stubs.o: src/universal_stubs.c
	@echo "CC src/universal_stubs.c"
	@$(CC) $(CFLAGS) -c src/universal_stubs.c -o build/obj/universal_stubs.o

build/obj/ultimate_stubs.o: src/ultimate_stubs.c
	@echo "CC src/ultimate_stubs.c"
	@$(CC) $(CFLAGS) -c src/ultimate_stubs.c -o build/obj/ultimate_stubs.o

# Platform-specific build targets
ifeq ($(PLATFORM),arm)
# ARM targets - create raw SD image
sd: $(KERNEL)
	@echo "Creating raw SD image for Raspberry Pi..."
	@dd if=/dev/zero of=system71.img bs=512 count=20480
	@dd if=$(KERNEL) of=system71.img bs=512 seek=2048 conv=notrunc
	@echo "✓ SD image created: system71.img"

iso: sd
	@echo "Note: Use 'make sd' to create bootable SD card image"

run: $(KERNEL)
	@echo "Running ARM kernel in QEMU..."
	qemu-system-arm -M raspi3 -kernel $(KERNEL) -m 512 -serial stdio -nographic

debug: $(KERNEL)
	qemu-system-arm -M raspi3 -kernel $(KERNEL) -m 512 -serial stdio -nographic -s -S
else ifeq ($(PLATFORM),ppc)
# PowerPC targets

# Bootloader target
BOOTLOADER_SRC = src/Platform/ppc/boot.fs
BOOTLOADER = $(BUILD_DIR)/boot.fs

$(BOOTLOADER): $(BOOTLOADER_SRC) | $(BUILD_DIR)
	@cp $(BOOTLOADER_SRC) $(BOOTLOADER)
	@echo "✓ Bootloader copied to $(BOOTLOADER)"

run: $(KERNEL) $(BOOTLOADER)
	@echo "Running PowerPC kernel in QEMU (mac99 machine)..."
	@echo "Kernel will be preloaded at 0x01000000"
	@echo "OpenBIOS will automatically detect and boot it"
	@echo "Serial output will appear below:"
	@echo "---"
	@echo ""
	@echo "Note: If boot hangs after 'Booting...', the kernel is loaded."
	@echo "This is the documented OpenBIOS -kernel bug workaround."
	@echo ""
	qemu-system-ppc -M mac99 -m 512 -serial stdio -monitor none -nographic \
		-device loader,file=$(KERNEL),addr=0x01000000,cpu-num=0

debug: $(KERNEL)
	@echo "Running PowerPC kernel in QEMU debug mode..."
	@echo "NOTE: OpenBIOS kernel boot may hang. This is a firmware bug."
	qemu-system-ppc -M mac99 -kernel $(KERNEL) -m 512 -serial stdio -nographic -s -S

test-qemu: $(KERNEL)
	@echo "Testing QEMU PPC boot (verbose output)..."
	@echo "  Machine: mac99 (Open Firmware compatible)"
	@echo "  Kernel: $(KERNEL)"
	@echo "  Memory: 512 MB"
	@echo ""
	@echo "To debug with GDB:"
	@echo "  make PLATFORM=ppc debug"
	@echo "  (in another terminal) powerpc-linux-gnu-gdb $(KERNEL)"
	@echo "  (gdb) target remote :1234"
	@echo "  (gdb) continue"
	@echo ""
	qemu-system-ppc -M mac99 -kernel $(KERNEL) -m 512 -serial stdio -nographic -trace 'enable=*' -d int 2>&1 | head -200

# Build OF Forth bootloader (no compilation needed, just copy)
bootloader: $(BOOTLOADER)
	@echo "Bootloader target completed"

# PowerPC kernel binary (raw format for direct execution)
KERNEL_BIN = kernel.bin

$(KERNEL_BIN): $(KERNEL)
	@echo "Converting ELF kernel to raw binary..."
	@$(OBJCOPY) -O binary $(KERNEL) $(KERNEL_BIN)
	@echo "✓ Binary kernel: $(KERNEL_BIN)"

# PowerPC disk image creation (no partition table, just FAT32)
# Uses mtools to avoid requiring root or mount privileges
PPC_DISK_IMAGE = ppc_system71.img

$(PPC_DISK_IMAGE): $(KERNEL) $(KERNEL_BIN)
	@echo "Creating PowerPC disk image (FAT32, 32 MB)..."
	@echo "  Image: $(PPC_DISK_IMAGE)"
	@echo "  Contents: kernel.elf and kernel.bin"
	@echo ""
	@# Create 32 MB FAT32 disk image using dd
	@dd if=/dev/zero of=$(PPC_DISK_IMAGE) bs=1M count=32 2>/dev/null
	@# Format as FAT32 with mtools-compatible label
	@mkfs.fat -F 32 -n "System71" $(PPC_DISK_IMAGE) 2>/dev/null
	@# Copy both ELF and binary kernel to disk
	@mcopy -i $(PPC_DISK_IMAGE) $(KERNEL) ::kernel.elf
	@mcopy -i $(PPC_DISK_IMAGE) $(KERNEL_BIN) ::kernel.bin
	@# Verify contents
	@echo "Disk contents:"
	@mdir -i $(PPC_DISK_IMAGE) -a ::
	@echo ""
	@echo "✓ Disk image created: $(PPC_DISK_IMAGE)"
	@echo ""
	@echo "Boot methods:"
	@echo "  make PLATFORM=ppc disk-run     (ELF boot attempt)"
	@echo "  make PLATFORM=ppc boot-bin     (raw binary boot)"

# Build bootable ISO/disk for PPC
iso: $(PPC_DISK_IMAGE)
	@echo "PPC disk image ready at $(PPC_DISK_IMAGE)"

# Boot from disk image with ELF
disk-run: $(PPC_DISK_IMAGE)
	@echo "Booting from disk image (ELF)..."
	@echo "Using OF 'boot' command for client interface setup"
	@echo ""
	@echo "At OpenBIOS prompt (0 >), use:"
	@echo "  boot hd:,\\kernel.elf"
	@echo ""
	qemu-system-ppc -M mac99 -m 512 -serial stdio -monitor none -nographic \
		-drive file=$(PPC_DISK_IMAGE),format=raw,if=ide

# Boot from disk image with raw binary
boot-bin: $(PPC_DISK_IMAGE)
	@echo "Booting from disk image (raw binary)..."
	@echo "Using Forth loader to load kernel.bin directly"
	@echo ""
	@echo "At OpenBIOS prompt (0 >), you can also manually:"
	@echo "  1000000 to load-base"
	@echo "  \" hd:,\\kernel.bin\" load"
	@echo "  1000000 execute"
	@echo ""
	qemu-system-ppc -M mac99 -m 512 -serial stdio -monitor none -nographic \
		-drive file=$(PPC_DISK_IMAGE),format=raw,if=ide
else
# x86 targets - create ISO image
iso: $(ISO)

$(ISO): $(KERNEL) grub.cfg | $(ISO_DIR)/boot/grub
	@echo "Creating bootable ISO..."
	@cp $(KERNEL) $(ISO_DIR)/boot/
	@cp grub.cfg $(ISO_DIR)/boot/grub/grub.cfg
	@$(GRUB) -d /usr/lib/grub/i386-pc -o $(ISO) $(ISO_DIR)

# Run with QEMU (PC speaker with PulseAudio backend)
# VGA: std (standard VGA, no corruption during boot)
# Resolution: Detected automatically from GRUB/multiboot framebuffer
run: $(ISO)
	qemu-system-i386 -cdrom $(ISO) -drive file=test_disk.img,format=raw,if=ide -m 1024 -vga std -serial file:/tmp/serial.log \
		-audiodev pa,id=snd0,server=/run/user/$(shell id -u)/pulse/native -machine pcspk-audiodev=snd0 -device sb16,audiodev=snd0

# Debug with QEMU
debug: $(ISO)
	qemu-system-i386 -cdrom $(ISO) -drive file=test_disk.img,format=raw,if=ide -m 1024 -vga std -s -S

sd:
	@echo "Note: Use 'make iso' to create bootable ISO image"
endif

# Clean
clean:
	rm -rf $(BUILD_DIR) obj isodir iso system71.iso kernel.elf src/patterns_rsrc.c

# Show compilation info
info:
	@echo "════════════════════════════════════════════════════════════════"
	@echo "  Build Information"
	@echo "════════════════════════════════════════════════════════════════"
	@echo "Platform: $(PLATFORM)"
	@if [ "$(PLATFORM)" = "arm" ]; then \
		echo "Pi Model: $(if $(PI_MODEL),$(PI_MODEL),runtime-detected)"; \
		echo "ARM Architecture: $(ARM_ARCH)"; \
		echo "CPU: $(PI_CPU_NAME) @ $(PI_CPU_FREQ) MHz"; \
	fi
	@echo "C Sources: $(words $(C_SOURCES)) files"
	@echo "ASM Sources: $(words $(ASM_SOURCES)) files"
	@echo "Total Objects: $(words $(OBJECTS)) files"
	@echo ""
	@echo "Usage Examples:"
	@echo "  make import-icons ICON_DIR=/path/to/pngs"
	@echo "  make PLATFORM=arm PI_MODEL=pi4 clean all"
	@echo "════════════════════════════════════════════════════════════════"

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
	@echo "  make [target] [PLATFORM=...] [PI_MODEL=...] [CONFIG=...] [options]"
	@echo ""
	@echo "MAIN TARGETS:"
	@echo "  all              Build kernel (default target)"
	@echo "  iso              Create bootable ISO image (x86) or SD image (ARM)"
	@echo "  sd               Create raw SD card image (ARM only)"
	@echo "  run              Run kernel in QEMU with serial logging"
	@echo "  debug            Launch QEMU in debug mode (waits for GDB)"
	@echo "  clean            Remove all build artifacts"
	@echo "  check-tools      Verify build tool versions"
	@echo "  check-exports    Validate exported symbol surface"
	@echo "  info             Show build statistics"
	@echo "  help             Show this help message"
	@echo "  import-icons ICON_DIR=...   Generate C icon resources from PNGs"
	@echo ""
	@echo "PLATFORM OPTIONS:"
	@echo "  PLATFORM=x86     Build for x86 (default)"
	@echo "  PLATFORM=arm     Build for ARM (Raspberry Pi)"
	@echo "  PLATFORM=ppc     Build for PowerPC (experimental)"
	@echo ""
	@echo "RASPBERRY PI MODEL OPTIONS (ARM only):"
	@echo "  PI_MODEL=pi3     Build for Raspberry Pi 3 (ARMv6z, Cortex-A53)"
	@echo "  PI_MODEL=pi4     Build for Raspberry Pi 4 (ARMv7-A, Cortex-A72)"
	@echo "  PI_MODEL=pi5     Build for Raspberry Pi 5 (ARMv7-A, Cortex-A76)"
	@echo "  PI_MODEL=        Runtime detection (default)"

.PHONY: import-icons
import-icons:
	@if [ -z "$(ICON_DIR)" ]; then echo "ERROR: ICON_DIR not set"; exit 1; fi
	@echo "Generating icons from $(ICON_DIR) ..."
	@python3 tools/gen_icons.py "$(ICON_DIR)" .
	@echo "✓ Generated icons: include/Resources/icons_generated.h, src/Resources/generated/icons_generated.c"

.PHONY: build-configurations

build-configurations:
	@echo "════════════════════════════════════════════════════════════════"
	@echo "BUILD CONFIGURATIONS:"
	@echo "════════════════════════════════════════════════════════════════"
	@echo ""
	@echo "CONFIG OPTIONS (stored in config/*.mk):"
	@echo "  CONFIG=default   Standard development build (default)"
	@echo "  CONFIG=debug     Debug build with smoke tests enabled"
	@echo "  CONFIG=release   Optimized release build"
	@echo ""
	@echo "PLATFORM AND MODEL OPTIONS:"
	@echo "  PLATFORM=x86             Build for x86 PC (default)"
	@echo "  PLATFORM=arm             Build for ARM (Raspberry Pi)"
	@echo "  PLATFORM=ppc             Build for PowerPC (experimental)"
	@echo "  PI_MODEL=pi3             Raspberry Pi 3 (ARMv6z, 1.2 GHz Cortex-A53)"
	@echo "  PI_MODEL=pi4             Raspberry Pi 4 (ARMv7-A, 1.5 GHz Cortex-A72)"
	@echo "  PI_MODEL=pi5             Raspberry Pi 5 (ARMv7-A, 2.4 GHz Cortex-A76)"
	@echo "  PI_MODEL=                Runtime detection (default)"
	@echo ""
	@echo "FEATURE FLAGS (override in config/*.mk or command line):"
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
	@echo "  make                           Build x86 with defaults"
	@echo "  make -j8                       Parallel build with 8 jobs"
	@echo "  make CONFIG=debug              Build with debug configuration"
	@echo "  make CONFIG=release iso        Build optimized ISO"
	@echo "  make PLATFORM=arm PI_MODEL=pi4 Build for Raspberry Pi 4 explicitly"
	@echo "  make PLATFORM=arm PI_MODEL=pi3 Build for Raspberry Pi 3"
	@echo "  make PLATFORM=arm PI_MODEL=pi5 Build for Raspberry Pi 5"
	@echo "  make PLATFORM=arm              Build with runtime Pi detection"
	@echo "  make PLATFORM=ppc             Build experimental PowerPC image"
	@echo "  make clean all                 Clean rebuild"
	@echo "  make CTRL_SMOKE_TEST=1         Build with control tests"
	@echo ""
	@echo "BUILD INFORMATION:"
	@echo "  Run 'make info' to see current build configuration details"
	@echo ""
	@echo "PARALLEL BUILDS:"
	@echo "  Use 'make -j<N>' for parallel compilation (recommended)"
	@echo "  Example: make -j\$$(nproc)      Use all CPU cores"
	@echo ""
	@echo "CROSS-COMPILATION:"
	@echo "  ARM builds require arm-linux-gnueabihf cross-compiler"
	@echo "  Install with: sudo apt-get install gcc-arm-linux-gnueabihf"
	@echo "  PowerPC builds use powerpc-linux-gnu toolchains (override CROSS_COMPILE as needed)"
	@echo ""
	@echo "For more information, see docs/BUILD_SYSTEM_IMPROVEMENTS.md"
	@echo "════════════════════════════════════════════════════════════════"

# Include dependency files (auto-generated by -MMD -MP)
-include $(DEPS)

.PHONY: all clean iso run debug info check-exports check-tools help sd import-icons build-configurations
