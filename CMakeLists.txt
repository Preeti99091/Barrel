cmake_minimum_required(VERSION 3.16)
project(Barrel VERSION 1.0.0 LANGUAGES C)

# C Standard
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Options
option(BUILD_SAMPLES "Build sample executables" ON)
option(BUILD_TOOLS   "Build tool executables (builder/dumper)" ON)
option(BUILD_SHARED_LIBS "Build Barrel as shared library" ON)

# Barrel library
file(GLOB BARREL_SOURCES "${CMAKE_SOURCE_DIR}/src/*.c")

add_library(Barrel ${BARREL_SOURCES})

# Handle library visibility macros
if(BUILD_SHARED_LIBS)
    # Define BUILD_LIBTYPE_SHARED
    target_compile_definitions(Barrel PRIVATE BUILD_LIBTYPE_SHARED)
    
    # Define USE_LIBTYPE_SHARED
    target_compile_definitions(Barrel INTERFACE USE_LIBTYPE_SHARED)
    
    # Set default visibility to hidden for non-marked symbols
    if(NOT MSVC)
        target_compile_options(Barrel PRIVATE -fvisibility=hidden)
    endif()
endif()

target_include_directories(Barrel
        PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# Compiler warnings
if(MSVC)
    target_compile_options(Barrel PRIVATE /W4 /WX)
else()
    target_compile_options(Barrel PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

# Sample executables
if(BUILD_SAMPLES)
    file(GLOB SAMPLE_SOURCES "${CMAKE_SOURCE_DIR}/sample/*.c")
    foreach(SRC_FILE ${SAMPLE_SOURCES})
        get_filename_component(EXE_NAME ${SRC_FILE} NAME_WE)
        add_executable(${EXE_NAME} ${SRC_FILE})
        target_link_libraries(${EXE_NAME} PRIVATE Barrel)
    endforeach()
endif()

# Tool executables
if(BUILD_TOOLS)
    add_subdirectory(tools/builder)
    add_subdirectory(tools/dumper)
endif()

# Install Barrel library
install(TARGETS Barrel
        EXPORT BarrelTargets
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib
        RUNTIME DESTINATION bin
        INCLUDES DESTINATION include
)

install(DIRECTORY include/
        DESTINATION include
        FILES_MATCHING PATTERN "*.h"
)

# Export for find_package
install(EXPORT BarrelTargets
        FILE BarrelTargets.cmake
        NAMESPACE Barrel::
        DESTINATION lib/cmake/Barrel
)
