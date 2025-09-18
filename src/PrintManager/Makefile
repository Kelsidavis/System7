# Makefile for Print Manager - System 7.1 Portable
#
# Complete Print Manager implementation with modern printing support
# Compatible with Mac OS System 7.1 Print Manager API

# Compiler settings
CC = gcc
CFLAGS = -Wall -Wextra -O2 -fPIC -std=c99
INCLUDES = -I../../include -I../../include/PrintManager -I../../include/System

# Platform-specific settings
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
    PLATFORM_CFLAGS = -DPLATFORM_MACOS
    LIBS = -framework ApplicationServices -framework CoreFoundation
endif
ifeq ($(UNAME_S),Linux)
    PLATFORM_CFLAGS = -DPLATFORM_LINUX
    LIBS = -lcups
endif
ifeq ($(OS),Windows_NT)
    PLATFORM_CFLAGS = -DPLATFORM_WINDOWS
    LIBS = -lwinspool -lcomdlg32 -lgdi32
endif

# Source files
SOURCES = \
    PrintManagerCore.c \
    PrintDialogs.c \
    PrintDrivers.c \
    PrintDocument.c \
    PageLayout.c \
    PrintSpooling.c \
    PrintPreview.c \
    PrintResources.c \
    BackgroundPrinting.c

# Object files
OBJECTS = $(SOURCES:.c=.o)

# Target library
TARGET_LIB = libPrintManager.a
TARGET_SO = libPrintManager.so

# Build rules
all: $(TARGET_LIB) $(TARGET_SO)

$(TARGET_LIB): $(OBJECTS)
	ar rcs $@ $^
	ranlib $@

$(TARGET_SO): $(OBJECTS)
	$(CC) -shared -o $@ $^ $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) $(PLATFORM_CFLAGS) $(INCLUDES) -c $< -o $@

# Dependencies
PrintManagerCore.o: PrintManagerCore.c ../../include/PrintManager/PrintManager.h ../../include/PrintManager/PrintTypes.h
PrintDialogs.o: PrintDialogs.c ../../include/PrintManager/PrintDialogs.h ../../include/PrintManager/PrintManager.h
PrintDrivers.o: PrintDrivers.c ../../include/PrintManager/PrintDrivers.h ../../include/PrintManager/PrintManager.h
PrintDocument.o: PrintDocument.c ../../include/PrintManager/PrintManager.h ../../include/PrintManager/PrintDrivers.h
PageLayout.o: PageLayout.c ../../include/PrintManager/PageLayout.h ../../include/PrintManager/PrintManager.h
PrintSpooling.o: PrintSpooling.c ../../include/PrintManager/PrintSpooling.h ../../include/PrintManager/PrintManager.h
PrintPreview.o: PrintPreview.c ../../include/PrintManager/PrintPreview.h ../../include/PrintManager/PrintManager.h
PrintResources.o: PrintResources.c ../../include/PrintManager/PrintResources.h ../../include/PrintManager/PrintManager.h
BackgroundPrinting.o: BackgroundPrinting.c ../../include/PrintManager/BackgroundPrinting.h ../../include/PrintManager/PrintManager.h

# Installation
PREFIX ?= /usr/local
LIBDIR = $(PREFIX)/lib
INCDIR = $(PREFIX)/include/PrintManager

install: $(TARGET_LIB) $(TARGET_SO)
	mkdir -p $(LIBDIR)
	mkdir -p $(INCDIR)
	cp $(TARGET_LIB) $(LIBDIR)/
	cp $(TARGET_SO) $(LIBDIR)/
	cp ../../include/PrintManager/*.h $(INCDIR)/

# Test program
test: test_printmanager

test_printmanager: test_printmanager.c $(TARGET_LIB)
	$(CC) $(CFLAGS) $(PLATFORM_CFLAGS) $(INCLUDES) -o $@ $< -L. -lPrintManager $(LIBS)

# Clean
clean:
	rm -f $(OBJECTS) $(TARGET_LIB) $(TARGET_SO) test_printmanager

# Documentation
docs:
	@echo "Print Manager Documentation"
	@echo "=========================="
	@echo ""
	@echo "Core API Functions:"
	@echo "  PrOpen()          - Initialize Print Manager"
	@echo "  PrClose()         - Cleanup Print Manager"
	@echo "  PrintDefault()    - Setup default print record"
	@echo "  PrValidate()      - Validate print record"
	@echo "  PrError()         - Get current error"
	@echo ""
	@echo "Dialog Functions:"
	@echo "  PrStlDialog()     - Show page setup dialog"
	@echo "  PrJobDialog()     - Show print dialog"
	@echo ""
	@echo "Document Functions:"
	@echo "  PrOpenDoc()       - Open print document"
	@echo "  PrCloseDoc()      - Close print document"
	@echo "  PrOpenPage()      - Open print page"
	@echo "  PrClosePage()     - Close print page"
	@echo "  PrPicFile()       - Print picture file"
	@echo ""
	@echo "Modern Extensions:"
	@echo "  InitPrintManager()     - Modern initialization"
	@echo "  GetPrinterList()       - Get available printers"
	@echo "  SetCurrentPrinter()    - Set active printer"
	@echo "  ShowPrintPreview()     - Show print preview"
	@echo ""

# Help
help:
	@echo "Print Manager Build System"
	@echo "========================="
	@echo ""
	@echo "Targets:"
	@echo "  all       - Build static and shared libraries"
	@echo "  install   - Install libraries and headers"
	@echo "  test      - Build test program"
	@echo "  clean     - Remove build files"
	@echo "  docs      - Show API documentation"
	@echo "  help      - Show this help"
	@echo ""
	@echo "Platform Support:"
	@echo "  macOS     - Native Cocoa printing"
	@echo "  Linux     - CUPS printing system"
	@echo "  Windows   - Windows Print Spooler"
	@echo ""

.PHONY: all install test clean docs help