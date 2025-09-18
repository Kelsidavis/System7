# Window Manager CMake Configuration
# Builds the System 7.1 Window Manager with full manager integration

set(WINDOWMGR_SOURCES
    window_manager.c
    WindowManagerCore.c
    WindowDisplay.c
    WindowDragging.c
    WindowEvents.c
    WindowLayering.c
    WindowParts.c
    WindowResizing.c
    WindowMgr_HAL.c
)

set(WINDOWMGR_HEADERS
    ${CMAKE_SOURCE_DIR}/include/WindowManager/window_manager.h
    ${CMAKE_SOURCE_DIR}/include/WindowManager/WindowManager.h
    ${CMAKE_SOURCE_DIR}/include/WindowManager/WindowTypes.h
    ${CMAKE_SOURCE_DIR}/include/WindowManager/WindowPlatform.h
    WindowManagerInternal.h
)

# Create Window Manager library
add_library(WindowManager STATIC ${WINDOWMGR_SOURCES})

# Include directories
target_include_directories(WindowManager PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/WindowManager
    ${CMAKE_SOURCE_DIR}/include/MemoryMgr
    ${CMAKE_SOURCE_DIR}/include/ResourceMgr
    ${CMAKE_SOURCE_DIR}/include/QuickDraw
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Link with critical managers
target_link_libraries(WindowManager
    PUBLIC
        MemoryMgr       # For handle/pointer allocation
        ResourceMgr     # For WIND resources
        QuickDraw       # For graphics operations
        pthread
    PRIVATE
        ${CMAKE_DL_LIBS}
)

# Platform-specific features
if(APPLE)
    target_compile_definitions(WindowManager PRIVATE HAS_COREGRAPHICS=1)
    target_link_libraries(WindowManager PRIVATE
        "-framework CoreGraphics"
        "-framework ApplicationServices"
    )
endif()

if(UNIX AND NOT APPLE)
    # X11 support for Linux/Unix
    find_package(X11)
    if(X11_FOUND)
        target_include_directories(WindowManager PRIVATE ${X11_INCLUDE_DIR})
        target_link_libraries(WindowManager PRIVATE ${X11_LIBRARIES})
        target_compile_definitions(WindowManager PRIVATE HAS_X11=1)
    endif()
endif()

if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
    target_compile_definitions(WindowManager PRIVATE PLATFORM_X86_64=1)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
    target_compile_definitions(WindowManager PRIVATE PLATFORM_ARM64=1)
endif()

# Compiler options
target_compile_options(WindowManager PRIVATE
    -Wall
    -Wextra
    -Wno-unused-parameter
    -Wno-sign-compare
    -fno-strict-aliasing
    -O2
)

# Install targets
install(TARGETS WindowManager
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)

install(FILES ${WINDOWMGR_HEADERS}
    DESTINATION include/WindowManager
)

# Add tests if enabled
if(BUILD_TESTS)
    add_executable(test_window_manager
        ${CMAKE_SOURCE_DIR}/tests/test_window_manager.c
    )

    target_link_libraries(test_window_manager
        WindowManager
        MemoryMgr
        ResourceMgr
        QuickDraw
    )

    add_test(NAME WindowManagerTests
        COMMAND test_window_manager
    )
endif()