# Scrap Manager CMake Configuration
# Builds the System 7.1 Scrap Manager with complete clipboard support

set(SCRAPMGR_SOURCES
    scrap_manager.c
    ScrapMgr_HAL.c
)

set(SCRAPMGR_HEADERS
    ${CMAKE_SOURCE_DIR}/include/ScrapManager/scrap_manager.h
)

# Create Scrap Manager library
add_library(ScrapManager STATIC ${SCRAPMGR_SOURCES})

# Include directories
target_include_directories(ScrapManager PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/ScrapManager
    ${CMAKE_SOURCE_DIR}/include/MemoryManager
    ${CMAKE_SOURCE_DIR}/include/FileManager
    ${CMAKE_SOURCE_DIR}/include/ResourceManager
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Link with dependencies
target_link_libraries(ScrapManager
    PUBLIC
        MemoryManager     # Handle-based memory
        FileManager       # Scrap file operations
        ResourceManager   # Resource handling
        pthread
    PRIVATE
        ${CMAKE_DL_LIBS}
)

# Platform-specific clipboard support
if(APPLE)
    target_compile_definitions(ScrapManager PRIVATE HAS_PASTEBOARD=1)
    target_link_libraries(ScrapManager PRIVATE
        "-framework ApplicationServices"
        "-framework CoreFoundation"
    )
endif()

if(UNIX AND NOT APPLE)
    # X11 clipboard support for Linux
    find_package(X11 REQUIRED)
    target_include_directories(ScrapManager PRIVATE ${X11_INCLUDE_DIR})
    target_link_libraries(ScrapManager PRIVATE ${X11_LIBRARIES})
    target_compile_definitions(ScrapManager PRIVATE HAS_X11=1)
endif()

# Architecture detection
if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
    target_compile_definitions(ScrapManager PRIVATE PLATFORM_X86_64=1)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
    target_compile_definitions(ScrapManager PRIVATE PLATFORM_ARM64=1)
endif()

# Compiler options
target_compile_options(ScrapManager PRIVATE
    -Wall
    -Wextra
    -Wno-unused-parameter
    -Wno-sign-compare
    -fno-strict-aliasing
    -O2  # Optimize for performance
)

# Install targets
install(TARGETS ScrapManager
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)

install(FILES ${SCRAPMGR_HEADERS}
    DESTINATION include/ScrapManager
)

# Add tests if enabled
if(BUILD_TESTS)
    if(EXISTS ${CMAKE_SOURCE_DIR}/tests/test_scrap_manager.c)
        add_executable(test_scrap_manager
            ${CMAKE_SOURCE_DIR}/tests/test_scrap_manager.c
        )

        target_link_libraries(test_scrap_manager
            ScrapManager
            MemoryManager
            FileManager
            ResourceManager
        )

        add_test(NAME ScrapManagerTests
            COMMAND test_scrap_manager
        )
    endif()
endif()