# BedWars Game - Uses engine_lib
cmake_minimum_required(VERSION 3.20)

# =============================================================================
# Build Options (inherited from engine's ENGINE_USE_PAK)
# =============================================================================

message(STATUS "[BedWars] Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "[BedWars] Use PAK (from engine): ${ENGINE_USE_PAK}")

# =============================================================================
# BedWars Shared Library (protocol, types, constants)
# =============================================================================
add_library(bedwars_shared STATIC
    shared/protocol/messages.hpp
    shared/protocol/serialization.hpp
    shared/protocol/serialization.cpp
    
    # Game types (moved from shared/game/)
    shared/game/item_types.hpp
    shared/game/team_types.hpp
    shared/game/generator_types.hpp
    shared/constants.hpp
)

target_include_directories(bedwars_shared PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(bedwars_shared PUBLIC engine_core)

# =============================================================================
# BedWars Server Library
# =============================================================================
add_library(bedwars_server STATIC
    server/bedwars_server.hpp
    server/bedwars_server.cpp
    
    # BedWars-specific scripting
    server/scripting/bedwars_script_engine.hpp
    server/scripting/bedwars_script_engine.cpp
    server/scripting/bedwars_api.hpp
    server/scripting/bedwars_api.cpp
    
    # Voxel terrain (moved from server/voxel/)
    server/voxel/terrain.hpp
    server/voxel/terrain.cpp
    
    # Game logic (moved from server/game/)
    server/game/inventory.hpp
    server/game/inventory.cpp
    server/game/player_state.hpp
    server/game/player_state.cpp
    server/game/team.hpp
    server/game/team.cpp
    server/game/generator.hpp
    server/game/generator.cpp
    server/game/bed.hpp
    server/game/bed.cpp
    server/game/game_state.hpp
    server/game/game_state.cpp
)

target_include_directories(bedwars_server PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}
)

# Link with engine_core (no more server_lib dependency)
target_link_libraries(bedwars_server PUBLIC bedwars_shared engine_core)

# =============================================================================
# BedWars Client Library
# =============================================================================
add_library(bedwars_client STATIC
    client/bedwars_client.hpp
    client/bedwars_client.cpp
)

target_include_directories(bedwars_client PUBLIC 
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/engine
)

target_link_libraries(bedwars_client PUBLIC bedwars_shared engine_client engine_ui engine_voxel)

# Note: RAYFLOW_USE_PAK is inherited from engine_client

# =============================================================================
# BedWars Dedicated Server Executable
# =============================================================================
add_executable(bedwars_server_demo
    app/server_main.cpp
)
target_link_libraries(bedwars_server_demo PRIVATE bedwars_server engine_core)

set_target_properties(bedwars_server_demo PROPERTIES
    OUTPUT_NAME "bedwars_server"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)

# =============================================================================
# BedWars Client Executable
# =============================================================================
add_executable(bedwars_client_demo
    app/client_main.cpp
)
# Link with bedwars_server for singleplayer embedded server support
target_link_libraries(bedwars_client_demo PRIVATE bedwars_client bedwars_server engine_client engine_ui)

set_target_properties(bedwars_client_demo PROPERTIES
    OUTPUT_NAME "bedwars"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)

# Note: RAYFLOW_USE_PAK is inherited from engine_client via bedwars_client

# =============================================================================
# Asset Packing Tool (for Release builds)
# =============================================================================
add_executable(bedwars_pack_assets
    tools/pack_assets.cpp
)
target_link_libraries(bedwars_pack_assets PRIVATE engine_core)

set_target_properties(bedwars_pack_assets PROPERTIES
    OUTPUT_NAME "bedwars_pack_assets"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)

# =============================================================================
# Copy Assets to Build Directory
# =============================================================================

# Game resources directory
set(BEDWARS_RESOURCES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/resources")

# Engine resources (fallback for shared assets)
set(ENGINE_RESOURCES_DIR "${CMAKE_SOURCE_DIR}/client/static")

# Asset directories to copy
set(BEDWARS_ASSET_DIRS
    textures
    shaders
    fonts
    ui
    models
    sounds
)

# In debug mode: copy loose files
if(NOT ENGINE_USE_PAK)
    # Copy game-specific resources first
    foreach(dir ${BEDWARS_ASSET_DIRS})
        if(EXISTS "${BEDWARS_RESOURCES_DIR}/${dir}")
            add_custom_command(TARGET bedwars_client_demo POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${BEDWARS_RESOURCES_DIR}/${dir}" "$<TARGET_FILE_DIR:bedwars_client_demo>/${dir}"
                COMMENT "[BedWars] Copying resources/${dir} to build directory"
            )
        endif()
    endforeach()
    
    # Copy engine resources (for assets not overridden by game)
    foreach(dir ${BEDWARS_ASSET_DIRS})
        if(EXISTS "${ENGINE_RESOURCES_DIR}/${dir}")
            add_custom_command(TARGET bedwars_client_demo POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${ENGINE_RESOURCES_DIR}/${dir}" "$<TARGET_FILE_DIR:bedwars_client_demo>/${dir}"
                COMMENT "[BedWars] Copying engine/${dir} to build directory"
            )
        endif()
    endforeach()
endif()

# In release mode: pack into .pak file
if(ENGINE_USE_PAK)
    # First, copy all resources to a staging directory
    set(BEDWARS_STAGING_DIR "${CMAKE_BINARY_DIR}/bedwars_staging")
    
    add_custom_target(bedwars_stage_assets
        COMMAND ${CMAKE_COMMAND} -E remove_directory "${BEDWARS_STAGING_DIR}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${BEDWARS_STAGING_DIR}"
        COMMENT "[BedWars] Preparing staging directory"
    )
    
    # Copy engine resources to staging
    foreach(dir ${BEDWARS_ASSET_DIRS})
        if(EXISTS "${ENGINE_RESOURCES_DIR}/${dir}")
            add_custom_command(TARGET bedwars_stage_assets POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${ENGINE_RESOURCES_DIR}/${dir}" "${BEDWARS_STAGING_DIR}/${dir}"
                COMMENT "[BedWars] Staging engine/${dir}"
            )
        endif()
    endforeach()
    
    # Copy game resources to staging (overrides engine)
    foreach(dir ${BEDWARS_ASSET_DIRS})
        if(EXISTS "${BEDWARS_RESOURCES_DIR}/${dir}")
            add_custom_command(TARGET bedwars_stage_assets POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${BEDWARS_RESOURCES_DIR}/${dir}" "${BEDWARS_STAGING_DIR}/${dir}"
                COMMENT "[BedWars] Staging resources/${dir}"
            )
        endif()
    endforeach()
    
    # Pack assets after staging
    add_custom_target(bedwars_pack_assets_run
        DEPENDS bedwars_pack_assets bedwars_stage_assets
        COMMAND bedwars_pack_assets 
            --input "${BEDWARS_STAGING_DIR}"
            --output "${CMAKE_BINARY_DIR}/assets.pak"
            --verbose
        COMMENT "[BedWars] Packing assets into assets.pak"
    )
    
    # Make client depend on packed assets
    add_dependencies(bedwars_client_demo bedwars_pack_assets_run)
endif()

# =============================================================================
# Install Rules (for packaging)
# =============================================================================
install(TARGETS bedwars_client_demo bedwars_server_demo
    RUNTIME DESTINATION bin
)

if(ENGINE_USE_PAK)
    # Install PAK file
    install(FILES "${CMAKE_BINARY_DIR}/assets.pak"
        DESTINATION bin
        OPTIONAL
    )
else()
    # Install loose assets
    foreach(dir ${BEDWARS_ASSET_DIRS})
        if(EXISTS "${CMAKE_SOURCE_DIR}/${dir}")
            install(DIRECTORY "${CMAKE_SOURCE_DIR}/${dir}"
                DESTINATION bin
            )
        endif()
    endforeach()
endif()


