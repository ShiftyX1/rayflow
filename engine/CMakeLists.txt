# Engine Library - Game-agnostic core
cmake_minimum_required(VERSION 3.20)

# =============================================================================
# Engine Options
# =============================================================================

# Auto-detect PAK mode based on build type
if(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    set(ENGINE_USE_PAK ON CACHE INTERNAL "Use PAK archives for assets")
else()
    set(ENGINE_USE_PAK OFF CACHE INTERNAL "Use PAK archives for assets")
endif()

message(STATUS "[Engine] Use PAK: ${ENGINE_USE_PAK}")

# =============================================================================
# Engine Core (headless - no raylib dependency)
# =============================================================================
add_library(engine_core STATIC
    # Core
    core/types.hpp
    core/game_interface.hpp
    core/byte_buffer.hpp
    core/server_engine.hpp
    
    # Scripting (Lua via sol2)
    core/scripting/scripting.hpp
    core/scripting/lua_state.hpp
    core/scripting/lua_state.cpp
    core/scripting/sandbox.hpp
    core/scripting/sandbox.cpp
    core/scripting/script_types.hpp
    core/scripting/script_engine_base.hpp
    core/scripting/script_engine_base.cpp
    
    # VFS (Virtual File System)
    vfs/pak_format.hpp
    vfs/vfs.hpp
    vfs/vfs.cpp
    vfs/archive_reader.hpp
    vfs/archive_reader.cpp
    vfs/archive_writer.hpp
    vfs/archive_writer.cpp
    
    # Transport - Local
    transport/transport.hpp
    transport/local_transport.hpp
    transport/local_transport.cpp
    
    # Transport - ENet (native implementation)
    transport/enet_common.hpp
    transport/enet_common.cpp
    transport/enet_client.hpp
    transport/enet_client.cpp
    transport/enet_server.hpp
    transport/enet_server.cpp
    
    # Maps I/O (no raylib dependency, used by both client and server)
    maps/rfmap_io.hpp
    maps/rfmap_io.cpp
    maps/runtime_paths.hpp
    maps/runtime_paths.cpp
    
    # Shared voxel types (needed by maps)
    modules/voxel/shared/block.hpp
    modules/voxel/shared/block_state.hpp
    modules/voxel/shared/block_shape.hpp
    
    # Universal ECS components (headless)
    ecs/components/common.hpp
    
    # Universal ECS systems (headless) - 2D physics, collision, AI
    ecs/systems/physics2d_system.hpp
    ecs/systems/physics2d_system.cpp
    ecs/systems/collision2d_system.hpp
    ecs/systems/collision2d_system.cpp
    ecs/systems/ai_system.hpp
    ecs/systems/ai_system.cpp
)

target_include_directories(engine_core PUBLIC 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
    $<BUILD_INTERFACE:${enet_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_compile_features(engine_core PUBLIC cxx_std_20)
target_link_libraries(engine_core PUBLIC enet EnTT::EnTT)

# Link LuaJIT and sol2 for scripting
rayflow_link_lua(engine_core)

if(WIN32)
    target_link_libraries(engine_core PUBLIC ws2_32 winmm)
endif()

# =============================================================================
# Engine Client (with raylib - for graphical games)
# Base client without game-specific modules
# =============================================================================
add_library(engine_client STATIC
    # Core engine
    core/client_engine.hpp
    core/client_engine.cpp
    
    # Client core
    client/core/config.cpp
    client/core/logger.cpp
    client/core/resources.cpp
    
    # ECS (universal - moved from client/ecs/)
    ecs/components.cpp
    ecs/systems/input_system.cpp
    ecs/systems/physics_system.cpp
    ecs/systems/player_system.cpp
    ecs/systems/render_system.cpp
    
    # Universal 2D rendering components and systems
    ecs/components/rendering.hpp
    ecs/systems/sprite_system.hpp
    ecs/systems/sprite_system.cpp
    ecs/systems/particle_system.hpp
    ecs/systems/particle_system.cpp
    ecs/systems/camera2d_system.hpp
    ecs/systems/camera2d_system.cpp
    
    # Renderer (universal - moved from client/renderer/)
    renderer/mesh.cpp
    renderer/skybox.cpp
)

target_include_directories(engine_client PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/engine>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(engine_client PUBLIC engine_core raylib EnTT::EnTT)

# Set PAK mode compile definition
if(ENGINE_USE_PAK)
    target_compile_definitions(engine_client PUBLIC RAYFLOW_USE_PAK=1)
else()
    target_compile_definitions(engine_client PUBLIC RAYFLOW_USE_PAK=0)
endif()

# =============================================================================
# Engine UI (with raylib - UI framework)
# =============================================================================
add_library(engine_ui STATIC
    ui/runtime/ui_manager.cpp
    ui/runtime/xmlui/css_lite.cpp
    ui/runtime/xmlui/ui_document.cpp
    ui/scripting/ui_script_engine.cpp
)

target_include_directories(engine_ui PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/engine>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(engine_ui PUBLIC engine_client tinyxml2)

# Debug UI (raygui) - available in all builds
target_sources(engine_ui PRIVATE
    ui/debug/debug_ui.cpp
    ui/debug/raygui_impl.cpp
)

# Enable DEBUG_UI in Debug builds
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(engine_ui PUBLIC DEBUG_UI=1)
endif()

# =============================================================================
# MODULES - Optional game-specific functionality
# =============================================================================

# -----------------------------------------------------------------------------
# Voxel Module - For voxel-based games (Minecraft-style)
# Client-side rendering only
# -----------------------------------------------------------------------------
add_library(engine_voxel STATIC
    # Client-side voxel rendering
    modules/voxel/client/world.cpp
    modules/voxel/client/chunk.cpp
    modules/voxel/client/block_registry.cpp
    modules/voxel/client/block_interaction.cpp
    modules/voxel/client/block_model_loader.cpp
    modules/voxel/client/texture_atlas.cpp
)

target_include_directories(engine_voxel PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/engine>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(engine_voxel PUBLIC engine_client)

# =============================================================================
# Alias for backwards compatibility
# =============================================================================
add_library(engine_lib ALIAS engine_core)

# Engine should NOT depend on game-specific code

# =============================================================================
# Tools
# =============================================================================
add_subdirectory(modules/voxel/tools/map_editor)

# Asset packer tool (no raylib dependency)
if(RAYFLOW_BUILD_PACK_ASSETS)
    add_executable(rayflow_pack_assets
        tools/pack_assets.cpp
    )
    target_include_directories(rayflow_pack_assets PRIVATE ${CMAKE_SOURCE_DIR})
    target_link_libraries(rayflow_pack_assets PRIVATE engine_core)
endif()
