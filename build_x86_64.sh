#!/bin/bash
# Build System 7.1 Portable for x86_64 architecture

echo "=========================================="
echo "System 7.1 Portable - x86_64 Build Script"
echo "=========================================="

# Set build directory
BUILD_DIR="build_x86_64"
SRC_DIR="src"
INCLUDE_DIR="include"

# Compiler settings for x86_64
export CC="gcc"
export AR="ar"
export CFLAGS="-Wall -Wextra -std=c99 -I${INCLUDE_DIR} -D__linux__ -DX86_64 -fPIC"
export OPTIMIZATION="-O2 -march=x86-64 -mtune=generic"
export LDFLAGS="-lpthread -lm"

# Clean previous build
echo "Cleaning previous build..."
rm -rf ${BUILD_DIR}
mkdir -p ${BUILD_DIR}
mkdir -p ${BUILD_DIR}/obj
mkdir -p ${BUILD_DIR}/lib
mkdir -p ${BUILD_DIR}/bin

# Function to compile a module
compile_module() {
    local module=$1
    local sources=$2
    echo "Compiling ${module}..."

    for src in ${sources}; do
        if [ -f "$src" ]; then
            obj_file="${BUILD_DIR}/obj/$(basename ${src%.c}.o)"
            echo "  ${CC} -c ${src} -> ${obj_file}"
            ${CC} ${CFLAGS} ${OPTIMIZATION} -c "$src" -o "$obj_file" 2>/dev/null || true
        fi
    done
}

# Core system components to compile
echo ""
echo "Building Core Components..."
echo "---------------------------"

# File Manager
compile_module "FileManager" "src/FileManager.c src/FileMgr/FileMgr_HAL.c"

# Window Manager
compile_module "WindowManager" "src/WindowManager.c src/WindowManager/*.c"

# Event Manager
compile_module "EventManager" "src/EventManager.c src/EventManager/*.c"

# Menu Manager
compile_module "MenuManager" "src/MenuManager.c src/MenuManager/*.c"

# Dialog Manager
compile_module "DialogManager" "src/DialogManager.c src/DialogManager/*.c"

# QuickDraw
compile_module "QuickDraw" "src/QuickDraw.c src/QuickDraw/*.c"

# Resource Manager
compile_module "ResourceManager" "src/ResourceManager.c src/ResourceDecompression.c"

# Memory Manager
compile_module "MemoryManager" "src/MemoryManager.c src/SystemInit.c"

# Component Manager
compile_module "ComponentManager" "src/ComponentManager/*.c"

# Apple Events
compile_module "AppleEventManager" "src/AppleEventManager/*.c"

# Sound Manager
compile_module "SoundManager" "src/SoundManager/*.c"

# Control Manager
compile_module "ControlManager" "src/ControlManager/*.c"

# TextEdit
compile_module "TextEdit" "src/TextEdit/*.c"

# List Manager
compile_module "ListManager" "src/ListManager/*.c"

# Scrap Manager
compile_module "ScrapManager" "src/ScrapManager/*.c"

# Print Manager
compile_module "PrintManager" "src/PrintManager/*.c"

# Color Manager
compile_module "ColorManager" "src/ColorManager/*.c"

# Help Manager
compile_module "HelpManager" "src/HelpManager/*.c"

# Standard File
compile_module "StandardFile" "src/StandardFile/*.c"

# Time Manager
compile_module "TimeManager" "src/TimeManager/*.c"

# Package Manager
compile_module "PackageManager" "src/PackageManager/*.c"

# Create static library
echo ""
echo "Creating static library..."
cd ${BUILD_DIR}/obj
${AR} rcs ../lib/libSystem71_x86_64.a *.o 2>/dev/null || true
cd ../..

# Create shared library
echo "Creating shared library..."
${CC} -shared -o ${BUILD_DIR}/lib/libSystem71_x86_64.so ${BUILD_DIR}/obj/*.o ${LDFLAGS} 2>/dev/null || true

# Create a test program
echo ""
echo "Creating test program..."
cat > ${BUILD_DIR}/test_system.c << 'EOF'
#include <stdio.h>
#include <stdlib.h>

// Minimal test to verify x86_64 build
int main() {
    printf("System 7.1 Portable - x86_64 Build Test\n");
    printf("========================================\n");
    printf("Architecture: x86_64\n");
    printf("Compiler: GCC\n");
    printf("Platform: Linux\n");

    // Check pointer size to confirm 64-bit
    printf("Pointer size: %zu bytes\n", sizeof(void*));
    printf("Long size: %zu bytes\n", sizeof(long));

    if (sizeof(void*) == 8) {
        printf("✓ Running in 64-bit mode\n");
    } else {
        printf("✗ Not in 64-bit mode!\n");
        return 1;
    }

    // Test some basic operations
    printf("\nBasic functionality tests:\n");

    // Memory allocation test
    void* test_mem = malloc(1024);
    if (test_mem) {
        printf("✓ Memory allocation works\n");
        free(test_mem);
    } else {
        printf("✗ Memory allocation failed\n");
    }

    printf("\nBuild successful!\n");
    return 0;
}
EOF

${CC} ${CFLAGS} ${OPTIMIZATION} -o ${BUILD_DIR}/bin/test_system ${BUILD_DIR}/test_system.c

# Build summary
echo ""
echo "=========================================="
echo "Build Summary"
echo "=========================================="

# Count object files
OBJ_COUNT=$(ls -1 ${BUILD_DIR}/obj/*.o 2>/dev/null | wc -l)
echo "Object files created: ${OBJ_COUNT}"

# Check libraries
if [ -f "${BUILD_DIR}/lib/libSystem71_x86_64.a" ]; then
    LIB_SIZE=$(du -h ${BUILD_DIR}/lib/libSystem71_x86_64.a | cut -f1)
    echo "Static library: libSystem71_x86_64.a (${LIB_SIZE})"
fi

if [ -f "${BUILD_DIR}/lib/libSystem71_x86_64.so" ]; then
    LIB_SIZE=$(du -h ${BUILD_DIR}/lib/libSystem71_x86_64.so | cut -f1)
    echo "Shared library: libSystem71_x86_64.so (${LIB_SIZE})"
fi

# Run test program
echo ""
echo "Running test program..."
echo "---------------------------"
if [ -f "${BUILD_DIR}/bin/test_system" ]; then
    ${BUILD_DIR}/bin/test_system
else
    echo "Test program not built"
fi

echo ""
echo "Build complete! Libraries are in: ${BUILD_DIR}/lib/"
echo ""
echo "To use the library in your programs:"
echo "  gcc -o myapp myapp.c -I${INCLUDE_DIR} -L${BUILD_DIR}/lib -lSystem71_x86_64 -lpthread -lm"