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

### Layering

1. **`shared/scripting/`** — No dependencies on server/ or client/. No raylib.
2. **`server/scripting/`** — May depend on shared/. No raylib. No client/.
3. **`ui/scripting/`** — May depend on shared/. May use raylib for UI commands.
4. **Never execute user scripts on client** — All game logic runs on server.

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

## File Format

### MapScriptData (in .rfmap)

```cpp
struct MapScriptData {
    std::string mainScript;           // Entry point script
    std::vector<Module> modules;      // Additional modules
    std::uint32_t version{1};         // API version for compatibility
};
```

### PAK Script Layout

```
pak_0.pak/
├── scripts/
│   ├── init.lua              # Entry point, loaded first
│   ├── bedwars/
│   │   ├── core.lua          # Core BedWars logic
│   │   ├── teams.lua         # Team management
│   │   ├── economy.lua       # Generators, shop
│   │   └── events.lua        # Event handlers
│   └── utils/
│       ├── math.lua          # Math utilities
│       └── table.lua         # Table utilities
└── ...
```

## Testing

### Unit Tests Required
- Sandbox validation (forbidden functions detected)
- Memory limit enforcement
- Instruction limit enforcement
- Time limit enforcement
- Command queue correctness
- Timer firing accuracy
- Event hook invocation

### Integration Tests Required
- User script in .rfmap loads and runs
- Engine script in pak loads and runs
- User script cannot access engine API
- Engine script can access all APIs
- Script errors don't crash server

## References

- **sol2 documentation**: https://sol2.readthedocs.io/
- **Lua 5.4 manual**: https://www.lua.org/manual/5.4/
- **Valve VScripts**: https://developer.valvesoftware.com/wiki/VScript
