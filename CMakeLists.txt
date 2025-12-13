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

# Source files
set(SOURCES
    client/main.cpp
    client/core/game.cpp
    client/core/config.cpp
    client/core/logger.cpp
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
)

add_executable(${PROJECT_NAME} ${SOURCES})

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
