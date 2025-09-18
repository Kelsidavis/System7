# Finder CMake Configuration
# Builds the System 7.1 Finder with complete desktop management

set(FINDER_SOURCES
    finder_main.c
    desktop_manager.c
    trash_folder.c
    alias_manager.c
    Finder_HAL.c
)

set(FINDER_HEADERS
    ${CMAKE_SOURCE_DIR}/include/Finder/finder.h
    ${CMAKE_SOURCE_DIR}/include/Finder/finder_types.h
)

# Create Finder library
add_library(Finder STATIC ${FINDER_SOURCES})

# Include directories
target_include_directories(Finder PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/Finder
    ${CMAKE_SOURCE_DIR}/include/WindowManager
    ${CMAKE_SOURCE_DIR}/include/MenuManager
    ${CMAKE_SOURCE_DIR}/include/EventManager
    ${CMAKE_SOURCE_DIR}/include/DialogManager
    ${CMAKE_SOURCE_DIR}/include/ControlManager
    ${CMAKE_SOURCE_DIR}/include/QuickDraw
    ${CMAKE_SOURCE_DIR}/include/FileManager
    ${CMAKE_SOURCE_DIR}/include/ResourceManager
    ${CMAKE_SOURCE_DIR}/include/MemoryManager
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Link with all UI and system managers
target_link_libraries(Finder
    PUBLIC
        WindowManager     # Window management
        MenuManager       # Menu handling
        EventManager      # Event processing
        DialogManager     # Info dialogs
        ControlManager    # Scroll bars
        QuickDraw        # Icon drawing
        FileManager      # File operations
        ResourceManager  # Icon resources
        MemoryManager    # Handle-based memory
        pthread
    PRIVATE
        ${CMAKE_DL_LIBS}
)

# Platform-specific desktop integration
if(APPLE)
    target_compile_definitions(Finder PRIVATE HAS_CORESERVICES=1)
    target_link_libraries(Finder PRIVATE
        "-framework CoreServices"
        "-framework CoreGraphics"
        "-framework ApplicationServices"
    )
endif()

if(UNIX AND NOT APPLE)
    # X11 and desktop support for Linux
    find_package(X11 REQUIRED)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(CAIRO REQUIRED cairo)

    target_include_directories(Finder PRIVATE
        ${X11_INCLUDE_DIR}
        ${CAIRO_INCLUDE_DIRS}
    )
    target_link_libraries(Finder PRIVATE
        ${X11_LIBRARIES}
        ${CAIRO_LIBRARIES}
    )
    target_compile_definitions(Finder PRIVATE HAS_X11=1)
endif()

# Architecture detection
if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
    target_compile_definitions(Finder PRIVATE PLATFORM_X86_64=1)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
    target_compile_definitions(Finder PRIVATE PLATFORM_ARM64=1)
endif()

# Compiler options
target_compile_options(Finder PRIVATE
    -Wall
    -Wextra
    -Wno-unused-parameter
    -Wno-sign-compare
    -fno-strict-aliasing
    -O2  # Optimize for performance
)

# Install targets
install(TARGETS Finder
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)

install(FILES ${FINDER_HEADERS}
    DESTINATION include/Finder
)

# Finder executable
add_executable(finder_app
    finder_main.c
)

target_link_libraries(finder_app
    Finder
    WindowManager
    MenuManager
    EventManager
    DialogManager
    ControlManager
    QuickDraw
    FileManager
    ResourceManager
    MemoryManager
)

install(TARGETS finder_app
    RUNTIME DESTINATION bin
)