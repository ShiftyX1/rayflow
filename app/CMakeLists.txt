# =============================================================================
# app/CMakeLists.txt
# Executables: rayflow, map_builder, rfds
# =============================================================================

# -----------------------------------------------------------------------------
# Main game executable
# -----------------------------------------------------------------------------
add_executable(rayflow
    main.cpp
)

target_link_libraries(rayflow PRIVATE
    client_lib
    server_lib
    shared_lib
    EnTT::EnTT
)

rayflow_link_raylib(rayflow)
rayflow_link_platform(rayflow)
rayflow_configure_executable(rayflow)

# -----------------------------------------------------------------------------
# Map Editor
# -----------------------------------------------------------------------------
if(RAYFLOW_BUILD_MAP_EDITOR)
    add_executable(map_builder
        map_editor_main.cpp
        map_editor_style.cpp
        ${CMAKE_SOURCE_DIR}/ui/debug/raygui_impl.cpp
    )
    
    target_link_libraries(map_builder PRIVATE
        client_lib
        server_lib
        shared_lib
    )
    
    rayflow_link_raylib(map_builder)
    rayflow_link_platform(map_builder)
    rayflow_configure_executable(map_builder)
endif()

# -----------------------------------------------------------------------------
# Dedicated Server (stub for now)
# -----------------------------------------------------------------------------
if(RAYFLOW_BUILD_DEDICATED_SERVER)
    add_executable(rfds
        dedicated_server_main.cpp
    )
    
    target_link_libraries(rfds PRIVATE
        server_lib
        shared_lib
    )
    
    # rfds does NOT link raylib - it's headless
    rayflow_link_platform(rfds)
endif()
