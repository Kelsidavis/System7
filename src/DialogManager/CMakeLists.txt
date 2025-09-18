# Dialog Manager CMake Configuration
# Builds the System 7.1 Dialog Manager with trap dispatch and alerts

set(DIALOGMGR_SOURCES
    dialog_manager_core.c
    dialog_manager_dispatch.c
    dialog_manager_private.c
    DialogManagerCore.c
    DialogItems.c
    ModalDialogs.c
    DialogMgr_HAL.c
)

set(DIALOGMGR_HEADERS
    ${CMAKE_SOURCE_DIR}/include/DialogManager/dialog_manager_core.h
    ${CMAKE_SOURCE_DIR}/include/DialogManager/dialog_manager_dispatch.h
    ${CMAKE_SOURCE_DIR}/include/DialogManager/dialog_manager_private.h
    ${CMAKE_SOURCE_DIR}/include/DialogManager/DialogManager.h
)

# Create Dialog Manager library
add_library(DialogManager STATIC ${DIALOGMGR_SOURCES})

# Include directories
target_include_directories(DialogManager PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/DialogManager
    ${CMAKE_SOURCE_DIR}/include/MemoryMgr
    ${CMAKE_SOURCE_DIR}/include/ResourceMgr
    ${CMAKE_SOURCE_DIR}/include/WindowManager
    ${CMAKE_SOURCE_DIR}/include/ControlManager
    ${CMAKE_SOURCE_DIR}/include/QuickDraw
    ${CMAKE_SOURCE_DIR}/include/EventManager
    ${CMAKE_SOURCE_DIR}/include/TextEdit
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Link with critical managers
target_link_libraries(DialogManager
    PUBLIC
        MemoryMgr       # For handle allocation
        ResourceMgr     # For DLOG/DITL/ALRT resources
        WindowManager   # For dialog windows
        ControlManager  # For dialog controls
        QuickDraw       # For drawing operations
        EventManager    # For event handling
        TextEdit        # For editable text items
        pthread
    PRIVATE
        ${CMAKE_DL_LIBS}
)

# Platform-specific features
if(APPLE)
    target_compile_definitions(DialogManager PRIVATE HAS_COREGRAPHICS=1)
endif()

if(UNIX AND NOT APPLE)
    # X11 support for Linux/Unix
    find_package(X11)
    if(X11_FOUND)
        target_include_directories(DialogManager PRIVATE ${X11_INCLUDE_DIR})
        target_link_libraries(DialogManager PRIVATE ${X11_LIBRARIES})
        target_compile_definitions(DialogManager PRIVATE HAS_X11=1)
    endif()
endif()

if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
    target_compile_definitions(DialogManager PRIVATE PLATFORM_X86_64=1)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
    target_compile_definitions(DialogManager PRIVATE PLATFORM_ARM64=1)
endif()

# Compiler options
target_compile_options(DialogManager PRIVATE
    -Wall
    -Wextra
    -Wno-unused-parameter
    -Wno-sign-compare
    -fno-strict-aliasing
    -O2
)

# Install targets
install(TARGETS DialogManager
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)

install(FILES ${DIALOGMGR_HEADERS}
    DESTINATION include/DialogManager
)

# Add tests if enabled
if(BUILD_TESTS)
    if(EXISTS ${CMAKE_SOURCE_DIR}/tests/test_dialog_manager.c)
        add_executable(test_dialog_manager
            ${CMAKE_SOURCE_DIR}/tests/test_dialog_manager.c
        )

        target_link_libraries(test_dialog_manager
            DialogManager
            MemoryMgr
            ResourceMgr
            WindowManager
            ControlManager
            QuickDraw
            EventManager
        )

        add_test(NAME DialogManagerTests
            COMMAND test_dialog_manager
        )
    endif()
endif()