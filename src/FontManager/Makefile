# Makefile for System 7.1 Font Manager
# Portable C implementation of Mac OS 7.1 Font Manager

# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -fPIC -O2 -g
INCLUDES = -I../../include -I../../include/FontManager

# Platform-specific settings
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
    CFLAGS += -D__unix__
    LIBS = -lfreetype -lfontconfig
endif
ifeq ($(UNAME_S),Darwin)
    CFLAGS += -D__APPLE__
    LIBS = -framework CoreText -framework ApplicationServices
endif
ifeq ($(OS),Windows_NT)
    CFLAGS += -D_WIN32
    LIBS = -lgdi32 -luser32
endif

# Source files
SOURCES = FontManagerCore.c \
          BitmapFonts.c \
          TrueTypeFonts.c \
          FontMetrics.c \
          FontCache.c \
          FontSubstitution.c \
          FontPlatform.c

# Object files
OBJECTS = $(SOURCES:.c=.o)

# Target library
TARGET = libFontManager.a
SHARED_TARGET = libFontManager.so

# Default target
all: $(TARGET) $(SHARED_TARGET)

# Static library
$(TARGET): $(OBJECTS)
	ar rcs $@ $^
	ranlib $@

# Shared library
$(SHARED_TARGET): $(OBJECTS)
	$(CC) -shared -o $@ $^ $(LIBS)

# Compile source files
%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Dependencies
FontManagerCore.o: FontManagerCore.c ../../include/FontManager/FontManager.h \
                   ../../include/FontManager/BitmapFonts.h \
                   ../../include/FontManager/TrueTypeFonts.h \
                   ../../include/FontManager/FontMetrics.h

BitmapFonts.o: BitmapFonts.c ../../include/FontManager/BitmapFonts.h \
               ../../include/FontManager/FontManager.h

TrueTypeFonts.o: TrueTypeFonts.c ../../include/FontManager/TrueTypeFonts.h \
                 ../../include/FontManager/FontManager.h

FontMetrics.o: FontMetrics.c ../../include/FontManager/FontMetrics.h \
               ../../include/FontManager/FontManager.h \
               ../../include/FontManager/BitmapFonts.h \
               ../../include/FontManager/TrueTypeFonts.h

FontCache.o: FontCache.c ../../include/FontManager/FontManager.h \
             ../../include/FontManager/BitmapFonts.h \
             ../../include/FontManager/TrueTypeFonts.h

FontSubstitution.o: FontSubstitution.c ../../include/FontManager/FontManager.h \
                    ../../include/FontManager/BitmapFonts.h \
                    ../../include/FontManager/TrueTypeFonts.h

FontPlatform.o: FontPlatform.c ../../include/FontManager/FontManager.h \
                ../../include/FontManager/BitmapFonts.h \
                ../../include/FontManager/TrueTypeFonts.h

# Test programs
test: test_font_manager

test_font_manager: test_font_manager.o $(TARGET)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

test_font_manager.o: test_font_manager.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Install targets
PREFIX = /usr/local
LIBDIR = $(PREFIX)/lib
INCDIR = $(PREFIX)/include/System71

install: $(TARGET) $(SHARED_TARGET)
	mkdir -p $(LIBDIR)
	mkdir -p $(INCDIR)/FontManager
	cp $(TARGET) $(SHARED_TARGET) $(LIBDIR)/
	cp ../../include/FontManager/*.h $(INCDIR)/FontManager/

uninstall:
	rm -f $(LIBDIR)/$(TARGET) $(LIBDIR)/$(SHARED_TARGET)
	rm -rf $(INCDIR)/FontManager

# Documentation
docs:
	doxygen Doxyfile

# Clean targets
clean:
	rm -f $(OBJECTS) $(TARGET) $(SHARED_TARGET)
	rm -f test_font_manager test_font_manager.o

distclean: clean
	rm -f *~ *.bak
	rm -rf docs/

# Debug targets
debug: CFLAGS += -DDEBUG -O0
debug: $(TARGET)

# Profiling
profile: CFLAGS += -pg
profile: $(TARGET)

# Memory checking
valgrind: test_font_manager
	valgrind --leak-check=full --show-leak-kinds=all ./test_font_manager

# Code analysis
analyze:
	cppcheck --enable=all --inconclusive --std=c99 $(SOURCES)

# Format code
format:
	clang-format -i $(SOURCES) *.h

.PHONY: all clean distclean install uninstall test docs debug profile valgrind analyze format