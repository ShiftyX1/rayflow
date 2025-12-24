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
# Shared Layer Tests (no raylib dependency)
# ==============================================================================
add_executable(rayflow_tests_shared
    shared/protocol/test_messages.cpp
    shared/transport/test_local_transport.cpp
    shared/voxel/test_block.cpp
    shared/maps/test_rfmap_io.cpp
    shared/vfs/vfs_test.cpp
)

# IMPORTANT: Clear inherited global include directories and set only what's needed
# This prevents picking up client/voxel/block.hpp instead of shared/voxel/block.hpp
set_property(TARGET rayflow_tests_shared PROPERTY INCLUDE_DIRECTORIES "")
target_include_directories(rayflow_tests_shared PRIVATE
    ${CMAKE_SOURCE_DIR}/shared
    ${CMAKE_CURRENT_SOURCE_DIR}/helpers
)

target_link_libraries(rayflow_tests_shared PRIVATE
    Catch2::Catch2WithMain
)

# Link shared_lib but don't inherit its interface includes (we set them explicitly above)
target_link_libraries(rayflow_tests_shared PRIVATE $<TARGET_FILE:shared_lib>)

# ==============================================================================
# Server Layer Tests (no raylib dependency)
# ==============================================================================
add_executable(rayflow_tests_server
    server/core/test_server_lifecycle.cpp
    server/core/test_session_flow.cpp
    server/validation/test_block_validation.cpp
    server/simulation/test_physics.cpp
    server/simulation/test_determinism.cpp
    server/voxel/test_terrain.cpp
)

# IMPORTANT: Clear inherited global include directories to avoid client headers
set_property(TARGET rayflow_tests_server PROPERTY INCLUDE_DIRECTORIES "")
target_include_directories(rayflow_tests_server PRIVATE
    ${CMAKE_SOURCE_DIR}/server
    ${CMAKE_SOURCE_DIR}/shared
    ${CMAKE_CURRENT_SOURCE_DIR}/helpers
)

target_link_libraries(rayflow_tests_server PRIVATE
    Catch2::Catch2WithMain
)
target_link_libraries(rayflow_tests_server PRIVATE $<TARGET_FILE:server_lib> $<TARGET_FILE:shared_lib>)

# ==============================================================================
# Client Layer Tests (mocked rendering, no actual raylib calls)
# ==============================================================================
# NOTE: Client tests require careful mocking of raylib dependencies.
# For now, only test components that don't directly call raylib.
add_executable(rayflow_tests_client
    client/net/test_client_session.cpp
    # client/voxel/test_client_world.cpp  # Add when ClientWorld is testable without raylib
)

# IMPORTANT: Clear inherited global include directories
set_property(TARGET rayflow_tests_client PROPERTY INCLUDE_DIRECTORIES "")
target_include_directories(rayflow_tests_client PRIVATE
    ${CMAKE_SOURCE_DIR}/shared
    ${CMAKE_CURRENT_SOURCE_DIR}/helpers
)

target_link_libraries(rayflow_tests_client PRIVATE
    Catch2::Catch2WithMain
)
target_link_libraries(rayflow_tests_client PRIVATE $<TARGET_FILE:shared_lib>)

# ==============================================================================
# Integration Tests (full client-server via LocalTransport)
# ==============================================================================
add_executable(rayflow_tests_integration
    integration/session/test_full_session.cpp
    integration/replication/test_replication.cpp
)

# IMPORTANT: Clear inherited global include directories
set_property(TARGET rayflow_tests_integration PROPERTY INCLUDE_DIRECTORIES "")
target_include_directories(rayflow_tests_integration PRIVATE
    ${CMAKE_SOURCE_DIR}/server
    ${CMAKE_SOURCE_DIR}/shared
    ${CMAKE_CURRENT_SOURCE_DIR}/helpers
)

target_link_libraries(rayflow_tests_integration PRIVATE
    Catch2::Catch2WithMain
)
target_link_libraries(rayflow_tests_integration PRIVATE $<TARGET_FILE:server_lib> $<TARGET_FILE:shared_lib>)

# ==============================================================================
# CTest Integration
# ==============================================================================
include(CTest)
include(Catch)

catch_discover_tests(rayflow_tests_shared
    TEST_PREFIX "shared::"
    REPORTER console
)

catch_discover_tests(rayflow_tests_server
    TEST_PREFIX "server::"
    REPORTER console
)

catch_discover_tests(rayflow_tests_client
    TEST_PREFIX "client::"
    REPORTER console
)

catch_discover_tests(rayflow_tests_integration
    TEST_PREFIX "integration::"
    REPORTER console
)

# ==============================================================================
# Coverage Target (optional)
# ==============================================================================
option(RAYFLOW_TEST_COVERAGE "Enable coverage reporting" OFF)

if (RAYFLOW_TEST_COVERAGE AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    message(STATUS "Enabling test coverage")
    
    target_compile_options(rayflow_tests_shared PRIVATE --coverage -O0 -g)
    target_link_options(rayflow_tests_shared PRIVATE --coverage)
    
    target_compile_options(rayflow_tests_server PRIVATE --coverage -O0 -g)
    target_link_options(rayflow_tests_server PRIVATE --coverage)
    
    target_compile_options(rayflow_tests_client PRIVATE --coverage -O0 -g)
    target_link_options(rayflow_tests_client PRIVATE --coverage)
    
    target_compile_options(rayflow_tests_integration PRIVATE --coverage -O0 -g)
    target_link_options(rayflow_tests_integration PRIVATE --coverage)
endif()
