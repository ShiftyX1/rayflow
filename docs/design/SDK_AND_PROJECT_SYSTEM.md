# Rayflow SDK & Project System Design

## Overview

Rayflow needs a project/SDK system similar to Source Engine SDK, Unity, or Godot:
- **Easy project initialization** from templates
- **Project manifest** describing game configuration
- **Rayflow as a library** (static/dynamic) for distribution
- **CLI tools** for project management

---

## 1. Project Structure

### Game Project Layout

```
my_game/
├── game.toml                    # Project manifest
├── CMakeLists.txt               # Generated/template CMake
├── src/
│   ├── shared/                  # Protocol, shared types
│   │   └── protocol.hpp
│   ├── server/                  # Server-side game logic
│   │   └── my_game_server.cpp
│   ├── client/                  # Client-side rendering/UI
│   │   └── my_game_client.cpp
│   └── app/
│       ├── client_main.cpp      # Client entry point
│       └── server_main.cpp      # Server entry point
├── resources/                   # Game assets
│   ├── textures/
│   ├── models/
│   ├── maps/
│   └── ui/
└── build/                       # Build output
```

---

## 2. Project Manifest (game.toml)

```toml
[project]
name = "my_awesome_game"
version = "0.1.0"
display_name = "My Awesome Game"
description = "A voxel RPG built on Rayflow"
authors = ["Developer Name"]

[engine]
# Rayflow version requirement
version = ">=0.1.0"
# Which engine modules to use
modules = ["voxel", "ui"]    # Optional: "physics", "audio", "ai"

[build]
# Build targets
targets = ["client", "server", "dedicated"]
# C++ standard
cpp_standard = 20
# Output binary names
client_name = "my_game"
server_name = "my_game_server"

[server]
# Default server settings
tick_rate = 30
max_players = 32
default_map = "lobby"

[client]
# Default client settings
window_width = 1280
window_height = 720
target_fps = 60
vsync = true

[assets]
# Asset packaging
use_pak = true                   # Use .pak in release
pak_name = "game_assets.pak"
# Directories to include
include = ["resources/*"]
exclude = ["resources/dev/*"]

[platforms]
# Target platforms for distribution
targets = ["windows", "linux", "macos"]

# Platform-specific overrides
[platforms.windows]
icon = "resources/icon.ico"

[platforms.macos]
bundle_id = "com.mycompany.mygame"
icon = "resources/icon.icns"
```

---

## 3. Rayflow SDK Distribution

### SDK Package Structure

```
rayflow-sdk-{version}-{platform}/
├── include/                     # Public headers
│   └── rayflow/
│       ├── rayflow.hpp          # Main include
│       ├── engine/
│       │   ├── core.hpp
│       │   ├── transport.hpp
│       │   ├── ecs.hpp
│       │   └── ...
│       └── modules/
│           ├── voxel.hpp
│           └── ...
├── lib/                         # Prebuilt libraries
│   ├── librayflow_core.a        # Static
│   ├── librayflow_core.so       # Dynamic (Linux)
│   ├── librayflow_core.dylib    # Dynamic (macOS)
│   ├── rayflow_core.lib         # Static (Windows)
│   ├── rayflow_core.dll         # Dynamic (Windows)
│   ├── librayflow_client.a
│   ├── librayflow_voxel.a
│   └── ...
├── bin/                         # CLI tools
│   ├── rayflow                  # Main CLI
│   ├── rayflow-pack             # Asset packer
│   └── rayflow-map-editor       # Map editor
├── templates/                   # Project templates
│   ├── minimal/
│   ├── voxel-game/
│   ├── multiplayer/
│   └── ...
├── cmake/                       # CMake modules for SDK
│   ├── FindRayflow.cmake
│   └── RayflowProject.cmake
└── docs/                        # Documentation
    └── ...
```

### Library Variants

| Library | Type | Contents |
|---------|------|----------|
| `rayflow_core` | Static/Dynamic | Transport, VFS, Maps, Scripting (headless) |
| `rayflow_client` | Static/Dynamic | Core + raylib integration, rendering base |
| `rayflow_ui` | Static/Dynamic | UI framework (XML+CSS) |
| `rayflow_voxel` | Static/Dynamic | Voxel module (client-side) |

---

## 4. CLI Tool (rayflow)

### Commands

```bash
# Create new project
rayflow new my_game                      # Interactive wizard
rayflow new my_game --template voxel     # From template

# Project management
rayflow init                             # Initialize in existing dir
rayflow build                            # Build project
rayflow build --release                  # Release build
rayflow run                              # Run client
rayflow run --server                     # Run dedicated server

# Asset management
rayflow pack                             # Pack assets to .pak
rayflow pack --watch                     # Watch and repack

# SDK management
rayflow sdk install 0.2.0                # Install specific version
rayflow sdk list                         # List installed versions
rayflow sdk update                       # Update to latest

# Development
rayflow generate cmake                   # Regenerate CMakeLists.txt
rayflow validate                         # Validate game.toml
```

### Implementation

```cpp
// engine/tools/cli/main.cpp
#include <rayflow/cli.hpp>

int main(int argc, char* argv[]) {
    rayflow::cli::App app;
    
    app.add_command("new", rayflow::cli::cmd_new);
    app.add_command("init", rayflow::cli::cmd_init);
    app.add_command("build", rayflow::cli::cmd_build);
    app.add_command("run", rayflow::cli::cmd_run);
    app.add_command("pack", rayflow::cli::cmd_pack);
    
    return app.run(argc, argv);
}
```

---

## 5. Project Templates

### minimal/

Bare-bones project with custom IGameServer/IGameClient:

```
minimal/
├── game.toml.template
├── CMakeLists.txt.template
└── src/
    ├── shared/
    │   └── protocol.hpp
    ├── server/
    │   └── game_server.cpp
    ├── client/
    │   └── game_client.cpp
    └── app/
        ├── client_main.cpp
        └── server_main.cpp
```

### voxel-game/

Full voxel game with world, blocks, UI:

```
voxel-game/
├── game.toml.template
├── CMakeLists.txt.template
├── src/
│   ├── shared/
│   │   ├── protocol.hpp
│   │   └── block_types.hpp
│   ├── server/
│   │   ├── game_server.cpp
│   │   └── world/
│   │       └── terrain.cpp
│   ├── client/
│   │   ├── game_client.cpp
│   │   └── ui/
│   │       └── hud.cpp
│   └── app/
│       ├── client_main.cpp
│       └── server_main.cpp
└── resources/
    ├── textures/
    │   └── terrain.png
    ├── maps/
    │   └── default.rfmap
    └── ui/
        ├── hud.xml
        └── hud.css
```

### multiplayer/

Template focused on networking:

```
multiplayer/
├── game.toml.template
├── CMakeLists.txt.template
└── src/
    ├── shared/
    │   ├── protocol.hpp
    │   ├── messages.hpp
    │   └── player_state.hpp
    ├── server/
    │   ├── game_server.cpp
    │   ├── match_manager.cpp
    │   └── player_manager.cpp
    ├── client/
    │   ├── game_client.cpp
    │   └── network_interpolation.cpp
    └── app/
        ├── client_main.cpp
        └── server_main.cpp
```

---

## 6. CMake Integration

### FindRayflow.cmake

```cmake
# FindRayflow.cmake - Find Rayflow SDK
#
# Usage:
#   find_package(Rayflow REQUIRED COMPONENTS core client voxel)
#
# Variables:
#   Rayflow_FOUND        - True if found
#   Rayflow_VERSION      - Version string
#   Rayflow_INCLUDE_DIRS - Include directories
#   Rayflow_LIBRARIES    - Libraries to link
#
# Targets:
#   Rayflow::core        - Core engine (headless)
#   Rayflow::client      - Client engine (with raylib)
#   Rayflow::voxel       - Voxel module
#   Rayflow::ui          - UI framework

find_path(Rayflow_INCLUDE_DIR
    NAMES rayflow/rayflow.hpp
    PATHS
        ${RAYFLOW_SDK_PATH}/include
        $ENV{RAYFLOW_SDK}/include
        /usr/local/include
        /usr/include
)

# Find libraries for each component...
foreach(_comp IN LISTS Rayflow_FIND_COMPONENTS)
    find_library(Rayflow_${_comp}_LIBRARY
        NAMES rayflow_${_comp}
        PATHS
            ${RAYFLOW_SDK_PATH}/lib
            $ENV{RAYFLOW_SDK}/lib
    )
    
    if(Rayflow_${_comp}_LIBRARY)
        add_library(Rayflow::${_comp} UNKNOWN IMPORTED)
        set_target_properties(Rayflow::${_comp} PROPERTIES
            IMPORTED_LOCATION "${Rayflow_${_comp}_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${Rayflow_INCLUDE_DIR}"
        )
    endif()
endforeach()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Rayflow
    REQUIRED_VARS Rayflow_INCLUDE_DIR
    VERSION_VAR Rayflow_VERSION
)
```

### Game Project CMakeLists.txt (generated)

```cmake
cmake_minimum_required(VERSION 3.21)

# Read project info from game.toml
# (parsed by rayflow CLI during generation)
project({{project_name}}
    VERSION {{version}}
    LANGUAGES CXX
)

# Find Rayflow SDK
find_package(Rayflow REQUIRED COMPONENTS {{components}})

# =============================================================================
# Game Shared Library
# =============================================================================
add_library(game_shared STATIC
    src/shared/protocol.hpp
    # ... more shared sources
)
target_link_libraries(game_shared PUBLIC Rayflow::core)
target_include_directories(game_shared PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

# =============================================================================
# Game Server
# =============================================================================
add_library(game_server STATIC
    src/server/game_server.cpp
    # ... more server sources
)
target_link_libraries(game_server PUBLIC game_shared Rayflow::core)

# =============================================================================
# Game Client
# =============================================================================
add_library(game_client STATIC
    src/client/game_client.cpp
    # ... more client sources
)
target_link_libraries(game_client PUBLIC game_shared Rayflow::client {{module_libs}})

# =============================================================================
# Executables
# =============================================================================

# Client executable
add_executable({{client_name}}
    src/app/client_main.cpp
)
target_link_libraries({{client_name}} PRIVATE game_client game_server)

# Dedicated server executable
add_executable({{server_name}}
    src/app/server_main.cpp
)
target_link_libraries({{server_name}} PRIVATE game_server)

# =============================================================================
# Assets
# =============================================================================
{{#if use_pak}}
add_custom_target(pack_assets
    COMMAND rayflow pack
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Packing game assets..."
)
add_dependencies({{client_name}} pack_assets)
{{/if}}
```

---

## 7. Build Rayflow as Library

### New CMake Options

```cmake
# cmake/RayflowConfig.cmake additions

# Library build options
option(RAYFLOW_BUILD_SHARED "Build shared libraries" OFF)
option(RAYFLOW_BUILD_SDK "Build SDK package" OFF)
option(RAYFLOW_SDK_OUTPUT_DIR "SDK output directory" "${CMAKE_BINARY_DIR}/sdk")

# What to include in SDK
option(RAYFLOW_SDK_INCLUDE_HEADERS "Include headers in SDK" ON)
option(RAYFLOW_SDK_INCLUDE_TOOLS "Include CLI tools in SDK" ON)
option(RAYFLOW_SDK_INCLUDE_TEMPLATES "Include project templates in SDK" ON)
```

### Dynamic Library Build

```cmake
# engine/CMakeLists.txt modifications

if(RAYFLOW_BUILD_SHARED)
    set(LIB_TYPE SHARED)
else()
    set(LIB_TYPE STATIC)
endif()

add_library(rayflow_core ${LIB_TYPE}
    # ... sources
)

# Export symbols on Windows
if(WIN32 AND RAYFLOW_BUILD_SHARED)
    target_compile_definitions(rayflow_core PRIVATE RAYFLOW_EXPORTS)
    target_compile_definitions(rayflow_core INTERFACE RAYFLOW_IMPORTS)
endif()

# Set library version
set_target_properties(rayflow_core PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)
```

### SDK Build Target

```cmake
# cmake/RayflowSDK.cmake

if(RAYFLOW_BUILD_SDK)
    set(SDK_DIR "${RAYFLOW_SDK_OUTPUT_DIR}/rayflow-sdk-${PROJECT_VERSION}-${RAYFLOW_PLATFORM}")
    
    # Install headers
    install(DIRECTORY ${CMAKE_SOURCE_DIR}/engine/
        DESTINATION ${SDK_DIR}/include/rayflow
        FILES_MATCHING PATTERN "*.hpp"
    )
    
    # Install libraries
    install(TARGETS rayflow_core rayflow_client rayflow_ui rayflow_voxel
        ARCHIVE DESTINATION ${SDK_DIR}/lib
        LIBRARY DESTINATION ${SDK_DIR}/lib
        RUNTIME DESTINATION ${SDK_DIR}/bin
    )
    
    # Install tools
    if(RAYFLOW_SDK_INCLUDE_TOOLS)
        install(TARGETS rayflow_cli rayflow_pack_assets map_builder
            RUNTIME DESTINATION ${SDK_DIR}/bin
        )
    endif()
    
    # Install templates
    if(RAYFLOW_SDK_INCLUDE_TEMPLATES)
        install(DIRECTORY ${CMAKE_SOURCE_DIR}/templates/
            DESTINATION ${SDK_DIR}/templates
        )
    endif()
    
    # Install CMake modules
    install(FILES
        ${CMAKE_SOURCE_DIR}/cmake/FindRayflow.cmake
        ${CMAKE_SOURCE_DIR}/cmake/RayflowProject.cmake
        DESTINATION ${SDK_DIR}/cmake
    )
endif()
```

---

## 8. Implementation Plan

### Phase 1: Core Infrastructure
1. [ ] Create `game.toml` parser (using toml11 or similar)
2. [ ] Implement project manifest system
3. [ ] Refactor engine CMake for library builds
4. [ ] Create `FindRayflow.cmake`

### Phase 2: CLI Tool
1. [ ] Create `rayflow` CLI skeleton
2. [ ] Implement `new` command with templates
3. [ ] Implement `build` command (wraps CMake)
4. [ ] Implement `run` command

### Phase 3: Templates
1. [ ] Create `minimal` template
2. [ ] Create `voxel-game` template
3. [ ] Create `multiplayer` template
4. [ ] Template variable substitution system

### Phase 4: SDK Packaging
1. [ ] SDK directory structure
2. [ ] Platform-specific builds (CI/CD)
3. [ ] Version management
4. [ ] Distribution (GitHub Releases, package managers)

---

## 9. Example: Creating a New Game

```bash
# Install Rayflow SDK (or build from source)
$ rayflow sdk install latest

# Create new voxel game project
$ rayflow new my_rpg --template voxel-game

Creating project 'my_rpg' from template 'voxel-game'...
  → Creating directory structure...
  → Generating game.toml...
  → Generating CMakeLists.txt...
  → Copying template files...
  → Done!

Next steps:
  cd my_rpg
  rayflow build
  rayflow run

# Edit game.toml as needed
$ cd my_rpg
$ vim game.toml

# Build the game
$ rayflow build
[1/5] Building rayflow_core...
[2/5] Building game_shared...
[3/5] Building game_server...
[4/5] Building game_client...
[5/5] Linking my_rpg...
Build complete!

# Run the game
$ rayflow run
Starting my_rpg client...
```

---

## 10. Migration: BedWars as Example Game

Current `games/bedwars/` already follows the pattern. To make it work with the new system:

1. Add `games/bedwars/game.toml`
2. Ensure it can build against SDK or source
3. Use as reference implementation for templates
