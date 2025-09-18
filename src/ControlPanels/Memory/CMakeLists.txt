# Memory Control Panel CMake Configuration
# Builds the System 7.1 Memory Control Panel

set(MEMORY_CONTROL_PANEL_SOURCES
    memory_control_panel.c
)

set(MEMORY_CONTROL_PANEL_HEADERS
    ${CMAKE_SOURCE_DIR}/include/ControlPanels/memory_control_panel.h
)

# Create Memory Control Panel library
add_library(MemoryControlPanel STATIC ${MEMORY_CONTROL_PANEL_SOURCES})

# Include directories
target_include_directories(MemoryControlPanel PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/ControlPanels
)

# Link with required libraries
target_link_libraries(MemoryControlPanel
    PUBLIC
        ToolboxCompat
        ResourceManager
        DialogManager
        MemoryManager
        GestaltManager
)

# Compiler options
target_compile_options(MemoryControlPanel PRIVATE
    -Wall
    -Wextra
    -Wno-unused-parameter
    -fno-strict-aliasing
)

# Platform-specific definitions
if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
    target_compile_definitions(MemoryControlPanel PRIVATE PLATFORM_X86_64=1)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
    target_compile_definitions(MemoryControlPanel PRIVATE PLATFORM_ARM64=1)
endif()

# Install targets
install(TARGETS MemoryControlPanel
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)

install(FILES ${MEMORY_CONTROL_PANEL_HEADERS}
    DESTINATION include/ControlPanels
)

# Add test executable if tests are enabled
if(BUILD_TESTS)
    add_executable(test_memory_control_panel
        ${CMAKE_SOURCE_DIR}/tests/memory_control_panel_tests.c
    )

    target_link_libraries(test_memory_control_panel
        MemoryControlPanel
        ${TEST_LIBRARIES}
    )

    add_test(NAME MemoryControlPanelTests
        COMMAND test_memory_control_panel
    )
endif()