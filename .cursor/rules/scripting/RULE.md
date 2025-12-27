# Scripting System Instructions

## Overview

RayFlow uses a **two-tier Lua scripting system** inspired by Valve's VScripts (Dota 2, CS2):

1. **User Level (Sandboxed)** — For community-made custom maps with restricted API
2. **Engine Level (Trusted)** — For official game logic and DLC, with full API access

Both tiers use Lua 5.4 via the **sol2** binding library.

## Architecture

```
shared/scripting/          # Shared foundations (both tiers use this)
├── lua_state.hpp/cpp      # sol2 wrapper with sandbox support
├── sandbox.hpp/cpp        # Security: limits, validation, forbidden functions
└── script_utils.hpp       # Data structures (MapScriptData, events, etc.)

server/scripting/          # Server-side script execution
├── script_engine.hpp/cpp  # Main engine (loads/runs map scripts)
├── game_api.hpp/cpp       # User-level API (sandboxed, limited)
└── engine_api.hpp/cpp     # Engine-level API (trusted, full access) [TODO]

ui/scripting/              # Client-side UI scripts
├── ui_script_engine.hpp/cpp  # UI script engine
└── ui_api.hpp/cpp         # UI manipulation API [TODO]
```

## Two-Tier System

### Tier 1: User Level (Sandboxed)

**Purpose**: Community custom maps, user-generated content.

**Source**: `.rfmap` files created via Map Editor.

**Security**:
- Memory limit: 32 MB
- Instruction limit: 5M per call
- Execution time: 2 seconds max
- **Forbidden**: `os`, `io`, `debug`, `loadfile`, `dofile`, `load`, `require`, `package`, `collectgarbage`, `rawget`, `rawset`, `setmetatable`

**API Access**: Limited `game.*`, `world.*`, `player.*` (read-only), `timer.*`

```lua
-- User script example (in .rfmap)
function on_player_join(playerId)
    game.broadcast("Welcome, player " .. playerId)
    timer.after(5, function()
        game.send_message(playerId, "Have fun!")
    end)
end

function on_block_break(playerId, x, y, z, blockType)
    if blockType == BLOCK.BEDROCK then
        return false  -- Cancel the break
    end
    return true
end
```

### Tier 2: Engine Level (Trusted)

**Purpose**: Official game modes, DLC content, core BedWars logic.

**Source**: Packaged in `pak_N.pak` archives where N is the DLC/content pack number:
- `pak_0.pak` — Base game scripts
- `pak_1.pak` — First DLC
- `pak_2.pak` — Second DLC
- etc.

**Security**:
- No memory limit (uses engine allocator)
- No instruction limit
- No time limit
- **Full Lua access** including `require` for internal modules

**API Access**: Full access to all APIs:
- Everything from User Level
- `engine.*` — Direct server state manipulation
- `entities.*` — Entity spawning, modification, removal
- `teams.*` — Team management, scoring
- `economy.*` — Resource generators, shop system
- `internal.*` — Low-level hooks, debug utilities

```lua
-- Engine script example (in pak_0.pak/scripts/bedwars/core.lua)
local BedWars = {}

function BedWars.init(config)
    -- Direct access to engine internals
    engine.set_tick_rate(config.tickRate or 20)
    engine.set_max_players(config.maxPlayers or 16)
    
    -- Register resource generators
    for _, gen in ipairs(config.generators) do
        economy.create_generator(gen.pos, gen.type, gen.interval)
    end
end

function BedWars.on_bed_destroyed(teamId, destroyerPlayerId)
    teams.set_respawn_enabled(teamId, false)
    engine.broadcast_event("bed_destroyed", {
        team = teamId,
        destroyer = destroyerPlayerId
    })
end

return BedWars
```

## Hard Rules

### Execution Context

**Critical**: Scripts run in different contexts based on tier and trust level:

| Script Type | Server | Client | Why |
|-------------|--------|--------|-----|
| **User Scripts** (custom maps) | ✅ Yes | ❌ **NEVER** | Prevents cheats, ensures consistency |
| **Engine Scripts** (developers) | ✅ Yes | ✅ Yes | Trusted code for effects, UI, predictions |

```
┌─────────────────────────────────────────────────────────────────┐
│                         SERVER                                  │
│  ┌───────────────────────┐  ┌────────────────────────────────┐ │
│  │  User Scripts         │  │  Engine Scripts (server/)      │ │
│  │  (Sandboxed)          │  │  (Trusted)                     │ │
│  │  - on_player_join     │  │  - BedWars core logic          │ │
│  │  - on_block_break     │  │  - Match management            │ │
│  │  - game.*, world.*    │  │  - Full engine.* API           │ │
│  └───────────────────────┘  └────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────┐
│                         CLIENT                                  │
│  ┌───────────────────────┐  ┌────────────────────────────────┐ │
│  │  ❌ NO User Scripts   │  │  ✅ Engine Scripts (client/)   │ │
│  │  (Security risk)      │  │  (Trusted)                     │ │
│  │                       │  │  - Visual effects, particles   │ │
│  │                       │  │  - Client-side UI logic        │ │
│  │                       │  │  - Sound triggers              │ │
│  │                       │  │  - Animation controllers       │ │
│  │                       │  │  - Input handling helpers      │ │
│  └───────────────────────┘  └────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────────┘
```

### Engine Script Locations

```
pak_0.pak/
└── scripts/
    ├── server/                    # Server-only engine scripts
    │   ├── init.lua
    │   └── bedwars/
    │       ├── core.lua           # Match logic, rules
    │       ├── teams.lua          # Team management
    │       └── economy.lua        # Generators, shop
    │
    ├── client/                    # Client-only engine scripts
    │   ├── init.lua
    │   ├── effects/
    │   │   ├── particles.lua      # Particle system helpers
    │   │   └── damage_flash.lua   # Screen effects
    │   ├── ui/
    │   │   ├── hud.lua            # HUD logic
    │   │   └── scoreboard.lua     # Scoreboard updates
    │   └── audio/
    │       └── sound_manager.lua  # Sound triggers
    │
    └── shared/                    # Shared between server & client
        └── utils/
            ├── math.lua           # Math utilities
            └── table.lua          # Table utilities
```

### Client Engine Script API

Client-side engine scripts have access to client-specific APIs:

```lua
-- client/effects/damage_flash.lua (Engine Script, runs on CLIENT)

local DamageFlash = {}

function DamageFlash.on_player_damaged(localPlayerId, damage, direction)
    -- Visual feedback only - doesn't affect game state
    effects.screen_flash(1.0, 0.0, 0.0, 0.3)  -- Red flash
    effects.camera_shake(damage * 0.1)
    audio.play("hit_received", 0.8)
    
    -- Directional indicator
    ui.show_damage_indicator(direction)
end

function DamageFlash.on_player_healed(localPlayerId, amount)
    effects.screen_flash(0.0, 1.0, 0.0, 0.2)  -- Green flash
    audio.play("heal", 0.5)
end

return DamageFlash
```

```lua
-- client/ui/hud.lua (Engine Script, runs on CLIENT)

local HUD = {}

function HUD.on_health_changed(newHealth, maxHealth)
    ui.set_progress("health_bar", newHealth / maxHealth)
    
    if newHealth < maxHealth * 0.25 then
        ui.add_class("health_bar", "critical")
        audio.play_loop("heartbeat")
    else
        ui.remove_class("health_bar", "critical")
        audio.stop("heartbeat")
    end
end

function HUD.on_inventory_changed(slot, item)
    ui.set_icon("hotbar_" .. slot, item.icon)
    ui.set_text("hotbar_" .. slot .. "_count", item.count)
end

return HUD
```

### Client Engine API Namespaces

| Namespace | Functions | Description |
|-----------|-----------|-------------|
| `effects` | `screen_flash()`, `camera_shake()`, `spawn_particles()` | Visual effects |
| `audio` | `play()`, `play_loop()`, `stop()`, `set_volume()` | Sound control |
| `ui` | `show()`, `hide()`, `set_text()`, `animate()` | UI manipulation |
| `input` | `is_key_down()`, `get_mouse_pos()`, `vibrate()` | Input state (read-only) |
| `local_player` | `get_position()`, `get_look_dir()`, `get_velocity()` | Local player state (interpolated) |
| `camera` | `set_fov()`, `set_offset()`, `shake()` | Camera control |

**Note**: Client scripts cannot modify game state — they react to events from server.

### Layering

1. **`shared/scripting/`** — No dependencies on server/ or client/. No raylib.
2. **`server/scripting/`** — May depend on shared/. No raylib. No client/.
3. **`client/scripting/`** — May depend on shared/. May use raylib. Engine scripts only.
4. **User scripts execute ONLY on server** — All game logic is server-authoritative.
5. **Engine scripts can run on both** — But server and client have different APIs.

### Security (User Scripts)

1. **Always validate scripts** before execution via `Sandbox::validate_script()`.
2. **Always use sandboxed state** for user scripts via `Sandbox::create()`.
3. **Never expose internal pointers** to Lua (use IDs, not raw pointers).
4. **Scripts return commands, not actions** — `ScriptCommand` queue pattern.
5. **Rate-limit script logging** — Prevent log spam from malicious scripts.

### Determinism

1. **No `std::rand()` or `std::random_device`** in script API.
2. **Use server-seeded PRNG** — Pass seed from `ServerHello`, use explicit RNG state.
3. **`random()` and `random_int()`** must use deterministic seeded generator.
4. **Time functions** return server tick time, not wall-clock time.

### Performance

1. **No allocations in hot path** — Reuse `ScriptCommand` vectors, pre-reserve.
2. **Cache function lookups** — `has_function()` result can be cached per script load.
3. **Batch commands** — Scripts queue commands, server executes in batch.
4. **Limit hook frequency** — `on_update` should not run every tick for user scripts.

### PAK Packaging (Engine Scripts)

1. **Engine scripts go in `pak_N.pak`** under `scripts/` directory.
2. **Use VFS to load** — `shared::vfs::read_text("scripts/bedwars/core.lua")`.
3. **Version check on load** — PAK header contains script API version.
4. **Signature verification** [FUTURE] — Engine scripts will be signed.

## API Namespaces

### User Level API

| Namespace | Functions | Description |
|-----------|-----------|-------------|
| `game` | `broadcast(msg)`, `send_message(pid, msg)`, `end_round(team)`, `start_round()` | Game flow control |
| `world` | `get_block(x,y,z)`, `set_block(x,y,z,type)`, `is_solid(x,y,z)` | World queries/modifications |
| `player` | `get_position(pid)`, `get_health(pid)`, `get_team(pid)`, `get_all()`, `is_alive(pid)` | Player info (read-only) |
| `timer` | `after(delay, fn)`, `every(interval, fn)`, `named(name, delay, fn)`, `cancel(name)` | Timers |
| `BLOCK` | `AIR`, `STONE`, `DIRT`, `GRASS`, etc. | Block type constants |
| `TEAM` | `NONE`, `RED`, `BLUE`, `GREEN`, `YELLOW` | Team constants |

### Engine Level API (extends User Level)

| Namespace | Functions | Description |
|-----------|-----------|-------------|
| `engine` | `set_tick_rate(n)`, `get_tick()`, `broadcast_event(name, data)`, `log_internal(msg)` | Engine control |
| `entities` | `spawn(type, pos)`, `remove(id)`, `set_property(id, key, val)`, `get_all_of_type(type)` | Entity management |
| `teams` | `create(id, name, color)`, `set_spawn(id, pos)`, `set_respawn_enabled(id, bool)`, `get_score(id)`, `add_score(id, n)` | Team management |
| `economy` | `create_generator(pos, type, interval)`, `give_resource(pid, type, amount)`, `get_balance(pid, type)` | Economy system |
| `internal` | `get_server_state()`, `force_sync()`, `debug_dump()` | Debug/internal |

## Event Hooks

### Map Events (both tiers)

```lua
function on_init() end                              -- Script loaded
function on_unload() end                            -- Script unloading
function on_update(dt) end                          -- Per-tick update (engine only for user scripts)

function on_player_join(playerId) end               -- Player connected
function on_player_leave(playerId) end              -- Player disconnected  
function on_player_spawn(playerId, x, y, z) end     -- Player spawned
function on_player_death(playerId, killerId) end    -- Player died

function on_block_break(playerId, x, y, z, type) end  -- Block broken (return false to cancel)
function on_block_place(playerId, x, y, z, type) end  -- Block placed (return false to cancel)

function on_round_start(roundNum) end               -- Round started
function on_round_end(winningTeam) end              -- Round ended
function on_match_start() end                       -- Match started
function on_match_end(winningTeam) end              -- Match ended
```

### Engine-Only Events

```lua
function on_bed_destroyed(teamId, destroyerId) end  -- Bed destroyed
function on_generator_tick(genId, itemType) end     -- Generator produced item
function on_shop_purchase(playerId, itemId) end     -- Item purchased
function on_team_eliminated(teamId) end             -- All players eliminated
```

## Current Implementation Status

### Implemented ✅
- [x] `LuaState` wrapper with sol2
- [x] Memory tracking allocator
- [x] Execution limiter (instructions + time)
- [x] Sandbox validation (forbidden functions, bytecode detection)
- [x] `ScriptEngine` for server-side execution
- [x] `GameAPI` basic structure (game, world, player, timer namespaces)
- [x] `UIScriptEngine` for client-side UI
- [x] Timer system (one-shot and repeating)
- [x] Command queue pattern

---

## Implementation Roadmap

### Phase 1: MVP — Working User Scripts (1-2 weeks)

**Goal**: User scripts can interact with real server state.

| Task | Priority | Complexity | Description |
|------|----------|------------|-------------|
| Connect GameAPI to ServerState | 🔴 Critical | Medium | `api_get_block`, `api_get_player_*` read from real terrain/players |
| `call_with_result<T>()` | 🔴 Critical | Easy | Return values from Lua (for `on_block_break` → `false` to cancel) |
| Deterministic PRNG | 🔴 Critical | Easy | Replace `thread_local g_rng` with `ScriptEngine::rng_` seeded by server |
| `SendToPlayer` command type | 🟡 High | Easy | Individual message sending |
| Basic hot-reload (manual) | 🟡 High | Easy | `reload_scripts()` command for Debug builds |

**Code changes required**:

```cpp
// server/scripting/game_api.hpp - Add ServerState pointer
class GameAPI {
public:
    void set_server_state(ServerState* state) { state_ = state; }
private:
    ServerState* state_{nullptr};
};

// shared/scripting/lua_state.hpp - Add templated call with result
template<typename R, typename... Args>
std::optional<R> call_with_result(const std::string& func, Args&&... args);

// server/scripting/script_engine.hpp - Deterministic RNG
class ScriptEngine {
    std::mt19937 rng_;  // Per-engine, seeded by server
public:
    void seed_rng(uint32_t seed) { rng_.seed(seed); }
    std::mt19937& rng() { return rng_; }
};
```

### Phase 2: Complete User Level API (2-3 weeks)

**Goal**: All documented User Level API functions work correctly.

| Task | Priority | Complexity | Description |
|------|----------|------------|-------------|
| All `player.*` functions | 🔴 Critical | Medium | Position, health, team, inventory queries |
| All `world.*` functions | 🔴 Critical | Medium | Block get/set with proper replication |
| Extended events | 🟡 High | Medium | `on_damage`, `on_heal`, `on_item_pickup` |
| `api_version()` check | 🟡 High | Easy | Script declares required API version |
| Rate-limited logging | 🟡 High | Easy | Token bucket for `print()`/`log()` |
| Variadic `call()` refactor | 🟢 Normal | Easy | Clean up call overloads |

**API version example**:
```lua
-- At script start
api_version(1)  -- Engine validates compatibility

function on_init()
    log("Script loaded!")
end
```

### Phase 3: Engine Level API (2-3 weeks)

**Goal**: Official game logic can use extended API.

| Task | Priority | Complexity | Description |
|------|----------|------------|-------------|
| `engine_api.hpp/cpp` | 🔴 Critical | High | Full access API for trusted scripts |
| Custom module loader | 🔴 Critical | Medium | `engine.load_module()` instead of `require` |
| Engine script folder | 🟡 High | Easy | Load from `scripts/` without PAK (dev mode) |
| `teams.*` namespace | 🟡 High | Medium | Team management API |
| `entities.*` namespace | 🟡 High | High | Entity spawning/modification |
| `economy.*` namespace | 🟢 Normal | Medium | Generators, resources |

**Module loading (not `require`)**:
```lua
-- Engine scripts use explicit loading
local Teams = engine.load_module("bedwars/teams")
local Economy = engine.load_module("bedwars/economy")

-- Modules return tables
return {
    init = function() ... end,
    on_player_join = function(pid) ... end,
}
```

### Phase 4: Production Features (When Needed)

**Goal**: Release-ready scripting system.

| Task | Priority | Complexity | Description |
|------|----------|------------|-------------|
| PAK script packaging | 🟡 High | Medium | Engine scripts in `pak_N.pak` |
| Script signature verification | 🟢 Normal | High | Signed engine scripts |
| Script profiler | 🟢 Normal | Medium | Performance monitoring |
| File watcher hot-reload | 🟢 Normal | Medium | Auto-reload on file change (Debug) |

---

## API Versioning

Scripts must declare their required API version for forward compatibility:

```lua
-- Required at script start
api_version(1)

-- Engine checks:
-- - If script version > engine version: error "Script requires newer API"
-- - If script version < engine version: warn, run in compatibility mode
```

**Version history**:
| Version | Changes |
|---------|---------|
| 1 | Initial API (game, world, player, timer) |
| 2 | [FUTURE] Extended events, economy API |
| 3 | [FUTURE] Entity API |

---

## File Storage & Security

### Directory Structure

User scripts are stored **separately from map files** for easier development and security:

```
game_root/
├── maps/
│   ├── bedwars_classic.rfmap      # Map geometry + metadata (NO script content)
│   ├── bedwars_duos.rfmap
│   └── custom_map.rfmap
│
├── scripts/
│   ├── maps/                       # User scripts (per-map)
│   │   ├── bedwars_classic/
│   │   │   ├── main.lua           # Entry point
│   │   │   └── helpers.lua        # Optional modules
│   │   ├── bedwars_duos/
│   │   │   └── main.lua
│   │   └── custom_map/
│   │       └── main.lua
│   │
│   └── engine/                     # Engine scripts (trusted)
│       ├── init.lua
│       └── bedwars/
│           ├── core.lua
│           └── ...
│
└── pak_0.pak                       # Release: engine scripts packaged here
```

### Map File Script Metadata

The `.rfmap` file stores **only metadata**, not script content:

```cpp
// In .rfmap file header
struct MapScriptMetadata {
    // Script reference
    std::string scriptDir;           // "bedwars_classic" → scripts/maps/bedwars_classic/
    std::string entryPoint;          // "main.lua" (default)
    
    // Security hashes (SHA-256)
    struct ScriptHash {
        std::string filename;        // "main.lua"
        std::array<uint8_t, 32> hash; // SHA-256 of file content
    };
    std::vector<ScriptHash> hashes;
    
    // API version required
    uint32_t apiVersion{1};
    
    // Total script size (for limits)
    uint32_t totalSizeBytes{0};
};
```

### Hash Verification Flow

```
┌─────────────────────────────────────────────────────────────────┐
│                        Server                                   │
│  1. Load map: bedwars_classic.rfmap                            │
│  2. Read scriptDir: "bedwars_classic"                          │
│  3. Load scripts from: scripts/maps/bedwars_classic/           │
│  4. Compute SHA-256 of each .lua file                          │
│  5. Compare with hashes in .rfmap                              │
│     ├─ Match: ✅ Load and execute scripts                      │
│     └─ Mismatch: ❌ Reject, log "Script tampered"             │
└─────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────┐
│                        Client                                   │
│  1. Server sends: MapInfo { mapId, scriptHashes[] }            │
│  2. Client checks local scripts against hashes                  │
│     ├─ Match: ✅ Continue                                       │
│     └─ Mismatch: ❌ Download correct scripts from server       │
│                   or reject connection                          │
└─────────────────────────────────────────────────────────────────┘
```

### Security Implementation

```cpp
// shared/scripting/script_hash.hpp

#include <array>
#include <string>
#include <vector>
#include <filesystem>

namespace shared::scripting {

using SHA256Hash = std::array<uint8_t, 32>;

struct ScriptFileHash {
    std::string relativePath;  // "main.lua" or "helpers/utils.lua"
    SHA256Hash hash;
};

struct ScriptManifest {
    std::string scriptDir;
    std::vector<ScriptFileHash> files;
    uint32_t apiVersion{1};
    
    // Compute hashes for all .lua files in directory
    static ScriptManifest from_directory(const std::filesystem::path& dir);
    
    // Verify files match stored hashes
    bool verify(const std::filesystem::path& baseDir) const;
    
    // Serialization for .rfmap
    void serialize(std::ostream& out) const;
    static ScriptManifest deserialize(std::istream& in);
};

// Hash computation
SHA256Hash compute_file_hash(const std::filesystem::path& file);
SHA256Hash compute_string_hash(const std::string& content);

// Compare hashes
bool hashes_equal(const SHA256Hash& a, const SHA256Hash& b);

} // namespace shared::scripting
```

```cpp
// server/scripting/script_loader.hpp

namespace server::scripting {

enum class ScriptLoadResult {
    Success,
    DirectoryNotFound,
    EntryPointMissing,
    HashMismatch,
    ValidationFailed,
    SizeLimitExceeded,
};

struct ScriptLoadError {
    ScriptLoadResult result;
    std::string details;  // e.g., "main.lua hash mismatch"
};

class ScriptLoader {
public:
    // Load scripts for a map, verify hashes
    std::expected<MapScriptData, ScriptLoadError> 
    load_map_scripts(const std::filesystem::path& scriptsBase,
                     const ScriptManifest& manifest);
    
    // Generate manifest for map editor
    ScriptManifest generate_manifest(const std::filesystem::path& scriptDir);
    
private:
    static constexpr size_t kMaxTotalScriptSize = 1024 * 1024;  // 1 MB limit
};

} // namespace server::scripting
```

### Map Editor Integration

When saving a map in the editor:

```cpp
void MapEditor::save_map(const std::filesystem::path& mapPath) {
    // 1. Save map geometry to .rfmap
    save_geometry(mapPath);
    
    // 2. Get script directory name from map name
    std::string scriptDir = mapPath.stem().string();  // "my_map.rfmap" → "my_map"
    
    // 3. If scripts exist, compute and store hashes
    auto scriptsPath = scripts_base_path() / "maps" / scriptDir;
    if (std::filesystem::exists(scriptsPath)) {
        auto manifest = ScriptManifest::from_directory(scriptsPath);
        save_script_metadata(mapPath, manifest);
    }
}
```

### Hot-Reload in Debug Mode

```cpp
// Debug only: skip hash verification, enable file watching
#ifdef RAYFLOW_DEBUG
    bool verify_hashes = false;  // Allow script editing without re-saving map
    enable_file_watcher(scriptDir);  // Auto-reload on change
#else
    bool verify_hashes = true;   // Production: strict verification
#endif
```

---

## File Format Details

### MapScriptMetadata Binary Format (in .rfmap)

```
┌──────────────────────────────────────────────────────────────┐
│ MapScriptMetadata Section                                    │
├──────────────────────────────────────────────────────────────┤
│ uint32_t  sectionSize          # Total section size          │
│ uint8_t   scriptDirLen         # Length of scriptDir string  │
│ char[]    scriptDir            # "bedwars_classic"           │
│ uint8_t   entryPointLen        # Length of entryPoint        │
│ char[]    entryPoint           # "main.lua"                  │
│ uint32_t  apiVersion           # Required API version        │
│ uint32_t  totalSizeBytes       # Total script size           │
│ uint16_t  fileCount            # Number of script files      │
│ ScriptHash[fileCount]          # Hash entries                │
├──────────────────────────────────────────────────────────────┤
│ ScriptHash Entry:                                            │
│   uint8_t   filenameLen                                      │
│   char[]    filename           # "main.lua"                  │
│   uint8_t[32] sha256           # SHA-256 hash                │
└──────────────────────────────────────────────────────────────┘
```

### Engine Scripts (pak_N.pak)

Engine scripts remain in PAK archives for release builds:

```
pak_0.pak/
├── scripts/
│   └── engine/
│       ├── init.lua
│       └── bedwars/
│           ├── core.lua
│           ├── teams.lua
│           └── economy.lua
└── ...
```

Engine scripts are **not hash-verified per-map** — they're signed at the PAK level (future feature).

---

## Testing

### Unit Tests Required
- Sandbox validation (forbidden functions detected)
- Memory limit enforcement
- Instruction limit enforcement
- Time limit enforcement
- Command queue correctness
- Timer firing accuracy
- Event hook invocation
- **SHA-256 hash computation correctness**
- **Hash verification pass/fail scenarios**
- **Script manifest serialization roundtrip**

### Integration Tests Required
- User script loads from scripts/maps/ directory
- Hash mismatch rejects script loading
- Engine script in pak loads and runs
- User script cannot access engine API
- Engine script can access all APIs
- Script errors don't crash server
- **Client with modified script gets rejected**
- **Map editor generates correct hashes**

## References

- **sol2 documentation**: https://sol2.readthedocs.io/
- **Lua 5.4 manual**: https://www.lua.org/manual/5.4/
- **Valve VScripts**: https://developer.valvesoftware.com/wiki/VScript
- **SHA-256 (OpenSSL/crypto++)**: For hash computation
