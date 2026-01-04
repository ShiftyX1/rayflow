# Architecture (authoritative server + thin client)

## Goal
Move the project to an authoritative-server architecture where **all gameplay rules and world simulation live on the server**, and the client is responsible for **input capture + rendering/UI** only.

## Hard invariants (never violate)
1. **Client does not read or mutate server state directly.** No shared pointers/references to server `World`/`Match` from the client.
2. **One message protocol for both offline and online.** Offline mode is the same server, connected via `LocalTransport`.
3. **Server is the single source of truth** for positions, damage, purchases, block changes, spawns/despawns, bed state.
4. **No separate "offline logic path".** If it works offline, it must work through transport.

## Mandatory layering
Physically split code into engine and game layers:

**Engine layer** (`engine/`):
- `core/` — ServerEngine, ClientEngine, base types
- `transport/` — IClientTransport, IServerTransport, LocalTransport, ENet
- `ecs/` — Entity Component System
- `maps/` — Map I/O (rfmap format)
- `renderer/` — Rendering abstractions
- `ui/` — UI framework (XML + CSS)
- `vfs/` — Virtual filesystem, PAK support
- `modules/voxel/` — Voxel engine (block types, chunks, meshing)
- `tools/` — Utility tools (pack_assets)

**Game layer** (`games/bedwars/`):
- `shared/` — Protocol message types (requests/responses/events/snapshots), entity IDs, serialization, shared enums/constants
- `server/` — Authoritative match engine, action validators, replication logic
- `client/` — Rendering, camera, sound, UI, input capture, ClientWorld

### Boundary rule
- `games/*/client/` **must not** include/import anything from `games/*/server/` (directly or indirectly). Only `shared/` and `engine/`.
- `games/*/server/` **must not** depend on raylib. Keep it headless.

## ECS / world ownership rule
- Server owns authoritative ECS/world state.
- Client owns a **render-only** representation (`ClientWorld`) built from snapshots/events.
- No "two truths": client state is always derived from server updates.

## World format requirements (BedWars maps)
- No procedural infinite world. Maps are **loaded from templates/prefabs** (schematic-like format).
- Match restart is done by **re-instantiating the map from template**.

## Client rendering systems

### Block interaction (`games/bedwars/client/voxel/block_interaction.hpp`)
- `BlockInteraction` handles:
  - Raycasting from camera to find targeted block
  - Break progress accumulation (client-side visual only)
  - Rendering block highlight wireframe
  - Rendering break overlay with stage textures
  - Emitting `BreakRequest` / `PlaceRequest` for server validation
- Break overlay textures:
  - Located at `resources/textures/destroy_stages/destroy_stage_0..9.png`
  - 16x16 RGBA PNGs with transparent background and black crack patterns
  - Rendered using `rlSetTexture` + `RL_QUADS` for each cube face

### Block registry (`engine/modules/voxel/block.hpp`)
- Block type IDs and metadata shared between client and server
- Atlas: `resources/textures/terrain.png` (16x16 tiles)
- Provides block info for hardness, tool requirements, texture indices

## Anti-scope creep
- Do not implement prediction/lag-compensation until migration Stage C is done.
- Do not add a special singleplayer branch with direct calls into server code.

