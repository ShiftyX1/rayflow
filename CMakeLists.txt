cmake_minimum_required(VERSION 3.15)

project(low_pulse CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/build)

include(FetchContent)
FetchContent_Declare(
    entt
    GIT_REPOSITORY https://github.com/skypjack/entt.git
    GIT_TAG v3.13.2
)
FetchContent_MakeAvailable(entt)

FetchContent_Declare(
    tinyxml2
    GIT_REPOSITORY https://github.com/leethomason/tinyxml2.git
    GIT_TAG 10.0.0
)
FetchContent_MakeAvailable(tinyxml2)

# Find raylib
find_package(raylib QUIET)

if (NOT raylib_FOUND)
    if (WIN32)
        find_path(RAYLIB_INCLUDE_DIR 
            NAMES raylib.h 
            PATHS 
                C:/msys64/ucrt64/include
        )
        
        find_library(RAYLIB_LIBRARY 
            NAMES raylib libraylib
            PATHS 
                C:/msys64/ucrt64/lib
        )
    else()
        find_path(RAYLIB_INCLUDE_DIR 
            NAMES raylib.h 
            PATHS 
                /opt/homebrew/opt/raylib/include
                /usr/local/include
                /usr/include
        )
        
        find_library(RAYLIB_LIBRARY 
            NAMES raylib
            PATHS 
                /opt/homebrew/opt/raylib/lib
                /usr/local/lib
                /usr/lib
        )
    endif()
    
    if (RAYLIB_INCLUDE_DIR AND RAYLIB_LIBRARY)
        message(STATUS "Found raylib: ${RAYLIB_LIBRARY}")
        include_directories(${RAYLIB_INCLUDE_DIR})
    else()
        if (WIN32)
            message(FATAL_ERROR "raylib not found! Please install it via: pacman -S mingw-w64-x86_64-raylib")
        else()
            message(FATAL_ERROR "raylib not found! Please install it via: brew install raylib")
        endif()
    endif()
endif()

include_directories(${CMAKE_SOURCE_DIR}/client)
include_directories(${CMAKE_SOURCE_DIR}/shared)
include_directories(${CMAKE_SOURCE_DIR}/server)
include_directories(${CMAKE_SOURCE_DIR}/ui)

add_library(shared_lib STATIC
    shared/transport/local_transport.cpp
)

target_include_directories(shared_lib PUBLIC ${CMAKE_SOURCE_DIR}/shared)

add_library(server_lib STATIC
    server/core/server.cpp
    server/voxel/terrain.cpp
)

target_include_directories(server_lib PUBLIC ${CMAKE_SOURCE_DIR}/server ${CMAKE_SOURCE_DIR}/shared)
target_link_libraries(server_lib PUBLIC shared_lib)

add_library(client_lib STATIC
    client/core/game.cpp
    client/core/config.cpp
    client/core/logger.cpp
    client/net/client_session.cpp
    client/ecs/components.cpp
    client/ecs/systems/physics_system.cpp
    client/ecs/systems/render_system.cpp
    client/ecs/systems/input_system.cpp
    client/ecs/systems/player_system.cpp
    client/voxel/world.cpp
    client/voxel/chunk.cpp
    client/voxel/block_registry.cpp
    client/voxel/block_interaction.cpp
    client/voxel/texture_atlas.cpp
    client/renderer/mesh.cpp

    ui/runtime/ui_manager.cpp

    ui/runtime/xmlui/css_lite.cpp
    ui/runtime/xmlui/ui_document.cpp
)

target_include_directories(client_lib PUBLIC ${CMAKE_SOURCE_DIR}/client ${CMAKE_SOURCE_DIR}/shared)

# Debug UI (raygui) is developer-only. Enable it by default for non-Release builds.
if (NOT CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_definitions(client_lib PUBLIC DEBUG_UI)
    target_sources(client_lib PRIVATE
        ui/debug/debug_ui.cpp
        ui/debug/raygui_impl.cpp
    )
endif()
target_link_libraries(client_lib PUBLIC shared_lib)

# client_lib needs EnTT and raylib headers during compilation
target_link_libraries(client_lib PUBLIC EnTT::EnTT)
target_link_libraries(client_lib PUBLIC tinyxml2)

if (raylib_FOUND)
    target_link_libraries(client_lib PUBLIC raylib)
else()
    target_link_libraries(client_lib PUBLIC ${RAYLIB_LIBRARY})
endif()

add_executable(${PROJECT_NAME}
    app/main.cpp
)

target_link_libraries(${PROJECT_NAME} PRIVATE client_lib server_lib shared_lib)

target_link_libraries(${PROJECT_NAME} PRIVATE EnTT::EnTT)

if (raylib_FOUND)
    target_link_libraries(${PROJECT_NAME} PRIVATE raylib)
else()
    target_link_libraries(${PROJECT_NAME} PRIVATE ${RAYLIB_LIBRARY})
endif()

if (APPLE)
    target_link_libraries(${PROJECT_NAME} PRIVATE "-framework IOKit" "-framework Cocoa" "-framework OpenGL")
elseif (WIN32)
    if (CYGWIN)
        target_link_libraries(${PROJECT_NAME} PRIVATE winmm gdi32 opengl32 user32)
    else()
        target_link_libraries(${PROJECT_NAME} PRIVATE winmm gdi32)
    endif()
endif()
