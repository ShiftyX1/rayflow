# BedWars Client Migration Plan

## Status: ✅ COMPLETED (January 3, 2026)

All migration phases have been completed successfully:
- All 141 tests pass
- `bedwars_client_demo` and `bedwars_server_demo` build and work
- `rayflow` and `map_builder` build with the new structure

---

## Goal
Полный перенос на новую engine-based архитектуру:
- `engine/` — game-agnostic движок (как Source Engine)
- `games/bedwars/` — game-specific код BedWars
- Старый `client/`, `ui/`, `app/` код **переносится в engine/** и очищается от game-specific логики

---

## Architecture Philosophy

### Engine (game-agnostic)
Движок предоставляет **инфраструктуру**, которую любая игра может использовать:
- ECS framework (компоненты, системы)
- Voxel engine (world, chunks, rendering)
- UI framework (XML+CSS runtime)
- Networking (transport layer)
- Rendering (skybox, mesh utilities)
- Config, logging, resources

### Games (game-specific)
Игры реализуют **gameplay** поверх движка:
- Protocol messages
- Game session (handshake, state sync)
- Game-specific components (inventory, tools)
- Game-specific UI (HUD, kill feed)
- IGameClient / IGameServer implementations

---

## Current State Analysis

### What needs to MOVE to engine/ ✅ DONE
```
client/                          → engine/client/
  core/
    config.hpp/cpp               → engine/client/core/
    logger.hpp/cpp               → engine/client/core/
    resources.hpp/cpp            → engine/client/core/
  ecs/
    components.hpp               → engine/client/ecs/ (generic parts)
    systems/
      input_system.*             → engine/client/ecs/systems/
      physics_system.*           → engine/client/ecs/systems/
      player_system.*            → engine/client/ecs/systems/
      render_system.*            → engine/client/ecs/systems/
  voxel/
    world.*                      → engine/client/voxel/
    chunk.*                      → engine/client/voxel/
    block.*                      → engine/client/voxel/
    block_interaction.*          → engine/client/voxel/
    block_registry.*             → engine/client/voxel/
    block_model_loader.*         → engine/client/voxel/
    texture_atlas.*              → engine/client/voxel/
  renderer/
    mesh.*                       → engine/client/renderer/
    skybox.*                     → engine/client/renderer/

ui/                              → engine/ui/
  runtime/
    ui_manager.*                 → engine/ui/runtime/
    ui_view_model.hpp            → engine/ui/runtime/
    ui_frame.hpp                 → engine/ui/runtime/
    ui_command.hpp               → engine/ui/runtime/
    xmlui/                       → engine/ui/runtime/xmlui/
  debug/
    debug_ui.*                   → engine/ui/debug/
    raygui_impl.cpp              → engine/ui/debug/
  raygui.h                       → engine/ui/
```

### What stays in games/bedwars/ (game-specific)
```
games/bedwars/
  shared/
    protocol/                    # BedWars protocol messages
  server/
    bedwars_server.*             # IGameServer (DONE)
  client/
    bedwars_client.*             # IGameClient
    session/
      bedwars_session.*          # Game session (protocol handling)
    hud/
      hud_elements.*             # Kill feed, notifications
  app/
    server_main.cpp              # DONE
    client_main.cpp              # Entry point
```

### What to DELETE after migration
```
client/core/game.hpp/cpp         # Old Game class → replaced by BedWarsClient
client/net/client_session.*      # → games/bedwars/client/session/
app/main.cpp                     # Old entry point
```

---

## Migration Tasks

### Phase 1: Move client/ → engine/client/

#### 1.1 Create engine/client/ structure
```bash
mkdir -p engine/client/core
mkdir -p engine/client/ecs/systems
mkdir -p engine/client/voxel
mkdir -p engine/client/renderer
```

#### 1.2 Move files (git mv to preserve history)
```bash
# Core
git mv client/core/config.* engine/client/core/
git mv client/core/logger.* engine/client/core/
git mv client/core/resources.* engine/client/core/

# ECS
git mv client/ecs/components.hpp engine/client/ecs/
git mv client/ecs/systems/* engine/client/ecs/systems/

# Voxel
git mv client/voxel/* engine/client/voxel/

# Renderer
git mv client/renderer/* engine/client/renderer/
```

#### 1.3 Update includes in moved files
Replace `#include "../../shared/` → `#include "shared/`
Replace `#include "../ecs/` → `#include "engine/client/ecs/`
etc.

### Phase 2: Move ui/ → engine/ui/

#### 2.1 Move files
```bash
git mv ui/* engine/ui/
```

#### 2.2 Update includes

### Phase 3: Extract game-specific code

#### 3.1 client/net/client_session.* 
This is BedWars-specific (uses shared::proto which is BedWars protocol).
Move to: `games/bedwars/client/session/bedwars_session.*`

#### 3.2 Game-specific ECS components
Extract from `components.hpp`:
- `ToolHolder` → `games/bedwars/client/components/tool_holder.hpp`

Keep in engine:
- `Transform`, `Velocity`, `BoxCollider`, `PlayerController`, `FirstPersonCamera`, etc.

#### 3.3 Game-specific UI
- HUD layout (`ui/hud.xml`, `ui/hud.css`) → `games/bedwars/client/static/ui/`
- Menu layouts → `games/bedwars/client/static/ui/`

### Phase 4: Update CMakeLists.txt

#### 4.1 engine/CMakeLists.txt
```cmake
# Engine Client (with raylib)
add_library(engine_client STATIC
    # Core
    client/core/config.cpp
    client/core/logger.cpp
    client/core/resources.cpp
    
    # Client engine
    core/client_engine.cpp
    
    # ECS
    client/ecs/systems/input_system.cpp
    client/ecs/systems/physics_system.cpp
    client/ecs/systems/player_system.cpp
    client/ecs/systems/render_system.cpp
    
    # Voxel
    client/voxel/world.cpp
    client/voxel/chunk.cpp
    client/voxel/block_interaction.cpp
    client/voxel/block_registry.cpp
    client/voxel/block_model_loader.cpp
    client/voxel/texture_atlas.cpp
    
    # Renderer
    client/renderer/mesh.cpp
    client/renderer/skybox.cpp
)

# Engine UI
add_library(engine_ui STATIC
    ui/runtime/ui_manager.cpp
    ui/runtime/xmlui/ui_document.cpp
    # ... other UI files
    ui/debug/debug_ui.cpp
    ui/debug/raygui_impl.cpp
)

target_link_libraries(engine_client PUBLIC engine_core raylib EnTT::EnTT)
target_link_libraries(engine_ui PUBLIC engine_client tinyxml2)
```

#### 4.2 games/bedwars/CMakeLists.txt
```cmake
# BedWars client
add_library(bedwars_client STATIC
    client/bedwars_client.cpp
    client/session/bedwars_session.cpp
)

target_link_libraries(bedwars_client PUBLIC 
    bedwars_shared 
    engine_client 
    engine_ui
)

# BedWars client executable
add_executable(bedwars_client_demo
    app/client_main.cpp
)
target_link_libraries(bedwars_client_demo PRIVATE bedwars_client)
```

### Phase 5: Implement BedWarsClient

#### 5.1 bedwars_client.hpp
```cpp
class BedWarsClient : public engine::IGameClient {
    // Uses engine systems
    std::unique_ptr<engine::ecs::InputSystem> inputSystem_;
    std::unique_ptr<engine::ecs::PlayerSystem> playerSystem_;
    std::unique_ptr<engine::ecs::RenderSystem> renderSystem_;
    std::unique_ptr<engine::voxel::World> world_;
    std::unique_ptr<engine::ui::UIManager> uiManager_;
    
    // Game-specific
    std::unique_ptr<BedWarsSession> session_;
    GameScreen gameScreen_;
    // ...
};
```

### Phase 6: Clean up old code

#### 6.1 Delete deprecated files
```bash
rm -rf client/          # Moved to engine/client/
rm -rf ui/              # Moved to engine/ui/
rm app/main.cpp         # Replaced by games/bedwars/app/client_main.cpp
```

#### 6.2 Update root CMakeLists.txt
Remove old `add_subdirectory(client)` etc.

---

## Final Directory Structure

```
engine/
  core/
    types.hpp
    game_interface.hpp
    byte_buffer.hpp
    server_engine.hpp
    client_engine.hpp/cpp
  transport/
    transport.hpp
    local_transport.*
    enet_*.*
  client/
    core/
      config.*
      logger.*
      resources.*
    ecs/
      components.hpp
      systems/
        input_system.*
        physics_system.*
        player_system.*
        render_system.*
    voxel/
      world.*
      chunk.*
      block.*
      block_interaction.*
      block_registry.*
      block_model_loader.*
      texture_atlas.*
    renderer/
      mesh.*
      skybox.*
  ui/
    raygui.h
    runtime/
      ui_manager.*
      ui_view_model.hpp
      ui_frame.hpp
      ui_command.hpp
      xmlui/
    debug/
      debug_ui.*
      raygui_impl.cpp

shared/
  voxel/block.hpp         # Block types (used by engine)
  constants.hpp           # Shared constants
  maps/                   # Map format (used by engine)

games/
  bedwars/
    shared/
      protocol/           # BedWars messages
    server/
      bedwars_server.*    # IGameServer
    client/
      bedwars_client.*    # IGameClient
      session/
        bedwars_session.* # Protocol session
      components/
        tool_holder.hpp   # Game-specific components
      static/
        ui/               # HUD, menu layouts
    app/
      server_main.cpp
      client_main.cpp
```

---

## Implementation Order

1. **Phase 1**: Move `client/` → `engine/client/`
2. **Phase 2**: Move `ui/` → `engine/ui/`
3. **Phase 3**: Extract game-specific code to `games/bedwars/client/`
4. **Phase 4**: Update all CMakeLists.txt
5. **Phase 5**: Implement `BedWarsClient` using engine APIs
6. **Phase 6**: Delete old code, update root CMake
7. **Test**: Build and run bedwars_client_demo

---

## Success Criteria

1. ✅ `engine/` contains only game-agnostic code
2. ✅ `games/bedwars/` contains only BedWars-specific code
3. ✅ `bedwars_client_demo` works (menu → singleplayer → multiplayer)
4. ✅ Old `client/`, `ui/`, `app/main.cpp` deleted
5. ✅ Any future game can use `engine/` without BedWars code

---

## Notes for Agent

- Use `git mv` to preserve file history
- Update includes incrementally after each move
- Build and test after each phase
- Game-specific = uses BedWars protocol, BedWars entities, BedWars rules
- Generic = could be used by any voxel game
