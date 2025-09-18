# Control Manager CMake Configuration
# Builds the System 7.1 Control Manager with complete control support

set(CONTROLMGR_SOURCES
    ControlManagerCore.c
    StandardControls.c
    ScrollbarControls.c
    PopupControls.c
    TextControls.c
    ControlTracking.c
    ControlDrawing.c
    ControlResources.c
    PlatformControls.c
    control_manager_sys7.c
    control_trap_glue.c
    string_conversion_stubs.c
    ControlMgr_HAL.c
)

set(CONTROLMGR_HEADERS
    ${CMAKE_SOURCE_DIR}/include/ControlManager/ControlManager.h
    ${CMAKE_SOURCE_DIR}/include/ControlManager/control_trap_glue.h
    ${CMAKE_SOURCE_DIR}/include/ControlManager/ControlTypes.h
    ${CMAKE_SOURCE_DIR}/include/ControlManager/StandardControls.h
    ${CMAKE_SOURCE_DIR}/include/ControlManager/PlatformControls.h
)

# Create Control Manager library
add_library(ControlManager STATIC ${CONTROLMGR_SOURCES})

# Include directories
target_include_directories(ControlManager PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/ControlManager
    ${CMAKE_SOURCE_DIR}/include/WindowManager
    ${CMAKE_SOURCE_DIR}/include/EventManager
    ${CMAKE_SOURCE_DIR}/include/QuickDraw
    ${CMAKE_SOURCE_DIR}/include/ResourceManager
    ${CMAKE_SOURCE_DIR}/include/MemoryManager
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Link with dependencies
target_link_libraries(ControlManager
    PUBLIC
        WindowManager     # Window integration
        EventManager      # Event tracking
        QuickDraw        # Drawing operations
        ResourceManager  # CDEF resources
        MemoryManager    # Handle-based memory
        pthread
    PRIVATE
        ${CMAKE_DL_LIBS}
)

# Platform-specific support
if(APPLE)
    target_compile_definitions(ControlManager PRIVATE HAS_COREGRAPHICS=1)
    target_link_libraries(ControlManager PRIVATE
        "-framework CoreGraphics"
        "-framework ApplicationServices"
    )
endif()

if(UNIX AND NOT APPLE)
    # X11 support for Linux/Unix
    find_package(X11 REQUIRED)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(CAIRO REQUIRED cairo)

    target_include_directories(ControlManager PRIVATE
        ${X11_INCLUDE_DIR}
        ${CAIRO_INCLUDE_DIRS}
    )
    target_link_libraries(ControlManager PRIVATE
        ${X11_LIBRARIES}
        ${CAIRO_LIBRARIES}
    )
    target_compile_definitions(ControlManager PRIVATE HAS_X11=1)
endif()

# Architecture detection
if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
    target_compile_definitions(ControlManager PRIVATE PLATFORM_X86_64=1)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
    target_compile_definitions(ControlManager PRIVATE PLATFORM_ARM64=1)
endif()

# Compiler options
target_compile_options(ControlManager PRIVATE
    -Wall
    -Wextra
    -Wno-unused-parameter
    -Wno-sign-compare
    -fno-strict-aliasing
    -O2  # Optimize for performance
)

# Install targets
install(TARGETS ControlManager
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)

install(FILES ${CONTROLMGR_HEADERS}
    DESTINATION include/ControlManager
)

# Add tests if enabled
if(BUILD_TESTS)
    add_executable(test_control_manager
        ${CMAKE_SOURCE_DIR}/tests/test_control_sys7.c
        ${CMAKE_SOURCE_DIR}/tests/test_control_trap_glue.c
    )

    target_link_libraries(test_control_manager
        ControlManager
        WindowManager
        EventManager
        QuickDraw
        ResourceManager
        MemoryManager
    )

    add_test(NAME ControlManagerTests
        COMMAND test_control_manager
    )
endif()