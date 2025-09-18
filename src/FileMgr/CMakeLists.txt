# File Manager CMake Configuration
# Builds the HFS File Manager with B-Tree support and volume management

set(FILEMGR_SOURCES
    tfs_dispatch.c
    volume_manager.c
    btree_services.c
    extent_manager.c
    FileMgr_HAL.c
)

set(FILEMGR_HEADERS
    ${CMAKE_SOURCE_DIR}/include/FileMgr/file_manager.h
    ${CMAKE_SOURCE_DIR}/include/FileMgr/hfs_structs.h
)

# Create File Manager library
add_library(FileMgr STATIC ${FILEMGR_SOURCES})

# Include directories
target_include_directories(FileMgr PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/FileMgr
    ${CMAKE_SOURCE_DIR}/include/MemoryMgr
    ${CMAKE_SOURCE_DIR}/include/ResourceMgr
)

# Link with Memory Manager and Resource Manager (critical dependencies!)
target_link_libraries(FileMgr
    PUBLIC
        MemoryMgr      # For handle/pointer allocation
        ResourceMgr    # For resource fork support
        pthread
    PRIVATE
        ${CMAKE_DL_LIBS}
)

# Platform-specific features
if(APPLE)
    target_compile_definitions(FileMgr PRIVATE HAS_RESOURCE_FORK=1)
    # Link with system frameworks for extended attributes
    target_link_libraries(FileMgr PRIVATE "-framework Foundation")
endif()

if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
    target_compile_definitions(FileMgr PRIVATE PLATFORM_X86_64=1)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
    target_compile_definitions(FileMgr PRIVATE PLATFORM_ARM64=1)
endif()

# Compiler options
target_compile_options(FileMgr PRIVATE
    -Wall
    -Wextra
    -Wno-unused-parameter
    -fno-strict-aliasing
    -O2  # Optimize for I/O performance
)

# Install targets
install(TARGETS FileMgr
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)

install(FILES ${FILEMGR_HEADERS}
    DESTINATION include/FileMgr
)