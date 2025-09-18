# StandardFile Package CMake configuration
# Copyright (c) 2024 System7 Project

set(STANDARDFILE_SOURCES
    StandardFile.c
    StandardFile_HAL.c
)

set(STANDARDFILE_HEADERS
    ${CMAKE_SOURCE_DIR}/include/StandardFile/StandardFile.h
    ${CMAKE_SOURCE_DIR}/include/StandardFile/StandardFile_HAL.h
)

# Create StandardFile library
add_library(StandardFile STATIC
    ${STANDARDFILE_SOURCES}
    ${STANDARDFILE_HEADERS}
)

# Include directories
target_include_directories(StandardFile PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/StandardFile
)

# Link dependencies
target_link_libraries(StandardFile
    PUBLIC
        Types
        Files
        Dialogs
        ListManager
        ControlManager
        EventManager
        MenuManager
        WindowManager
        MemoryManager
    PRIVATE
        ${CMAKE_THREAD_LIBS_INIT}
)

# Platform-specific configurations
if(APPLE)
    target_compile_definitions(StandardFile PRIVATE STANDARDFILE_USE_COCOA=1)
    target_link_libraries(StandardFile PRIVATE
        "-framework Cocoa"
        "-framework AppKit"
    )
elseif(UNIX AND NOT APPLE)
    target_compile_definitions(StandardFile PRIVATE STANDARDFILE_USE_CUSTOM=1)
    # GTK support can be added here when implemented
    # find_package(PkgConfig REQUIRED)
    # pkg_check_modules(GTK3 gtk+-3.0)
    # if(GTK3_FOUND)
    #     target_compile_definitions(StandardFile PRIVATE STANDARDFILE_USE_GTK=1)
    #     target_include_directories(StandardFile PRIVATE ${GTK3_INCLUDE_DIRS})
    #     target_link_libraries(StandardFile PRIVATE ${GTK3_LIBRARIES})
    # endif()
elseif(WIN32)
    target_compile_definitions(StandardFile PRIVATE STANDARDFILE_USE_CUSTOM=1)
    # Win32 support can be added here
    # target_compile_definitions(StandardFile PRIVATE STANDARDFILE_USE_WIN32=1)
    # target_link_libraries(StandardFile PRIVATE comdlg32)
endif()

# Export target
set_target_properties(StandardFile PROPERTIES
    PUBLIC_HEADER "${STANDARDFILE_HEADERS}"
)

# Installation rules
install(TARGETS StandardFile
    ARCHIVE DESTINATION lib
    PUBLIC_HEADER DESTINATION include/StandardFile
)