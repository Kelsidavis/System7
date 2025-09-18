# List Manager CMake Configuration
# Builds the System 7.1 List Manager with complete list display support

set(LISTMGR_SOURCES
    list_manager.c
    ListManager.c
    ListMgr_HAL.c
)

set(LISTMGR_HEADERS
    ${CMAKE_SOURCE_DIR}/include/ListManager/list_manager.h
    ${CMAKE_SOURCE_DIR}/include/ListManager/ListManager.h
)

# Create List Manager library
add_library(ListManager STATIC ${LISTMGR_SOURCES})

# Include directories
target_include_directories(ListManager PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/ListManager
    ${CMAKE_SOURCE_DIR}/include/WindowManager
    ${CMAKE_SOURCE_DIR}/include/ControlManager
    ${CMAKE_SOURCE_DIR}/include/EventManager
    ${CMAKE_SOURCE_DIR}/include/QuickDraw
    ${CMAKE_SOURCE_DIR}/include/MemoryManager
    ${CMAKE_SOURCE_DIR}/include/ResourceManager
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Link with dependencies
target_link_libraries(ListManager
    PUBLIC
        WindowManager     # Window integration
        ControlManager    # Scroll bars
        EventManager      # Mouse/keyboard input
        QuickDraw        # Drawing operations
        MemoryManager    # Handle-based storage
        ResourceManager  # LDEF resources
        pthread
    PRIVATE
        ${CMAKE_DL_LIBS}
)

# Platform-specific support
if(APPLE)
    target_compile_definitions(ListManager PRIVATE HAS_COREGRAPHICS=1)
    target_link_libraries(ListManager PRIVATE
        "-framework CoreGraphics"
    )
endif()

if(UNIX AND NOT APPLE)
    # X11 support for Linux
    find_package(X11)
    if(X11_FOUND)
        target_include_directories(ListManager PRIVATE ${X11_INCLUDE_DIR})
        target_link_libraries(ListManager PRIVATE ${X11_LIBRARIES})
        target_compile_definitions(ListManager PRIVATE HAS_X11=1)
    endif()
endif()

# Architecture detection
if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
    target_compile_definitions(ListManager PRIVATE PLATFORM_X86_64=1)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
    target_compile_definitions(ListManager PRIVATE PLATFORM_ARM64=1)
endif()

# Compiler options
target_compile_options(ListManager PRIVATE
    -Wall
    -Wextra
    -Wno-unused-parameter
    -Wno-sign-compare
    -fno-strict-aliasing
    -O2  # Optimize for performance
)

# Install targets
install(TARGETS ListManager
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)

install(FILES ${LISTMGR_HEADERS}
    DESTINATION include/ListManager
)

# Add tests if enabled
if(BUILD_TESTS)
    if(EXISTS ${CMAKE_SOURCE_DIR}/tests/test_list_manager.c)
        add_executable(test_list_manager
            ${CMAKE_SOURCE_DIR}/tests/test_list_manager.c
        )

        target_link_libraries(test_list_manager
            ListManager
            WindowManager
            ControlManager
            EventManager
            QuickDraw
            MemoryManager
            ResourceManager
        )

        add_test(NAME ListManagerTests
            COMMAND test_list_manager
        )
    endif()
endif()