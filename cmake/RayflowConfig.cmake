# =============================================================================
# RayflowConfig.cmake
# Project-wide configuration: version, C++ standard, compiler flags
# =============================================================================

# -----------------------------------------------------------------------------
# Version from Git tags
# -----------------------------------------------------------------------------
# Format: vMAJOR.MINOR.PATCH or MAJOR.MINOR.PATCH
# Falls back to 0.0.0-dev if no tags found
find_package(Git QUIET)

if(GIT_FOUND AND EXISTS "${CMAKE_SOURCE_DIR}/.git")
    execute_process(
        COMMAND ${GIT_EXECUTABLE} describe --tags --abbrev=0
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_TAG
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        RESULT_VARIABLE GIT_TAG_RESULT
    )
    
    if(GIT_TAG_RESULT EQUAL 0 AND GIT_TAG)
        # Strip leading 'v' if present
        string(REGEX REPLACE "^v" "" RAYFLOW_VERSION "${GIT_TAG}")
    else()
        set(RAYFLOW_VERSION "0.0.0-dev")
    endif()
    
    # Get commit hash for detailed version info
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_COMMIT
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
else()
    set(RAYFLOW_VERSION "0.0.0-dev")
    set(GIT_COMMIT "unknown")
endif()

# Parse version components
string(REGEX MATCH "^([0-9]+)\\.([0-9]+)\\.([0-9]+)" _version_match "${RAYFLOW_VERSION}")
if(_version_match)
    set(RAYFLOW_VERSION_MAJOR ${CMAKE_MATCH_1})
    set(RAYFLOW_VERSION_MINOR ${CMAKE_MATCH_2})
    set(RAYFLOW_VERSION_PATCH ${CMAKE_MATCH_3})
else()
    set(RAYFLOW_VERSION_MAJOR 0)
    set(RAYFLOW_VERSION_MINOR 0)
    set(RAYFLOW_VERSION_PATCH 0)
endif()

message(STATUS "RayFlow version: ${RAYFLOW_VERSION} (commit: ${GIT_COMMIT})")

# -----------------------------------------------------------------------------
# C++ Standard
# -----------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# -----------------------------------------------------------------------------
# Compiler flags
# -----------------------------------------------------------------------------
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(
        -Wall
        -Wextra
        -Wpedantic
        $<$<CONFIG:Debug>:-g>
        $<$<CONFIG:Debug>:-O0>
        $<$<CONFIG:Release>:-O3>
        $<$<CONFIG:Release>:-DNDEBUG>
    )
elseif(MSVC)
    add_compile_options(
        /W4
        /utf-8
        $<$<CONFIG:Debug>:/Od>
        $<$<CONFIG:Debug>:/Zi>
        $<$<CONFIG:Release>:/O2>
        $<$<CONFIG:Release>:/DNDEBUG>
    )
endif()

# -----------------------------------------------------------------------------
# Build type defaults
# -----------------------------------------------------------------------------
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type" FORCE)
    message(STATUS "Build type not specified, defaulting to Debug")
endif()

# -----------------------------------------------------------------------------
# Output directories
# -----------------------------------------------------------------------------
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# -----------------------------------------------------------------------------
# Export compile commands for IDE support
# -----------------------------------------------------------------------------
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# -----------------------------------------------------------------------------
# Options
# -----------------------------------------------------------------------------
# RAYFLOW_USE_PAK: ON for Release, OFF for Debug (can be overridden)
if(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo" OR CMAKE_BUILD_TYPE STREQUAL "MinSizeRel")
    option(RAYFLOW_USE_PAK "Use packed .pak assets instead of loose files" ON)
else()
    option(RAYFLOW_USE_PAK "Use packed .pak assets instead of loose files" OFF)
endif()

option(RAYFLOW_BUILD_TESTS "Build unit and integration tests" OFF)
option(RAYFLOW_BUILD_MAP_EDITOR "Build rayflow map editor" ON)
option(RAYFLOW_BUILD_DEDICATED_SERVER "Build rayflow dedicated server" ON)
option(RAYFLOW_BUILD_PACK_ASSETS "Build pack_assets tool" ON)

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "RAYFLOW_USE_PAK: ${RAYFLOW_USE_PAK}")
message(STATUS "RAYFLOW_BUILD_TESTS: ${RAYFLOW_BUILD_TESTS}")
message(STATUS "RAYFLOW_BUILD_MAP_EDITOR: ${RAYFLOW_BUILD_MAP_EDITOR}")
message(STATUS "RAYFLOW_BUILD_DEDICATED_SERVER: ${RAYFLOW_BUILD_DEDICATED_SERVER}")
