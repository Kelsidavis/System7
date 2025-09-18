# TextEdit CMake Configuration
# Builds the System 7.1 TextEdit Manager with complete text editing support

set(TEXTEDIT_SOURCES
    TextEditCore.c
    TextDisplay.c
    TextSelection.c
    TextInput.c
    TextFormatting.c
    TextClipboard.c
    TextUndo.c
    textedit_core.c
    TextEdit_HAL.c
)

set(TEXTEDIT_HEADERS
    ${CMAKE_SOURCE_DIR}/include/TextEdit/textedit.h
    ${CMAKE_SOURCE_DIR}/include/TextEdit/TextEditTypes.h
)

# Create TextEdit library
add_library(TextEdit STATIC ${TEXTEDIT_SOURCES})

# Include directories
target_include_directories(TextEdit PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/TextEdit
    ${CMAKE_SOURCE_DIR}/include/WindowManager
    ${CMAKE_SOURCE_DIR}/include/EventManager
    ${CMAKE_SOURCE_DIR}/include/QuickDraw
    ${CMAKE_SOURCE_DIR}/include/ControlManager
    ${CMAKE_SOURCE_DIR}/include/ResourceManager
    ${CMAKE_SOURCE_DIR}/include/MemoryManager
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Link with dependencies
target_link_libraries(TextEdit
    PUBLIC
        WindowManager     # Window integration
        EventManager      # Keyboard/mouse input
        QuickDraw        # Text rendering
        ControlManager   # Scroll bars
        ResourceManager  # Style resources
        MemoryManager    # Handle-based text storage
        pthread
    PRIVATE
        ${CMAKE_DL_LIBS}
)

# Platform-specific text rendering
if(APPLE)
    target_compile_definitions(TextEdit PRIVATE HAS_CORETEXT=1)
    target_link_libraries(TextEdit PRIVATE
        "-framework CoreText"
        "-framework CoreGraphics"
        "-framework ApplicationServices"
    )
endif()

if(UNIX AND NOT APPLE)
    # Pango/Cairo for text rendering on Linux
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(PANGO REQUIRED pango)
    pkg_check_modules(PANGOCAIRO REQUIRED pangocairo)
    pkg_check_modules(CAIRO REQUIRED cairo)

    target_include_directories(TextEdit PRIVATE
        ${PANGO_INCLUDE_DIRS}
        ${PANGOCAIRO_INCLUDE_DIRS}
        ${CAIRO_INCLUDE_DIRS}
    )
    target_link_libraries(TextEdit PRIVATE
        ${PANGO_LIBRARIES}
        ${PANGOCAIRO_LIBRARIES}
        ${CAIRO_LIBRARIES}
    )
    target_compile_definitions(TextEdit PRIVATE HAS_PANGO=1)
endif()

# Architecture detection
if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
    target_compile_definitions(TextEdit PRIVATE PLATFORM_X86_64=1)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
    target_compile_definitions(TextEdit PRIVATE PLATFORM_ARM64=1)
endif()

# Compiler options
target_compile_options(TextEdit PRIVATE
    -Wall
    -Wextra
    -Wno-unused-parameter
    -Wno-sign-compare
    -fno-strict-aliasing
    -O2  # Optimize for performance
)

# Install targets
install(TARGETS TextEdit
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)

install(FILES ${TEXTEDIT_HEADERS}
    DESTINATION include/TextEdit
)

# Add tests if enabled
if(BUILD_TESTS)
    if(EXISTS ${CMAKE_SOURCE_DIR}/tests/test_textedit.c)
        add_executable(test_textedit
            ${CMAKE_SOURCE_DIR}/tests/test_textedit.c
        )

        target_link_libraries(test_textedit
            TextEdit
            WindowManager
            EventManager
            QuickDraw
            ControlManager
            ResourceManager
            MemoryManager
        )

        add_test(NAME TextEditTests
            COMMAND test_textedit
        )
    endif()
endif()