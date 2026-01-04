# =============================================================================
# RayFlow - Voxel BedWars Game
# =============================================================================
cmake_minimum_required(VERSION 3.21)

project(rayflow
    VERSION 0.0.0
    DESCRIPTION "Voxel-based BedWars game"
    LANGUAGES C CXX
)

# =============================================================================
# Include CMake modules
# =============================================================================
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

include(cmake/RayflowConfig.cmake)
include(cmake/RayflowDependencies.cmake)
include(cmake/RayflowPlatform.cmake)

# =============================================================================
# Engine (game-agnostic core + tools)
# =============================================================================
add_subdirectory(engine)

# =============================================================================
# Games
# =============================================================================
add_subdirectory(games/bedwars)

# =============================================================================
# Tests (optional)
# =============================================================================
if(RAYFLOW_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# =============================================================================
# Asset deployment - Each game manages its own resources in games/[game]/resources/
# See games/bedwars/CMakeLists.txt for example
# =============================================================================

# =============================================================================
# Installation (for packaging)
# =============================================================================
# Main game is now in games/bedwars/

if(RAYFLOW_BUILD_MAP_EDITOR)
    install(TARGETS map_builder
        RUNTIME DESTINATION bin
        BUNDLE DESTINATION .
    )
endif()

if(RAYFLOW_USE_PAK)
    install(FILES ${RAYFLOW_PAK_FILE}
        DESTINATION bin
    )
endif()

# =============================================================================
# Summary
# =============================================================================
message(STATUS "")
message(STATUS "========================================")
message(STATUS "RayFlow Configuration Summary")
message(STATUS "========================================")
message(STATUS "  Version:          ${RAYFLOW_VERSION}")
message(STATUS "  Platform:         ${RAYFLOW_PLATFORM}")
message(STATUS "  Build type:       ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ Standard:     ${CMAKE_CXX_STANDARD}")
message(STATUS "  Generator:        ${CMAKE_GENERATOR}")
message(STATUS "")
message(STATUS "  Options:")
message(STATUS "    USE_PAK:        ${RAYFLOW_USE_PAK}")
message(STATUS "    BUILD_TESTS:    ${RAYFLOW_BUILD_TESTS}")
message(STATUS "    MAP_EDITOR:     ${RAYFLOW_BUILD_MAP_EDITOR}")
message(STATUS "========================================")
message(STATUS "")
