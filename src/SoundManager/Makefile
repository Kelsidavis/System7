# Makefile for Sound Manager - Mac OS 7.1 Portable Implementation
#
# Builds complete Sound Manager library with all components:
# - Core Sound Manager functionality
# - Audio synthesis (square wave, sampled, wave table, MIDI)
# - Multi-channel mixing with effects
# - Audio recording and input processing
# - Hardware abstraction layer
# - Sound resource management
#
# Copyright (c) 2025 - System 7.1 Portable Project

CC = gcc
AR = ar
CFLAGS = -Wall -Wextra -O2 -fPIC -std=c99
CPPFLAGS = -I../../include -I../../include/SoundManager
LDFLAGS = -shared
LIBS = -lm -lpthread

# Platform-specific audio libraries
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
    LIBS += -lasound -lpulse-simple -lpulse
    CPPFLAGS += -DUSE_ALSA -DUSE_PULSE
endif
ifeq ($(UNAME_S),Darwin)
    LIBS += -framework CoreAudio -framework AudioUnit -framework AudioToolbox
    CPPFLAGS += -DUSE_COREAUDIO
endif

# Source files
SOURCES = \
	SoundManagerCore.c \
	SoundSynthesis.c \
	SoundMixing.c \
	SoundInput.c \
	SoundHardware.c \
	SoundResources.c

# Object files
OBJECTS = $(SOURCES:.c=.o)

# Target library
TARGET = libSoundManager.a
SHARED_TARGET = libSoundManager.so

# Default target
all: $(TARGET) $(SHARED_TARGET)

# Static library
$(TARGET): $(OBJECTS)
	$(AR) rcs $@ $(OBJECTS)
	@echo "Built Sound Manager static library: $(TARGET)"

# Shared library
$(SHARED_TARGET): $(OBJECTS)
	$(CC) $(LDFLAGS) -o $@ $(OBJECTS) $(LIBS)
	@echo "Built Sound Manager shared library: $(SHARED_TARGET)"

# Object files
%.o: %.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

# Clean
clean:
	rm -f $(OBJECTS) $(TARGET) $(SHARED_TARGET)
	@echo "Cleaned Sound Manager build files"

# Install (copy to system directories)
install: $(TARGET) $(SHARED_TARGET)
	cp $(TARGET) ../../lib/
	cp $(SHARED_TARGET) ../../lib/
	cp ../../include/SoundManager/*.h ../../include/
	@echo "Installed Sound Manager library and headers"

# Test build (requires test framework)
test: $(TARGET)
	@echo "Building Sound Manager tests..."
	$(CC) $(CFLAGS) $(CPPFLAGS) -o test_soundmanager \
		test_soundmanager.c $(TARGET) $(LIBS)
	@echo "Built Sound Manager tests"

# Debug build
debug: CFLAGS += -g -DDEBUG -O0
debug: $(TARGET)

# Performance optimized build
release: CFLAGS += -O3 -DNDEBUG -march=native
release: $(TARGET)

# Dependencies
SoundManagerCore.o: SoundManagerCore.c ../../include/SoundManager/SoundManager.h \
	../../include/SoundManager/SoundTypes.h ../../include/SoundManager/SoundSynthesis.h \
	../../include/SoundManager/SoundHardware.h

SoundSynthesis.o: SoundSynthesis.c ../../include/SoundManager/SoundSynthesis.h \
	../../include/SoundManager/SoundTypes.h

SoundMixing.o: SoundMixing.c ../../include/SoundManager/SoundSynthesis.h \
	../../include/SoundManager/SoundTypes.h

SoundInput.o: SoundInput.c ../../include/SoundManager/SoundManager.h \
	../../include/SoundManager/SoundHardware.h

SoundHardware.o: SoundHardware.c ../../include/SoundManager/SoundHardware.h \
	../../include/SoundManager/SoundTypes.h

SoundResources.o: SoundResources.c ../../include/SoundManager/SoundManager.h \
	../../include/SoundManager/SoundTypes.h

.PHONY: all clean install test debug release