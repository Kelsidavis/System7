# CMakeLists.txt for System 7.1 Package Manager
# Builds the complete Mac OS Package Manager implementation

cmake_minimum_required(VERSION 3.16)

# Package Manager Library
set(PACKAGE_MANAGER_SOURCES
    PackageManagerCore.c
    SANEPackage.c
    StringPackage.c
    StandardFilePackage.c
    ListManagerPackage.c
    PackageDispatch.c
)

set(PACKAGE_MANAGER_HEADERS
    ${CMAKE_SOURCE_DIR}/include/PackageManager/PackageManager.h
    ${CMAKE_SOURCE_DIR}/include/PackageManager/PackageTypes.h
    ${CMAKE_SOURCE_DIR}/include/PackageManager/SANEPackage.h
    ${CMAKE_SOURCE_DIR}/include/PackageManager/StringPackage.h
    ${CMAKE_SOURCE_DIR}/include/PackageManager/StandardFilePackage.h
    ${CMAKE_SOURCE_DIR}/include/PackageManager/ListManagerPackage.h
)

# Create Package Manager library
add_library(PackageManager STATIC ${PACKAGE_MANAGER_SOURCES})

# Include directories
target_include_directories(PackageManager PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/PackageManager
)

# Compiler flags for Package Manager
target_compile_options(PackageManager PRIVATE
    -Wall -Wextra -Wpedantic
    -fno-strict-aliasing
    -D_GNU_SOURCE
)

# Link math library for SANE package
target_link_libraries(PackageManager PUBLIC m pthread)

# Set properties
set_target_properties(PackageManager PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED ON
    POSITION_INDEPENDENT_CODE ON
)

# Install headers
install(FILES ${PACKAGE_MANAGER_HEADERS}
    DESTINATION include/PackageManager
)

# Install library
install(TARGETS PackageManager
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

# Create package config file
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/PackageManagerConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/PackageManagerConfig.cmake
    @ONLY
)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/PackageManagerConfig.cmake
    DESTINATION lib/cmake/PackageManager
)