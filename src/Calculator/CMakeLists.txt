# Calculator CMake Configuration
# Builds the System 7.1 Calculator desk accessory

set(CALCULATOR_SOURCES
    calculator.c
    Calculator_HAL.c
)

set(CALCULATOR_HEADERS
    ${CMAKE_SOURCE_DIR}/include/Calculator/calculator.h
    ${CMAKE_SOURCE_DIR}/include/Calculator/Calculator_HAL.h
)

# Create Calculator library
add_library(Calculator STATIC ${CALCULATOR_SOURCES})

# Include directories
target_include_directories(Calculator PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/Calculator
    ${CMAKE_SOURCE_DIR}/include/WindowManager
    ${CMAKE_SOURCE_DIR}/include/EventManager
    ${CMAKE_SOURCE_DIR}/include/QuickDraw
    ${CMAKE_SOURCE_DIR}/include/MemoryManager
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Link with dependencies
target_link_libraries(Calculator
    PUBLIC
        WindowManager     # Window management
        EventManager      # Event handling
        QuickDraw        # Drawing operations
        MemoryManager    # Memory allocation
        m                # Math library
        pthread
    PRIVATE
        ${CMAKE_DL_LIBS}
)

# Platform-specific support
if(APPLE)
    target_compile_definitions(Calculator PRIVATE HAS_COREGRAPHICS=1)
    target_link_libraries(Calculator PRIVATE
        "-framework CoreGraphics"
        "-framework ApplicationServices"
    )
endif()

if(UNIX AND NOT APPLE)
    # X11 and Cairo support for Linux
    find_package(X11)
    find_package(PkgConfig)

    if(X11_FOUND)
        target_include_directories(Calculator PRIVATE ${X11_INCLUDE_DIR})
        target_link_libraries(Calculator PRIVATE ${X11_LIBRARIES})
        target_compile_definitions(Calculator PRIVATE HAS_X11=1)
    endif()

    if(PkgConfig_FOUND)
        pkg_check_modules(CAIRO cairo)
        if(CAIRO_FOUND)
            target_include_directories(Calculator PRIVATE ${CAIRO_INCLUDE_DIRS})
            target_link_libraries(Calculator PRIVATE ${CAIRO_LIBRARIES})
            target_compile_definitions(Calculator PRIVATE HAS_CAIRO=1)
        endif()
    endif()
endif()

# Architecture detection
if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
    target_compile_definitions(Calculator PRIVATE PLATFORM_X86_64=1)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
    target_compile_definitions(Calculator PRIVATE PLATFORM_ARM64=1)
endif()

# Compiler options
target_compile_options(Calculator PRIVATE
    -Wall
    -Wextra
    -Wno-unused-parameter
    -Wno-sign-compare
    -fno-strict-aliasing
    -O2  # Optimize for performance
)

# Install targets
install(TARGETS Calculator
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)

install(FILES ${CALCULATOR_HEADERS}
    DESTINATION include/Calculator
)

# Add tests if enabled
if(BUILD_TESTS)
    if(EXISTS ${CMAKE_SOURCE_DIR}/tests/test_calculator.c)
        add_executable(test_calculator
            ${CMAKE_SOURCE_DIR}/tests/test_calculator.c
        )

        target_link_libraries(test_calculator
            Calculator
            WindowManager
            EventManager
            QuickDraw
            MemoryManager
        )

        add_test(NAME CalculatorTests
            COMMAND test_calculator
        )
    endif()
endif()