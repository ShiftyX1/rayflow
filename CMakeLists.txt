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
# Subdirectories
# =============================================================================
add_subdirectory(shared)
add_subdirectory(server)
add_subdirectory(client)
add_subdirectory(app)
add_subdirectory(tools)

# =============================================================================
# Tests (optional)
# =============================================================================
if(RAYFLOW_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# =============================================================================
# Asset deployment
# =============================================================================
set(RAYFLOW_ASSETS_SRC ${CMAKE_SOURCE_DIR}/client/static)
set(RAYFLOW_PAK_FILE ${CMAKE_BINARY_DIR}/assets.pak)

if(RAYFLOW_USE_PAK AND RAYFLOW_BUILD_PACK_ASSETS)
    # Release mode: Pack assets into .pak file
    add_custom_target(rayflow_pack_assets_target ALL
        COMMAND $<TARGET_FILE:rayflow_pack_assets>
            --input ${RAYFLOW_ASSETS_SRC}
            --output ${RAYFLOW_PAK_FILE}
            --verbose
        DEPENDS rayflow_pack_assets
        COMMENT "Packing assets into ${RAYFLOW_PAK_FILE}"
        VERBATIM
    )
    add_dependencies(rayflow rayflow_pack_assets_target)
    if(RAYFLOW_BUILD_MAP_EDITOR)
        add_dependencies(map_builder rayflow_pack_assets_target)
    endif()
else()
    # Debug mode: Copy loose files
    add_custom_target(rayflow_copy_assets ALL
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${RAYFLOW_ASSETS_SRC}/textures
            ${CMAKE_BINARY_DIR}/textures
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${RAYFLOW_ASSETS_SRC}/shaders
            ${CMAKE_BINARY_DIR}/shaders
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${RAYFLOW_ASSETS_SRC}/fonts
            ${CMAKE_BINARY_DIR}/fonts
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${RAYFLOW_ASSETS_SRC}/ui
            ${CMAKE_BINARY_DIR}/ui
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${RAYFLOW_ASSETS_SRC}/models
            ${CMAKE_BINARY_DIR}/models
        COMMENT "Copying loose assets to build directory"
        VERBATIM
    )
    add_dependencies(rayflow rayflow_copy_assets)
    if(RAYFLOW_BUILD_MAP_EDITOR)
        add_dependencies(map_builder rayflow_copy_assets)
    endif()
endif()

# =============================================================================
# Installation (for packaging)
# =============================================================================
install(TARGETS rayflow
    RUNTIME DESTINATION bin
    BUNDLE DESTINATION .
)

if(RAYFLOW_BUILD_MAP_EDITOR)
    install(TARGETS map_builder
        RUNTIME DESTINATION bin
        BUNDLE DESTINATION .
    )
endif()

if(RAYFLOW_BUILD_DEDICATED_SERVER)
    install(TARGETS rfds
        RUNTIME DESTINATION bin
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
message(STATUS "    DEDICATED_SRV:  ${RAYFLOW_BUILD_DEDICATED_SERVER}")
message(STATUS "========================================")
message(STATUS "")
