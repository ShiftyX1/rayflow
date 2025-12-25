# =============================================================================
# tools/CMakeLists.txt
# Build tools: pack_assets
# =============================================================================

# -----------------------------------------------------------------------------
# Asset packer tool
# -----------------------------------------------------------------------------
if(RAYFLOW_BUILD_PACK_ASSETS)
    add_executable(rayflow_pack_assets
        pack_assets.cpp
    )
    
    target_include_directories(rayflow_pack_assets PRIVATE
        ${CMAKE_SOURCE_DIR}/shared
    )
    
    target_link_libraries(rayflow_pack_assets PRIVATE
        shared_lib
    )
endif()
