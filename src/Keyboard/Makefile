# Keyboard Control Panel Makefile
# Part of System 7.1 Portable Project
# Builds the Keyboard control panel and test suite

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
BUILD_DIR = ../../build/Keyboard
INCLUDE_DIR = ../../include/Keyboard

# Source files
SRCS = keyboard_control_panel.c keyboard_tests.c
OBJS = $(SRCS:%.c=$(BUILD_DIR)/%.o)

# Output library and executables
TARGET_LIB = $(BUILD_DIR)/libkeyboard.a
TEST_BIN = $(BUILD_DIR)/keyboard_test

# Default target
all: $(TARGET_LIB) $(TEST_BIN)

# Create build directory
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Object files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(RELEASE_FLAGS) -c $< -o $@

# Static library (excluding test files)
LIB_OBJS = $(BUILD_DIR)/keyboard_control_panel.o
$(TARGET_LIB): $(LIB_OBJS)
	ar rcs $@ $(LIB_OBJS)

# Test executable
$(TEST_BIN): $(BUILD_DIR)/keyboard_tests.o $(TARGET_LIB)
	$(CC) $(CFLAGS) $(RELEASE_FLAGS) $< -L$(BUILD_DIR) -lkeyboard -o $@

# Debug build
debug: CFLAGS += $(DEBUG_FLAGS)
debug: clean $(TARGET_LIB) $(TEST_BIN)

# Run tests
test: $(TEST_BIN)
	$(TEST_BIN)

# Clean
clean:
	rm -rf $(BUILD_DIR)

# Test compilation (without linking - headers only)
check:
	$(CC) $(CFLAGS) -fsyntax-only keyboard_control_panel.c
	$(CC) $(CFLAGS) -fsyntax-only keyboard_tests.c

# Install headers
install-headers:
	mkdir -p $(DESTDIR)/usr/local/include/system71/Keyboard
	cp $(INCLUDE_DIR)/*.h $(DESTDIR)/usr/local/include/system71/Keyboard/

# Install library
install: $(TARGET_LIB) install-headers
	mkdir -p $(DESTDIR)/usr/local/lib
	cp $(TARGET_LIB) $(DESTDIR)/usr/local/lib/

.PHONY: all debug test clean check install-headers install