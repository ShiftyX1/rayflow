# =============================================================================
# shared/CMakeLists.txt
# shared_lib - Common code used by both client and server
# =============================================================================

add_library(shared_lib STATIC
    transport/local_transport.cpp
    transport/enet_common.cpp
    transport/enet_connection.cpp
    transport/enet_server.cpp
    transport/enet_client.cpp
    maps/rfmap_io.cpp
    maps/runtime_paths.cpp
    vfs/vfs.cpp
    vfs/archive_reader.cpp
    vfs/archive_writer.cpp
    scripting/lua_state.cpp
    scripting/sandbox.cpp
)

target_include_directories(shared_lib PUBLIC
    ${CMAKE_SOURCE_DIR}/shared
    ${enet_SOURCE_DIR}/include
)

# Link ENet for network transport
target_link_libraries(shared_lib PUBLIC enet)

# Platform-specific dependencies for ENet
if(WIN32)
    target_link_libraries(shared_lib PUBLIC ws2_32 winmm)
endif()

# Link LuaJIT and sol2 for scripting support
rayflow_link_lua(shared_lib)

# No raylib dependency in shared - it must stay headless
