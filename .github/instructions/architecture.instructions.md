# Architecture (authoritative server + thin client)

## Goal
Move the project to an authoritative-server architecture where **all gameplay rules and world simulation live on the server**, and the client is responsible for **input capture + rendering/UI** only.

## Hard invariants (never violate)
1. **Client does not read or mutate server state directly.** No shared pointers/references to server `World`/`Match` from the client.
2. **One message protocol for both offline and online.** Offline mode is the same server, connected via `LocalTransport`.
3. **Server is the single source of truth** for positions, damage, purchases, block changes, spawns/despawns, bed state.
4. **No separate “offline logic path”.** If it works offline, it must work through transport.

## Mandatory layering
Physically split code into 3 layers:

- `shared/`
  - Protocol message types (requests/responses/events/snapshots)
  - Entity IDs: `PlayerId`, `EntityId`, `MatchId`, `ChunkId`
  - Serialization helpers and protocol versioning
  - Shared enums/constants: block types, item types, teams, death reasons, rejection reasons

- `server/`
  - Authoritative match engine: `Match`, `World`, `Rules`
  - Action validators (place/break/hit/buy/use)
  - Connection manager abstraction (transport-agnostic)
  - Replication logic (snapshots/events) + server logs/metrics

- `client/`
  - Rendering, camera, sound, UI
  - Input capture and sending input-commands via transport
  - `ClientWorld` / `RenderState` replicated from server messages

### Boundary rule
- `client/` **must not** include/import anything from `server/` (directly or indirectly). Only `shared/`.

## ECS / world ownership rule
- Server owns authoritative ECS/world state.
- Client owns a **render-only** representation (`ClientWorld`) built from snapshots/events.
- No “two truths”: client state is always derived from server updates.

## World format requirements (BedWars maps)
- No procedural infinite world. Maps are **loaded from templates/prefabs** (schematic-like format).
- Match restart is done by **re-instantiating the map from template**.

## Anti-scope creep
- Do not implement prediction/lag-compensation until migration Stage C is done.
- Do not add a special singleplayer branch with direct calls into server code.
