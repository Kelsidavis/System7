# ColorManager CMake configuration
# Copyright (c) 2024 System7 Project

set(COLORMANAGER_SOURCES
    ColorManager.c
    ColorManager_HAL.c
)

set(COLORMANAGER_HEADERS
    ${CMAKE_SOURCE_DIR}/include/ColorManager/ColorTypes.h
    ${CMAKE_SOURCE_DIR}/include/ColorManager/ColorManager_HAL.h
    ${CMAKE_SOURCE_DIR}/include/ColorManager/ColorManager.h
)

# Create ColorManager library
add_library(ColorManager STATIC
    ${COLORMANAGER_SOURCES}
    ${COLORMANAGER_HEADERS}
)

# Include directories
target_include_directories(ColorManager PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/ColorManager
)

# Link dependencies
target_link_libraries(ColorManager
    PUBLIC
        Types
        MemoryManager
        QuickDraw
    PRIVATE
        ${CMAKE_THREAD_LIBS_INIT}
        m  # Math library for color calculations
)

# Platform-specific configurations
if(APPLE)
    target_link_libraries(ColorManager PRIVATE
        "-framework CoreGraphics"
        "-framework ApplicationServices"
    )
elseif(UNIX AND NOT APPLE)
    # Find X11 for Linux
    find_package(X11 REQUIRED)
    if(X11_FOUND)
        target_include_directories(ColorManager PRIVATE ${X11_INCLUDE_DIR})
        target_link_libraries(ColorManager PRIVATE ${X11_LIBRARIES})
    endif()
elseif(WIN32)
    target_link_libraries(ColorManager PRIVATE
        gdi32
        user32
    )
endif()

# Export target
set_target_properties(ColorManager PROPERTIES
    PUBLIC_HEADER "${COLORMANAGER_HEADERS}"
)

# Installation rules
install(TARGETS ColorManager
    ARCHIVE DESTINATION lib
    PUBLIC_HEADER DESTINATION include/ColorManager
)