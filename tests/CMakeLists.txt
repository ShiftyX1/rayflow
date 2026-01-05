# Tests CMakeLists.txt
# This file configures the test suite for the rayflow voxel BedWars engine.
# 
# Build tests with: cmake -B build -DRAYFLOW_BUILD_TESTS=ON
# Run tests with: cd build && ctest --output-on-failure

cmake_minimum_required(VERSION 3.15)

# Fetch Catch2 test framework
include(FetchContent)
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.5.2
)
FetchContent_MakeAvailable(Catch2)

# ==============================================================================
# TODO: Add tests for new architecture
# ==============================================================================
# Tests for engine/, games/bedwars/ should be added here.
# For now, tests are disabled until the migration is complete.

message(STATUS "Tests: Legacy tests removed. New tests TBD.")
