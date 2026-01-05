# =============================================================================
# Rayflow SDK Installation
# =============================================================================
# Configures install targets for the Rayflow SDK distribution
# 
# Usage: 
#   cmake -B build -DRAYFLOW_BUILD_SDK=ON -DCMAKE_BUILD_TYPE=Release
#   cmake --build build
#   cmake --install build --prefix ./rayflow-sdk
#
# SDK Structure:
#   rayflow-sdk/
#   ├── include/rayflow/     - Headers
#   ├── lib/                 - Static libraries
#   └── lib/cmake/rayflow/   - CMake config files
# =============================================================================

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(RAYFLOW_SDK_VERSION ${PROJECT_VERSION})

# =============================================================================
# Install Headers
# =============================================================================

# Engine core headers
install(DIRECTORY ${CMAKE_SOURCE_DIR}/engine/core/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/rayflow/core
    FILES_MATCHING PATTERN "*.hpp" PATTERN "*.h"
)

install(DIRECTORY ${CMAKE_SOURCE_DIR}/engine/vfs/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/rayflow/vfs
    FILES_MATCHING PATTERN "*.hpp" PATTERN "*.h"
)

install(DIRECTORY ${CMAKE_SOURCE_DIR}/engine/transport/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/rayflow/transport
    FILES_MATCHING PATTERN "*.hpp" PATTERN "*.h"
)

install(DIRECTORY ${CMAKE_SOURCE_DIR}/engine/maps/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/rayflow/maps
    FILES_MATCHING PATTERN "*.hpp" PATTERN "*.h"
)

install(DIRECTORY ${CMAKE_SOURCE_DIR}/engine/ecs/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/rayflow/ecs
    FILES_MATCHING PATTERN "*.hpp" PATTERN "*.h"
)

# Engine client headers
install(DIRECTORY ${CMAKE_SOURCE_DIR}/engine/client/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/rayflow/client
    FILES_MATCHING PATTERN "*.hpp" PATTERN "*.h"
)

install(DIRECTORY ${CMAKE_SOURCE_DIR}/engine/renderer/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/rayflow/renderer
    FILES_MATCHING PATTERN "*.hpp" PATTERN "*.h"
)

# Engine UI headers
install(DIRECTORY ${CMAKE_SOURCE_DIR}/engine/ui/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/rayflow/ui
    FILES_MATCHING PATTERN "*.hpp" PATTERN "*.h"
)

# Engine modules headers
install(DIRECTORY ${CMAKE_SOURCE_DIR}/engine/modules/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/rayflow/modules
    FILES_MATCHING PATTERN "*.hpp" PATTERN "*.h"
)

# =============================================================================
# Install Static Libraries (just copy compiled .a/.lib files)
# =============================================================================

# Engine libraries
install(TARGETS engine_core engine_client engine_ui engine_voxel
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

# Copy dependency libraries manually (they're IMPORTED so can't use install(TARGETS))
install(CODE "
    # Copy LuaJIT
    file(GLOB LUAJIT_LIBS \"${CMAKE_BINARY_DIR}/_deps/luajit-install/lib/*\")
    foreach(lib \${LUAJIT_LIBS})
        file(INSTALL \${lib} DESTINATION \"\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}\")
    endforeach()
    
    # Copy enet
    if(EXISTS \"${CMAKE_BINARY_DIR}/_deps/enet-build/libenet.a\")
        file(INSTALL \"${CMAKE_BINARY_DIR}/_deps/enet-build/libenet.a\" 
             DESTINATION \"\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}\")
    endif()
")

# Copy LuaJIT headers
install(DIRECTORY ${CMAKE_BINARY_DIR}/_deps/luajit-install/include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/rayflow/lua
    FILES_MATCHING PATTERN "*.h"
)

# Copy enet headers
install(DIRECTORY ${enet_SOURCE_DIR}/include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/rayflow/enet
    FILES_MATCHING PATTERN "*.h"
)

# =============================================================================
# Generate CMake Package Config (simplified - no EXPORT)
# =============================================================================

# Write config file directly (simpler than template for IMPORTED targets)
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/rayflow-config.cmake" "
# =============================================================================
# Rayflow SDK Config
# =============================================================================
# Usage in your CMakeLists.txt:
#   find_package(rayflow REQUIRED)
#   target_link_libraries(my_game PRIVATE rayflow::engine)
#
# Available targets:
#   rayflow::core   - Headless engine (server, tools)
#   rayflow::client - Client with raylib
#   rayflow::ui     - UI framework
#   rayflow::voxel  - Voxel module
#   rayflow::engine - All-in-one (links all above)
# =============================================================================

cmake_minimum_required(VERSION 3.20)

if(TARGET rayflow::core)
    return()
endif()

get_filename_component(RAYFLOW_SDK_DIR \"\${CMAKE_CURRENT_LIST_DIR}/../../..\" ABSOLUTE)

# =============================================================================
# Platform detection
# =============================================================================
if(WIN32)
    set(_RAYFLOW_LIB_PREFIX \"\")
    set(_RAYFLOW_LIB_SUFFIX \".lib\")
    set(_RAYFLOW_LUAJIT_NAME \"lua51\")
elseif(APPLE)
    set(_RAYFLOW_LIB_PREFIX \"lib\")
    set(_RAYFLOW_LIB_SUFFIX \".a\")
    set(_RAYFLOW_LUAJIT_NAME \"luajit-5.1\")
else()
    set(_RAYFLOW_LIB_PREFIX \"lib\")
    set(_RAYFLOW_LIB_SUFFIX \".a\")
    set(_RAYFLOW_LUAJIT_NAME \"luajit-5.1\")
endif()

# =============================================================================
# Find dependencies (user must have raylib installed)
# =============================================================================
find_package(raylib CONFIG REQUIRED)

# EnTT (header-only, shipped with SDK)
if(NOT TARGET EnTT::EnTT)
    add_library(EnTT::EnTT INTERFACE IMPORTED)
    set_target_properties(EnTT::EnTT PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES \"\${RAYFLOW_SDK_DIR}/include\"
    )
endif()

# =============================================================================
# Internal dependency libraries (shipped with SDK)
# =============================================================================

# ENet
add_library(rayflow::enet STATIC IMPORTED)
set_target_properties(rayflow::enet PROPERTIES
    IMPORTED_LOCATION \"\${RAYFLOW_SDK_DIR}/lib/static/\${_RAYFLOW_LIB_PREFIX}enet\${_RAYFLOW_LIB_SUFFIX}\"
)
if(WIN32)
    set_property(TARGET rayflow::enet APPEND PROPERTY
        INTERFACE_LINK_LIBRARIES ws2_32 winmm
    )
endif()

# LuaJIT
add_library(rayflow::luajit STATIC IMPORTED)
set_target_properties(rayflow::luajit PROPERTIES
    IMPORTED_LOCATION \"\${RAYFLOW_SDK_DIR}/lib/\${_RAYFLOW_LIB_PREFIX}\${_RAYFLOW_LUAJIT_NAME}\${_RAYFLOW_LIB_SUFFIX}\"
    INTERFACE_INCLUDE_DIRECTORIES \"\${RAYFLOW_SDK_DIR}/include/rayflow/lua\"
)

# TinyXML2
add_library(rayflow::tinyxml2 STATIC IMPORTED)
set_target_properties(rayflow::tinyxml2 PROPERTIES
    IMPORTED_LOCATION \"\${RAYFLOW_SDK_DIR}/lib/\${_RAYFLOW_LIB_PREFIX}tinyxml2\${_RAYFLOW_LIB_SUFFIX}\"
    INTERFACE_INCLUDE_DIRECTORIES \"\${RAYFLOW_SDK_DIR}/include\"
)

# =============================================================================
# Engine libraries
# =============================================================================

# Core (headless - server, tools)
add_library(rayflow::core STATIC IMPORTED)
set_target_properties(rayflow::core PROPERTIES
    IMPORTED_LOCATION \"\${RAYFLOW_SDK_DIR}/lib/\${_RAYFLOW_LIB_PREFIX}engine_core\${_RAYFLOW_LIB_SUFFIX}\"
    INTERFACE_INCLUDE_DIRECTORIES \"\${RAYFLOW_SDK_DIR}/include;\${RAYFLOW_SDK_DIR}/include/rayflow\"
    INTERFACE_LINK_LIBRARIES \"rayflow::enet;rayflow::luajit;EnTT::EnTT\"
)

# Client (with raylib)
add_library(rayflow::client STATIC IMPORTED)
set_target_properties(rayflow::client PROPERTIES
    IMPORTED_LOCATION \"\${RAYFLOW_SDK_DIR}/lib/\${_RAYFLOW_LIB_PREFIX}engine_client\${_RAYFLOW_LIB_SUFFIX}\"
    INTERFACE_INCLUDE_DIRECTORIES \"\${RAYFLOW_SDK_DIR}/include;\${RAYFLOW_SDK_DIR}/include/rayflow\"
    INTERFACE_LINK_LIBRARIES \"rayflow::core;raylib;EnTT::EnTT\"
)

# UI framework
add_library(rayflow::ui STATIC IMPORTED)
set_target_properties(rayflow::ui PROPERTIES
    IMPORTED_LOCATION \"\${RAYFLOW_SDK_DIR}/lib/\${_RAYFLOW_LIB_PREFIX}engine_ui\${_RAYFLOW_LIB_SUFFIX}\"
    INTERFACE_INCLUDE_DIRECTORIES \"\${RAYFLOW_SDK_DIR}/include;\${RAYFLOW_SDK_DIR}/include/rayflow\"
    INTERFACE_LINK_LIBRARIES \"rayflow::client;rayflow::tinyxml2\"
)

# Voxel module
add_library(rayflow::voxel STATIC IMPORTED)
set_target_properties(rayflow::voxel PROPERTIES
    IMPORTED_LOCATION \"\${RAYFLOW_SDK_DIR}/lib/\${_RAYFLOW_LIB_PREFIX}engine_voxel\${_RAYFLOW_LIB_SUFFIX}\"
    INTERFACE_INCLUDE_DIRECTORIES \"\${RAYFLOW_SDK_DIR}/include;\${RAYFLOW_SDK_DIR}/include/rayflow\"
    INTERFACE_LINK_LIBRARIES \"rayflow::client\"
)

# =============================================================================
# Convenience target (all-in-one)
# =============================================================================
add_library(rayflow::engine INTERFACE IMPORTED)
set_target_properties(rayflow::engine PROPERTIES
    INTERFACE_LINK_LIBRARIES \"rayflow::voxel;rayflow::ui;rayflow::client;rayflow::core\"
)

# =============================================================================
# Export variables
# =============================================================================
set(RAYFLOW_FOUND TRUE)
set(RAYFLOW_VERSION ${RAYFLOW_SDK_VERSION})
set(RAYFLOW_SDK_DIR \"\${RAYFLOW_SDK_DIR}\")
set(RAYFLOW_INCLUDE_DIRS \"\${RAYFLOW_SDK_DIR}/include;\${RAYFLOW_SDK_DIR}/include/rayflow\")

message(STATUS \"Found Rayflow SDK v${RAYFLOW_SDK_VERSION} at \${RAYFLOW_SDK_DIR}\")
")

# Version file
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/rayflow-config-version.cmake"
    VERSION ${RAYFLOW_SDK_VERSION}
    COMPATIBILITY SameMajorVersion
)

# Install cmake files
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/rayflow-config.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/rayflow-config-version.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/rayflow
)

# =============================================================================
# SDK Info
# =============================================================================

message(STATUS "")
message(STATUS "[SDK] Rayflow SDK v${RAYFLOW_SDK_VERSION}")
message(STATUS "[SDK] Install with: cmake --install build --prefix ./rayflow-sdk")
message(STATUS "[SDK] Headers: \${prefix}/include/rayflow/")
message(STATUS "[SDK] Libraries: \${prefix}/lib/")
message(STATUS "[SDK] CMake config: \${prefix}/lib/cmake/rayflow/")
message(STATUS "")
