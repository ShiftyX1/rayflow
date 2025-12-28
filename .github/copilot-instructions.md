
# Copilot system instructions for this project

This repository is a voxel-based BedWars-like game.

## Current project state (Dec 2025)
- Input is **server-authoritative**: client captures input and sends intent frames.
- Movement/physics is **server-authoritative**:
	- Server runs a fixed tick loop (default 30 TPS).
	- Server simulates gravity + jump + basic AABB vs voxel blocks.
	- Server sends `StateSnapshot` with authoritative player position.
- **Multiplayer networking is implemented**:
	- `rfds` (RayFlow Dedicated Server) — headless multi-client server using ENet.
	- Client connects via `--connect host:port` flag.
	- ENet transport in `shared/transport/enet_*.hpp/cpp`.
	- `DedicatedServer` class in `server/core/dedicated_server.hpp/cpp`.
	- Singleplayer still uses `LocalTransport` (same protocol).
- Rendering is **client-only**:
	- Client interpolates to authoritative positions and renders.
	- Client does **not** run local physics for the player.
	- Block break overlay uses separate textures (`textures/destroy_stages/destroy_stage_*.png`).
	- `BlockInteraction` manages raycasting, break progress, and overlay rendering.
	- Player HUD is a custom retained-mode UI (XML + CSS-lite) rendered via raylib:
		- Runtime lives in `ui/runtime/xmlui/` and uses `tinyxml2`.
		- HUD loads from `ui/hud.xml` + `ui/hud.css` (copied from `client/static/ui/` into `build/ui/`).
		- HP bar is currently implemented as a `HealthBar` node using textures in `textures/ui/health_bar/`.
		- `UIViewModel::player.health/max_health` exist for HUD; currently filled with a temporary placeholder.
- World generation is currently a **temporary seed-based terrain placeholder** for migration:
	- Server chooses a `worldSeed` and includes it in `ServerHello`.
	- Client uses that seed to render the same terrain as the server collides against.
	- This is *not* the final BedWars map system (see scope constraints below).
- Shared voxel block IDs/constants live in `shared/voxel/block.hpp` and must stay stable.

## Non-negotiable architecture rules
1. Authoritative server + thin client.
	- Server executes all simulation, rules, validation, match timing.
	- Client does rendering/UI/audio + captures input and sends commands.
2. Client never directly reads/writes server state.
3. One protocol for offline and online.
	- “Singleplayer” is the same server, connected via LocalTransport.
4. No special offline logic branch.
5. Do not add prediction/lag compensation until the refactor reaches Stage C.

## Mandatory code layering
The codebase must be physically separated into:
- shared/  (protocol/types/ids/enums/serialization)
- server/  (match/world/rules/validators/replication)
- client/  (render/ui/input/client replica)

Boundary: client/ must not depend on server/ (directly or indirectly). Only shared/.

## BedWars scope constraints
- No infinite procedural world, no persistent world saves.
- Maps are loaded from templates/prefabs (schematic-like).
- Match restart is implemented by re-instantiating the map template.

Note: seed-based procedural terrain is allowed only as a temporary migration aid.

## Strict rules for future code (extensibility + performance)

### Layering + dependencies (hard)
- `client/` must never include or link against anything in `server/`.
- `server/` must not depend on `raylib` (keep it headless). No `raylib.h` includes in `server/` or `shared/`.
- Any shared IDs/enums/constants used by both sides must live in `shared/`.
- Avoid “copying” shared concepts into both client and server (e.g., block IDs). Use one shared definition.

### Server tick performance (hard)
- No allocations in the hot tick path: avoid per-tick `new`, `std::vector` growth, `std::string` construction.
- Prefer fixed-size or reserved buffers; reuse containers.
- Avoid `iostream` on server in hot paths; prefer lightweight logging with throttling.
- Keep math/simple structs POD where possible; minimize virtual dispatch in tick.
- Any O(N*M) loops must have a clear bound and be justified.

### Determinism + reproducibility (hard)
- Simulation time is integer `serverTick` with fixed `dt = 1/tickRate`.
- Do not use client-provided time/positions as truth.
- Avoid global RNG (`std::rand`, `std::srand`) and time-based randomness inside simulation. Use explicit, local PRNG state seeded by server-controlled values.

### Protocol evolution (hard)
- Protocol changes must be backwards-conscious:
	- Additive fields are preferred over breaking renames.
	- Bump protocol version for incompatible changes.
	- Avoid sending large world state every tick.
- Client->server messages are intents only; server->client messages are authoritative results.

### Replication model (hard)
- Client state must be rebuildable from server snapshots/events.
- Prefer serverTick-stamped snapshots and avoid relying on client frame rate.
- Do not add prediction/lag compensation until Stage C.

### Logging (hard)
- Server must log: receive command -> validate -> applied/rejected (+ reason).
- Logging must be rate-limited; do not spam every tick unless explicitly debugging.

## Instruction files
When making changes, follow the detailed rules in `.cursor/rules/`:
- `.cursor/rules/main/RULE.md` — main project rules (summary)
- `.cursor/rules/architecture/RULE.md` — architecture rules
- `.cursor/rules/protocol-transport/RULE.md` — transport and protocol specs
- `.cursor/rules/server-simulation/RULE.md` — server simulation rules
- `.cursor/rules/dedicated-server/RULE.md` — RFDS dedicated server implementation
- `.cursor/rules/scripting/RULE.md` — Lua scripting system (two-tier: user + engine)
- `.cursor/rules/comments/RULE.md` — commenting conventions
- `.cursor/rules/bedwars-gameplay/RULE.md` — gameplay systems
- `.cursor/rules/migration-plan/RULE.md` — migration stages
- `.cursor/rules/lighting-minecraft/RULE.md` — Minecraft-style lighting
- `.cursor/rules/lighting-baked/RULE.md` — baked lighting (legacy)
- `.cursor/rules/map-editor/RULE.md` — map template and editor
- `.cursor/rules/testing/RULE.md` — testing guidelines
- `.cursor/rules/gui/RULE.md` — GUI approach
- `.cursor/rules/ui-framework/RULE.md` — UI framework details (AI agent guide)
- `.cursor/rules/resources/RULE.md` — VFS and asset packaging
- `.cursor/rules/build-system/RULE.md` — CMake, dependencies, CI/CD
- `.cursor/rules/config/RULE.md` — configuration file format

Legacy instructions in `.github/instructions/` are kept for reference but superseded by `.cursor/rules/`.

