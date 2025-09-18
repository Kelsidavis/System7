# Memory Manager CMake Configuration
# Builds the System 7.1 Memory Manager with HAL support

set(MEMORYMGR_SOURCES
    memory_manager_core.c
    heap_compaction.c
    blockmove_optimization.c
    MemoryMgr_HAL.c
)

set(MEMORYMGR_HEADERS
    ${CMAKE_SOURCE_DIR}/include/MemoryMgr/memory_manager_types.h
    ${CMAKE_SOURCE_DIR}/include/MemoryMgr/MemoryMgr_HAL.h
)

# Create Memory Manager library
add_library(MemoryMgr STATIC ${MEMORYMGR_SOURCES})

# Include directories
target_include_directories(MemoryMgr PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/MemoryMgr
)

# Platform-specific definitions and optimizations
if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
    target_compile_definitions(MemoryMgr PRIVATE PLATFORM_X86_64=1)
    # Enable SSE/AVX for BlockMove optimizations
    target_compile_options(MemoryMgr PRIVATE
        -msse4.2
        -mavx
        $<$<COMPILE_LANGUAGE:C>:-mavx2>
    )
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
    target_compile_definitions(MemoryMgr PRIVATE PLATFORM_ARM64=1)
    # ARM64 NEON is always available
endif()

# Link with required libraries
target_link_libraries(MemoryMgr
    PUBLIC
        pthread
    PRIVATE
        ${CMAKE_DL_LIBS}
)

# Compiler options
target_compile_options(MemoryMgr PRIVATE
    -Wall
    -Wextra
    -Wno-unused-parameter
    -fno-strict-aliasing
    -O2  # Optimize for speed (critical for Memory Manager)
)

# Install targets
install(TARGETS MemoryMgr
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)

install(FILES ${MEMORYMGR_HEADERS}
    DESTINATION include/MemoryMgr
)