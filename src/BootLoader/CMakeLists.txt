# System 7.1 Boot Loader - Cross-Platform Build System
# CMake configuration for x86_64 and ARM64 targets

cmake_minimum_required(VERSION 3.16)
project(System71BootLoader VERSION 1.0.0 LANGUAGES C)

# ============================================================================
# Build Configuration
# ============================================================================

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Build type configuration
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
endif()

# ============================================================================
# Platform Detection
# ============================================================================

if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
    set(BOOT_PLATFORM "x86_64")
    add_definitions(-DPLATFORM_X86_64=1)
    message(STATUS "Building for x86_64 platform")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
    set(BOOT_PLATFORM "arm64")
    add_definitions(-DPLATFORM_ARM64=1)
    message(STATUS "Building for ARM64 platform")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "riscv64")
    set(BOOT_PLATFORM "riscv")
    add_definitions(-DPLATFORM_RISCV=1)
    message(STATUS "Building for RISC-V platform")
else()
    message(FATAL_ERROR "Unsupported platform: ${CMAKE_SYSTEM_PROCESSOR}")
endif()

# ============================================================================
# Compiler Options
# ============================================================================

# Common compiler flags
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wno-unused-parameter")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-strict-aliasing")

# Debug build flags
set(CMAKE_C_FLAGS_DEBUG "-g -O0 -DDEBUG=1")

# Release build flags
set(CMAKE_C_FLAGS_RELEASE "-O2 -DNDEBUG")

# Platform-specific compiler flags
if(BOOT_PLATFORM STREQUAL "x86_64")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=x86-64 -mtune=generic")
elseif(BOOT_PLATFORM STREQUAL "arm64")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=armv8-a")
endif()

# ============================================================================
# Include Directories
# ============================================================================

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    ${CMAKE_CURRENT_SOURCE_DIR}/hal
    ${CMAKE_CURRENT_SOURCE_DIR}/storage
)

# ============================================================================
# Source Files
# ============================================================================

# Core boot loader sources
set(CORE_SOURCES
    boot_sequence_manager.c
    modern_boot_sequence.c
    hal/boot_hal.c
)

# Platform-specific sources
if(BOOT_PLATFORM STREQUAL "x86_64")
    set(PLATFORM_SOURCES
        platforms/x86_64/x86_64_boot.c
    )
elseif(BOOT_PLATFORM STREQUAL "arm64")
    set(PLATFORM_SOURCES
        platforms/arm64/arm64_boot.c
    )
endif()

# Storage subsystem sources
set(STORAGE_SOURCES
    # storage/modern_storage.c  # Will be implemented
)

# Test sources
set(TEST_SOURCES
    boot_test.c
    boot_validation_test.c
    modern_boot_test.c
)

# ============================================================================
# Targets
# ============================================================================

# Core boot loader library
add_library(bootloader_core STATIC
    ${CORE_SOURCES}
    ${PLATFORM_SOURCES}
    ${STORAGE_SOURCES}
)

target_compile_definitions(bootloader_core PRIVATE
    SYSTEM71_BOOTLOADER=1
    BOOT_PLATFORM_${BOOT_PLATFORM}=1
)

# Basic test executable
add_executable(boot_test
    ${TEST_SOURCES}
)

target_link_libraries(boot_test bootloader_core)

# Validation test executable
add_executable(boot_validation_test
    boot_validation_test.c
)

target_link_libraries(boot_validation_test bootloader_core)

# Modern boot test executable
add_executable(modern_boot_test
    modern_boot_test.c
)

target_link_libraries(modern_boot_test bootloader_core)

# HAL test executable
add_executable(hal_test
    hal/boot_hal.c
)

target_compile_definitions(hal_test PRIVATE
    HAL_STANDALONE_TEST=1
)

# ============================================================================
# Feature Detection
# ============================================================================

include(CheckIncludeFile)
include(CheckFunctionExists)
include(CheckSymbolExists)

# Check for platform-specific headers
if(BOOT_PLATFORM STREQUAL "x86_64")
    check_include_file("sys/io.h" HAVE_SYS_IO_H)
    check_include_file("cpuid.h" HAVE_CPUID_H)
elseif(BOOT_PLATFORM STREQUAL "arm64")
    check_include_file("sys/auxv.h" HAVE_SYS_AUXV_H)
endif()

# Check for UEFI support
if(EXISTS "/sys/firmware/efi")
    set(HAVE_UEFI 1)
    add_definitions(-DHAVE_UEFI=1)
endif()

# Check for device tree support
if(EXISTS "/proc/device-tree" OR EXISTS "/sys/firmware/devicetree")
    set(HAVE_DEVICETREE 1)
    add_definitions(-DHAVE_DEVICETREE=1)
endif()

# Check for ACPI support
if(EXISTS "/sys/firmware/acpi" OR EXISTS "/proc/acpi")
    set(HAVE_ACPI 1)
    add_definitions(-DHAVE_ACPI=1)
endif()

# ============================================================================
# Dependencies
# ============================================================================

# Find required system libraries
find_library(PTHREAD_LIB pthread)
if(PTHREAD_LIB)
    target_link_libraries(bootloader_core ${PTHREAD_LIB})
endif()

# Platform-specific dependencies
if(BOOT_PLATFORM STREQUAL "x86_64")
    # Check for UEFI development libraries
    find_path(EFI_INCLUDE_DIR efi/efi.h)
    if(EFI_INCLUDE_DIR)
        include_directories(${EFI_INCLUDE_DIR})
        add_definitions(-DHAVE_EFI_HEADERS=1)
    endif()
endif()

# ============================================================================
# Installation
# ============================================================================

install(TARGETS bootloader_core
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)

install(TARGETS boot_test boot_validation_test hal_test
    RUNTIME DESTINATION bin
)

install(FILES
    hal/boot_hal.h
    storage/modern_storage.h
    DESTINATION include/system71/bootloader
)

# ============================================================================
# Testing
# ============================================================================

enable_testing()

# Basic functionality test
add_test(NAME boot_functionality_test
    COMMAND boot_test
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

# Comprehensive validation test
add_test(NAME boot_validation_test
    COMMAND boot_validation_test
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

# HAL unit test
add_test(NAME hal_unit_test
    COMMAND hal_test
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

# Platform-specific tests
if(BOOT_PLATFORM STREQUAL "x86_64")
    add_test(NAME x86_64_platform_test
        COMMAND boot_test --platform-test
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )
elseif(BOOT_PLATFORM STREQUAL "arm64")
    add_test(NAME arm64_platform_test
        COMMAND boot_test --platform-test
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )
endif()

# ============================================================================
# Custom Targets
# ============================================================================

# Documentation generation
find_program(DOXYGEN_EXECUTABLE doxygen)
if(DOXYGEN_EXECUTABLE)
    add_custom_target(docs
        COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Generating documentation with Doxygen"
        VERBATIM
    )
endif()

# Static analysis
find_program(CPPCHECK_EXECUTABLE cppcheck)
if(CPPCHECK_EXECUTABLE)
    add_custom_target(static-analysis
        COMMAND ${CPPCHECK_EXECUTABLE}
        --enable=all
        --std=c99
        --suppress=missingIncludeSystem
        --quiet
        ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Running static analysis with cppcheck"
        VERBATIM
    )
endif()

# Code formatting
find_program(CLANG_FORMAT_EXECUTABLE clang-format)
if(CLANG_FORMAT_EXECUTABLE)
    file(GLOB_RECURSE ALL_SOURCE_FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/*.c
        ${CMAKE_CURRENT_SOURCE_DIR}/*.h
    )

    add_custom_target(format
        COMMAND ${CLANG_FORMAT_EXECUTABLE}
        -i
        ${ALL_SOURCE_FILES}
        COMMENT "Formatting code with clang-format"
        VERBATIM
    )
endif()

# Cross-compilation support
add_custom_target(cross-compile-x86_64
    COMMAND ${CMAKE_COMMAND}
    -DCMAKE_SYSTEM_PROCESSOR=x86_64
    -DCMAKE_C_COMPILER=gcc
    -S ${CMAKE_CURRENT_SOURCE_DIR}
    -B ${CMAKE_CURRENT_BINARY_DIR}/build-x86_64
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_CURRENT_BINARY_DIR}/build-x86_64
    COMMENT "Cross-compiling for x86_64"
    VERBATIM
)

add_custom_target(cross-compile-arm64
    COMMAND ${CMAKE_COMMAND}
    -DCMAKE_SYSTEM_PROCESSOR=aarch64
    -DCMAKE_C_COMPILER=aarch64-linux-gnu-gcc
    -S ${CMAKE_CURRENT_SOURCE_DIR}
    -B ${CMAKE_CURRENT_BINARY_DIR}/build-arm64
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_CURRENT_BINARY_DIR}/build-arm64
    COMMENT "Cross-compiling for ARM64"
    VERBATIM
)

# ============================================================================
# Packaging
# ============================================================================

set(CPACK_PACKAGE_NAME "system71-bootloader")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "System 7.1 Boot Loader for Modern Targets")
set(CPACK_PACKAGE_VENDOR "System 7.1 Portable Project")
set(CPACK_PACKAGE_CONTACT "system71-portable@example.com")

set(CPACK_GENERATOR "TGZ")
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    list(APPEND CPACK_GENERATOR "DEB" "RPM")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    list(APPEND CPACK_GENERATOR "productbuild")
endif()

include(CPack)

# ============================================================================
# Summary
# ============================================================================

message(STATUS "")
message(STATUS "System 7.1 Boot Loader Configuration Summary:")
message(STATUS "  Platform: ${BOOT_PLATFORM}")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C Compiler: ${CMAKE_C_COMPILER}")
message(STATUS "  C Flags: ${CMAKE_C_FLAGS}")
message(STATUS "")
message(STATUS "Platform Features:")
message(STATUS "  UEFI Support: ${HAVE_UEFI}")
message(STATUS "  ACPI Support: ${HAVE_ACPI}")
message(STATUS "  Device Tree Support: ${HAVE_DEVICETREE}")
message(STATUS "")
message(STATUS "Build Targets:")
message(STATUS "  bootloader_core - Core boot loader library")
message(STATUS "  boot_test - Basic functionality test")
message(STATUS "  boot_validation_test - Comprehensive validation")
message(STATUS "  hal_test - HAL unit tests")
message(STATUS "")
message(STATUS "Custom Targets:")
message(STATUS "  make docs - Generate documentation")
message(STATUS "  make static-analysis - Run static analysis")
message(STATUS "  make format - Format source code")
message(STATUS "  make test - Run all tests")
message(STATUS "")