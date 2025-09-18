# Process Manager CMake Configuration
# Builds the Process Manager with HAL support for x86_64 and ARM64

set(PROCESSMGR_SOURCES
    ProcessManager.c
    CooperativeScheduler.c
    EventIntegration.c
    ProcessMgr_HAL.c
)

set(PROCESSMGR_HEADERS
    ${CMAKE_SOURCE_DIR}/include/ProcessMgr/ProcessMgr.h
    ${CMAKE_SOURCE_DIR}/include/ProcessMgr/ProcessMgr_HAL.h
)

# Create Process Manager library
add_library(ProcessMgr STATIC ${PROCESSMGR_SOURCES})

# Include directories
target_include_directories(ProcessMgr PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/ProcessMgr
)

# Platform-specific definitions
if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
    target_compile_definitions(ProcessMgr PRIVATE PLATFORM_X86_64=1)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
    target_compile_definitions(ProcessMgr PRIVATE PLATFORM_ARM64=1)
endif()

# Link with required libraries
target_link_libraries(ProcessMgr
    PUBLIC
        pthread
        rt
    PRIVATE
        ${CMAKE_DL_LIBS}
)

# Compiler options
target_compile_options(ProcessMgr PRIVATE
    -Wall
    -Wextra
    -Wno-unused-parameter
    -fno-strict-aliasing
)

# Install targets
install(TARGETS ProcessMgr
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)

install(FILES ${PROCESSMGR_HEADERS}
    DESTINATION include/ProcessMgr
)