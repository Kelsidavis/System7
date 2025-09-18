# Sound Manager CMake Configuration
# Builds the System 7.1 Sound Manager with audio synthesis and playback

set(SOUNDMGR_SOURCES
    SoundManagerCore.c
    SoundSynthesis.c
    SoundMixing.c
    SoundResources.c
    SoundHardware.c
    SoundInput.c
    SoundMgr_HAL.c
)

set(SOUNDMGR_HEADERS
    ${CMAKE_SOURCE_DIR}/include/SoundManager/SoundManager.h
    ${CMAKE_SOURCE_DIR}/include/SoundManager/SoundTypes.h
    ${CMAKE_SOURCE_DIR}/include/SoundManager/SoundSynthesis.h
    ${CMAKE_SOURCE_DIR}/include/SoundManager/SoundHardware.h
    ${CMAKE_SOURCE_DIR}/include/SoundManager/SoundMgr_HAL.h
)

# Create Sound Manager library
add_library(SoundManager STATIC ${SOUNDMGR_SOURCES})

# Include directories
target_include_directories(SoundManager PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/SoundManager
    ${CMAKE_SOURCE_DIR}/include/MemoryManager
    ${CMAKE_SOURCE_DIR}/include/ResourceManager
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Link with dependencies
target_link_libraries(SoundManager
    PUBLIC
        MemoryManager     # Handle-based memory
        ResourceManager   # Sound resources
        m                # Math library
        pthread          # Threading
    PRIVATE
        ${CMAKE_DL_LIBS}
)

# Platform-specific audio support
if(APPLE)
    target_compile_definitions(SoundManager PRIVATE HAS_COREAUDIO=1)
    target_link_libraries(SoundManager PRIVATE
        "-framework CoreAudio"
        "-framework AudioToolbox"
        "-framework AudioUnit"
        "-framework CoreServices"
    )
endif()

if(UNIX AND NOT APPLE)
    # Check for audio libraries on Linux
    find_package(PkgConfig)

    # Try ALSA first
    if(PkgConfig_FOUND)
        pkg_check_modules(ALSA alsa)
        if(ALSA_FOUND)
            target_include_directories(SoundManager PRIVATE ${ALSA_INCLUDE_DIRS})
            target_link_libraries(SoundManager PRIVATE ${ALSA_LIBRARIES})
            target_compile_definitions(SoundManager PRIVATE HAS_ALSA=1)
        else()
            # Try PulseAudio as fallback
            pkg_check_modules(PULSE libpulse-simple)
            if(PULSE_FOUND)
                target_include_directories(SoundManager PRIVATE ${PULSE_INCLUDE_DIRS})
                target_link_libraries(SoundManager PRIVATE ${PULSE_LIBRARIES})
                target_compile_definitions(SoundManager PRIVATE HAS_PULSEAUDIO=1)
            endif()
        endif()
    endif()
endif()

# Architecture detection
if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
    target_compile_definitions(SoundManager PRIVATE PLATFORM_X86_64=1)
    # Enable SSE optimizations for mixing
    target_compile_options(SoundManager PRIVATE -msse2)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
    target_compile_definitions(SoundManager PRIVATE PLATFORM_ARM64=1)
    # Enable NEON optimizations for mixing
    if(NOT APPLE)
        target_compile_options(SoundManager PRIVATE -mfpu=neon)
    endif()
endif()

# Compiler options
target_compile_options(SoundManager PRIVATE
    -Wall
    -Wextra
    -Wno-unused-parameter
    -Wno-sign-compare
    -fno-strict-aliasing
    -O2  # Optimize for performance (critical for audio)
)

# Install targets
install(TARGETS SoundManager
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)

install(FILES ${SOUNDMGR_HEADERS}
    DESTINATION include/SoundManager
)

# Add tests if enabled
if(BUILD_TESTS)
    if(EXISTS ${CMAKE_SOURCE_DIR}/tests/test_sound_manager.c)
        add_executable(test_sound_manager
            ${CMAKE_SOURCE_DIR}/tests/test_sound_manager.c
        )

        target_link_libraries(test_sound_manager
            SoundManager
            MemoryManager
            ResourceManager
        )

        add_test(NAME SoundManagerTests
            COMMAND test_sound_manager
        )
    endif()
endif()