# Rayflow Project Rules

This is a voxel-based BedWars-like game with authoritative server architecture.

## Current Project State (Dec 2025)

- Input is **server-authoritative**: client captures input and sends intent frames
- Movement/physics is **server-authoritative**: Server runs a fixed tick loop (default 30 TPS), simulates gravity + jump + basic AABB vs voxel blocks, sends `StateSnapshot` with authoritative player position
- Rendering is **client-only**: Client interpolates to authoritative positions and renders, does **not** run local physics for the player
- World generation is currently a **temporary seed-based terrain placeholder** for migration
- Shared voxel block IDs/constants live in `shared/voxel/block.hpp` and must stay stable

## Non-negotiable Architecture Rules

1. **Authoritative server + thin client**: Server executes all simulation, rules, validation, match timing. Client does rendering/UI/audio + captures input and sends commands.
2. **Client never directly reads/writes server state**
3. **One protocol for offline and online**: "Singleplayer" is the same server, connected via LocalTransport
4. **No special offline logic branch**
5. **Do not add prediction/lag compensation** until the refactor reaches Stage C

## Mandatory Code Layering

The codebase must be physically separated into:
- `shared/` — protocol/types/ids/enums/serialization
- `server/` — match/world/rules/validators/replication
- `client/` — render/ui/input/client replica

**Boundary**: `client/` must not depend on `server/` (directly or indirectly). Only `shared/`.

### Layering Dependencies (Hard Rules)

- `client/` must never include or link against anything in `server/`
- `server/` must not depend on `raylib` (keep it headless). No `raylib.h` includes in `server/` or `shared/`
- Any shared IDs/enums/constants used by both sides must live in `shared/`
- Avoid "copying" shared concepts into both client and server (e.g., block IDs). Use one shared definition

## BedWars Scope Constraints

- No infinite procedural world, no persistent world saves
- Maps are loaded from templates/prefabs (schematic-like)
- Match restart is implemented by re-instantiating the map template

Note: seed-based procedural terrain is allowed only as a temporary migration aid.

## Performance Rules (Hard)

### Server Tick Performance
- No allocations in the hot tick path: avoid per-tick `new`, `std::vector` growth, `std::string` construction
- Prefer fixed-size or reserved buffers; reuse containers
- Avoid `iostream` on server in hot paths; prefer lightweight logging with throttling
- Keep math/simple structs POD where possible; minimize virtual dispatch in tick
- Any O(N*M) loops must have a clear bound and be justified

### Determinism & Reproducibility
- Simulation time is integer `serverTick` with fixed `dt = 1/tickRate`
- Do not use client-provided time/positions as truth
- Avoid global RNG (`std::rand`, `std::srand`) and time-based randomness inside simulation. Use explicit, local PRNG state seeded by server-controlled values

## Protocol Rules (Hard)

- Protocol changes must be backwards-conscious: additive fields are preferred over breaking renames
- Bump protocol version for incompatible changes
- Avoid sending large world state every tick
- Client→server messages are intents only; server→client messages are authoritative results

## Replication Model (Hard)

- Client state must be rebuildable from server snapshots/events
- Prefer serverTick-stamped snapshots and avoid relying on client frame rate
- Do not add prediction/lag compensation until Stage C

## Logging (Hard)

- Server must log: receive command → validate → applied/rejected (+ reason)
- Logging must be rate-limited; do not spam every tick unless explicitly debugging

## Related Rules

See other rule files for detailed specifications:
- `architecture` — detailed architecture rules
- `protocol-transport` — transport and protocol specs
- `server-simulation` — server simulation rules
- `comments` — commenting conventions
- `bedwars-gameplay` — gameplay systems
- `migration-plan` — migration stages
- `lighting-minecraft` — Minecraft-style lighting
- `lighting-baked` — baked lighting (legacy)
- `map-editor` — map template and editor
- `testing` — testing guidelines
- `gui` — GUI approach
- `ui-framework` — UI framework details
- `resources` — VFS and asset packaging
- `build-system` — CMake and dependencies
- `config` — configuration file format

