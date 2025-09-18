# TimeManager CMake configuration
# Copyright (c) 2024 System7.1-Portable Project

set(TIMEMANAGER_SOURCES
    TimeManager.c
    TimeManager_HAL.c
)

set(TIMEMANAGER_HEADERS
    ${CMAKE_SOURCE_DIR}/include/TimeManager/TimeManager.h
    ${CMAKE_SOURCE_DIR}/include/TimeManager/TimeManager_HAL.h
)

# Create TimeManager library
add_library(TimeManager STATIC
    ${TIMEMANAGER_SOURCES}
    ${TIMEMANAGER_HEADERS}
)

# Include directories
target_include_directories(TimeManager PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/TimeManager
)

# Link dependencies
target_link_libraries(TimeManager
    PUBLIC
        Types
        Memory
    PRIVATE
        ${CMAKE_THREAD_LIBS_INIT}
)

# Platform-specific configurations
if(APPLE)
    # macOS uses mach time and dispatch
    find_library(DISPATCH dispatch)
    target_link_libraries(TimeManager PRIVATE ${DISPATCH})
elseif(UNIX AND NOT APPLE)
    # Linux uses POSIX timers
    target_link_libraries(TimeManager PRIVATE rt pthread)
elseif(WIN32)
    # Windows uses multimedia timers
    target_link_libraries(TimeManager PRIVATE winmm)
endif()

# Export target
set_target_properties(TimeManager PROPERTIES
    PUBLIC_HEADER "${TIMEMANAGER_HEADERS}"
)

# Installation rules
install(TARGETS TimeManager
    ARCHIVE DESTINATION lib
    PUBLIC_HEADER DESTINATION include/TimeManager
)