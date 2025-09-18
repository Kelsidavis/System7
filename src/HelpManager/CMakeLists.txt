# HelpManager CMake configuration
# Copyright (c) 2024 System7 Project

set(HELPMANAGER_SOURCES
    HelpManager.c
    HelpManager_HAL.c
)

set(HELPMANAGER_HEADERS
    ${CMAKE_SOURCE_DIR}/include/HelpManager/HelpManager.h
    ${CMAKE_SOURCE_DIR}/include/HelpManager/HelpManager_HAL.h
)

# Create HelpManager library
add_library(HelpManager STATIC
    ${HELPMANAGER_SOURCES}
    ${HELPMANAGER_HEADERS}
)

# Include directories
target_include_directories(HelpManager PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/HelpManager
)

# Link dependencies
target_link_libraries(HelpManager
    PUBLIC
        Types
        QuickDraw
        WindowManager
        MenuManager
        ResourceManager
    PRIVATE
        ${CMAKE_THREAD_LIBS_INIT}
)

# Platform-specific configurations
if(APPLE)
    target_link_libraries(HelpManager PRIVATE
        "-framework Cocoa"
        "-framework WebKit"
    )
elseif(UNIX AND NOT APPLE)
    # Find GTK for Linux
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GTK3 gtk+-3.0)
    if(GTK3_FOUND)
        target_include_directories(HelpManager PRIVATE ${GTK3_INCLUDE_DIRS})
        target_link_libraries(HelpManager PRIVATE ${GTK3_LIBRARIES})
    endif()

    # Find WebKit2GTK for HTML help
    pkg_check_modules(WEBKIT2GTK webkit2gtk-4.0)
    if(WEBKIT2GTK_FOUND)
        target_include_directories(HelpManager PRIVATE ${WEBKIT2GTK_INCLUDE_DIRS})
        target_link_libraries(HelpManager PRIVATE ${WEBKIT2GTK_LIBRARIES})
    endif()
elseif(WIN32)
    target_link_libraries(HelpManager PRIVATE
        htmlhelp
        comctl32
        user32
        gdi32
    )
endif()

# Export target
set_target_properties(HelpManager PROPERTIES
    PUBLIC_HEADER "${HELPMANAGER_HEADERS}"
)

# Installation rules
install(TARGETS HelpManager
    ARCHIVE DESTINATION lib
    PUBLIC_HEADER DESTINATION include/HelpManager
)