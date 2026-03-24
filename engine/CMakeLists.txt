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

# DX11 renderer — Windows only, auto-detected
if(WIN32)
    option(RAYFLOW_RENDERER_DX11 "Build DirectX 11 render backend" ON)
else()
    set(RAYFLOW_RENDERER_DX11 OFF)
endif()
message(STATUS "[Engine] DirectX 11: ${RAYFLOW_RENDERER_DX11}")

# =============================================================================
# Engine Core (headless - no raylib dependency)
# =============================================================================
add_library(engine_core SHARED
    # Core
    core/export.hpp
    core/types.hpp
    core/game_interface.hpp
    core/game_interface.cpp
    core/byte_buffer.hpp
    core/server_engine.hpp
    core/server_engine.cpp
    core/logging.cpp
    
    # Scripting (Lua via sol2)
    core/scripting/scripting.hpp
    core/scripting/lua_state.hpp
    core/scripting/lua_state.cpp
    core/scripting/lua_state_call.hpp
    core/scripting/sandbox.hpp
    core/scripting/sandbox.cpp
    core/scripting/script_types.hpp
    core/scripting/script_engine_base.hpp
    core/scripting/script_engine_base.cpp
    core/scripting/script_api_module.hpp
    
    # Scripting API modules (generic, game-agnostic)
    core/scripting/api/utils.hpp
    core/scripting/api/utils.cpp
    core/scripting/api/engine_api.hpp
    core/scripting/api/engine_api.cpp
    core/scripting/api/math_api.hpp
    core/scripting/api/math_api.cpp
    core/scripting/api/timer_api.hpp
    core/scripting/api/timer_api.cpp
    core/scripting/api/events_api.hpp
    core/scripting/api/events_api.cpp
    
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
    modules/voxel/shared/block_data.hpp
    modules/voxel/shared/block_data.cpp
    modules/voxel/shared/block_state.hpp
    modules/voxel/shared/block_shape.hpp
    
    # ECS base class (out-of-line destructor for DLL export)
    ecs/system.hpp
    ecs/system.cpp

    # Universal ECS components (headless)
    ecs/components/common.hpp
    
    # Explicit EnTT type_index instantiations for cross-DLL ECS
    ecs/ecs_type_registry.cpp

    # Developer console (log sink + Lua state)
    core/console/console_log_sink.hpp
    core/console/console_log_sink.cpp
    core/console/console_lua_state.hpp
    core/console/console_lua_state.cpp
)

target_include_directories(engine_core PUBLIC 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
    $<BUILD_INTERFACE:${enet_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_compile_features(engine_core PUBLIC cxx_std_20)
target_compile_definitions(engine_core PRIVATE RAYFLOW_CORE_EXPORTS ENTT_API_EXPORT)
target_compile_definitions(engine_core INTERFACE ENTT_API_IMPORT)
set_target_properties(engine_core PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)
target_link_libraries(engine_core PUBLIC enet EnTT::EnTT)

# Link LuaJIT and sol2 for scripting
rayflow_link_lua(engine_core)

if(WIN32)
    target_link_libraries(engine_core PUBLIC ws2_32 winmm)
endif()

# =============================================================================
# Engine Client (GLFW + OpenGL — graphical games)
# Base client without game-specific modules
# =============================================================================
add_library(engine_client SHARED
    # Client core
    client/core/window.cpp
    client/core/input.cpp
    client/core/config.cpp
    client/core/logger.cpp
    client/core/resources.cpp
    
    # ECS (universal - moved from client/ecs/)
    ecs/components.cpp
    
    # 2D rendering components (header-only, kept for type definitions)
    ecs/components/rendering.hpp
    
    # Renderer (universal - moved from client/renderer/)
    renderer/gpu/render_device.cpp
    renderer/gl_render_device.cpp
    renderer/gl_shader.cpp
    renderer/gl_texture.cpp
    renderer/gl_framebuffer.cpp
    renderer/gl_mesh.cpp
    renderer/gl_font.cpp
    renderer/batch_2d.cpp
    renderer/camera.cpp
    renderer/stb_impl.cpp
    renderer/mesh.cpp
    renderer/skybox.cpp
    renderer/render_pipeline.cpp
)

target_include_directories(engine_client PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/engine>
    $<INSTALL_INTERFACE:include>
)

target_compile_definitions(engine_client PRIVATE RAYFLOW_CLIENT_EXPORTS)
if(APPLE)
    target_compile_definitions(engine_client PRIVATE GL_SILENCE_DEPRECATION)
endif()
set_target_properties(engine_client PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)
target_link_libraries(engine_client PUBLIC engine_core glfw glad glm::glm stb_headers EnTT::EnTT)

# DX11 render backend (Windows only)
if(RAYFLOW_RENDERER_DX11)
    target_sources(engine_client PRIVATE
        renderer/dx11/dx11_render_device.cpp
        renderer/dx11/dx11_shader.cpp
        renderer/dx11/dx11_texture.cpp
        renderer/dx11/dx11_mesh.cpp
        renderer/dx11/dx11_framebuffer.cpp
    )
    target_compile_definitions(engine_client PUBLIC RAYFLOW_HAS_DX11=1)
    target_link_libraries(engine_client PUBLIC d3d11 dxgi d3dcompiler)
endif()

# Dear ImGui — compiled directly into engine_client so that all ImGui global
# state (GImGui, backend data) lives in a single DLL.  Symbols are exported
# via IMGUI_API so that game executables can call ImGui functions.
target_sources(engine_client PRIVATE
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)
if(RAYFLOW_RENDERER_DX11)
    target_sources(engine_client PRIVATE
        ${imgui_SOURCE_DIR}/backends/imgui_impl_dx11.cpp
    )
endif()
target_include_directories(engine_client PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)
# When compiling engine_client, export ImGui symbols; consumers import them.
if(WIN32)
    target_compile_definitions(engine_client
        PRIVATE "IMGUI_API=__declspec(dllexport)"
    )
    target_compile_definitions(engine_client
        INTERFACE "IMGUI_API=__declspec(dllimport)"
    )
else()
    target_compile_definitions(engine_client
        PUBLIC "IMGUI_API=__attribute__((visibility(\"default\")))"
    )
endif()

# Debug UI (Dear ImGui panels) - compiled in engine_client alongside GLFW/glad
target_sources(engine_client PRIVATE
    ui/debug/debug_ui.cpp
    ui/debug/console_panel.cpp
)

# Enable DEBUG_UI in Debug builds
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(engine_client PUBLIC DEBUG_UI=1)
endif()

# Set PAK mode compile definition
if(ENGINE_USE_PAK)
    target_compile_definitions(engine_client PUBLIC RAYFLOW_USE_PAK=1)
else()
    target_compile_definitions(engine_client PUBLIC RAYFLOW_USE_PAK=0)
endif()

# =============================================================================
# Engine UI framework
# =============================================================================
add_library(engine_ui SHARED
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

target_compile_definitions(engine_ui PRIVATE RAYFLOW_UI_EXPORTS)
set_target_properties(engine_ui PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)
target_link_libraries(engine_ui PUBLIC engine_client tinyxml2)

# =============================================================================
# MODULES - Optional game-specific functionality
# =============================================================================

# -----------------------------------------------------------------------------
# Voxel Module - For voxel-based games (Minecraft-style)
# Client-side rendering only
# -----------------------------------------------------------------------------
add_library(engine_voxel SHARED
    # Client-side voxel rendering
    modules/voxel/client/world.cpp
    modules/voxel/client/chunk.cpp
    modules/voxel/client/block_registry.cpp
    modules/voxel/client/block_interaction.cpp
    modules/voxel/client/block_model_loader.cpp
    modules/voxel/client/texture_atlas.cpp
    
    # Voxel-dependent ECS systems (use voxel::World directly)
    ecs/systems/render_system.cpp
)

target_include_directories(engine_voxel PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/engine>
    $<INSTALL_INTERFACE:include>
)

target_compile_definitions(engine_voxel PRIVATE RAYFLOW_VOXEL_EXPORTS)
set_target_properties(engine_voxel PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)
target_link_libraries(engine_voxel PUBLIC engine_client)

# =============================================================================
# Engine App (glue layer — ClientEngine + all modules)
# ClientEngine depends on core, client, ui, and voxel, so it lives in a
# separate library that sits on top of all modules, breaking the circular
# dependency that would otherwise exist between engine_client and engine_voxel/ui.
# =============================================================================
add_library(engine_app SHARED
    core/client_engine.hpp
    core/client_engine.cpp
)

target_include_directories(engine_app PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/engine>
    $<INSTALL_INTERFACE:include>
)

target_compile_definitions(engine_app PRIVATE RAYFLOW_APP_EXPORTS)
set_target_properties(engine_app PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)
target_link_libraries(engine_app PUBLIC engine_core engine_client engine_ui engine_voxel)

# =============================================================================
# Alias for backwards compatibility
# =============================================================================
add_library(engine_lib ALIAS engine_core)

# Engine should NOT depend on game-specific code

# =============================================================================
# Deprecated 2D Systems (disabled by default)
# =============================================================================
option(RAYFLOW_BUILD_DEPRECATED_SYSTEMS "Build deprecated 2D ECS systems (sprite, particle, camera2d, physics2d, collision2d, ai)" OFF)
if(RAYFLOW_BUILD_DEPRECATED_SYSTEMS)
    message(WARNING "[Engine] Building DEPRECATED 2D systems — these will be removed in a future release")
    target_sources(engine_core PRIVATE
        ecs/systems/physics2d_system.hpp
        ecs/systems/physics2d_system.cpp
        ecs/systems/collision2d_system.hpp
        ecs/systems/collision2d_system.cpp
        ecs/systems/ai_system.hpp
        ecs/systems/ai_system.cpp
    )
    target_sources(engine_client PRIVATE
        ecs/systems/sprite_system.hpp
        ecs/systems/sprite_system.cpp
        ecs/systems/particle_system.hpp
        ecs/systems/particle_system.cpp
        ecs/systems/camera2d_system.hpp
        ecs/systems/camera2d_system.cpp
    )
    target_compile_definitions(engine_core PRIVATE RAYFLOW_DEPRECATED_SYSTEMS=1)
    target_compile_definitions(engine_client PRIVATE RAYFLOW_DEPRECATED_SYSTEMS=1)
endif()
