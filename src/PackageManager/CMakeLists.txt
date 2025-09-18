# PackageManager CMake configuration
# Copyright (c) 2024 System7.1-Portable Project

set(PACKAGEMANAGER_SOURCES
    PackageManager.c
    PackageManager_HAL.c
)

set(PACKAGEMANAGER_HEADERS
    ${CMAKE_SOURCE_DIR}/include/PackageManager/PackageManager.h
    ${CMAKE_SOURCE_DIR}/include/PackageManager/PackageManager_HAL.h
)

# Create PackageManager library
add_library(PackageManager STATIC
    ${PACKAGEMANAGER_SOURCES}
    ${PACKAGEMANAGER_HEADERS}
)

# Include directories
target_include_directories(PackageManager PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/PackageManager
)

# Link dependencies
target_link_libraries(PackageManager
    PUBLIC
        Types
        Memory
    PRIVATE
        ${CMAKE_THREAD_LIBS_INIT}
        m  # Math library
)

# Platform-specific configurations
if(APPLE)
    find_library(APPKIT AppKit)
    find_library(CORE_FOUNDATION CoreFoundation)
    target_link_libraries(PackageManager PRIVATE
        ${APPKIT}
        ${CORE_FOUNDATION}
    )
elseif(UNIX AND NOT APPLE)
    # Find GTK for Linux
    find_package(PkgConfig)
    if(PkgConfig_FOUND)
        pkg_check_modules(GTK3 gtk+-3.0)
        if(GTK3_FOUND)
            target_include_directories(PackageManager PRIVATE ${GTK3_INCLUDE_DIRS})
            target_link_libraries(PackageManager PRIVATE ${GTK3_LIBRARIES})
        endif()
    endif()
elseif(WIN32)
    target_link_libraries(PackageManager PRIVATE
        comdlg32
        user32
    )
endif()

# Export target
set_target_properties(PackageManager PROPERTIES
    PUBLIC_HEADER "${PACKAGEMANAGER_HEADERS}"
)

# Installation rules
install(TARGETS PackageManager
    ARCHIVE DESTINATION lib
    PUBLIC_HEADER DESTINATION include/PackageManager
)