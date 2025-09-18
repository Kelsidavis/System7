# Date & Time Control Panel Makefile
# Part of System 7.1 Portable Project

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
BUILD_DIR = ../../build/Datetime
INCLUDE_DIR = ../../include/Datetime

# Source files
SRCS = datetime_cdev.c
OBJS = $(SRCS:%.c=$(BUILD_DIR)/%.o)

# Output library
TARGET_LIB = $(BUILD_DIR)/libdatetime.a

# Default target
all: $(TARGET_LIB)

# Create build directory
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Object files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(RELEASE_FLAGS) -c $< -o $@

# Static library
$(TARGET_LIB): $(OBJS)
	ar rcs $@ $(OBJS)

# Debug build
debug: CFLAGS += $(DEBUG_FLAGS)
debug: clean $(TARGET_LIB)

# Clean
clean:
	rm -rf $(BUILD_DIR)

# Test compilation (without linking - headers only)
check:
	$(CC) $(CFLAGS) -fsyntax-only $(SRCS)

# Install headers
install-headers:
	mkdir -p $(DESTDIR)/usr/local/include/system71/Datetime
	cp $(INCLUDE_DIR)/*.h $(DESTDIR)/usr/local/include/system71/Datetime/

.PHONY: all debug clean check install-headers