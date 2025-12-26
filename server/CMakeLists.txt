# =============================================================================
# server/CMakeLists.txt
# server_lib - Server-side simulation (headless, no raylib)
# =============================================================================

add_library(server_lib STATIC
    core/server.cpp
    voxel/terrain.cpp
    scripting/script_engine.cpp
    scripting/game_api.cpp
)

target_include_directories(server_lib PUBLIC
    ${CMAKE_SOURCE_DIR}/server
    ${CMAKE_SOURCE_DIR}/shared
)

target_link_libraries(server_lib PUBLIC
    shared_lib
)

# Server must NOT depend on raylib - it's headless
