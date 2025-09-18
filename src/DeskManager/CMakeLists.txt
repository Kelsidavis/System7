# DeskManager CMake Configuration
# Builds the System 7.1 Desk Accessory Manager and built-in accessories

set(DESKMGR_SOURCES
    DeskManagerCore.c
    DALoader.c
    DAWindows.c
    SystemMenu.c
    BuiltinDAs.c
    AlarmClock.c
    AlarmClock_HAL.c
    Calculator.c
    Chooser.c
    KeyCaps.c
)

set(DESKMGR_HEADERS
    ${CMAKE_SOURCE_DIR}/include/DeskManager/DeskManager.h
    ${CMAKE_SOURCE_DIR}/include/DeskManager/DeskAccessory.h
    ${CMAKE_SOURCE_DIR}/include/DeskManager/AlarmClock.h
    ${CMAKE_SOURCE_DIR}/include/DeskManager/AlarmClock_HAL.h
    ${CMAKE_SOURCE_DIR}/include/DeskManager/Calculator.h
    ${CMAKE_SOURCE_DIR}/include/DeskManager/Chooser.h
    ${CMAKE_SOURCE_DIR}/include/DeskManager/KeyCaps.h
    ${CMAKE_SOURCE_DIR}/include/DeskManager/Types.h
)

# Create DeskManager library
add_library(DeskManager STATIC ${DESKMGR_SOURCES})

# Include directories
target_include_directories(DeskManager PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/DeskManager
    ${CMAKE_SOURCE_DIR}/include/WindowManager
    ${CMAKE_SOURCE_DIR}/include/MenuManager
    ${CMAKE_SOURCE_DIR}/include/EventManager
    ${CMAKE_SOURCE_DIR}/include/QuickDraw
    ${CMAKE_SOURCE_DIR}/include/DialogManager
    ${CMAKE_SOURCE_DIR}/include/MemoryManager
    ${CMAKE_SOURCE_DIR}/include/ResourceManager
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Link with dependencies
target_link_libraries(DeskManager
    PUBLIC
        WindowManager     # Window management
        MenuManager      # Menu integration
        EventManager     # Event handling
        QuickDraw       # Drawing operations
        DialogManager   # Dialog support
        MemoryManager   # Memory allocation
        ResourceManager # Resource loading
        m               # Math library
        pthread         # Threading support
    PRIVATE
        ${CMAKE_DL_LIBS}
)

# Platform-specific support
if(APPLE)
    target_compile_definitions(DeskManager PRIVATE HAS_COREGRAPHICS=1)
    target_link_libraries(DeskManager PRIVATE
        "-framework CoreGraphics"
        "-framework ApplicationServices"
        "-framework AudioToolbox"
        "-framework CoreFoundation"
    )
endif()

if(UNIX AND NOT APPLE)
    # X11 and Cairo support for Linux
    find_package(X11)
    find_package(PkgConfig)

    if(X11_FOUND)
        target_include_directories(DeskManager PRIVATE ${X11_INCLUDE_DIR})
        target_link_libraries(DeskManager PRIVATE ${X11_LIBRARIES})
        target_compile_definitions(DeskManager PRIVATE HAS_X11=1)
    endif()

    if(PkgConfig_FOUND)
        pkg_check_modules(CAIRO cairo)
        if(CAIRO_FOUND)
            target_include_directories(DeskManager PRIVATE ${CAIRO_INCLUDE_DIRS})
            target_link_libraries(DeskManager PRIVATE ${CAIRO_LIBRARIES})
            target_compile_definitions(DeskManager PRIVATE HAS_CAIRO=1)
        endif()
    endif()
endif()

# Architecture detection
if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
    target_compile_definitions(DeskManager PRIVATE PLATFORM_X86_64=1)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
    target_compile_definitions(DeskManager PRIVATE PLATFORM_ARM64=1)
endif()

# Compiler options
target_compile_options(DeskManager PRIVATE
    -Wall
    -Wextra
    -Wno-unused-parameter
    -Wno-sign-compare
    -fno-strict-aliasing
    -O2  # Optimize for performance
)

# Install targets
install(TARGETS DeskManager
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)

install(FILES ${DESKMGR_HEADERS}
    DESTINATION include/DeskManager
)

# Add tests if enabled
if(BUILD_TESTS)
    if(EXISTS ${CMAKE_SOURCE_DIR}/tests/test_alarm_clock.c)
        add_executable(test_alarm_clock
            ${CMAKE_SOURCE_DIR}/tests/test_alarm_clock.c
        )

        target_link_libraries(test_alarm_clock
            DeskManager
            WindowManager
            EventManager
            QuickDraw
            MemoryManager
        )

        add_test(NAME AlarmClockTests
            COMMAND test_alarm_clock
        )
    endif()
endif()