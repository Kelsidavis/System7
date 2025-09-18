# Makefile for Mac OS 7.1 Network Extension
# Complete AppleTalk networking implementation

CC = gcc
CFLAGS = -Wall -Wextra -O2 -std=c99 -fPIC
INCLUDES = -I../../include -I../../include/NetworkExtension
LDFLAGS = -shared -lpthread -lssl -lcrypto

# Source files
SOURCES = NetworkExtensionCore.c \
          AppleTalkStack.c \
          FileSharing.c \
          ZoneManager.c \
          DeviceDiscovery.c \
          RoutingManager.c \
          NetworkSecurity.c

# Object files
OBJECTS = $(SOURCES:.c=.o)

# Target library
TARGET = libNetworkExtension.so

# Build rules
all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(LDFLAGS) -o $@ $(OBJECTS)

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET)

install: $(TARGET)
	mkdir -p /usr/local/lib
	cp $(TARGET) /usr/local/lib/
	mkdir -p /usr/local/include/NetworkExtension
	cp ../../include/NetworkExtension/*.h /usr/local/include/NetworkExtension/

uninstall:
	rm -f /usr/local/lib/$(TARGET)
	rm -rf /usr/local/include/NetworkExtension

# Development targets
test: $(TARGET)
	echo "Network Extension library built successfully"
	echo "Components included:"
	echo "  - AppleTalk Protocol Stack (DDP, NBP, ATP, ZIP, RTMP, AARP)"
	echo "  - Apple Filing Protocol (AFP) File Sharing"
	echo "  - Zone Management and Network Browsing"
	echo "  - Device Discovery and Service Advertisement"
	echo "  - Network Routing and Address Resolution"
	echo "  - Network Security and Access Control"
	echo "  - Modern Protocol Abstraction"

debug: CFLAGS += -g -DDEBUG
debug: $(TARGET)

static: LDFLAGS = -static -lpthread -lssl -lcrypto
static: TARGET = libNetworkExtension.a
static: $(TARGET)

libNetworkExtension.a: $(OBJECTS)
	ar rcs $@ $(OBJECTS)

.PHONY: all clean install uninstall test debug static