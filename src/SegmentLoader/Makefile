#
# Makefile for System 7.1 Portable Segment Loader
#
# This makefile builds the complete Segment Loader implementation,
# which is the critical component for launching Mac applications.
#

# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -O2 -g
INCLUDES = -I../../include -I../../include/SegmentLoader
DEFINES = -DSYSTEM71_PORTABLE -DDEBUG_BUILD

# Library paths
LIBDIR = ../../lib
LIBS = -lsystem7_memory -lsystem7_resource -lsystem7_file -lsystem7_debug

# Source files
SOURCES = \
	SegmentLoaderCore.c \
	ApplicationLauncher.c \
	CodePatching.c \
	AppHeapManager.c \
	FinderInterface.c \
	SegmentCache.c \
	ApplicationSwitcher.c

# Object files
OBJECTS = $(SOURCES:.c=.o)

# Target library
TARGET = libsystem7_segmentloader.a

# Build rules
all: $(TARGET)

$(TARGET): $(OBJECTS)
	@echo "Creating Segment Loader library..."
	ar rcs $(TARGET) $(OBJECTS)
	@echo "Segment Loader library created: $(TARGET)"

%.o: %.c
	@echo "Compiling $<..."
	$(CC) $(CFLAGS) $(INCLUDES) $(DEFINES) -c $< -o $@

# Individual component targets
segmentloader: SegmentLoaderCore.o
	@echo "Built Segment Loader Core"

applauncher: ApplicationLauncher.o
	@echo "Built Application Launcher"

codepatching: CodePatching.o
	@echo "Built Code Patching"

heapmanager: AppHeapManager.o
	@echo "Built App Heap Manager"

finderinterface: FinderInterface.o
	@echo "Built Finder Interface"

segmentcache: SegmentCache.o
	@echo "Built Segment Cache"

appswitcher: ApplicationSwitcher.o
	@echo "Built Application Switcher"

# Test targets
test: $(TARGET)
	@echo "Building Segment Loader tests..."
	$(CC) $(CFLAGS) $(INCLUDES) $(DEFINES) -DTEST_BUILD \
		-o test_segmentloader test_segmentloader.c $(TARGET) $(LIBS)
	@echo "Running Segment Loader tests..."
	./test_segmentloader

# Installation
install: $(TARGET)
	@echo "Installing Segment Loader library..."
	mkdir -p $(LIBDIR)
	cp $(TARGET) $(LIBDIR)/
	@echo "Segment Loader library installed to $(LIBDIR)"

# Clean up
clean:
	@echo "Cleaning Segment Loader build files..."
	rm -f $(OBJECTS) $(TARGET) test_segmentloader
	@echo "Clean completed"

# Dependencies
SegmentLoaderCore.o: SegmentLoaderCore.c ../../include/SegmentLoader/SegmentLoader.h
ApplicationLauncher.o: ApplicationLauncher.c ../../include/SegmentLoader/SegmentLoader.h
CodePatching.o: CodePatching.c ../../include/SegmentLoader/CodePatching.h
AppHeapManager.o: AppHeapManager.c ../../include/SegmentLoader/AppHeapManager.h
FinderInterface.o: FinderInterface.c ../../include/SegmentLoader/FinderInterface.h
SegmentCache.o: SegmentCache.c ../../include/SegmentLoader/SegmentLoader.h
ApplicationSwitcher.o: ApplicationSwitcher.c ../../include/SegmentLoader/SegmentLoader.h

# Debug targets
debug: CFLAGS += -DDEBUG -O0
debug: $(TARGET)

# Release target
release: CFLAGS += -DNDEBUG -O3
release: clean $(TARGET)

# Documentation
docs:
	@echo "Generating Segment Loader documentation..."
	doxygen Doxyfile

# Help
help:
	@echo "System 7.1 Portable Segment Loader Build System"
	@echo ""
	@echo "Targets:"
	@echo "  all          - Build complete Segment Loader library"
	@echo "  segmentloader - Build core segment loader"
	@echo "  applauncher  - Build application launcher"
	@echo "  codepatching - Build code patching system"
	@echo "  heapmanager  - Build heap and A5 world manager"
	@echo "  finderinterface - Build Finder interface"
	@echo "  segmentcache - Build segment cache"
	@echo "  appswitcher  - Build application switcher"
	@echo "  test         - Build and run tests"
	@echo "  install      - Install library"
	@echo "  clean        - Clean build files"
	@echo "  debug        - Build debug version"
	@echo "  release      - Build release version"
	@echo "  docs         - Generate documentation"
	@echo "  help         - Show this help"
	@echo ""
	@echo "The Segment Loader is THE CRITICAL COMPONENT for running Mac applications."
	@echo "Without it, System 7.1 Portable cannot launch or execute Mac software."

.PHONY: all test install clean debug release docs help segmentloader applauncher codepatching heapmanager finderinterface segmentcache appswitcher