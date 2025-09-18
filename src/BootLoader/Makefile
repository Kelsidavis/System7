# RE-AGENT-BANNER
# Mac OS System 7 Boot Loader Manager Build System
# Reverse engineered from System.rsrc and startup control panels
#
# PROVENANCE: evidence.boot_loader.json->build integration
# PURPOSE: Build boot loader manager components for analysis and verification

# Compiler and tools configuration
CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -g -O0
INCLUDES = -Iinclude -I/System/Library/Frameworks/CoreServices.framework/Frameworks/CarbonCore.framework/Headers
LDFLAGS = -framework CoreServices

# Source and build directories
SRCDIR = src
INCDIR = include
BUILDDIR = build
OBJDIR = $(BUILDDIR)/obj

# Boot loader source files - EVIDENCE: reimplementation components
BOOT_SOURCES = \
	$(SRCDIR)/boot_sequence_manager.c \
	$(SRCDIR)/startup_disk_manager.c \
	$(SRCDIR)/startup_device_manager.c

# Object files
BOOT_OBJECTS = $(BOOT_SOURCES:$(SRCDIR)/%.c=$(OBJDIR)/%.o)

# Main targets
TARGET = $(BUILDDIR)/boot_loader_manager
TEST_TARGET = $(BUILDDIR)/boot_loader_test

# Header dependencies - EVIDENCE: structure definitions
HEADERS = \
	$(INCDIR)/boot_loader.h

# Default target
all: $(TARGET) $(TEST_TARGET)

# Create build directories
$(OBJDIR):
	mkdir -p $(OBJDIR)

# Compile object files
$(OBJDIR)/%.o: $(SRCDIR)/%.c $(HEADERS) | $(OBJDIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Build main boot loader manager library
$(TARGET): $(BOOT_OBJECTS)
	$(CC) $(BOOT_OBJECTS) $(LDFLAGS) -o $@

# Build test executable
$(TEST_TARGET): $(BOOT_OBJECTS) $(OBJDIR)/boot_loader_test.o
	$(CC) $(BOOT_OBJECTS) $(OBJDIR)/boot_loader_test.o $(LDFLAGS) -o $@

# Test file compilation
$(OBJDIR)/boot_loader_test.o: tests/boot_loader_test.c $(HEADERS) | $(OBJDIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Analysis targets - EVIDENCE: verification and compliance
analysis: static-analysis verification compliance-check

static-analysis: $(BOOT_SOURCES) $(HEADERS)
	@echo "Running static analysis on boot loader components..."
	# Run cppcheck for static analysis
	cppcheck --enable=all --std=c99 --suppress=missingIncludeSystem $(INCLUDES) $(BOOT_SOURCES)

verification: $(TARGET)
	@echo "Running verification against original evidence..."
	# Compare function signatures and behavior
	$(BUILDDIR)/verify_evidence.sh

compliance-check: $(BOOT_SOURCES) $(HEADERS)
	@echo "Running compliance audit..."
	# Check for evidence density and proper attribution
	$(BUILDDIR)/compliance_audit.sh

# Documentation generation
docs: $(BOOT_SOURCES) $(HEADERS)
	@echo "Generating documentation from evidence..."
	mkdir -p docs/boot_loader
	# Generate documentation with evidence provenance
	doxygen docs/Doxyfile.boot_loader

# Test execution
test: $(TEST_TARGET)
	@echo "Running boot loader manager tests..."
	$(TEST_TARGET)

# Clean build artifacts
clean:
	rm -rf $(BUILDDIR)

# Debug build with additional information
debug: CFLAGS += -DDEBUG -g3
debug: $(TARGET)

# Evidence verification targets
verify-evidence: evidence.boot_loader.json mappings.boot_loader.json layouts.boot_loader.json
	@echo "Verifying evidence consistency..."
	python3 tools/verify_evidence.py evidence.boot_loader.json

# Coverage analysis
coverage: CFLAGS += --coverage
coverage: LDFLAGS += --coverage
coverage: $(TARGET) test
	gcov $(BOOT_SOURCES)
	lcov --capture --directory . --output-file coverage.info
	genhtml coverage.info --output-directory coverage_report

# Performance profiling
profile: CFLAGS += -pg
profile: LDFLAGS += -pg
profile: $(TARGET)

# Install for system integration testing
install: $(TARGET)
	mkdir -p $(DESTDIR)/usr/local/bin
	cp $(TARGET) $(DESTDIR)/usr/local/bin/boot_loader_manager

# Reverse engineering verification
re-verify: $(TARGET)
	@echo "Verifying implementation against original System.rsrc..."
	# Compare behavior with original evidence
	tools/compare_with_original.sh System.rsrc $(TARGET)

# Help target
help:
	@echo "Mac OS System 7 Boot Loader Manager Build System"
	@echo ""
	@echo "PROVENANCE: evidence.boot_loader.json"
	@echo "EVIDENCE: System.rsrc, Startup Disk.rsrc, Startup Device.rsrc"
	@echo ""
	@echo "Targets:"
	@echo "  all              - Build all components"
	@echo "  test             - Run test suite"
	@echo "  analysis         - Run static analysis"
	@echo "  verification     - Verify against evidence"
	@echo "  compliance-check - Check compliance with evidence"
	@echo "  docs             - Generate documentation"
	@echo "  clean            - Clean build artifacts"
	@echo "  debug            - Build with debug information"
	@echo "  coverage         - Generate code coverage report"
	@echo "  profile          - Build with profiling support"
	@echo "  re-verify        - Verify against original binaries"
	@echo "  help             - Show this help"

# File dependencies
$(OBJDIR)/boot_sequence_manager.o: \
	$(SRCDIR)/boot_sequence_manager.c \
	$(INCDIR)/boot_loader.h \
	evidence.boot_loader.json

$(OBJDIR)/startup_disk_manager.o: \
	$(SRCDIR)/startup_disk_manager.c \
	$(INCDIR)/boot_loader.h \
	evidence.boot_loader.json

$(OBJDIR)/startup_device_manager.o: \
	$(SRCDIR)/startup_device_manager.c \
	$(INCDIR)/boot_loader.h \
	evidence.boot_loader.json

# Evidence files as dependencies
evidence.boot_loader.json: inputs/boot_analysis/system_aflj.json inputs/boot_analysis/system_izzj.json
mappings.boot_loader.json: evidence.boot_loader.json
layouts.boot_loader.json: evidence.boot_loader.json mappings.boot_loader.json

# Phony targets
.PHONY: all clean test analysis verification compliance-check docs debug \
        coverage profile install re-verify help verify-evidence static-analysis

# RE-AGENT-TRAILER-JSON
# {
#   "analysis": {
#     "file": "Makefile.boot_loader",
#     "purpose": "Boot loader manager build system",
#     "evidence_integration": "complete",
#     "verification_targets": 6,
#     "analysis_targets": 4
#   },
#   "provenance": {
#     "evidence_sources": [
#       "evidence.boot_loader.json",
#       "mappings.boot_loader.json",
#       "layouts.boot_loader.json"
#     ],
#     "build_dependencies": "mac_os_frameworks"
#   },
#   "verification": {
#     "static_analysis": "cppcheck",
#     "evidence_verification": "scripted",
#     "compliance_audit": "automated"
#   }
# }