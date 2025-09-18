# Makefile for Component Manager - System 7.1 Portable Implementation

# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -fPIC -O2
CPPFLAGS = -I../../include -I../../include/ComponentManager

# Platform-specific settings
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
    CFLAGS += -DLINUX
    LDFLAGS += -ldl -lpthread
    SHARED_EXT = .so
endif
ifeq ($(UNAME_S),Darwin)
    CFLAGS += -DMACOS
    LDFLAGS += -framework CoreFoundation -framework Security
    SHARED_EXT = .dylib
endif
ifeq ($(OS),Windows_NT)
    CFLAGS += -DWINDOWS
    LDFLAGS += -lkernel32 -luser32 -lcrypt32
    SHARED_EXT = .dll
endif

# Optional features
ifdef ENABLE_SECURITY
    CFLAGS += -DCOMPONENTMGR_SECURITY_ENABLED
    LDFLAGS += -lssl -lcrypto
endif

ifdef ENABLE_NETWORK
    CFLAGS += -DCOMPONENTMGR_NETWORK_ENABLED
endif

# Source files
CORE_SOURCES = \
    ComponentManagerCore.c \
    ComponentRegistry.c \
    ComponentInstances.c \
    ComponentLoader.c \
    ComponentDispatch.c \
    ComponentResources.c \
    ComponentNegotiation.c \
    ComponentSecurity.c

# Object files
CORE_OBJECTS = $(CORE_SOURCES:.c=.o)

# Targets
all: libcomponentmanager.a libcomponentmanager$(SHARED_EXT)

# Static library
libcomponentmanager.a: $(CORE_OBJECTS)
	ar rcs $@ $^

# Shared library
libcomponentmanager$(SHARED_EXT): $(CORE_OBJECTS)
	$(CC) -shared -o $@ $^ $(LDFLAGS)

# Object files
%.o: %.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

# Clean
clean:
	rm -f $(CORE_OBJECTS) libcomponentmanager.a libcomponentmanager$(SHARED_EXT)

# Install
install: all
	mkdir -p /usr/local/lib
	mkdir -p /usr/local/include/ComponentManager
	cp libcomponentmanager.a /usr/local/lib/
	cp libcomponentmanager$(SHARED_EXT) /usr/local/lib/
	cp ../../include/ComponentManager/*.h /usr/local/include/ComponentManager/

# Dependencies
ComponentManagerCore.o: ComponentManagerCore.c ../../include/ComponentManager/ComponentManager.h
ComponentRegistry.o: ComponentRegistry.c ../../include/ComponentManager/ComponentRegistry.h
ComponentInstances.o: ComponentInstances.c ../../include/ComponentManager/ComponentInstances.h
ComponentLoader.o: ComponentLoader.c ../../include/ComponentManager/ComponentLoader.h
ComponentDispatch.o: ComponentDispatch.c ../../include/ComponentManager/ComponentDispatch.h
ComponentResources.o: ComponentResources.c ../../include/ComponentManager/ComponentResources.h
ComponentNegotiation.o: ComponentNegotiation.c ../../include/ComponentManager/ComponentNegotiation.h
ComponentSecurity.o: ComponentSecurity.c ../../include/ComponentManager/ComponentSecurity.h

.PHONY: all clean install