# =============================================================================
# Map Editor - Voxel map creation and editing tool
# =============================================================================

if(RAYFLOW_BUILD_MAP_EDITOR)
    add_executable(map_builder
        map_editor_main.cpp
        map_editor_style.cpp
        editor_session.cpp
        ${CMAKE_SOURCE_DIR}/engine/ui/debug/raygui_impl.cpp
    )
    
    target_include_directories(map_builder PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/engine
    )
    
    target_link_libraries(map_builder PRIVATE
        bedwars_client_lib
        bedwars_server_lib
        engine_client
        engine_ui
        engine_voxel
    )
    
    rayflow_link_raylib(map_builder)
    rayflow_link_platform(map_builder)
    rayflow_configure_executable(map_builder)
endif()
