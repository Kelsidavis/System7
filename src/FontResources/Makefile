# Font Resources Makefile
# Part of System 7.1 Portable Project
# Builds portable font resource system for classic Mac OS fonts

CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -I../../include -I. -fPIC
DEBUG_FLAGS = -g -O0 -DDEBUG
RELEASE_FLAGS = -O2 -DNDEBUG

# Platform detection
UNAME_S := $(shell uname -s)
UNAME_M := $(shell uname -m)

ifeq ($(UNAME_S),Linux)
    CFLAGS += -D__linux__
endif
ifeq ($(UNAME_S),Darwin)
    CFLAGS += -D__APPLE__
endif
ifeq ($(UNAME_M),arm64)
    CFLAGS += -DARM64
endif
ifeq ($(UNAME_M),x86_64)
    CFLAGS += -DX86_64
endif

# Directories
SRC_DIR = .
BUILD_DIR = ../../build/FontResources
INCLUDE_DIR = ../../include/FontResources

# Source files
SRCS = SystemFontData.c FontResourceConverter.c FontTest.c ModernFontLoader.c FontDownloader.c ModernFontTest.c EmbeddedFonts.c
OBJS = $(SRCS:%.c=$(BUILD_DIR)/%.o)

# Output library and executables
TARGET_LIB = $(BUILD_DIR)/libfonts.a
TEST_BIN = $(BUILD_DIR)/font_test
MODERN_TEST_BIN = $(BUILD_DIR)/modern_font_test
DOWNLOADER_BIN = $(BUILD_DIR)/font_downloader

# Default target
all: $(TARGET_LIB) $(TEST_BIN) $(MODERN_TEST_BIN) $(DOWNLOADER_BIN)

# Create build directory
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Object files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(RELEASE_FLAGS) -c $< -o $@

# Static library (excluding test files)
LIB_OBJS = $(BUILD_DIR)/SystemFontData.o $(BUILD_DIR)/FontResourceConverter.o $(BUILD_DIR)/ModernFontLoader.o $(BUILD_DIR)/EmbeddedFonts.o
$(TARGET_LIB): $(LIB_OBJS)
	ar rcs $@ $(LIB_OBJS)

# Test executables
$(TEST_BIN): $(BUILD_DIR)/FontTest.o $(TARGET_LIB)
	$(CC) $(CFLAGS) $(RELEASE_FLAGS) $< -L$(BUILD_DIR) -lfonts -o $@

$(MODERN_TEST_BIN): $(BUILD_DIR)/ModernFontTest.o $(TARGET_LIB)
	$(CC) $(CFLAGS) $(RELEASE_FLAGS) $< -L$(BUILD_DIR) -lfonts -o $@

$(DOWNLOADER_BIN): $(BUILD_DIR)/FontDownloader.o $(TARGET_LIB)
	$(CC) $(CFLAGS) $(RELEASE_FLAGS) $< -L$(BUILD_DIR) -lfonts -lcurl -o $@

# Debug build
debug: CFLAGS += $(DEBUG_FLAGS)
debug: clean $(TARGET_LIB) $(TEST_BIN) $(MODERN_TEST_BIN) $(DOWNLOADER_BIN)

# Run tests
test: $(TEST_BIN)
	$(TEST_BIN)

# Run modern font tests
test-modern: $(MODERN_TEST_BIN)
	$(MODERN_TEST_BIN)

# Setup font download system
setup-fonts: $(DOWNLOADER_BIN)
	$(DOWNLOADER_BIN) test

# Clean
clean:
	rm -rf $(BUILD_DIR)

# Test compilation (without linking - headers only)
check:
	$(CC) $(CFLAGS) -fsyntax-only SystemFontData.c
	$(CC) $(CFLAGS) -fsyntax-only FontResourceConverter.c
	$(CC) $(CFLAGS) -fsyntax-only FontTest.c

# Install headers
install-headers:
	mkdir -p $(DESTDIR)/usr/local/include/system71/FontResources
	cp $(INCLUDE_DIR)/*.h $(DESTDIR)/usr/local/include/system71/FontResources/

# Install library
install: $(TARGET_LIB) install-headers
	mkdir -p $(DESTDIR)/usr/local/lib
	cp $(TARGET_LIB) $(DESTDIR)/usr/local/lib/

# Generate font data from .rsrc files (requires original resources)
convert-fonts:
	@echo "Converting System 7.1 font resources..."
	@echo "This would extract and convert the original .rsrc files"
	@echo "Currently using static font data structures"

.PHONY: all debug test clean check install-headers install convert-fonts