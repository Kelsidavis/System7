# System 7.1 Portable - Master Build System
# Organized codebase for cross-platform development
#
# Targets:
#   all        - Build all components
#   libs       - Build all libraries
#   examples   - Build example applications
#   tests      - Build and run test suites
#   clean      - Remove all build artifacts
#   debug      - Build with debug symbols
#   install    - Install libraries and headers
#   docs       - Generate documentation

# Compiler settings
CC = gcc
AR = ar
CFLAGS = -Wall -Wextra -std=c99 -Iinclude
DEBUG_FLAGS = -g -O0 -DDEBUG
RELEASE_FLAGS = -O2 -DNDEBUG
LDFLAGS = -lpthread

# Platform detection
UNAME_S := $(shell uname -s)
UNAME_M := $(shell uname -m)

ifeq ($(UNAME_S),Linux)
    CFLAGS += -D__linux__ -fPIC
    LIB_EXT = .so
endif
ifeq ($(UNAME_S),Darwin)
    CFLAGS += -D__APPLE__ -fPIC
    LIB_EXT = .dylib
endif
ifeq ($(UNAME_M),arm64)
    CFLAGS += -DARM64
endif
ifeq ($(UNAME_M),x86_64)
    CFLAGS += -DX86_64
endif

# Directory structure
SRC_DIR = src
INCLUDE_DIR = include
BUILD_DIR = build
TEST_DIR = tests
EXAMPLE_DIR = examples
DOCS_DIR = docs

# Core System Components
CORE_MODULES = ADBManager DeviceManager DialogManager EditionManager \
               EventManager MenuManager WindowManager TrapDispatcher \
               HelpManager FileManager ResourceManager MemoryManager \
               FontManager SoundManager PrintManager ComponentManager \
               Finder Datetime Chooser GestaltManager FontResources \
               ControlManager QuickDraw TextEdit ScrapManager ListManager \
               Calculator DeskManager

# Find all source files organized by module
define find_module_sources
$(wildcard $(SRC_DIR)/$(1)/*.c) $(wildcard $(SRC_DIR)/$(1)_*.c)
endef

# Generate source lists for each module
ADB_SRCS = $(call find_module_sources,ADBManager) $(wildcard $(SRC_DIR)/ADB*.c)
DEVICE_SRCS = $(call find_module_sources,DeviceManager)
DIALOG_SRCS = $(call find_module_sources,DialogManager)
EDITION_SRCS = $(call find_module_sources,EditionManager)
EVENT_SRCS = $(call find_module_sources,EventManager)
MENU_SRCS = $(call find_module_sources,MenuManager)
WINDOW_SRCS = $(call find_module_sources,WindowManager)
TRAP_SRCS = $(call find_module_sources,TrapDispatcher) $(wildcard $(SRC_DIR)/TrapDispatcher*.c)
HELP_SRCS = $(call find_module_sources,HelpManager)
RESOURCE_SRCS = $(wildcard $(SRC_DIR)/Resource*.c) $(wildcard $(SRC_DIR)/HFS_*.c)
MEMORY_SRCS = $(wildcard $(SRC_DIR)/*Mem*.c) $(wildcard $(SRC_DIR)/SystemInit.c)
FILE_SRCS = $(wildcard $(SRC_DIR)/File*.c) $(wildcard $(SRC_DIR)/SCSI*.c)
FONT_SRCS = $(wildcard $(SRC_DIR)/FontManager/*.c)
SOUND_SRCS = $(wildcard $(SRC_DIR)/SoundManager/*.c)
PRINT_SRCS = $(wildcard $(SRC_DIR)/PrintManager/*.c)
COMPONENT_SRCS = $(wildcard $(SRC_DIR)/ComponentManager/*.c)
FINDER_SRCS = $(wildcard $(SRC_DIR)/Finder/*.c)
DATETIME_SRCS = $(wildcard $(SRC_DIR)/Datetime/*.c)
CHOOSER_SRCS = $(wildcard $(SRC_DIR)/Chooser/*.c)
GESTALT_SRCS = $(wildcard $(SRC_DIR)/GestaltManager/*.c)
FONTRES_SRCS = $(wildcard $(SRC_DIR)/FontResources/*.c)
CONTROL_SRCS = $(wildcard $(SRC_DIR)/ControlManager/*.c)
QUICKDRAW_SRCS = $(wildcard $(SRC_DIR)/QuickDraw/*.c)
TEXTEDIT_SRCS = $(wildcard $(SRC_DIR)/TextEdit/*.c)
SCRAP_SRCS = $(wildcard $(SRC_DIR)/ScrapManager/*.c)
LIST_SRCS = $(wildcard $(SRC_DIR)/ListManager/*.c)
CALCULATOR_SRCS = $(wildcard $(SRC_DIR)/Calculator/*.c)
DESKMGR_SRCS = $(wildcard $(SRC_DIR)/DeskManager/*.c)

# All core sources
ALL_SRCS = $(ADB_SRCS) $(DEVICE_SRCS) $(DIALOG_SRCS) $(EDITION_SRCS) \
           $(EVENT_SRCS) $(MENU_SRCS) $(WINDOW_SRCS) $(TRAP_SRCS) \
           $(HELP_SRCS) $(RESOURCE_SRCS) $(MEMORY_SRCS) $(FILE_SRCS) \
           $(FONT_SRCS) $(SOUND_SRCS) $(PRINT_SRCS) $(COMPONENT_SRCS) \
           $(FINDER_SRCS) $(DATETIME_SRCS) $(CHOOSER_SRCS) $(GESTALT_SRCS) \
           $(FONTRES_SRCS) $(CONTROL_SRCS) $(QUICKDRAW_SRCS) $(TEXTEDIT_SRCS) \
           $(SCRAP_SRCS) $(LIST_SRCS) $(CALCULATOR_SRCS) $(DESKMGR_SRCS)

# Object files
ALL_OBJS = $(ALL_SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)

# Test files
TEST_SRCS = $(wildcard $(TEST_DIR)/*.c)
TEST_OBJS = $(TEST_SRCS:$(TEST_DIR)/%.c=$(BUILD_DIR)/test_%.o)
TEST_BINS = $(TEST_SRCS:$(TEST_DIR)/%.c=$(BUILD_DIR)/%)

# Example files
EXAMPLE_SRCS = $(wildcard $(EXAMPLE_DIR)/*.c)
EXAMPLE_OBJS = $(EXAMPLE_SRCS:$(EXAMPLE_DIR)/%.c=$(BUILD_DIR)/%.o)
EXAMPLE_BINS = $(EXAMPLE_SRCS:$(EXAMPLE_DIR)/%.c=$(BUILD_DIR)/%)

# Library targets
STATIC_LIB = $(BUILD_DIR)/libsystem71.a
SHARED_LIB = $(BUILD_DIR)/libsystem71$(LIB_EXT)

# Default target
all: libs examples tests

# Create build directory
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Core library compilation
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(RELEASE_FLAGS) -c $< -o $@

# Test compilation
$(BUILD_DIR)/test_%.o: $(TEST_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(RELEASE_FLAGS) -c $< -o $@

# Example compilation
$(BUILD_DIR)/%.o: $(EXAMPLE_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(RELEASE_FLAGS) -c $< -o $@

# Libraries
libs: $(STATIC_LIB) $(SHARED_LIB)

$(STATIC_LIB): $(ALL_OBJS) | $(BUILD_DIR)
	$(AR) rcs $@ $(ALL_OBJS)

$(SHARED_LIB): $(ALL_OBJS) | $(BUILD_DIR)
	$(CC) -shared -o $@ $(ALL_OBJS) $(LDFLAGS)

# Examples
examples: $(EXAMPLE_BINS)

$(BUILD_DIR)/%: $(BUILD_DIR)/%.o $(STATIC_LIB)
	$(CC) $(CFLAGS) $(RELEASE_FLAGS) -o $@ $< -L$(BUILD_DIR) -lsystem71 $(LDFLAGS)

# Tests
tests: $(TEST_BINS)
	@echo "Running test suite..."
	@for test in $(TEST_BINS); do \
		if [ -x $$test ]; then \
			echo "Running $$test..."; \
			$$test || echo "Test $$test failed"; \
		fi; \
	done

$(BUILD_DIR)/test_%: $(BUILD_DIR)/test_%.o $(STATIC_LIB)
	$(CC) $(CFLAGS) $(RELEASE_FLAGS) -o $@ $< -L$(BUILD_DIR) -lsystem71 $(LDFLAGS)

# Debug build
debug: CFLAGS += $(DEBUG_FLAGS)
debug: clean all

# Documentation
docs:
	@echo "System 7.1 Portable Codebase"
	@echo "============================"
	@echo ""
	@echo "A modern, organized implementation of classic Mac OS System 7.1 components"
	@echo "designed for cross-platform compatibility and educational purposes."
	@echo ""
	@echo "Directory Structure:"
	@echo "  src/        - Source code organized by functional modules"
	@echo "  include/    - Header files with clean API definitions"
	@echo "  build/      - Compiled objects, libraries, and build artifacts"
	@echo "  tests/      - Comprehensive test suites"
	@echo "  examples/   - Usage examples and demo applications"
	@echo "  docs/       - Technical documentation and guides"
	@echo "  tools/      - Build tools and utilities"
	@echo ""
	@echo "Available Targets:"
	@echo "  all         - Build libraries, examples, and tests"
	@echo "  libs        - Build static and shared libraries"
	@echo "  examples    - Build example applications"
	@echo "  tests       - Build and run test suites"
	@echo "  debug       - Build with debug symbols"
	@echo "  clean       - Remove all build artifacts"
	@echo "  install     - Install libraries and headers system-wide"
	@echo "  docs        - Show this documentation"
	@echo ""
	@echo "Core Modules:"
	@for module in $(CORE_MODULES); do echo "  - $$module"; done

# Installation
install: libs
	@echo "Installing System 7.1 Portable libraries..."
	sudo mkdir -p /usr/local/lib /usr/local/include/system71
	sudo cp $(STATIC_LIB) $(SHARED_LIB) /usr/local/lib/
	sudo cp -r $(INCLUDE_DIR)/* /usr/local/include/system71/
	sudo ldconfig 2>/dev/null || true
	@echo "Installation complete."

# Cleanup
clean:
	rm -rf $(BUILD_DIR)
	@echo "Build artifacts cleaned."

# Include module-specific makefiles if they exist
-include $(BUILD_DIR)/*.mk

.PHONY: all libs examples tests debug docs install clean