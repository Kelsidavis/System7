# Resource Manager CMake Configuration
# Builds the System 7.1 Resource Manager with Memory Manager integration

set(RESOURCEMGR_SOURCES
    resource_manager.c
    resource_traps.c
    ResourceMgr_HAL.c
)

set(RESOURCEMGR_HEADERS
    ${CMAKE_SOURCE_DIR}/include/ResourceMgr/resource_manager.h
    ${CMAKE_SOURCE_DIR}/include/ResourceMgr/resource_types.h
)

# Create Resource Manager library
add_library(ResourceMgr STATIC ${RESOURCEMGR_SOURCES})

# Include directories
target_include_directories(ResourceMgr PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/ResourceMgr
    ${CMAKE_SOURCE_DIR}/include/MemoryMgr
)

# Link with Memory Manager (critical dependency!)
target_link_libraries(ResourceMgr
    PUBLIC
        MemoryMgr  # Resource Manager uses Memory Manager for all allocations
        pthread
    PRIVATE
        ${CMAKE_DL_LIBS}
)

# Platform-specific features
if(APPLE)
    target_compile_definitions(ResourceMgr PRIVATE HAS_MACOS_RESOURCES=1)
    target_link_libraries(ResourceMgr PRIVATE "-framework CoreFoundation")
endif()

if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
    target_compile_definitions(ResourceMgr PRIVATE PLATFORM_X86_64=1)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
    target_compile_definitions(ResourceMgr PRIVATE PLATFORM_ARM64=1)
endif()

# Compiler options
target_compile_options(ResourceMgr PRIVATE
    -Wall
    -Wextra
    -Wno-unused-parameter
    -fno-strict-aliasing
)

# Install targets
install(TARGETS ResourceMgr
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)

install(FILES ${RESOURCEMGR_HEADERS}
    DESTINATION include/ResourceMgr
)

# Add tests if enabled
if(BUILD_TESTS)
    add_executable(test_resource_manager
        ${CMAKE_SOURCE_DIR}/tests/test_resource_manager.c
    )

    target_link_libraries(test_resource_manager
        ResourceMgr
        MemoryMgr
    )

    add_test(NAME ResourceManagerTests
        COMMAND test_resource_manager
    )
endif()