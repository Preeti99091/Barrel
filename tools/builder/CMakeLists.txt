cmake_minimum_required(VERSION 3.16)
project(BRLBuilder LANGUAGES C)

# -----------------------------
# C Standard
# -----------------------------
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# -----------------------------
# FetchContent for argtable3
# -----------------------------
include(FetchContent)

FetchContent_Declare(
        argtable3
        GIT_REPOSITORY https://github.com/argtable/argtable3.git
        GIT_TAG        master
)

# Download and make available
FetchContent_MakeAvailable(argtable3)

# -----------------------------
# Tool sources
# -----------------------------
file(GLOB TOOL_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.c")

# -----------------------------
# Executable
# -----------------------------
add_executable(buildbrl ${TOOL_SOURCES})

# Include directories
target_include_directories(buildbrl
        PRIVATE
        ${CMAKE_SOURCE_DIR}/include     # Barrel headers
)

# -----------------------------
# Link libraries
# -----------------------------
target_link_libraries(buildbrl
        PRIVATE
        Barrel          # from parent project
        argtable3       # argument parsing
        pthread
)
