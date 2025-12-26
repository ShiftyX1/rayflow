# =============================================================================
# client/CMakeLists.txt
# client_lib - Client-side rendering, UI, input (depends on raylib)
# =============================================================================

add_library(client_lib STATIC
    core/game.cpp
    core/config.cpp
    core/logger.cpp
    core/resources.cpp
    net/client_session.cpp
    ecs/components.cpp
    ecs/systems/physics_system.cpp
    ecs/systems/render_system.cpp
    ecs/systems/input_system.cpp
    ecs/systems/player_system.cpp
    voxel/world.cpp
    voxel/chunk.cpp
    voxel/block_registry.cpp
    voxel/block_interaction.cpp
    voxel/texture_atlas.cpp
    renderer/mesh.cpp
    renderer/skybox.cpp
)

target_include_directories(client_lib PUBLIC
    ${CMAKE_SOURCE_DIR}/client
    ${CMAKE_SOURCE_DIR}/shared
    ${CMAKE_SOURCE_DIR}/ui
)

# -----------------------------------------------------------------------------
# Dependencies
# -----------------------------------------------------------------------------
target_link_libraries(client_lib PUBLIC
    shared_lib
    EnTT::EnTT
    tinyxml2
)

# raylib
rayflow_link_raylib(client_lib)

# -----------------------------------------------------------------------------
# UI Runtime
# -----------------------------------------------------------------------------
target_sources(client_lib PRIVATE
    ${CMAKE_SOURCE_DIR}/ui/runtime/ui_manager.cpp
    ${CMAKE_SOURCE_DIR}/ui/runtime/xmlui/css_lite.cpp
    ${CMAKE_SOURCE_DIR}/ui/runtime/xmlui/ui_document.cpp
    ${CMAKE_SOURCE_DIR}/ui/scripting/ui_script_engine.cpp
)

# -----------------------------------------------------------------------------
# Debug UI (raygui) - only for non-Release builds
# -----------------------------------------------------------------------------
if(NOT CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_definitions(client_lib PUBLIC DEBUG_UI)
    target_sources(client_lib PRIVATE
        ${CMAKE_SOURCE_DIR}/ui/debug/debug_ui.cpp
        ${CMAKE_SOURCE_DIR}/ui/debug/raygui_impl.cpp
    )
endif()

# -----------------------------------------------------------------------------
# PAK mode compile definition
# -----------------------------------------------------------------------------
if(RAYFLOW_USE_PAK)
    target_compile_definitions(client_lib PUBLIC RAYFLOW_USE_PAK=1)
else()
    target_compile_definitions(client_lib PUBLIC RAYFLOW_USE_PAK=0)
endif()
