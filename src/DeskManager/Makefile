# Makefile for System 7.1 Portable Desk Manager
#
# This makefile builds the complete Desk Manager implementation
# including all built-in desk accessories.

# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -O2 -I../../include
LDFLAGS = -lm

# Source files
CORE_SOURCES = \
    DeskManagerCore.c \
    DALoader.c \
    DAWindows.c \
    SystemMenu.c \
    BuiltinDAs.c

DA_SOURCES = \
    Calculator.c \
    KeyCaps.c \
    AlarmClock.c \
    Chooser.c

ALL_SOURCES = $(CORE_SOURCES) $(DA_SOURCES)

# Object files
CORE_OBJECTS = $(CORE_SOURCES:.c=.o)
DA_OBJECTS = $(DA_SOURCES:.c=.o)
ALL_OBJECTS = $(ALL_SOURCES:.c=.o)

# Headers
HEADERS = \
    ../../include/DeskManager/DeskManager.h \
    ../../include/DeskManager/DeskAccessory.h \
    ../../include/DeskManager/Calculator.h \
    ../../include/DeskManager/KeyCaps.h \
    ../../include/DeskManager/AlarmClock.h \
    ../../include/DeskManager/Chooser.h \
    ../../include/DeskManager/Types.h

# Target library
LIBRARY = libdeskmanager.a

# Test program
TEST_PROGRAM = test_deskmanager
TEST_SOURCES = test_deskmanager.c

# Build targets
.PHONY: all clean test install help

all: $(LIBRARY)

$(LIBRARY): $(ALL_OBJECTS)
	ar rcs $@ $^
	ranlib $@
	@echo "Desk Manager library built successfully: $(LIBRARY)"

# Core components
$(CORE_OBJECTS): %.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

# Desk accessories
$(DA_OBJECTS): %.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

# Test program
test: $(TEST_PROGRAM)

$(TEST_PROGRAM): $(TEST_SOURCES) $(LIBRARY)
	$(CC) $(CFLAGS) -o $@ $< -L. -ldeskmanager $(LDFLAGS)
	@echo "Test program built: $(TEST_PROGRAM)"

# Create simple test file if it doesn't exist
$(TEST_SOURCES):
	@echo "Creating simple test program..."
	@cat > $@ << 'EOF'
/*
 * Simple test program for Desk Manager
 */
#include <stdio.h>
#include <assert.h>
#include "DeskManager/DeskManager.h"
#include "DeskManager/Calculator.h"

int main(void) {
    printf("Testing Desk Manager...\n");

    // Test initialization
    int result = DeskManager_Initialize();
    assert(result == DESK_ERR_NONE);
    printf("✓ Desk Manager initialized\n");

    // Test DA availability
    assert(DeskManager_IsDAAvailable("Calculator"));
    assert(DeskManager_IsDAAvailable("Key Caps"));
    assert(DeskManager_IsDAAvailable("Alarm Clock"));
    assert(DeskManager_IsDAAvailable("Chooser"));
    printf("✓ All built-in DAs available\n");

    // Test opening Calculator
    int16_t calcRef = OpenDeskAcc("Calculator");
    assert(calcRef > 0);
    printf("✓ Calculator opened (ref: %d)\n", calcRef);

    // Test DA count
    assert(DeskManager_GetDACount() == 1);
    printf("✓ DA count correct\n");

    // Test closing Calculator
    CloseDeskAcc(calcRef);
    assert(DeskManager_GetDACount() == 0);
    printf("✓ Calculator closed\n");

    // Test shutdown
    DeskManager_Shutdown();
    printf("✓ Desk Manager shutdown\n");

    printf("\nAll tests passed! Desk Manager is working correctly.\n");
    return 0;
}
EOF

# Documentation
docs:
	@echo "Desk Manager Documentation:"
	@echo ""
	@echo "Core Components:"
	@echo "  - DeskManagerCore: Main DA management system"
	@echo "  - DALoader: Resource loading and DA registry"
	@echo "  - DAWindows: Window management for DAs"
	@echo "  - SystemMenu: Apple menu integration"
	@echo "  - BuiltinDAs: Built-in DA registration"
	@echo ""
	@echo "Built-in Desk Accessories:"
	@echo "  - Calculator: Full-featured calculator with scientific functions"
	@echo "  - Key Caps: Keyboard layout display and character mapping"
	@echo "  - Alarm Clock: Time display and alarm management"
	@echo "  - Chooser: Device and printer selection interface"
	@echo ""
	@echo "See README.md for detailed documentation."

# Installation (example)
install: $(LIBRARY)
	@if [ -d "/usr/local/lib" ]; then \
		cp $(LIBRARY) /usr/local/lib/; \
		echo "Library installed to /usr/local/lib/"; \
	else \
		echo "Install directory /usr/local/lib not found"; \
	fi
	@if [ -d "/usr/local/include" ]; then \
		mkdir -p /usr/local/include/DeskManager; \
		cp ../../include/DeskManager/*.h /usr/local/include/DeskManager/; \
		echo "Headers installed to /usr/local/include/DeskManager/"; \
	else \
		echo "Install directory /usr/local/include not found"; \
	fi

# Debug build
debug: CFLAGS += -g -DDEBUG -O0
debug: clean $(LIBRARY)
	@echo "Debug build completed"

# Release build
release: CFLAGS += -DNDEBUG -O3
release: clean $(LIBRARY)
	@echo "Release build completed"

# Static analysis
analyze:
	@echo "Running static analysis..."
	@which cppcheck > /dev/null 2>&1 && \
		cppcheck --enable=all --std=c99 $(ALL_SOURCES) || \
		echo "cppcheck not found, skipping static analysis"

# Format code
format:
	@echo "Formatting code..."
	@which clang-format > /dev/null 2>&1 && \
		clang-format -i $(ALL_SOURCES) || \
		echo "clang-format not found, skipping code formatting"

# Clean build artifacts
clean:
	rm -f $(ALL_OBJECTS) $(LIBRARY) $(TEST_PROGRAM) $(TEST_SOURCES)
	@echo "Build artifacts cleaned"

# Help
help:
	@echo "Desk Manager Build System"
	@echo ""
	@echo "Available targets:"
	@echo "  all       - Build the complete Desk Manager library (default)"
	@echo "  test      - Build and create test program"
	@echo "  debug     - Build debug version with symbols"
	@echo "  release   - Build optimized release version"
	@echo "  docs      - Show documentation summary"
	@echo "  install   - Install library and headers (requires privileges)"
	@echo "  analyze   - Run static analysis (requires cppcheck)"
	@echo "  format    - Format source code (requires clang-format)"
	@echo "  clean     - Remove build artifacts"
	@echo "  help      - Show this help message"
	@echo ""
	@echo "Components built:"
	@echo "  Core:     $(CORE_SOURCES)"
	@echo "  DAs:      $(DA_SOURCES)"
	@echo ""
	@echo "Example usage:"
	@echo "  make                    # Build library"
	@echo "  make test && ./test_deskmanager  # Build and run tests"
	@echo "  make clean debug        # Clean build and build debug version"

# Dependencies
DeskManagerCore.o: DeskManagerCore.c $(HEADERS)
DALoader.o: DALoader.c $(HEADERS)
DAWindows.o: DAWindows.c $(HEADERS)
SystemMenu.o: SystemMenu.c $(HEADERS)
BuiltinDAs.o: BuiltinDAs.c $(HEADERS)
Calculator.o: Calculator.c ../../include/DeskManager/Calculator.h $(HEADERS)
KeyCaps.o: KeyCaps.c ../../include/DeskManager/KeyCaps.h $(HEADERS)
AlarmClock.o: AlarmClock.c ../../include/DeskManager/AlarmClock.h $(HEADERS)
Chooser.o: Chooser.c ../../include/DeskManager/Chooser.h $(HEADERS)