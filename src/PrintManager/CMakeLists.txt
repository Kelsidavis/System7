# PrintManager CMake configuration
# Copyright (c) 2024 System7.1-Portable Project

set(PRINTMANAGER_SOURCES
    PrintManager.c
    PrintManager_HAL.c
)

set(PRINTMANAGER_HEADERS
    ${CMAKE_SOURCE_DIR}/include/PrintManager/PrintManager.h
    ${CMAKE_SOURCE_DIR}/include/PrintManager/PrintManager_HAL.h
)

# Create PrintManager library
add_library(PrintManager STATIC
    ${PRINTMANAGER_SOURCES}
    ${PRINTMANAGER_HEADERS}
)

# Include directories
target_include_directories(PrintManager PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/PrintManager
)

# Link dependencies
target_link_libraries(PrintManager
    PUBLIC
        Types
        QuickDraw
        DialogManager
        Memory
    PRIVATE
        ${CMAKE_THREAD_LIBS_INIT}
)

# Platform-specific configurations
if(APPLE)
    find_library(APPLICATION_SERVICES ApplicationServices)
    target_link_libraries(PrintManager PRIVATE
        ${APPLICATION_SERVICES}
    )
elseif(UNIX AND NOT APPLE)
    # Find CUPS for Linux
    find_package(Cups)
    if(CUPS_FOUND)
        target_include_directories(PrintManager PRIVATE ${CUPS_INCLUDE_DIRS})
        target_link_libraries(PrintManager PRIVATE ${CUPS_LIBRARIES})
    endif()
elseif(WIN32)
    target_link_libraries(PrintManager PRIVATE
        winspool
        gdi32
        user32
    )
endif()

# Export target
set_target_properties(PrintManager PROPERTIES
    PUBLIC_HEADER "${PRINTMANAGER_HEADERS}"
)

# Installation rules
install(TARGETS PrintManager
    ARCHIVE DESTINATION lib
    PUBLIC_HEADER DESTINATION include/PrintManager
)