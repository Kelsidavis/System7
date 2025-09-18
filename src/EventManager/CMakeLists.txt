# Event Manager CMake Configuration
# Builds the System 7.1 Event Manager with complete event routing

set(EVENTMGR_SOURCES
    event_manager.c
    EventManagerCore.c
    MouseEvents.c
    KeyboardEvents.c
    SystemEvents.c
    EventMgr_HAL.c
)

set(EVENTMGR_HEADERS
    ${CMAKE_SOURCE_DIR}/include/EventManager/event_manager.h
    ${CMAKE_SOURCE_DIR}/include/EventManager/EventManager.h
    ${CMAKE_SOURCE_DIR}/include/EventManager/EventTypes.h
)

# Create Event Manager library
add_library(EventManager STATIC ${EVENTMGR_SOURCES})

# Include directories
target_include_directories(EventManager PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/EventManager
    ${CMAKE_SOURCE_DIR}/include/WindowManager
    ${CMAKE_SOURCE_DIR}/include/MenuManager
    ${CMAKE_SOURCE_DIR}/include/DialogManager
    ${CMAKE_SOURCE_DIR}/include/QuickDraw
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Link with UI managers for event routing
target_link_libraries(EventManager
    PUBLIC
        WindowManager   # Window events
        MenuManager     # Menu events
        DialogManager   # Dialog events
        QuickDraw       # Drawing coordination
        pthread
    PRIVATE
        ${CMAKE_DL_LIBS}
)

# Platform-specific event support
if(APPLE)
    target_compile_definitions(EventManager PRIVATE HAS_COREGRAPHICS=1)
    target_link_libraries(EventManager PRIVATE
        "-framework CoreGraphics"
        "-framework ApplicationServices"
    )
endif()

if(UNIX AND NOT APPLE)
    # X11 support for Linux/Unix
    find_package(X11 REQUIRED)
    target_include_directories(EventManager PRIVATE ${X11_INCLUDE_DIR})
    target_link_libraries(EventManager PRIVATE ${X11_LIBRARIES})
    target_compile_definitions(EventManager PRIVATE HAS_X11=1)
endif()

if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
    target_compile_definitions(EventManager PRIVATE PLATFORM_X86_64=1)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
    target_compile_definitions(EventManager PRIVATE PLATFORM_ARM64=1)
endif()

# Compiler options
target_compile_options(EventManager PRIVATE
    -Wall
    -Wextra
    -Wno-unused-parameter
    -Wno-sign-compare
    -fno-strict-aliasing
    -O2  # Optimize for low latency
)

# Install targets
install(TARGETS EventManager
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)

install(FILES ${EVENTMGR_HEADERS}
    DESTINATION include/EventManager
)

# Add tests if enabled
if(BUILD_TESTS)
    if(EXISTS ${CMAKE_SOURCE_DIR}/tests/test_event_manager.c)
        add_executable(test_event_manager
            ${CMAKE_SOURCE_DIR}/tests/test_event_manager.c
        )

        target_link_libraries(test_event_manager
            EventManager
            WindowManager
            MenuManager
            DialogManager
            QuickDraw
        )

        add_test(NAME EventManagerTests
            COMMAND test_event_manager
        )
    endif()
endif()