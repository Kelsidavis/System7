# Menu Manager CMake Configuration
# Builds the System 7.1 Menu Manager with dispatch mechanism and screen optimization

set(MENUMGR_SOURCES
    menu_dispatch.c
    menu_geometry.c
    menu_savebits.c
    MenuManagerCore.c
    MenuDisplay.c
    MenuSelection.c
    MenuItems.c
    MenuBarManager.c
    MenuResources.c
    PopupMenus.c
    MenuMgr_HAL.c
)

set(MENUMGR_HEADERS
    ${CMAKE_SOURCE_DIR}/include/MenuManager/menu_dispatch.h
    ${CMAKE_SOURCE_DIR}/include/MenuManager/menu_private.h
    ${CMAKE_SOURCE_DIR}/include/MenuManager/MenuManager.h
)

# Create Menu Manager library
add_library(MenuManager STATIC ${MENUMGR_SOURCES})

# Include directories
target_include_directories(MenuManager PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/MenuManager
    ${CMAKE_SOURCE_DIR}/include/MemoryMgr
    ${CMAKE_SOURCE_DIR}/include/ResourceMgr
    ${CMAKE_SOURCE_DIR}/include/WindowManager
    ${CMAKE_SOURCE_DIR}/include/QuickDraw
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Link with critical managers
target_link_libraries(MenuManager
    PUBLIC
        MemoryMgr       # For handle/pointer allocation
        ResourceMgr     # For MENU resources
        WindowManager   # For window coordination
        QuickDraw       # For drawing operations
        pthread
    PRIVATE
        ${CMAKE_DL_LIBS}
)

# Platform-specific features
if(APPLE)
    target_compile_definitions(MenuManager PRIVATE HAS_COREGRAPHICS=1)
    target_link_libraries(MenuManager PRIVATE
        "-framework CoreGraphics"
        "-framework ApplicationServices"
    )
endif()

if(UNIX AND NOT APPLE)
    # X11 support for Linux/Unix
    find_package(X11)
    if(X11_FOUND)
        target_include_directories(MenuManager PRIVATE ${X11_INCLUDE_DIR})
        target_link_libraries(MenuManager PRIVATE ${X11_LIBRARIES})
        target_compile_definitions(MenuManager PRIVATE HAS_X11=1)
    endif()
endif()

if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
    target_compile_definitions(MenuManager PRIVATE PLATFORM_X86_64=1)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
    target_compile_definitions(MenuManager PRIVATE PLATFORM_ARM64=1)
endif()

# Compiler options
target_compile_options(MenuManager PRIVATE
    -Wall
    -Wextra
    -Wno-unused-parameter
    -Wno-sign-compare
    -fno-strict-aliasing
    -O2  # Optimize for menu response time
)

# Install targets
install(TARGETS MenuManager
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)

install(FILES ${MENUMGR_HEADERS}
    DESTINATION include/MenuManager
)

# Add tests if enabled
if(BUILD_TESTS)
    add_executable(test_menu_manager
        ${CMAKE_SOURCE_DIR}/tests/test_menu_manager.c
    )

    target_link_libraries(test_menu_manager
        MenuManager
        MemoryMgr
        ResourceMgr
        WindowManager
        QuickDraw
    )

    add_test(NAME MenuManagerTests
        COMMAND test_menu_manager
    )
endif()