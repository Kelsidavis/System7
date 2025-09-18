# QuickDraw CMake Configuration
# Builds the System 7.1 QuickDraw graphics engine with region operations

set(QUICKDRAW_SOURCES
    quickdraw_core.c
    quickdraw_drawing.c
    quickdraw_shapes.c
    quickdraw_bitmap.c
    quickdraw_pictures.c
    quickdraw_portable.c
    QuickDrawCore.c
    Regions.c
    Bitmaps.c
    Text.c
    Patterns.c
    Coordinates.c
    ColorQuickDraw.c
    QuickDraw_HAL.c
)

set(QUICKDRAW_HEADERS
    ${CMAKE_SOURCE_DIR}/include/QuickDraw/quickdraw.h
    ${CMAKE_SOURCE_DIR}/include/QuickDraw/quickdraw_types.h
    ${CMAKE_SOURCE_DIR}/include/QuickDraw/quickdraw_portable.h
    ${CMAKE_SOURCE_DIR}/include/QuickDraw/QuickDraw.h
    QuickDrawPlatform.h
)

# Create QuickDraw library
add_library(QuickDraw STATIC ${QUICKDRAW_SOURCES})

# Include directories
target_include_directories(QuickDraw PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/QuickDraw
    ${CMAKE_SOURCE_DIR}/include/MemoryMgr
    ${CMAKE_SOURCE_DIR}/include/WindowManager
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Link with critical managers
target_link_libraries(QuickDraw
    PUBLIC
        MemoryMgr       # For handle/region allocation
        pthread
        m               # Math library
    PRIVATE
        ${CMAKE_DL_LIBS}
)

# Platform-specific graphics support
if(APPLE)
    target_compile_definitions(QuickDraw PRIVATE HAS_COREGRAPHICS=1)
    target_link_libraries(QuickDraw PRIVATE
        "-framework CoreGraphics"
        "-framework ApplicationServices"
    )
endif()

if(UNIX AND NOT APPLE)
    # X11 and Cairo support for Linux/Unix
    find_package(X11)
    find_package(PkgConfig)

    if(X11_FOUND)
        target_include_directories(QuickDraw PRIVATE ${X11_INCLUDE_DIR})
        target_link_libraries(QuickDraw PRIVATE ${X11_LIBRARIES})
        target_compile_definitions(QuickDraw PRIVATE HAS_X11=1)
    endif()

    if(PkgConfig_FOUND)
        pkg_check_modules(CAIRO cairo)
        if(CAIRO_FOUND)
            target_include_directories(QuickDraw PRIVATE ${CAIRO_INCLUDE_DIRS})
            target_link_libraries(QuickDraw PRIVATE ${CAIRO_LIBRARIES})
            target_compile_definitions(QuickDraw PRIVATE HAS_CAIRO=1)
        endif()
    endif()
endif()

if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
    target_compile_definitions(QuickDraw PRIVATE PLATFORM_X86_64=1)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
    target_compile_definitions(QuickDraw PRIVATE PLATFORM_ARM64=1)
endif()

# Compiler options
target_compile_options(QuickDraw PRIVATE
    -Wall
    -Wextra
    -Wno-unused-parameter
    -Wno-sign-compare
    -fno-strict-aliasing
    -O2  # Optimize for graphics performance
)

# Install targets
install(TARGETS QuickDraw
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)

install(FILES ${QUICKDRAW_HEADERS}
    DESTINATION include/QuickDraw
)

# Add tests if enabled
if(BUILD_TESTS)
    if(EXISTS ${CMAKE_SOURCE_DIR}/tests/test_quickdraw.c)
        add_executable(test_quickdraw
            ${CMAKE_SOURCE_DIR}/tests/test_quickdraw.c
        )

        target_link_libraries(test_quickdraw
            QuickDraw
            MemoryMgr
        )

        add_test(NAME QuickDrawTests
            COMMAND test_quickdraw
        )
    endif()
endif()